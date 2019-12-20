#include <atomic>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include <boost/random/mersenne_twister.hpp>

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <poplarlib/collector.grpc.pb.h>

namespace simplemetrics {

using UPStub = std::unique_ptr<poplar::PoplarEventCollector::Stub>;
using UPStream = std::unique_ptr<grpc::ClientWriter<poplar::EventMetrics>>;

class EventStream {
public:
    explicit EventStream(UPStub& stub)
        : _options{},
          _response{},
          _context{},
          // multiple streams can write to the same collector name
          _stream{stub->StreamEvents(&_context, &_response)} {
        _options.set_no_compression().set_buffer_hint();
    }

    void write(const poplar::EventMetrics& event) {
        auto success = _stream->Write(event, _options);
        if (!success) {
            std::cout << "Couldn't write: stream was closed";
            throw std::bad_function_call();
        }
    }

    ~EventStream() {
        std::cout << "Closing EventStream." << std::endl;
        if (!_stream) {
            std::cout << "No _stream." << std::endl;
            return;
        }
        std::cout << "Calling Finish." << std::endl;
        if (!_stream->WritesDone()) {
            // TODO: barf
            std::cout << "Errors in doing the writes?" << std::endl;
        }
        auto status = _stream->Finish();
        std::cout << "Called Finish." << std::endl;
        if (!status.ok()) {
            std::cout << "Problem closing the stream:\n"
                      << _context.debug_error_string() << std::endl;
        } else {
            std::cout << "Closed stream" << std::endl;
        }
    }

private:
    grpc::WriteOptions _options;
    poplar::PoplarResponse _response;
    grpc::ClientContext _context;
    UPStream _stream;
};

class Collector {
public:
    Collector(const Collector&) = delete;

    explicit Collector(UPStub& stub, std::string name)
        : _name{std::move(name)}, _stub{stub}, _id{} {
        _id.set_name(_name);

        grpc::ClientContext context;
        poplar::PoplarResponse response;
        poplar::CreateOptions options = createOptions(_name);
        auto status = _stub->CreateCollector(&context, options, &response);
        if (!status.ok()) {
            std::cout << "Status not okay\n" << status.error_message();
            throw std::bad_function_call();
        }
        std::cout << "Created collector with options\n" << options.DebugString() << std::endl;
    }

    ~Collector() {
        std::cout << "Closing collector." << std::endl;
        if (!_stub) {
            return;
        }
        grpc::ClientContext context;
        poplar::PoplarResponse response;
        // causes flush to disk etc
        auto status = _stub->CloseCollector(&context, _id, &response);
        if (!status.ok()) {
            std::cout << "Couldn't close collector: " << status.error_message();
        }
        std::cout << "Closed collector with id \n" << _id.DebugString() << std::endl;
    }

private:
    static auto createPath(const std::string& name) {
        std::stringstream str;
        str << name << ".ftdc";
        return str.str();
    }

    static poplar::CreateOptions createOptions(const std::string& name) {
        poplar::CreateOptions options;
        options.set_name(name);
        options.set_events(poplar::CreateOptions_EventsCollectorType_BASIC);
        // end in .ftdc -- each stream should have a different path -- unique id for the stream
        options.set_path(createPath(name));
        // how many events between compression and write events
        options.set_chunksize(1000);  // probably not less than 10k maybe more - play with it; less
        // means more time in cpu&io to compress
        // flush to disk intermittently
        options.set_streaming(true);
        // dynamic means shape changes over time
        options.set_dynamic(false);
        // may not matter but shrug it works
        options.set_recorder(poplar::CreateOptions_RecorderType_PERF);
        options.set_events(poplar::CreateOptions_EventsCollectorType_BASIC);
        return options;
    }

    std::string _name;
    // _id should always be the same as the name in the options to createcollector
    poplar::PoplarID _id;

public:
    UPStub& _stub;
};

class OperationImpl {
    static poplar::EventMetrics createMetricsEvent(const std::string& name) {
        poplar::EventMetrics out;
        // only need to send this the first time
        out.set_name(name);
        reset(out, false);
        return out;
    }

    static void reset(poplar::EventMetrics& out, bool clearName) {
        if (clearName) {
            // only need to send this the first time
            out.clear_name();
        }

        out.mutable_timers()->mutable_duration()->set_nanos(0);
        out.mutable_timers()->mutable_duration()->set_seconds(0);

        out.mutable_counters()->set_errors(0);
        // increment number every time the Actor sends an event - it's incremented once per
        // iteration of the test
        out.mutable_counters()->set_number(0);
        // ops is number of things done in that iteration
        out.mutable_counters()->set_ops(0);
        // bytes
        out.mutable_counters()->set_size(0);

        // phase of test
        out.mutable_gauges()->set_state(0);
        // threads
        out.mutable_gauges()->set_workers(0);
        out.mutable_gauges()->set_failed(false);

        // TODO: what's the difference between time and timers?
        out.mutable_time()->set_seconds(0);
        out.mutable_time()->set_nanos(0);
    }

public:
    explicit OperationImpl(const std::string& name, UPStub& stub) : _storage{createMetricsEvent(name)}, _stream{std::make_unique<EventStream>(stub)} {}

    void success() {
        this->report();
        reset(_storage, false);
    }

    void addDocuments(int docs) {
        _storage.mutable_counters()->set_number(_storage.mutable_counters()->number() + docs);
    }

    void addBytes(int bytes) {
        _storage.mutable_counters()->set_size(_storage.mutable_counters()->size() + bytes);
    }

    void report() {
        _stream->write(_storage);
    }

private:
    poplar::EventMetrics _storage;
    std::unique_ptr<EventStream> _stream;
};

class OperationContext {
public:
    explicit OperationContext(OperationImpl& impl) : _impl{std::addressof(impl)} {}

    void success() {
        _impl->success();
    }

    void addBytes(int bytes) {
        _impl->addBytes(bytes);
    }

    void addDocuments(int docs) {
        _impl->addDocuments(docs);
    }

private:
    OperationImpl* _impl;
};

class Operation {
public:
    explicit Operation(OperationImpl& impl) : _impl{std::addressof(impl)} {}

    OperationContext start() {
        return OperationContext{*_impl};
    }

private:
    OperationImpl* _impl;
};

class CollectorAndOps {
public:
    template <typename... Args>
    explicit CollectorAndOps(std::string name, Args&&... args)
        : _name{std::move(name)},
          _collector{std::make_unique<Collector>(std::forward<Args>(args)...)},
          _ops{} {}

    Operation operation(int actorId) {
        const auto& impl = _ops.try_emplace(actorId, _name, _collector->_stub);
        return Operation{impl.first->second};
    }

private:
    std::string _name;
    std::unique_ptr<Collector> _collector;
    // actorId => ops
    std::unordered_map<int, OperationImpl> _ops;
};

static auto actorDotOp(const std::string& actorName, const std::string& opName) {
    std::stringstream str;
    str << actorName << "." << opName;
    return str.str();
}

class Registry {
public:
    explicit Registry() : _stub{createCollectorStub()} {}

    Operation operation(const std::string& actorName, const std::string& opName, int actorId) {
        std::string name = actorDotOp(actorName, opName);
        std::cout << "Creating operation " << name << std::endl;
        if (auto it = _collectors.find(name); it == _collectors.end()) {
            // lolz
            auto inserted = _collectors.insert({name, CollectorAndOps{name, _stub, name}});
            return inserted.first->second.operation(actorId);
        } else {
            return it->second.operation(actorId);
        }
    }

private:
    static UPStub createCollectorStub() {
        auto channel = grpc::CreateChannel("localhost:2288", grpc::InsecureChannelCredentials());
        return poplar::PoplarEventCollector::NewStub(channel);
    }

    UPStub _stub;
    std::unordered_map<std::string, CollectorAndOps> _collectors{};
};

}  // namespace simplemetrics
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

auto randomSuffix(const std::string& prefix) {
    boost::random::mt19937_64 rng;
    rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    std::stringstream out;
    out << prefix << rng();
    return out.str();
}

auto randomPath() {
    return randomSuffix("run");
}

auto randomName() {
    return randomSuffix("InsertRemove.Insert");
}

poplar::CreateOptions createOptions(const std::string& name) {
    poplar::CreateOptions options;
    options.set_name(name);
    options.set_events(poplar::CreateOptions_EventsCollectorType_BASIC);
    // end in .ftdc -- each stream should have a different path -- unique id for the stream
    options.set_path(randomPath());
    // how many events between compression and write events
    options.set_chunksize(10000);  // probably not less than 10k maybe more - play with it; less
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

using UPStub = std::unique_ptr<poplar::PoplarEventCollector::Stub>;
using UPStream = std::unique_ptr<grpc::ClientWriter<poplar::EventMetrics>>;

UPStub createCollectorStub() {
    auto channel = grpc::CreateChannel("localhost:2288", grpc::InsecureChannelCredentials());
    return poplar::PoplarEventCollector::NewStub(channel);
}

class EventStream {
public:
    explicit EventStream(UPStub& stub)
            : _response{},
              _context{},
            // multiple streams can write to the same collector name
              _stream{stub->StreamEvents(&_context, &_response)} {
        std::cout << "Created stream. response:\n" << _response.DebugString() << std::endl;
    }

    void write(const poplar::EventMetrics& event) {
        auto success = _stream->Write(event);
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
    poplar::PoplarResponse _response;
    grpc::ClientContext _context;
    UPStream _stream;
};

class Collector {
public:
    explicit Collector(UPStub& stub, const std::string& name) : _stub{stub}, _id{} {
        _id.set_name(name);

        grpc::ClientContext context;
        poplar::PoplarResponse response;
        poplar::CreateOptions options = createOptions(name);
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
    UPStub& _stub;
    // _id should always be the same as the name in the options to createcollector
    poplar::PoplarID _id;
};

class OperationImpl {
    // TODO
    enum class State {
        kOpen,
        kClosed,
    };
    static std::string makeName(const std::string& actorName, const std::string& opName, int actorId) {
        std::stringstream str;
        str << actorName << "." << opName << "." << actorId;
        return str.str();
    }

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
    OperationImpl(const std::string& actorName, const std::string& opName, int actorId, UPStub& stub)
            : _state{State::kClosed},
              _name{makeName(actorName, opName, actorId)},
              _collector{stub, _name},
              _stream{stub},
              _storage{createMetricsEvent(_name)} {}

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
        _stream.write(_storage);
    }

private:
    State _state;
    std::string _name;
    poplar::EventMetrics _storage;
    Collector _collector;
    EventStream _stream;
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

class Registry {
private:
    // Thread -> EventStream
    using OperationsByThread = std::unordered_map<int /*ActorId*/, OperationImpl>;
    // OperationName -> all threads for that OperationName
    using OperationsByType = std::unordered_map<std::string, OperationsByThread>;
    // OperationsMap is a map of
    // actor name -> operation name -> actor id -> OperationImpl (time series).
    using OperationsMap = std::unordered_map<std::string, OperationsByType>;

public:
    explicit Registry(UPStub stub) : _stub{std::move(stub)}, _ops{} {}

    Operation operation(const std::string& actorName, const std::string& opName, int actorId) {
        auto& opsByType = this->_ops[actorName];
        auto& opsByThread = opsByType[opName];
        // opIt is pair<actorid,operationImpl>
        auto opIt = opsByThread.try_emplace(actorId, actorName, opName, actorId, _stub).first;
        return Operation{opIt->second};
    }

private:
    UPStub _stub;
    OperationsMap _ops;
};

}  // namespace simplemetrics
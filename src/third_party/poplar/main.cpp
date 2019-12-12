
#include <cstdlib>
#include <iostream>
#include <sstream>

#include <boost/random/mersenne_twister.hpp>

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <poplarlib/collector.grpc.pb.h>

poplar::EventMetrics createMetricsEvent(const std::string& name) {
    poplar::EventMetrics out;
    // TODO: what's this versus the collector name?
    out.set_name(name);
    out.mutable_timers()->mutable_duration()->set_nanos(100);
    out.mutable_timers()->mutable_duration()->set_seconds(30);

    out.mutable_counters()->set_errors(0);
    // increment number every time the Actor sends an event - it's incremented once per iteration of
    // the test
    out.mutable_counters()->set_number(1);
    // ops is number of things done in that iteration
    out.mutable_counters()->set_ops(1);
    // bytes
    out.mutable_counters()->set_size(32893043);

    // phase of test
    out.mutable_gauges()->set_state(1);
    // threads
    out.mutable_gauges()->set_workers(1);
    out.mutable_gauges()->set_failed(false);

    out.mutable_time()->set_seconds(50000);
    out.mutable_time()->set_nanos(30);
    return out;
}

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
        if (!_stream) {
            return;
        }
        auto status = _stream->Finish();
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


int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // disable output buffering
    std::cout.rdbuf()->pubsetbuf(nullptr, 0);

    const std::string name = randomName();

    auto stub = createCollectorStub();
    auto collector = Collector(stub, name);
    auto stream = EventStream(stub);

    for (unsigned int i = 1; i <= 10000; ++i) {
        // TODO: if we're running out of air, only 'need' to send the name in the first event sent
        // on the stream
        auto event = createMetricsEvent(name);
        stream.write(event);
        if (i % 5000 == 0) {
            std::cout << "Wrote " << i << " events. Latest is \n"
                      << event.DebugString() << std::endl;
        }
    }

    return EXIT_SUCCESS;
}

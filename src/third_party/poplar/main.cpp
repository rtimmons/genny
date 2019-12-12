
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
    out.mutable_counters()->set_number(1);
    out.mutable_counters()->set_ops(1);

    out.mutable_gauges()->set_state(1);
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

poplar::CreateOptions createOptions() {
    poplar::CreateOptions options;
    options.set_name(randomName());
    options.set_path(randomPath());
    options.set_chunksize(10000);
    options.set_streaming(true);
    options.set_dynamic(false);
    options.set_recorder(poplar::CreateOptions_RecorderType_PERF);
    return options;
}

using UPStub = std::unique_ptr<poplar::PoplarEventCollector::Stub>;
using UPStream = std::unique_ptr<grpc::ClientWriter<poplar::EventMetrics>>;

UPStub createCollectorStub() {
    auto channel = grpc::CreateChannel("localhost:2288",
                                       grpc::InsecureChannelCredentials());
    return poplar::PoplarEventCollector::NewStub(channel);
}

class EventStream {
public:
    explicit EventStream(UPStub& stub)
    : _response{},
      _context{},
      _stream{stub->StreamEvents(&_context, &_response)}
      {}

    void write(const poplar::EventMetrics& event) {
        auto success = _stream->Write(event);
        if (!success) {
            std::cout << "Couldn't write: stream was closed";
            throw std::bad_function_call();
        }
    }

    ~EventStream() {
        _stream->Finish();
    }
private:
    poplar::PoplarResponse _response;
    grpc::ClientContext _context;
    UPStream _stream;
};

class Collector {
public:
    explicit Collector(UPStub& stub, const std::string name)
    : _stub{stub},
     _id{} {
        std::cout << "Creating collector" << std::endl;
        _id.set_name(name);

        grpc::ClientContext context;
        poplar::PoplarResponse response;
        poplar::CreateOptions options = createOptions();
        auto status = _stub->CreateCollector(&context, options, &response);
        if (!status.ok()) {
            std::cout << "Status not okay\n" << status.error_message();
            throw std::bad_function_call();
        }
        std::cout << "Created collector" << std::endl;
    }

    ~Collector() {
        std::cout << "Closing collector" << std::endl;
        grpc::ClientContext context;
        poplar::PoplarResponse response;
        auto status = _stub->CloseCollector(&context, _id, &response);
        if (!status.ok()) {
            std::cout << "Couldn't close collector: " << status.error_message();
        }
        std::cout << "Closed collector" << std::endl;
    }

private:
    UPStub& _stub;
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

    auto event = createMetricsEvent(name);
    stream.write(event);

    return EXIT_SUCCESS;
}
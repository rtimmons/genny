
#include <cstdlib>
#include <iostream>

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <poplarlib/collector.grpc.pb.h>

constexpr auto name = "InsertRemove.Insert";


poplar::EventMetrics createMetricsEvent() {
    poplar::EventMetrics out;
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

poplar::CreateOptions createOptions() {
    poplar::CreateOptions options;
    options.set_name(name);
    options.set_path("t1");
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

void callCreateCollector(UPStub& stub) {
    poplar::CreateOptions options = createOptions();

    grpc::ClientContext context;
    poplar::PoplarResponse response;
    auto status = stub->CreateCollector(&context, options, &response);
    if (!status.ok()) {
        std::cout << "Status not okay\n" << status.error_message();
        throw std::bad_function_call();
    }
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


void closeCollector(UPStub& stub) {
    poplar::PoplarID id;
    id.set_name(name);

    grpc::ClientContext context;
    poplar::PoplarResponse response;
    auto status = stub->CloseCollector(&context, id, &response);

    if (!status.ok()) {
        std::cout << "Couldn't close collector: " << status.error_message();
        throw std::bad_function_call();
    }
}


int main() {
    auto stub = createCollectorStub();
    callCreateCollector(stub);

    auto stream = EventStream(stub);
    auto event = createMetricsEvent();
    stream.write(event);

    closeCollector(stub);
    std::cout << "Closed collector" << std::endl;

    return EXIT_SUCCESS;
}

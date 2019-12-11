
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

int main() {
    auto collector =
        poplar::PoplarEventCollector::NewStub(grpc::CreateChannel("localhost:2288", grpc::InsecureChannelCredentials()));

    poplar::CreateOptions options;
    options.set_name(name);
    options.set_path("t1");
    options.set_chunksize(10000);
    options.set_streaming(true);
    options.set_dynamic(false);
    options.set_recorder(poplar::CreateOptions_RecorderType_PERF);

    grpc::ClientContext context;
    poplar::PoplarResponse response;
    auto status = collector->CreateCollector(&context, options, &response);
    if (!status.ok()) {
        std::cout << "Status not okay\n" << status.error_message();
        return EXIT_FAILURE;
    }

    poplar::PoplarID id;
    id.set_name(name);

    poplar::EventMetrics out = createMetricsEvent();

    grpc::ClientContext context2;
    std::cout << "Starting stream" << std::endl;
    auto stream = collector->StreamEvents(&context2, &response);
    std::cout << "Created stream" << std::endl;
    auto success = stream->Write(out);
    if (!success) {
        std::cout << "Couldn't write because " << response.DebugString();
        return EXIT_FAILURE;
    }

    grpc::ClientContext context3;
    collector->CloseCollector(&context3, id, &response);

    std::cout << "Closed stream" << std::endl;
    return EXIT_SUCCESS;
}

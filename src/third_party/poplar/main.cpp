
#include <cstdlib>
#include <iostream>

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <poplarlib/collector.grpc.pb.h>

constexpr auto name = "InsertRemove.Insert";


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

    google::protobuf::Duration duration;
    duration.set_seconds(32);
    duration.set_nanos(100);
    poplar::EventMetricsTimers timers;
    timers.set_allocated_duration(&duration);

    google::protobuf::Timestamp timestamp;
    timestamp.set_seconds(50000);
    timestamp.set_nanos(0);

    poplar::EventMetricsCounters counters;
    counters.set_errors(0);
    counters.set_number(1);
    counters.set_ops(1);

    poplar::EventMetricsGauges gauges;
    gauges.set_state(1);
    gauges.set_workers(1);
    gauges.set_failed(0);

    poplar::EventMetrics out;
    out.set_name(name);
    out.set_allocated_timers(&timers);
    out.set_allocated_time(&timestamp);
    out.set_allocated_gauges(&gauges);

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

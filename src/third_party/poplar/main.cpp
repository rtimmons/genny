
#include <cstdlib>
#include <iostream>

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <poplarlib/collector.grpc.pb.h>

constexpr auto name = "InsertRemove.Insert";


int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

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

    poplar::EventMetrics out;
    out.set_name(name);

    out.mutable_timers()->mutable_duration()->set_seconds(100);
    out.mutable_timers()->mutable_duration()->set_nanos(30);

    out.mutable_counters()->set_errors(0);
    out.mutable_counters()->set_number(1);
    out.mutable_counters()->set_ops(1);

    out.mutable_gauges()->set_state(1);
    out.mutable_gauges()->set_workers(1);
    out.mutable_gauges()->set_failed(false);

    grpc::ClientContext context2;
    std::cout << "Starting stream" << std::endl;
    auto stream = collector->StreamEvents(&context2, &response);
    std::cout << "Created stream" << std::endl;
    for(int i=0; i < 3; ++i) {
        std::cout << "Writing message " << i << std::endl;
        auto success = stream->Write(out);
        if (!success) {
            std::cout << "Couldn't write because " << response.DebugString();
            return EXIT_FAILURE;
        }
    }
    auto status1 = stream->Finish();
    if (!status1.ok()) {
        std::cout << "Failed to finish: " << status1.error_message() << std::endl;
        return EXIT_FAILURE;
    }

    grpc::ClientContext context3;
    auto status2 = collector->CloseCollector(&context3, id, &response);
    if (!status2.ok()) {
        std::cout << "Failed to close: " << status2.error_message() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Closed stream" << std::endl;
    google::protobuf::ShutdownProtobufLibrary();

    return EXIT_SUCCESS;
}

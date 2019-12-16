#include <poplarlib/SimpleMetrics.hpp>

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    using namespace simplemetrics;

    // disable output buffering
    std::cout.rdbuf()->pubsetbuf(nullptr, 0);

    Registry reg{createCollectorStub()};
    auto op = reg.operation("Insert", "InsertRemove", 1);
    std::cout << "Created op." << std::endl;

    for (int i = 0; i < 10000; ++i) {
        auto ctx = op.start();
        ctx.addBytes(1234 + i);
        ctx.success();
    }


    //    auto stub = createCollectorStub();
    //    auto collector = Collector(stub, name);
    //    auto stream = EventStream(stub);
    //
    //    for (unsigned int i = 1; i <= 10000; ++i) {
    //        // TODO: if we're running out of air, only 'need' to send the name in the first event
    //        sent
    //        // on the stream
    //        auto event = createMetricsEvent(name);
    //        stream.write(event);
    //        if (i % 5000 == 0) {
    //            std::cout << "Wrote " << i << " events. Latest is \n"
    //                      << event.DebugString() << std::endl;
    //        }
    //    }
    std::cout << "Done." << std::endl;
    return EXIT_SUCCESS;
}

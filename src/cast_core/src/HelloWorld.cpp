// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cast_core/actors/HelloWorld.hpp>

#include <string>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>

#include <poplarlib/SimpleMetrics.hpp>

namespace genny::actor {

simplemetrics::Registry& reg() {
    static simplemetrics::Registry singleton{};
    return singleton;
}

/** @private */
struct HelloWorld::PhaseConfig {
    std::string message;

    /** record data about each iteration */
    simplemetrics::Operation operation;

//    simplemetrics::Operation syntheticOperation;

    explicit PhaseConfig(PhaseContext& context, ActorId actorId)
        : message{context["Message"].maybe<std::string>().value_or("Hello, World!")},
          operation{reg().operation("HelloWorld", "Write", actorId)}
          // syntheticOperation{other->registry.operation("HelloWorld", "SyntheticOperation", actorId)}
          {}
};

auto& ops() {
    static std::atomic_int out = 0;
    return out;
}

auto& started() {
    static std::atomic<std::chrono::time_point<std::chrono::system_clock>> start =
            std::chrono::system_clock::now();
    return start;
}

auto& reported() {
    static std::atomic_bool report = false;
    return report;
}

void HelloWorld::run() {
    started();
    for (auto&& config : _loop) {
        for (auto _ : config) {
            auto ctx = config->operation.start();
//            BOOST_LOG_TRIVIAL(info) << config->message;
//            ++_helloCounter;
//            BOOST_LOG_TRIVIAL(info) << "Counter: " << _helloCounter;
            ctx.addDocuments(1);
            ctx.addBytes(config->message.size());
            ctx.success();
            ops()++;

//            config->syntheticOperation.report(metrics::clock::now(),
//                                              std::chrono::milliseconds{_helloCounter});
        }
    }
    if(!reported().exchange(true)) { // was false now true
        std::cout << "Took " << std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now() - started().load()).count() << " seconds to do " << ops() << " ops." << std::endl;
    }
}

HelloWorld::HelloWorld(genny::ActorContext& context)
    : Actor(context),
      _helloCounter{WorkloadContext::getActorSharedState<HelloWorld, HelloWorldCounter>()},
      _loop{context, HelloWorld::id()} {}

namespace {
auto registerHelloWorld = genny::Cast::registerDefault<genny::actor::HelloWorld>();
}
}  // namespace genny::actor

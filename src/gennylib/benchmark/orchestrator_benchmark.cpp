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

#include <atomic>
#include <chrono>
#include <thread>

#include <testlib/helpers.hpp>

#include <gennylib/Orchestrator.hpp>

#include <value_generators/DefaultRandom.hpp>

using namespace genny;
using namespace std;
using namespace std::chrono;


TEST_CASE("Orchestrator Perf", "[benchmark]") {
    const auto dur = milliseconds{200};

    genny::metrics::Registry metrics;
    genny::Orchestrator o{};
    o.addRequiredTokens(2);

    atomic_long regIters(0);
    {
        genny::DefaultRandom rand;
        rand.seed(1234);
        int i = 0;
        bool done = false;
        auto start = system_clock::now();

        // calculate rand() for dur duration
        while (!done) {
            rand();
            ++regIters;

            ++i;
            i %= 1000;
            if (i == 0) {
                // this dominates the loop time, so only do it 0.1% of the time
                done = system_clock::now() - start >= dur;
            }
        }
    }

    atomic_long orchIters(0);
    {
        // holds phase open for `dur` duration
        auto t1 = std::thread{[&]() {
            o.awaitPhaseStart();
            std::this_thread::sleep_for(dur);
            o.awaitPhaseEnd();
        }};

        auto t2 = std::thread{[&]() {
            // setup before timing starts
            genny::DefaultRandom rand;
            rand.seed(1234);

            auto phase = o.awaitPhaseStart();
            REQUIRE(o.awaitPhaseEnd(false));

            int i = 0;
            bool done = false;

            while (!done) {
                rand();
                ++orchIters;

                ++i;
                i %= 1000;
                if (i == 0) {
                    // checking the phase # is also what dominates the loop
                    // we get to "cheat" above by only checking current time every 1000 iterations,
                    // so same cheat here is to only call (expensive) currentPhase() every 100
                    // iterations
                    done = phase != o.currentPhase();
                }
            }
        }};

        t1.join();
        t2.join();
    }

    REQUIRE(regIters >= 5000000);  // at least 5 million times in 200 milliseconds (sanity check);
                                   // my macbook was doing 7e6.
    REQUIRE(orchIters >= (regIters * 980) / 1000);  // at least 98.0% as fast
}

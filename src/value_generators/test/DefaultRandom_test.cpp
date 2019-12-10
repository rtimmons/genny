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


#include <algorithm>
#include <iostream>

#include <boost/random/uniform_int_distribution.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

#include <value_generators/DefaultRandom.hpp>

namespace genny {

namespace {
TEST_CASE("genny DefaultRandom") {

    SECTION("Can be used in std::shuffle") {
        auto output = std::vector<int>(100);
        std::iota(std::begin(output), std::end(output), 1);

        DefaultRandom rng;
        rng.seed(12345);

        const auto input = output;
        std::shuffle(output.begin(), output.end(), rng);

        REQUIRE(input != output);
    }

    SECTION("Always get the same results") {
        DefaultRandom rng;
        rng.seed(12345);
        {
            std::vector<unsigned long long> first10 = {
                rng(), rng(), rng(), rng(), rng(), rng(), rng(), rng(), rng(), rng()};
            REQUIRE(first10 ==
                    std::vector<unsigned long long>{
                        6597103971274460346ull,
                        7386862472818278521ull,
                        12716877617435052285ull,
                        10325298820568433954ull,
                        10596756003076376996ull,
                        3831213995552687045ull,
                        528733477622103608ull,
                        12708413529556200236ull,
                        8657856827947486933ull,
                        3821290918431110937ull,
                    });
        }

        {
            // Also have the same output in uniform distribution
            //
            // This test needs to pass across platforms. The std::mt19937_64
            // and std::uniform_int_distribution aren't consistent across
            // platforms. As much as it pains me to link to reddit,
            // this has a good rundown: https://www.reddit.com/r/cpp/comments/7i21sn/
            boost::random::uniform_int_distribution<int64_t> dist{0, 1000};
            std::vector<int64_t> first10 = {dist(rng),
                                            dist(rng),
                                            dist(rng),
                                            dist(rng),
                                            dist(rng),
                                            dist(rng),
                                            dist(rng),
                                            dist(rng),
                                            dist(rng),
                                            dist(rng)};
            REQUIRE(first10 ==
                    std::vector<int64_t>{
                        3,
                        13,
                        420,
                        616,
                        895,
                        411,
                        261,
                        26,
                        108,
                        367,
                    });
        }
    }
}
}  // namespace

}  // namespace genny

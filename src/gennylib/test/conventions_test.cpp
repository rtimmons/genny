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

#include <chrono>
#include <cmath>

#include <yaml-cpp/yaml.h>

#include <gennylib/conventions.hpp>
#include <testlib/MongoTestFixture.hpp>

#include <testlib/helpers.hpp>


namespace genny {
namespace {
using namespace std::chrono;

TEST_CASE("genny::TimeSpec conversions") {
    SECTION("Can convert to genny::TimeSpec") {
        REQUIRE(YAML::Load("D: 3 seconds")["D"].as<TimeSpec>().count() == 3 * std::pow(10, 9));
        REQUIRE(YAML::Load("0 second").as<TimeSpec>().count() == 0);
        REQUIRE(YAML::Load("20 millisecond").as<TimeSpec>().count() == 20 * std::pow(10, 6));
        REQUIRE(YAML::Load("33 microsecond").as<TimeSpec>().count() == 33 * std::pow(10, 3));
        REQUIRE(YAML::Load("2e3 microseconds").as<TimeSpec>().count() == 2000 * std::pow(10, 3));
        REQUIRE(YAML::Load("10.3e2 nanoseconds").as<TimeSpec>().count() == 1030);
        REQUIRE(YAML::Load("3 hour").as<TimeSpec>().count() == 3 * 3600 * std::pow(10, 9));
        REQUIRE(YAML::Load("2 minutes").as<TimeSpec>().count() == 2 * 60 * std::pow(10, 9));
    }

    SECTION("Overlooks small typos") {
        REQUIRE(YAML::Load("D: 3 secondsasdfadsf     ")["D"].as<TimeSpec>().count() ==
                3 * std::pow(10, 9));
    }

    SECTION("Barfs on unknown types") {
        REQUIRE_THROWS(YAML::Load("-1 nanosecond").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("foo").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("[1,2,3]").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("[]").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("{}").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("what nanoseconds").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("29 picoseconds").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("1e3 centuries").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("mongodb").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("1").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("333").as<TimeSpec>());
    }

    SECTION("Barfs on invalid number of spaces") {
        REQUIRE_THROWS(YAML::Load("1  second").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("1second").as<TimeSpec>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["Duration"] = TimeSpec{30};
        REQUIRE(n["Duration"].as<TimeSpec>().count() == 30);
    }
}

TEST_CASE("genny::IntegerSpec conversions") {
    SECTION("Can convert to genny::IntegerSpec") {
        REQUIRE(YAML::Load("Repeat: 300")["Repeat"].as<IntegerSpec>().value == 300);
        REQUIRE(YAML::Load("0").as<IntegerSpec>().value == 0);
        REQUIRE(YAML::Load("1e3").as<IntegerSpec>().value == 1000);
        REQUIRE(YAML::Load("10.3e2").as<IntegerSpec>().value == 1030);
    }

    SECTION("Barfs on invalid values") {
        REQUIRE_THROWS(YAML::Load("-1").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("1e100000").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("1e-3").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("foo").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("-e1").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("e").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("0.1").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("-100.33e-1").as<IntegerSpec>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["Repeat"] = IntegerSpec{30};
        REQUIRE(n["Repeat"].as<IntegerSpec>().value == 30);
    }
}

TEST_CASE("genny::RateSpec conversions") {
    SECTION("Can convert to genny::RateSpec") {
        REQUIRE(YAML::Load("Rate: 300 per 2 nanoseconds")["Rate"].as<RateSpec>().operations == 300);
        REQUIRE(YAML::Load("Rate: 300 per 2 nanoseconds")["Rate"].as<RateSpec>().per.count() == 2);
    }

    SECTION("Barfs on invalid values") {
        REQUIRE_THROWS(YAML::Load("-1 per -1 nanosecond").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("-1 per -1 nanosecond").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("1 pe 1000 nanoseconds").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("per").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("nanoseconds per 1").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("1per2second").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("0per").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("xper").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("{foo}").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("").as<RateSpec>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["Rate"] = RateSpec{20, 30};
        REQUIRE(n["Rate"].as<RateSpec>().per.count() == 20);
        REQUIRE(n["Rate"].as<RateSpec>().operations == 30);
    }
}

}  // namespace
}  // namespace genny

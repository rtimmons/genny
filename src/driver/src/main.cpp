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
#include <fstream>
#include <iostream>
#include <optional>

#include <bsoncxx/builder/basic/array.hpp>

int main(int argc, char** argv) {
    using bsoncxx::builder::basic::kvp;
    const unsigned long long HOW_MANY_EVENTS = 5 * 1000 * 1000;
    std::ofstream output("t1", std::ios::out | std::ios::binary);
    const auto start = std::chrono::system_clock::now();
    size_t written = 0;
    for (unsigned i = 0; i < HOW_MANY_EVENTS; ++i) {
        bsoncxx::array::value arr = bsoncxx::builder::basic::make_array(
            // roughly 6 fields per event that we care about
            1,
            500,
            3,
            500,
            250,
            1);
        const std::uint8_t* data = arr.view().data();
        const size_t length = arr.view().length();
        written += length;
        output.write((char*)data, length);
    }
    const auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now() - start)
                         .count();

    std::cout << "Wrote " << HOW_MANY_EVENTS << " events (" << written << " bytes) after " << dur
              << " milliseconds" << std::endl;
    output.flush();
    output.close();
    return EXIT_SUCCESS;
}

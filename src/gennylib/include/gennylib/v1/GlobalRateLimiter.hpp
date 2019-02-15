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

#ifndef HEADER_FE10BCC4_FF45_4D79_B92F_72CE19437F81_INCLUDED
#define HEADER_FE10BCC4_FF45_4D79_B92F_72CE19437F81_INCLUDED

#include <algorithm>
#include <atomic>
#include <chrono>

#include <gennylib/conventions.hpp>

namespace genny::v1 {

/**
 * Rate limiter that applies globally across all threads using the token
 * bucket algorithm.
 *
 * Use GlobalRateLimiter to rate limit from the perspective of the
 * testing target (e.g. MongoDB server). Use RateLimiter to
 * control the schedule of each individual thread.
 *
 * Despite the naming similarities, there should be distinct
 * use case for each class. If you're unsure and "just need
 * a rate limiter", use this one.
 *
 * Notes
 * 1. There can be multiple global rate limiters each responsible for
 * a subset of threads. Coordinating across multiple global rate limiters
 * is currently not supported.
 *
 * 2. The caller is expected to be "nice" and respect the burst size. The
 * rate limiter itself does not know anything about the rate-limited functionality
 * or enforce any behavior about it. This is different to the non-global RateLimiter,
 * which invokes the rate-limited callback function itself.
 *
 * Inspired by
 * https://github.com/facebook/folly/blob/7c6897aa18e71964e097fc238c93b3efa98b2c61/folly/TokenBucket.h
 */
template <typename ClockT = std::chrono::steady_clock>
class BaseGlobalRateLimiter {
public:
    // This should be replaced with std::hardware_destructive_interference_size, which is
    // part of c++17 but not part of any (major) standard library. Search P0154R1
    // for more information.
    //
    // 64 is the cache line size for recent Intel and AMD processors.
    static const int CacheLineSize = 64;

    static_assert(ClockT::is_steady, "Clock must be steady");
    static_assert(std::is_same<typename ClockT::duration, std::chrono::nanoseconds>::value,
                  "Clock representation must be nano seconds");

public:
    explicit BaseGlobalRateLimiter(const RateSpec& rs)
        : _burstSize(rs.operations),
          _rateNS(rs.per.count()),
          _lastEmptiedTimeNS{ClockT::now().time_since_epoch().count()} {};

    // No copies or moves.
    BaseGlobalRateLimiter(const BaseGlobalRateLimiter& other) = delete;
    BaseGlobalRateLimiter& operator=(const BaseGlobalRateLimiter& other) = delete;

    BaseGlobalRateLimiter(BaseGlobalRateLimiter&& other) = delete;
    BaseGlobalRateLimiter& operator=(BaseGlobalRateLimiter&& other) = delete;

    ~BaseGlobalRateLimiter() = default;

    /**
     * Request to consume _burstSize number of tokens from the bucket. Does not block
     * if the bucket is empty; does block while waiting for concurrent accesses to the
     * bucket to finish.
     *
     * @return bool whether consume() succeeded. The caller is responsible for using an
     * appropriate back-off strategy if this function returns false.
     */
    bool consumeIfWithinRate(const typename ClockT::time_point& now) {
        // The time the bucket was emptied before this consumeIfWithinRate() call.
        int64_t curEmptiedTime = _lastEmptiedTimeNS.load();

        // The time the bucket was emptied after this consumeIfWithinRate() call.
        int64_t newEmptiedTime;

        auto nowInTicks = now.time_since_epoch().count();
        do {
            newEmptiedTime = curEmptiedTime + _rateNS;

            // If the new emptied time is in the future, the bucket is empty. Return early.
            if (nowInTicks < newEmptiedTime) {
                return false;
            }
            // Use the "weak" version for performance at the expense of false negatives (i.e.
            // `compare_exchange` not comparing equal when it should).
        } while (!_lastEmptiedTimeNS.compare_exchange_weak(curEmptiedTime, newEmptiedTime));

        return true;
    }

    constexpr int64_t getBurstSize() const {
        return _burstSize;
    }

    constexpr int64_t getRate() const {
        return _rateNS;
    }

    /**
     * Get the number of threads using this rate limiter. This number can help the caller
     * decide how congested the rate limiter is and find an appropriate time to wait until
     * retrying.
     *
     * E.g. if there are X users, each caller on average gets called per (_rateNS * _numUsers).
     * So it makes sense for each caller to wait for a duration of the same magnitude.
     * @return
     */
    constexpr int64_t getNumUsers() const {
        return _numUsers;
    }

    void addUser() {
        _numUsers++;
    }

private:
    // Manually align _lastEmptiedTimeNS here to vastly improve performance.
    // Lazily initialized by the first call to consumeIfWithinRate().
    // Note that std::chrono::time_point is not trivially copyable and can't be used here.
    alignas(BaseGlobalRateLimiter::CacheLineSize) std::atomic_int64_t _lastEmptiedTimeNS = 0;

    // Note that the rate limiter as-is doesn't use the burst size, but it is cleaner to
    // store the burst size and the rate together, since they're specified together in
    // the YAML as RateSpec.
    const int64_t _burstSize;
    const int64_t _rateNS;

    // Number of threads using this rate limiter.
    int64_t _numUsers = 0;
};

using GlobalRateLimiter = BaseGlobalRateLimiter<std::chrono::steady_clock>;

}  // namespace genny::v1

#endif  // HEADER_FE10BCC4_FF45_4D79_B92F_72CE19437F81_INCLUDED

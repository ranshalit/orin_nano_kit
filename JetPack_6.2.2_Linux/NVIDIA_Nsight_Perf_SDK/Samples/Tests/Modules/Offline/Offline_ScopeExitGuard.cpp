/*
* Copyright 2014-2023 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <stdint.h>
#include <doctest_proxy.h>
#include "NvPerfScopeExitGuard.h"

namespace nv { namespace perf { namespace test {

    NVPW_TEST_SUITE_BEGIN("ScopeExitGuard");

    NVPW_TEST_CASE("Unwind")
    {
        int stateMask = 0;
        auto state0 = ScopeExitGuard([&]() {
            NVPW_REQUIRE(stateMask == 0);
        });

        stateMask |= 1;
        auto state1 = ScopeExitGuard([&]() {
            NVPW_REQUIRE(stateMask == 1);
            stateMask &= ~1;
        });

        stateMask |= 2;
        auto state2 = ScopeExitGuard([&]() {
            NVPW_REQUIRE(stateMask == 3);
            stateMask &= ~2;
        });

        stateMask |= 4;
        auto state4 = ScopeExitGuard([&]() {
            NVPW_REQUIRE(stateMask == 7);
            stateMask &= ~4;
        });
    }

    NVPW_TEST_CASE("Dismiss")
    {
        int stateMask = 0;

        auto state1a = ScopeExitGuard([&]() {
            NVPW_REQUIRE(stateMask == 1);
        });

        stateMask |= 1;
        auto state2 = ScopeExitGuard([&]() {
            stateMask |= 2;
            NVPW_REQUIRE(stateMask == 3);
        });
        NVPW_SUBCASE("EarlyDismiss")
        {
            state2.Dismiss(); // multiple dismissals are allowed
        }

        auto state1b = ScopeExitGuard([&]() {
            NVPW_REQUIRE(stateMask == 1);
        });
        state2.Dismiss();
    }

    NVPW_TEST_SUITE_END();

}}} // nv::perf::test
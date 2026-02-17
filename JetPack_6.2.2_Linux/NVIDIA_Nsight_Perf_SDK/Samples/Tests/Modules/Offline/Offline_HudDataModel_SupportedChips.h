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

namespace nv { namespace perf { namespace test {
namespace {
    static std::vector<const char*> supportedChips{
#if defined (__aarch64__)
        "GA10B"
#else
        "TU102",
        "TU117",
        "GA102",
        "AD102"
#endif
    };
}
}}} // namespace nv::perf::test::
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

#pragma once

#include <doctest_proxy.h>
#include <doctest_reporters.h>
#include <TestCommon.h>

#include <NvPerfInit.h>
#include <NvPerfPeriodicSamplerGpu.h>

namespace nv { namespace perf { namespace test {

    // returns "size_t(~0)" if no compatible device found
    size_t GetCompatibleGpuDeviceIndex();

}}} // nv::perf::test
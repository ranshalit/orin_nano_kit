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

#include "DeviceUtilities.h"
#include <nvperf_target.h>

namespace nv { namespace perf { namespace test {

    using namespace nv::perf::sampler;

    size_t GetCompatibleGpuDeviceIndex()
    {
        NVPW_GetDeviceCount_Params getDeviceCountParams = { NVPW_GetDeviceCount_Params_STRUCT_SIZE };
        NVPA_Status nvpaStatus = NVPW_GetDeviceCount(&getDeviceCountParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "Failed NVPW_GetDeviceCount: %u\n", nvpaStatus);
            return size_t(~0);
        }

        for (size_t deviceIndex = 0; deviceIndex < getDeviceCountParams.numDevices; ++deviceIndex)
        {
            if (GpuPeriodicSamplerIsGpuSupported(deviceIndex))
            {
                return deviceIndex;
            }
        }
        return size_t(~0);
    }

}}} // nv::perf::test
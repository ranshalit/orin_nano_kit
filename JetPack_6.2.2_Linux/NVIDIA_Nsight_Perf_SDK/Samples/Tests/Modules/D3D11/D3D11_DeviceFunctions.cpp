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
#include "D3D11Utilities.h"
#include <NvPerfD3D11.h>

namespace nv { namespace perf { namespace test {
    NVPW_TEST_SUITE_BEGIN("D3D11");

    NVPW_TEST_CASE("DeviceFunctions_FindAdapter")
    {
        const bool iterateDevicesSuccess = ForEachD3D11Device([](IDXGIAdapter* pAdapter, ID3D11Device* pDevice, ID3D11DeviceContext* pImmediateContext)
        {
            NVPW_REQUIRE(!!pAdapter);
            NVPW_REQUIRE(!!pDevice);
            NVPW_REQUIRE(!!pImmediateContext);
            Microsoft::WRL::ComPtr<IDXGIAdapter> pAdapter2;
            NVPW_REQUIRE(nv::perf::D3D11FindAdapterForDevice(pDevice, &pAdapter2));
            NVPW_SUBCASE("CompareWithGivenAdapter")
            {
                NVPW_REQUIRE(pAdapter2.Get() == pAdapter);
            }
        });
        NVPW_REQUIRE(iterateDevicesSuccess);
    }

    NVPW_TEST_CASE("DeviceFunctions_IsNvidiaDevice")
    {
        const bool iterateDevicesSuccess = ForEachD3D11Device([](IDXGIAdapter* pAdapter, ID3D11Device* pDevice, ID3D11DeviceContext* pImmediateContext)
        {
            NVPW_REQUIRE(!!pAdapter);
            NVPW_REQUIRE(!!pDevice);
            NVPW_REQUIRE(!!pImmediateContext);
            const bool isNvidiaDevice = nv::perf::D3D11IsNvidiaDevice(pDevice);
            NVPW_CHECK(isNvidiaDevice == nv::perf::D3D11IsNvidiaDevice(pImmediateContext));
            if (!isNvidiaDevice)
            {
                return;
            }
            NVPW_SUBCASE("CompareWithVendorId")
            {
                Microsoft::WRL::ComPtr<IDXGIAdapter> pAdapter2;
                DXGI_ADAPTER_DESC adapterDesc = {};
                NVPW_REQUIRE(nv::perf::D3D11FindAdapterForDevice(pDevice, &pAdapter2, &adapterDesc));
                NVPW_REQUIRE(adapterDesc.VendorId == 0x10de);
            }
        });
        NVPW_REQUIRE(iterateDevicesSuccess);
    }

    NVPW_TEST_CASE("DeviceFunctions_GetNvperfDeviceIndex")
    {
        const bool iterateDevicesSuccess = ForEachD3D11Device([](IDXGIAdapter* pAdapter, ID3D11Device* pDevice, ID3D11DeviceContext* pImmediateContext)
        {
            NVPW_REQUIRE(!!pAdapter);
            NVPW_REQUIRE(!!pDevice);
            NVPW_REQUIRE(!!pImmediateContext);
            const bool isNvidiaDevice = nv::perf::D3D11IsNvidiaDevice(pDevice);
            if (!isNvidiaDevice)
            {
                return;
            }
            size_t nvperfDeviceIndex = nv::perf::D3D11GetNvperfDeviceIndex(pDevice);
            NVPW_SUBCASE("CompareWithRepeatCall")
            {
                size_t nvperfDeviceIndex2 = nv::perf::D3D11GetNvperfDeviceIndex(pDevice);
                NVPW_REQUIRE(nvperfDeviceIndex == nvperfDeviceIndex2);
            }
        });
        NVPW_REQUIRE(iterateDevicesSuccess);
    }

    NVPW_TEST_CASE("DeviceFunctions_GetDeviceIdentifiers")
    {
        const bool iterateDevicesSuccess = ForEachD3D11Device([](IDXGIAdapter* pAdapter, ID3D11Device* pDevice, ID3D11DeviceContext* pImmediateContext)
        {
            NVPW_REQUIRE(!!pAdapter);
            NVPW_REQUIRE(!!pDevice);
            NVPW_REQUIRE(!!pImmediateContext);
            const bool isNvidiaDevice = nv::perf::D3D11IsNvidiaDevice(pDevice);
            if (!isNvidiaDevice)
            {
                return;
            }
            nv::perf::DeviceIdentifiers deviceIdentifiers = {};
            deviceIdentifiers = nv::perf::D3D11GetDeviceIdentifiers(pDevice);
            if (!deviceIdentifiers.pDeviceName)
            {
                return;
            }
            NVPW_REQUIRE(!!deviceIdentifiers.pChipName);
        });
        NVPW_REQUIRE(iterateDevicesSuccess);
    }

    NVPW_TEST_CASE("DeviceFunctions_Profiler_IsGpuSupported")
    {
        const bool iterateDevicesSuccess = ForEachD3D11Device([](IDXGIAdapter* pAdapter, ID3D11Device* pDevice, ID3D11DeviceContext* pImmediateContext)
        {
            NVPW_REQUIRE(!!pAdapter);
            NVPW_REQUIRE(!!pDevice);
            NVPW_REQUIRE(!!pImmediateContext);
            const bool isNvidiaDevice = nv::perf::D3D11IsNvidiaDevice(pDevice);
            if (!isNvidiaDevice)
            {
                return;
            }
            // NOTE: sliIndex parameter is not tested
            const bool isGpuSupported = nv::perf::profiler::D3D11IsGpuSupported(pDevice);
            NVPW_REQUIRE(isGpuSupported == nv::perf::profiler::D3D11IsGpuSupported(pImmediateContext));
        });
        NVPW_REQUIRE(iterateDevicesSuccess);
    }

    NVPW_TEST_SUITE_END();
}}} // namespace nv::perf::test

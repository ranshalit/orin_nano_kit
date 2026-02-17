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
#include <dxgi.h>
#include <NvPerfScopeExitGuard.h>

namespace nv { namespace perf { namespace test {
    HRESULT D3D11CreateNvidiaDevice(ID3D11Device** ppDevice, ID3D11DeviceContext** ppImmediateContext)
    {
        *ppDevice = nullptr;
        *ppImmediateContext = nullptr;

        Microsoft::WRL::ComPtr<IDXGIFactory1> pFactory;
        HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
        if (FAILED(result))
        {
            return result;
        }

        Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
        for (
            UINT adapterIndex = 0;
            DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &pAdapter);
            ++adapterIndex)
        {
            if (DxgiIsNvidiaDevice(pAdapter.Get()))
            {
                break;
            }
            pAdapter = nullptr;
        }
        if (!pAdapter)
        {
            return DXGI_ERROR_NOT_FOUND;
        }

        D3D_FEATURE_LEVEL pFeatureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };
        result = D3D11CreateDevice(
            pAdapter.Get(),                // pAdapter
            D3D_DRIVER_TYPE_UNKNOWN,       // DriverType
            nullptr,                       // Software
            0,                             // Flags
            pFeatureLevels,                // pFeatureLevels
            sizeof(pFeatureLevels)
              / sizeof(pFeatureLevels[0]), // FeatureLevels
            D3D11_SDK_VERSION,             // SDKVersion
            ppDevice,                      // ppDevice
            NULL,                          // pFeatureLevel
            ppImmediateContext             // ppImmediateContext
        );
        if (FAILED(result))
        {
            return result;
        }
        return S_OK;
    }

    bool ForEachD3D11Device(const std::function<void(IDXGIAdapter* pAdapter, ID3D11Device* pDevice, ID3D11DeviceContext* pImmediateContext)>& fn)
    {
        Microsoft::WRL::ComPtr<IDXGIFactory1> pFactory;
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
        if (FAILED(hr))
        {
            return false;
        }

        bool success = true;
        Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
        for (
            UINT adapterIndex = 0;
            DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &pAdapter);
            ++adapterIndex)
        {
            Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> pImmediateContext;
            D3D_FEATURE_LEVEL pFeatureLevels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
            };
            hr = D3D11CreateDevice(
                pAdapter.Get(),                // pAdapter
                D3D_DRIVER_TYPE_UNKNOWN,       // DriverType
                nullptr,                       // Software
                0,                             // Flags
                pFeatureLevels,                // pFeatureLevels
                sizeof(pFeatureLevels)
                  / sizeof(pFeatureLevels[0]), // FeatureLevels
                D3D11_SDK_VERSION,             // SDKVersion
                &pDevice,                      // ppDevice
                NULL,                          // pFeatureLevel
                &pImmediateContext             // ppImmediateContext
            );
            auto releaseAdapterGuard = ScopeExitGuard([&]() {
                pAdapter = nullptr;
            });
            NVPW_CHECK_MESSAGE(SUCCEEDED(hr), "could not create D3D11 device for DXGI adapter at index: ", adapterIndex);
            if (FAILED(hr))
            {
                success = false;
                continue;
            }
            fn(pAdapter.Get(), pDevice.Get(), pImmediateContext.Get());
        }

        return success;
    }
}}} // nv::perf::test

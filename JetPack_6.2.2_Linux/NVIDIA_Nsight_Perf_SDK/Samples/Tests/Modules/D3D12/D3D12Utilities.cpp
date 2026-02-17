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
#include "D3D12Utilities.h"
#include <dxgi.h>

namespace nv { namespace perf { namespace test {
    HRESULT D3D12CreateNvidiaDevice(ID3D12Device** ppDevice)
    {
        *ppDevice = nullptr;
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

        result = D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), (void**)ppDevice);

        if (FAILED(result))
        {
            return result;
        }
        return S_OK;
    }
}}} // nv::perf::test

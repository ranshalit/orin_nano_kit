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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <d3d11.h>
#include <NvPerfInit.h>
#include <NvPerfD3D.h>
#include <NvPerfD3D11.h>
#include <functional>
#include <codecvt>
#include <locale>
#include <string>
namespace nv { namespace perf { namespace test {
    HRESULT D3D11CreateNvidiaDevice(ID3D11Device** ppDevice, ID3D11DeviceContext** ppImmediateContext);

    bool ForEachD3D11Device(const std::function<void(IDXGIAdapter* pAdapter, ID3D11Device* pDevice, ID3D11DeviceContext* pImmediateContext)>& fn);

    inline std::wstring StringToWString(const std::string& input)
    {
        return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(input);
    }
}}} // nv::perf::test

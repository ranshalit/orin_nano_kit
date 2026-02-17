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

#define DOCTEST_CONFIG_IMPLEMENT
#include "VulkanUtilities.h"
#include <nvperf_host_impl.h>

using namespace nv::perf;

static void OutOfTestCasesHandler(const doctest::AssertData& ad)
{
    using namespace doctest;
    std::cerr << Color::LightGrey << skipPathFromFilename(ad.m_file) << "(" << ad.m_line << "): ";
    std::cerr << Color::Red << failureString(ad.m_at) << ": InitializeNvPerf() failed, this test will be aborted" << std::endl;
}

int main(int argc, char** argv)
{
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    context.setOption("no-breaks", false); // unset this during debugging
    context.setAsDefaultForAssertsOutOfTestCases(); // this is required in order to allow testing InitializeNvPerf() outside of test cases
    context.setAssertHandler(OutOfTestCasesHandler);

#if defined(VK_NO_PROTOTYPES)
    const bool loadVulkanLibrarySuccess = nv::perf::test::LoadVulkanLibrary();
    NVPW_REQUIRE(loadVulkanLibrarySuccess);

    const bool loadExportedVulkanFunctionSuccess = nv::perf::test::LoadExportedVulkanFunctions();
    NVPW_REQUIRE(loadExportedVulkanFunctionSuccess);

    const bool loadGlobalLevelVulkanFunctionSuccess = nv::perf::test::LoadGlobalLevelVulkanFunctions();
    NVPW_REQUIRE(loadGlobalLevelVulkanFunctionSuccess);
#endif

    const bool initializeNvPerfResult = InitializeNvPerf();
    NVPW_REQUIRE(initializeNvPerfResult);

    int res = context.run();

#if defined(VK_NO_PROTOTYPES)
    const bool freeVulkanLibrarySuccess = nv::perf::test::FreeVulkanLibrary();
    NVPW_REQUIRE(freeVulkanLibrarySuccess);
#endif

    return res;
}

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
#include "EGLUtilities.h"
#include <NvPerfReportGeneratorEGL.h>
#include <NvPerfScopeExitGuard.h>

namespace nv { namespace perf { namespace test {
    NVPW_TEST_SUITE_BEGIN("EGL");

    NVPW_TEST_CASE("ReportGenerator")
    {
        if (glfwInit() != GL_TRUE)
        {
            NVPW_TEST_MESSAGE("glfwInit() failed, test is skipped.\n");
            return;
        }
        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
        glfwWindowHint(GLFW_SRGB_CAPABLE, GL_FALSE);
        glfwWindowHint(GLFW_DECORATED, GL_TRUE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);

        GLFWwindow* pWindow = glfwCreateWindow(128, 128, "", nullptr, nullptr);
        NVPW_REQUIRE(pWindow);

        auto destroyWindow = ScopeExitGuard([&]() {
            glfwDestroyWindow(pWindow);
        });
        glfwMakeContextCurrent(pWindow);

        if (!EGLIsNvidiaDevice())
        {
            NVPW_TEST_MESSAGE(EGLGetDeviceName().c_str(), " is not an NVIDIA Device, test is skipped.\n");
            return;
        }
        if (!profiler::EGLIsGpuSupported())
        {
            NVPW_TEST_MESSAGE(EGLGetDeviceName().c_str(), " is unsupported, test is skipped.");
            return;
        }

        profiler::ReportGeneratorEGL nvperf;
        NVPW_REQUIRE(nvperf.InitializeReportGenerator());
        auto resetNvPerf = ScopeExitGuard([&]() {
            nvperf.Reset();
        });
        nvperf.SetFrameLevelRangeName("Frame");
        nvperf.SetNumNestingLevels(2);

        NVPW_SUBCASE("EmptyFrame")
        {
            nvperf.outputOptions.directoryName = "HtmlReports/TestEGL/ReportGenerator_EmptyFrame";
            nvperf.StartCollectionOnNextFrame();
            do
            {
                {
                    auto finish = ScopeExitGuard([&]() {
                        glFinish();
                    });
                    NVPW_REQUIRE(nvperf.OnFrameStart());
                    nvperf.PushRange("Draw");
                    nvperf.PopRange();
                    NVPW_REQUIRE(nvperf.OnFrameEnd());
                }
                NVPW_REQUIRE_EQ(GL_NO_ERROR, glGetError());
            } while (nvperf.IsCollectingReport());

            auto fileExists = [&](const char* fileName) {
                FILE* pFile = OpenFile(fileName, "rb");
                if (!pFile)
                {
                    return false;
                }
                fclose(pFile);
                return true;
            };

            const char* fileNames[] = {
                "00000_Frame.html",
                "00001_Draw.html",
                "nvperf_metrics.csv",
                "nvperf_metrics_summary.csv",
                "readme.html",
                "summary.html",
            };

            const std::string reportDirectoryName = nvperf.GetLastReportDirectoryName();
            for (const char* fileName : fileNames)
            {
                std::string fullPath = reportDirectoryName + fileName;
                NVPW_CHECK(fileExists(fullPath.c_str()));
            }
        }
    }

    NVPW_TEST_SUITE_END();
}}} // namespace nv::perf::test
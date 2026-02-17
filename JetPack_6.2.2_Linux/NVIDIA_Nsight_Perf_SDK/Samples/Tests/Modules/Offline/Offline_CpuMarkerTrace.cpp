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

#include <array>
#include <doctest_proxy.h>

#include "NvPerfCpuMarkerTrace.h"

namespace nv { namespace perf { namespace test {

    namespace {

        typedef int DefaultType;
        constexpr size_t defaultFrameCount = 12;
        constexpr size_t defaultMarkerCount = 6;
        constexpr size_t defaultNameCharCount = 50;

        constexpr std::array<const char*, 8> defaultMarkerNames = {
            "zero",
            "one",
            "two",
            "three",
            "four",
            "five",
            "six",
            "seven",
        };

        template <class TUserData>
        void CheckDefaultMarkers(typename nv::perf::CpuMarkerTrace<TUserData>& cpuMarkerTrace, const std::size_t validCount)
        {
            auto markers = cpuMarkerTrace.GetOldestFrameMarkers();
            NVPW_CHECK(markers.validMarkerCount == validCount);
            NVPW_CHECK(markers.droppedMarkerCount == 0);
            NVPW_CHECK(markers.droppedNameCharCount == 0);
            NVPW_REQUIRE(markers.pBegin != nullptr);
            NVPW_REQUIRE(markers.pEnd != nullptr);

            NVPW_CHECK((markers.pEnd - markers.pBegin) == markers.validMarkerCount);
            for (size_t markerIdx = 0; markerIdx < markers.validMarkerCount; ++markerIdx)
            {
                const auto& marker = markers.pBegin[markerIdx];
                NVPW_CHECK(marker.pName == defaultMarkerNames[markerIdx]);
            }

            NVPW_CHECK(markers.pUserData != nullptr);
            cpuMarkerTrace.ReleaseOldestFrame();
        }

        template <class TUserData>
        void WriteDefaultMarkers(typename nv::perf::CpuMarkerTrace<TUserData>& cpuMarkerTrace, const std::size_t markerCount)
        {
            for (size_t markerIdx = 0; markerIdx < markerCount; ++markerIdx)
            {
                NVPW_CHECK(cpuMarkerTrace.PushMarker(defaultMarkerNames[markerIdx]));
            }
            NVPW_CHECK(cpuMarkerTrace.GetCurrentFrameDroppedMarkerCount() == 0);
            NVPW_CHECK(cpuMarkerTrace.OnFrameEnd());
        }

    } // namespace

    NVPW_TEST_SUITE_BEGIN("CpuMarkerTrace");

    NVPW_TEST_CASE("Single Frame Behavior - With Buffer")
    {
        CpuMarkerTrace<DefaultType> cpuMarkerTrace;
        std::vector<uint8_t> buffer(CpuMarkerTrace<DefaultType>::GetTotalMemoryUsage(defaultFrameCount, defaultMarkerCount, defaultNameCharCount));
        cpuMarkerTrace.Initialize(defaultFrameCount, defaultMarkerCount, defaultNameCharCount, buffer.data());
        size_t markerCount = 0;

        NVPW_SUBCASE("Send 6 markers")
        {
            markerCount = 6;
            WriteDefaultMarkers(cpuMarkerTrace, markerCount);
        }

        NVPW_CHECK(cpuMarkerTrace.GetUnreadFrameCount() == 1);
        CheckDefaultMarkers<DefaultType>(cpuMarkerTrace, markerCount);

        cpuMarkerTrace.Reset();
    }

    
    NVPW_TEST_CASE("Single Frame Behavior - With Complex UserData")
    {
        struct ComplexData
        {
            std::vector<double> v;
            char c;
            uint16_t s;
            uint64_t l;
            std::array<float, 3> a;
        };

        CpuMarkerTrace<ComplexData> cpuMarkerTrace;
        cpuMarkerTrace.Initialize(defaultFrameCount, defaultMarkerCount, defaultNameCharCount);
        size_t markerCount = 0;

        NVPW_SUBCASE("Send 6 markers")
        {
            markerCount = 6;
            WriteDefaultMarkers(cpuMarkerTrace, markerCount);
        }

        NVPW_CHECK(cpuMarkerTrace.GetUnreadFrameCount() == 1);
        CheckDefaultMarkers<ComplexData>(cpuMarkerTrace, markerCount);

        cpuMarkerTrace.Reset();
    }

    NVPW_TEST_CASE("Single Frame Behavior")
    {
        CpuMarkerTrace<DefaultType> cpuMarkerTrace;
        cpuMarkerTrace.Initialize(defaultFrameCount, defaultMarkerCount, defaultNameCharCount);
        size_t markerCount = 0;

        NVPW_SUBCASE("Send 1 Marker")
        {
            markerCount = 1;

            NVPW_SUBCASE("with length")
            {
                NVPW_CHECK(cpuMarkerTrace.PushMarker(defaultMarkerNames[0], strlen(defaultMarkerNames[0])));
            }

            NVPW_SUBCASE("without length")
            {
                NVPW_CHECK(cpuMarkerTrace.PushMarker(defaultMarkerNames[0]));
            }

            NVPW_CHECK(cpuMarkerTrace.GetCurrentFrameDroppedMarkerCount() == 0);
            NVPW_CHECK(cpuMarkerTrace.OnFrameEnd());
            NVPW_CHECK(cpuMarkerTrace.GetUnreadFrameCount() == 1);
        }

        NVPW_SUBCASE("Send 6 markers")
        {
            markerCount = 6;
            WriteDefaultMarkers(cpuMarkerTrace, markerCount);
        }

        CheckDefaultMarkers(cpuMarkerTrace, markerCount);

        cpuMarkerTrace.Reset();
    }

    NVPW_TEST_CASE("Multiple Frame Behavior")
    {
        constexpr size_t totalIterations = 5;
        constexpr size_t numberOfInterleavedCalls = 5;

        // The idea here is to interleave many calls and ensure all data is as expected
        CpuMarkerTrace<DefaultType> cpuMarkerTrace;
        cpuMarkerTrace.Initialize(defaultFrameCount, defaultMarkerCount, defaultNameCharCount);

        // these are used to create a cycle of markers for each frame
        // that has a variety of marker numbers, and also is reproducible
        // in both the write and read loops
        size_t numberOfMarkersToWrite = 1;
        size_t numberOfMarkersToRead = 1;
        auto advanceNumberOfMarkers = [](size_t oldNumber) {
            return (oldNumber + 4) % 6;
        };

        size_t totalWriteCount = 0;
        size_t totalReadCount = 0;

        // Example sequence of events:
        // 5 frame writes 0 frame reads
        // 4 writes 1 read
        // 3 writes 2 reads
        // 2 writes 3 reads
        // 1 write  4 reads
        // 0 writes 5 reads

        for (size_t iteration = 0; iteration < totalIterations; ++iteration)
        {
            for (size_t callIdx = 0; callIdx <= numberOfInterleavedCalls; ++callIdx)
            {
                for (size_t writeIdx = 0; writeIdx < (numberOfInterleavedCalls - callIdx); ++writeIdx)
                {
                    WriteDefaultMarkers(cpuMarkerTrace, numberOfMarkersToWrite);
                    numberOfMarkersToWrite = advanceNumberOfMarkers(numberOfMarkersToWrite);
                    totalWriteCount++;
                }

                for (size_t readIdx = 0; readIdx < callIdx; ++readIdx)
                {
                    auto markers = cpuMarkerTrace.GetOldestFrameMarkers();
                    CheckDefaultMarkers(cpuMarkerTrace, numberOfMarkersToRead);
                    numberOfMarkersToRead = advanceNumberOfMarkers(numberOfMarkersToRead);
                    totalReadCount++;
                }
            }
        }

        constexpr size_t totalOperations = totalIterations * ((numberOfInterleavedCalls * (numberOfInterleavedCalls + 1)) / 2);
        NVPW_CHECK(totalWriteCount == totalOperations);
        NVPW_CHECK(totalReadCount == totalOperations);

        cpuMarkerTrace.Reset();
    }

    NVPW_TEST_CASE("Negative Tests")
    {
        CpuMarkerTrace<DefaultType> cpuMarkerTrace;
        cpuMarkerTrace.Initialize(defaultFrameCount, defaultMarkerCount, defaultNameCharCount);

        NVPW_SUBCASE("Frame Overrun")
        {
            for (int frameIdx = 0; frameIdx < (defaultFrameCount * 2); ++frameIdx)
            {
                auto result = cpuMarkerTrace.OnFrameEnd();
                if (frameIdx >= defaultFrameCount)
                {
                    NVPW_CHECK_FALSE(result);
                }
            }
        }

        NVPW_SUBCASE("Frame Marker Overrun")
        {
            constexpr size_t markerCount = 3;

            for (size_t frameIdx = 0; frameIdx < defaultFrameCount - 1; ++frameIdx)
            {
                WriteDefaultMarkers(cpuMarkerTrace, markerCount);
            }

            for (size_t markerIdx = 0; markerIdx < markerCount; ++markerIdx)
            {
                NVPW_CHECK(cpuMarkerTrace.PushMarker(defaultMarkerNames[markerIdx]));
            }
            NVPW_CHECK(cpuMarkerTrace.GetCurrentFrameDroppedMarkerCount() == 0);

            // should be false to indicate buffer is now full
            NVPW_CHECK_FALSE(cpuMarkerTrace.OnFrameEnd());

            // frame buffer is now full, test sending markers
            for (size_t markerIdx = 0; markerIdx < markerCount; ++markerIdx)
            {
                NVPW_CHECK_FALSE(cpuMarkerTrace.PushMarker(defaultMarkerNames[markerIdx]));
            }

            // should be false to indicate the buffer is still full
            NVPW_CHECK_FALSE(cpuMarkerTrace.OnFrameEnd());

            // remove one frame from buffer
            CheckDefaultMarkers(cpuMarkerTrace, markerCount);

            // should be true to indicate it is ok to proceed
            NVPW_CHECK(cpuMarkerTrace.OnFrameEnd());

            // try writing again
            for (size_t markerIdx = 0; markerIdx < markerCount; ++markerIdx)
            {
                NVPW_CHECK(cpuMarkerTrace.PushMarker(defaultMarkerNames[markerIdx]));
            }
            NVPW_CHECK(cpuMarkerTrace.GetCurrentFrameDroppedMarkerCount() == 0);

            // should be false to indicate buffer is now full
            NVPW_CHECK_FALSE(cpuMarkerTrace.OnFrameEnd());

            // verify all data
            for (size_t frameIdx = 0; frameIdx < defaultFrameCount; ++frameIdx)
            {
                CheckDefaultMarkers(cpuMarkerTrace, markerCount);
            }
        }

        NVPW_SUBCASE("Marker Overrun")
        {
            constexpr size_t droppedMarkers = 2;
            constexpr size_t markerCount = defaultMarkerCount + droppedMarkers;

            for (int markerIdx = 0; markerIdx < markerCount; ++markerIdx)
            {
                if (markerIdx < defaultMarkerCount)
                {
                    NVPW_CHECK(cpuMarkerTrace.PushMarker(defaultMarkerNames[markerIdx]));
                }
                else
                {
                    NVPW_CHECK_FALSE(cpuMarkerTrace.PushMarker(defaultMarkerNames[markerIdx]));
                }
            }

            NVPW_CHECK(cpuMarkerTrace.GetCurrentFrameDroppedMarkerCount() == droppedMarkers);
            NVPW_CHECK(cpuMarkerTrace.OnFrameEnd());
            NVPW_CHECK(cpuMarkerTrace.GetUnreadFrameCount() == 1);

            {
                auto markers = cpuMarkerTrace.GetOldestFrameMarkers();
                NVPW_CHECK(markers.validMarkerCount == defaultMarkerCount);
                NVPW_CHECK(markers.droppedMarkerCount == droppedMarkers);
                NVPW_CHECK(markers.droppedNameCharCount == 0);
                NVPW_REQUIRE(markers.pBegin != nullptr);
                NVPW_REQUIRE(markers.pEnd != nullptr);

                NVPW_CHECK((markers.pEnd - markers.pBegin) == markers.validMarkerCount);
                for (size_t markerIdx = 0; markerIdx < markers.validMarkerCount; ++markerIdx)
                {
                    const auto& marker = markers.pBegin[markerIdx];
                    NVPW_CHECK(marker.pName == defaultMarkerNames[markerIdx]);
                }

                NVPW_CHECK(markers.pUserData != nullptr);
                cpuMarkerTrace.ReleaseOldestFrame();
            }
        }

        NVPW_SUBCASE("NameChar Overrun")
        {
            constexpr size_t markerNameSize = 10;
            constexpr char markerName[markerNameSize] = "123456789";
            constexpr size_t totalCharCount = 60;
            constexpr size_t droppedCharCount = totalCharCount - defaultNameCharCount;
            constexpr size_t validMarkers = defaultNameCharCount / markerNameSize;
            constexpr size_t droppedMarkers = droppedCharCount / markerNameSize;
            

            for (int charIdx = 0; charIdx < totalCharCount; charIdx += markerNameSize)
            {
                if (charIdx + markerNameSize <= defaultNameCharCount)
                {
                    NVPW_CHECK(cpuMarkerTrace.PushMarker(markerName));
                }
                else
                {
                    NVPW_CHECK_FALSE(cpuMarkerTrace.PushMarker(markerName));
                }
            }

            NVPW_CHECK(cpuMarkerTrace.GetCurrentFrameDroppedMarkerCount() == droppedMarkers);
            NVPW_CHECK(cpuMarkerTrace.OnFrameEnd());
            NVPW_CHECK(cpuMarkerTrace.GetUnreadFrameCount() == 1);

            {
                auto markers = cpuMarkerTrace.GetOldestFrameMarkers();
                NVPW_CHECK(markers.validMarkerCount == validMarkers);
                NVPW_CHECK(markers.droppedMarkerCount == droppedMarkers);
                NVPW_CHECK(markers.droppedNameCharCount == droppedCharCount);
                NVPW_REQUIRE(markers.pBegin != nullptr);
                NVPW_REQUIRE(markers.pEnd != nullptr);

                NVPW_CHECK((markers.pEnd - markers.pBegin) == markers.validMarkerCount);
                for (size_t markerIdx = 0; markerIdx < markers.validMarkerCount; ++markerIdx)
                {
                    const auto& marker = markers.pBegin[markerIdx];
                    NVPW_CHECK(marker.pName == markerName);
                }

                NVPW_CHECK(markers.pUserData != nullptr);
                cpuMarkerTrace.ReleaseOldestFrame();
            }

        }

        NVPW_SUBCASE("Get From Empty")
        {
            auto markers = cpuMarkerTrace.GetOldestFrameMarkers();
            auto defaultMarkers = CpuMarkerTrace<DefaultType>::FrameMarkers();

            NVPW_CHECK(markers.validMarkerCount == defaultMarkers.validMarkerCount);
            NVPW_CHECK(markers.droppedMarkerCount == defaultMarkers.droppedMarkerCount);
            NVPW_CHECK(markers.droppedNameCharCount == defaultMarkers.droppedNameCharCount);
            NVPW_CHECK(markers.pBegin == defaultMarkers.pBegin);
            NVPW_CHECK(markers.pEnd == defaultMarkers.pEnd);
            NVPW_CHECK(markers.pUserData == defaultMarkers.pUserData);
        }


    }

    NVPW_TEST_CASE("UserData")
    {
        constexpr DefaultType modifiedUserData = 9001;
        constexpr size_t markerCount = 3;

        CpuMarkerTrace<DefaultType> cpuMarkerTrace;
        cpuMarkerTrace.Initialize(defaultFrameCount, defaultMarkerCount, defaultNameCharCount);

        auto updateData = [=](CpuMarkerTrace<DefaultType>::FrameUserDataFnParams params) {
            NVPW_CHECK(params.validMarkerCount == markerCount);
            NVPW_CHECK(params.droppedMarkerCount == 0);
            NVPW_CHECK(params.droppedCharCount == 0);
            NVPW_REQUIRE(params.pUserData != nullptr);
            *params.pUserData = modifiedUserData;
        };

        for (size_t markerIdx = 0; markerIdx < markerCount; ++markerIdx)
        {
            NVPW_CHECK(cpuMarkerTrace.PushMarker(defaultMarkerNames[markerIdx]));
        }
        NVPW_CHECK(cpuMarkerTrace.GetCurrentFrameDroppedMarkerCount() == 0);

        cpuMarkerTrace.UpdateCurrentFrameUserData(updateData);

        NVPW_CHECK(cpuMarkerTrace.OnFrameEnd());

        auto markers = cpuMarkerTrace.GetOldestFrameMarkers();
        NVPW_CHECK(markers.validMarkerCount == markerCount);
        NVPW_CHECK(markers.droppedMarkerCount == 0);
        NVPW_CHECK(markers.droppedNameCharCount == 0);
        NVPW_REQUIRE(markers.pBegin != nullptr);
        NVPW_REQUIRE(markers.pEnd != nullptr);

        NVPW_CHECK((markers.pEnd - markers.pBegin) == markers.validMarkerCount);
        for (size_t markerIdx = 0; markerIdx < markers.validMarkerCount; ++markerIdx)
        {
            const auto& marker = markers.pBegin[markerIdx];
            NVPW_CHECK(marker.pName == defaultMarkerNames[markerIdx]);
        }

        NVPW_CHECK(markers.pUserData != nullptr);
        NVPW_CHECK(*markers.pUserData == modifiedUserData);
    }

    NVPW_TEST_SUITE_END();

}}} // namespace nv::perf::test
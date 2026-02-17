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

#include "Offline.h"
#include <functional>
#include <algorithm>
#include <string>
#include <NvPerfCounterData.h>
#include <NvPerfPeriodicSamplerGpu.h>

namespace nv { namespace perf { namespace test {

    NVPW_TEST_SUITE_BEGIN("CounterData");

    NVPW_TEST_CASE("RingBuffer")
    {
        using namespace nv::perf::sampler;

        struct RingBufferCounterDataTest : public RingBufferCounterData
        {
            const uint32_t FirstTrigger = 8;
            std::vector<uint32_t> triggerCountRingBuffer; // one for each range
            uint32_t numPopulatedRanges = 0;
            uint32_t numCompletedRanges = 0;

            bool Initialize(uint32_t maxTriggerLatency)
            {
                const bool validate = true;
                bool success = RingBufferCounterData::Initialize(maxTriggerLatency, validate, [&](uint32_t maxSamples, NVPW_PeriodicSampler_CounterData_AppendMode appendMode, std::vector<uint8_t>& counterData) {
                    triggerCountRingBuffer.resize(maxTriggerLatency, 0);
                    return true;
                });
                if (!success)
                {
                    return false;
                }
                return true;
            }

            void PopulateTriggers(uint32_t numFullTriggers, uint32_t numPartialTriggers)
            {
                uint32_t triggerCount = numPopulatedRanges ? triggerCountRingBuffer[numPopulatedRanges - 1] + 1: FirstTrigger;
                for (uint32_t ii = 0; ii < numPartialTriggers; ++ii)
                {
                    if (numPopulatedRanges == m_numTotalRanges)
                    {
                        numPopulatedRanges = 0;
                    }
                    triggerCountRingBuffer[numPopulatedRanges] = triggerCount++;
                    numPopulatedRanges += 1;
                }
                numCompletedRanges = CircularIncrement(numCompletedRanges - 1, numFullTriggers) + 1;
            }

            virtual bool GetTriggerCount(uint32_t rangeIndex, uint32_t& triggerCount) const override
            {
                triggerCount = triggerCountRingBuffer[rangeIndex];
                return true;
            }

            virtual bool GetLatestInfo(uint32_t& numPopulatedRanges_, uint32_t& numCompletedRanges_) const override
            {
                numPopulatedRanges_ = numPopulatedRanges;
                numCompletedRanges_ = numCompletedRanges;
                return true;
            }
        };
        
        RingBufferCounterDataTest counterData;
        NVPW_REQUIRE(counterData.Initialize(10));

        NVPW_SUBCASE("PopulateTriggers")
        {
            {
                const uint32_t numFullTriggers = 3;
                const uint32_t numPartialTriggers = 4;
                counterData.PopulateTriggers(numFullTriggers, numPartialTriggers);
                NVPW_CHECK(counterData.numCompletedRanges == 3);
                NVPW_CHECK(counterData.numPopulatedRanges == 4);
                NVPW_CHECK(counterData.triggerCountRingBuffer == std::vector<uint32_t>({ 8, 9, 10, 11, 0, 0, 0, 0, 0, 0 }));
            }
            {
                const uint32_t numFullTriggers = 8;
                const uint32_t numPartialTriggers = 9;
                counterData.PopulateTriggers(numFullTriggers, numPartialTriggers);
                NVPW_CHECK(counterData.numCompletedRanges == 1); // (3 + 8) % 10
                NVPW_CHECK(counterData.numPopulatedRanges == 3); // (4 + 9) % 10
                NVPW_CHECK(counterData.triggerCountRingBuffer == std::vector<uint32_t>({ 18, 19, 20, 11, 12, 13, 14, 15, 16, 17 }));
            }
        }

        NVPW_SUBCASE("UtilFuncs")
        {
            NVPW_CHECK(counterData.CircularIncrement(5, 0) == 5);
            NVPW_CHECK(counterData.CircularIncrement(5, 4) == 9);
            NVPW_CHECK(counterData.CircularIncrement(5, 6) == 1);
            NVPW_CHECK(counterData.CircularIncrement(5, 4 + 10) == 9);
            NVPW_CHECK(counterData.CircularIncrement(5, 6 + 10) == 1);

            NVPW_CHECK(counterData.Distance(5, 5) == 0);
            NVPW_CHECK(counterData.Distance(5, 9) == 4);
            NVPW_CHECK(counterData.Distance(5, 1) == 6);
        }

        NVPW_SUBCASE("All")
        {
            NVPW_CHECK(counterData.GetNumUnreadRanges() == 0);

            uint32_t numFullTriggers = 3;
            uint32_t numPartialTriggers = 4;
            counterData.PopulateTriggers(numFullTriggers, numPartialTriggers); // numCompletedRanges = 3; numPopulatedRanges = 4; triggerCountRingBuffer = { 8, 9, 10, 11, 0, 0, 0, 0, 0, 0 }
            NVPW_CHECK(counterData.UpdatePut()); // m_put = { /*rangeDataIndex*/2, /*triggerCount*/10 }; m_get = { /*rangeDataIndex*/~0, /*triggerCount*/0 }
            NVPW_CHECK(counterData.GetNumUnreadRanges() == 3);

            NVPW_CHECK(!counterData.ConsumeData([&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop) {
                return false;
            }));
            uint32_t expectedRangeIndex = 0;
            NVPW_CHECK(counterData.ConsumeData([&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop) {
                stop = false;
                if (rangeIndex == 2)
                {
                    stop = true;
                    return true;
                }
                NVPW_CHECK(expectedRangeIndex == rangeIndex);
                expectedRangeIndex = (expectedRangeIndex + 1) % 10;
                return true;
            }));
            NVPW_CHECK(expectedRangeIndex == 2);
            NVPW_CHECK(counterData.GetNumUnreadRanges() == 3); // since we have not yet updated GET
            NVPW_CHECK(counterData.UpdateGet(2));  // m_put = { /*rangeDataIndex*/2, /*triggerCount*/10 }; m_get = { /*rangeDataIndex*/1, /*triggerCount*/9 }
            NVPW_CHECK(counterData.GetNumUnreadRanges() == 1);

            NVPW_CHECK(counterData.ConsumeData([&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop) {
                stop = false;
                NVPW_CHECK(expectedRangeIndex == rangeIndex);
                expectedRangeIndex = (expectedRangeIndex + 1) % 10;
                return true;
            }));
            NVPW_CHECK(expectedRangeIndex == 3);
            NVPW_CHECK(counterData.UpdateGet(1));  // m_put = { /*rangeDataIndex*/2, /*triggerCount*/10 }; m_get = { /*rangeDataIndex*/2, /*triggerCount*/10 }
            NVPW_CHECK(counterData.GetNumUnreadRanges() == 0);

            numFullTriggers = 2;
            numPartialTriggers = 1;
            counterData.PopulateTriggers(numFullTriggers, numPartialTriggers); // numCompletedRanges = 5; numPopulatedRanges = 5; triggerCountRingBuffer = { 8, 9, 10, 11, 12, 0, 0, 0, 0, 0 }
            NVPW_CHECK(counterData.UpdatePut()); // m_put = { /*rangeDataIndex*/4, /*triggerCount*/12 }; m_get = { /*rangeDataIndex*/2, /*triggerCount*/10 }
            NVPW_CHECK(counterData.GetNumUnreadRanges() == 2);

            numFullTriggers = 6;
            numPartialTriggers = 8;
            counterData.PopulateTriggers(numFullTriggers, numPartialTriggers); // numCompletedRanges = 1; numPopulatedRanges = 3; triggerCountRingBuffer = { 18, 19, 20, 11, 12, 13, 14, 15, 16, 17 }
            NVPW_CHECK(counterData.UpdatePut()); // m_put = { /*rangeDataIndex*/1, /*triggerCount*/18 }; m_get = { /*rangeDataIndex*/2, /*triggerCount*/10 }
            NVPW_CHECK(counterData.GetNumUnreadRanges() == 8);
            NVPW_CHECK(counterData.ConsumeData([&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop) {
                stop = false;
                NVPW_CHECK(expectedRangeIndex == rangeIndex);
                expectedRangeIndex = (expectedRangeIndex + 1) % 10;
                return true;
            }));
            NVPW_CHECK(counterData.UpdateGet(8)); // m_put = { /*rangeDataIndex*/1, /*triggerCount*/18 }; m_get = { /*rangeDataIndex*/1, /*triggerCount*/18 }
            NVPW_CHECK(counterData.GetNumUnreadRanges() == 0);

            {
                ScopedNvPerfLogDisabler logDisabler;
                numFullTriggers = 12;
                numPartialTriggers = 13;
                counterData.PopulateTriggers(numFullTriggers, numPartialTriggers);
                NVPW_CHECK(!counterData.UpdatePut()); // put has beaten get for one round
            }
        }
    }

    NVPW_TEST_CASE("CounterDataCombiner")
    {
        NVPW_SUBCASE("GetOverlapFactor")
        {
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 5, 8) == doctest::Approx(0.0));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 5, 10) == doctest::Approx(0.0));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 5, 15) == doctest::Approx(0.5));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 5, 20) == doctest::Approx(1.0));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 5, 25) == doctest::Approx(1.0));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 10, 15) == doctest::Approx(0.5));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 10, 20) == doctest::Approx(1.0));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 10, 25) == doctest::Approx(1.0));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 14, 19) == doctest::Approx(0.5));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 14, 20) == doctest::Approx(0.6));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 14, 25) == doctest::Approx(0.6));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 20, 25) == doctest::Approx(0.0));
            NVPW_CHECK(CounterDataCombiner::GetOverlapFactor(10, 20, 21, 25) == doctest::Approx(0.0));
        }
    }

    NVPW_TEST_SUITE_END();

}}} // nv::perf::test
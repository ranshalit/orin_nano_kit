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
#include <string.h>

inline bool operator==(const NVPW_DimUnitFactor& lhs, const NVPW_DimUnitFactor& rhs)
{
    return nv::perf::operator==(lhs, rhs);
}

inline bool operator<(const NVPW_DimUnitFactor& lhs, const NVPW_DimUnitFactor& rhs)
{
    return nv::perf::operator<(lhs, rhs);
}

namespace nv { namespace perf { namespace test {

    NVPW_TEST_SUITE_BEGIN("MetricsEvaluator");

    NVPW_TEST_CASE("MetricsEnumeration")
    {
#if defined (__aarch64__)
        const char* pChipName = "GA10B";
#else
        const char* pChipName = "TU102";
#endif
        MetricsEvaluator metricsEvaluator = CreateMetricsEvaluator(pChipName);
        auto test = [&](std::function<MetricsEnumerator(NVPW_MetricsEvaluator* pMetricsEvaluator)>&& createEnumerator) {
            const MetricsEnumerator enumerator = createEnumerator(metricsEvaluator);
            NVPW_REQUIRE(!enumerator.empty());

            size_t numActualMetrics = 0;
            for (const char* pMetricName : enumerator)
            {
                NVPW_CHECK(pMetricName);
                NVPW_CHECK(strcmp(pMetricName, ""));
                ++numActualMetrics;
            }
            NVPW_CHECK(numActualMetrics == enumerator.size());
        };

        NVPW_SUBCASE("Counters")
        {
            test(EnumerateCounters);
        }
        NVPW_SUBCASE("Ratios")
        {
            test(EnumerateRatios);
        }
        NVPW_SUBCASE("Throughputs")
        {
            test(EnumerateThroughputs);
        }
    }

    NVPW_TEST_CASE("MetricEvalRequestTwoWayConversions")
    {
#if defined (__aarch64__)
        const char* pChipName = "GA10B";
#else
        const char* pChipName = "TU102";
#endif
        MetricsEvaluator metricsEvaluator = CreateMetricsEvaluator(pChipName);
        auto test = [&](const char* pMetricName) {
            NVPW_MetricEvalRequest metricEvalRequest{};
            NVPW_CHECK(ToMetricEvalRequest(metricsEvaluator, pMetricName, metricEvalRequest));
            const std::string convertedName = ToString(metricsEvaluator, metricEvalRequest);
            NVPW_CHECK(convertedName == std::string(pMetricName));
        };

        NVPW_SUBCASE("Counters")
        {
            test("smsp__warps_launched.sum");
            test("smsp__warps_launched.sum.peak_sustained");
            test("smsp__warps_launched.sum.peak_sustained_active");
            test("smsp__warps_launched.sum.peak_sustained_active.per_second");
            test("smsp__warps_launched.avg");
            test("smsp__warps_launched.avg.peak_sustained_elapsed");
            test("smsp__warps_launched.avg.peak_sustained_elapsed.per_second");
            test("smsp__warps_launched.avg.peak_sustained_frame");
            test("smsp__warps_launched.avg.peak_sustained_frame.per_second");
            test("smsp__warps_launched.avg.per_cycle_elapsed");
            test("smsp__warps_launched.max");
            test("smsp__warps_launched.max.peak_sustained_region");
            test("smsp__warps_launched.max.peak_sustained_region.per_second");
            test("smsp__warps_launched.max.per_cycle_active");
            test("smsp__warps_launched.max.per_cycle_elapsed");
            test("smsp__warps_launched.max.per_cycle_in_frame");
            test("smsp__warps_launched.max.per_cycle_in_region");
            test("smsp__warps_launched.min");
            test("smsp__warps_launched.min.per_second");
            test("smsp__warps_launched.min.pct_of_peak_sustained_active");
            test("smsp__warps_launched.min.pct_of_peak_sustained_elapsed");
            test("smsp__warps_launched.min.pct_of_peak_sustained_frame");
            test("smsp__warps_launched.min.pct_of_peak_sustained_region");
        }
        NVPW_SUBCASE("Ratios")
        {
            test("smsp__average_warp_latency.max_rate");
            test("smsp__average_warp_latency.pct");
            test("smsp__average_warp_latency.ratio");
        }
        NVPW_SUBCASE("Throughputs")
        {
            test("l1tex__throughput.avg.pct_of_peak_sustained_active");
            test("l1tex__throughput.avg.pct_of_peak_sustained_elapsed");
            test("l1tex__throughput.max.pct_of_peak_sustained_frame");
            test("l1tex__throughput.min.pct_of_peak_sustained_region");
        }
    }

    NVPW_TEST_CASE("DimUnit")
    {
#if defined (__aarch64__)
        const char* pChipName = "GA10B";
#else
        const char* pChipName = "TU102";
#endif
        MetricsEvaluator metricsEvaluator = CreateMetricsEvaluator(pChipName);
        auto test = [&](const char* pMetricName, const std::vector<std::pair<NVPW_DimUnitName, int8_t>>& expectedDimUnits) {
            NVPW_INFO("pMetricName: ", pMetricName);
            NVPW_MetricEvalRequest metricEvalRequest{};
            NVPW_REQUIRE(ToMetricEvalRequest(metricsEvaluator, pMetricName, metricEvalRequest));

            std::vector<NVPW_DimUnitFactor> actualDimUnitFactors;
            NVPW_REQUIRE(GetMetricDimUnits(metricsEvaluator, metricEvalRequest, actualDimUnitFactors));
            std::sort(actualDimUnitFactors.begin(), actualDimUnitFactors.end());
            NVPW_INFO("actualDimUnitFactors: ", ToString(actualDimUnitFactors, [&](NVPW_DimUnitName dimUnit, bool plural) { return ToCString(metricsEvaluator, dimUnit, plural); }));

            std::vector<NVPW_DimUnitFactor> expectedDimUnitFactors;
            for (const auto dimUnit : expectedDimUnits)
            {
                expectedDimUnitFactors.emplace_back(NVPW_DimUnitFactor{static_cast<uint32_t>(dimUnit.first), dimUnit.second});
            }
            std::sort(expectedDimUnitFactors.begin(), expectedDimUnitFactors.end());
            NVPW_INFO("expectedDimUnitFactors: ", ToString(expectedDimUnitFactors, [&](NVPW_DimUnitName dimUnit, bool plural) { return ToCString(metricsEvaluator, dimUnit, plural); }));
            NVPW_CHECK(actualDimUnitFactors == expectedDimUnitFactors);
        };

        NVPW_SUBCASE("Counters")
        {
            test("smsp__warps_active.sum",                                       { { NVPW_DIM_UNIT_GPC_CYCLES,  int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(1) }                                         });
            test("smsp__warps_active.sum.peak_sustained",                        { { NVPW_DIM_UNIT_WARPS,       int8_t(1) }                                                                             });
            test("smsp__warps_active.sum.peak_sustained_active",                 { { NVPW_DIM_UNIT_GPC_CYCLES,  int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(1) }                                         });
            test("smsp__warps_active.sum.peak_sustained_active.per_second",      { { NVPW_DIM_UNIT_GPC_CYCLES,  int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(1) }, { NVPW_DIM_UNIT_SECONDS, int8_t(-1) }  });
            test("smsp__warps_active.sum.peak_sustained_elapsed",                { { NVPW_DIM_UNIT_GPC_CYCLES,  int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(1) }                                         });
            test("smsp__warps_active.sum.peak_sustained_elapsed.per_second",     { { NVPW_DIM_UNIT_GPC_CYCLES,  int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(1) }, { NVPW_DIM_UNIT_SECONDS, int8_t(-1) }  });
            test("smsp__warps_active.sum.peak_sustained_frame"  ,                { { NVPW_DIM_UNIT_GPC_CYCLES,  int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(1) }                                         });
            test("smsp__warps_active.sum.peak_sustained_frame.per_second",       { { NVPW_DIM_UNIT_GPC_CYCLES,  int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(1) }, { NVPW_DIM_UNIT_SECONDS, int8_t(-1) }  });
            test("smsp__warps_active.sum.peak_sustained_region",                 { { NVPW_DIM_UNIT_GPC_CYCLES,  int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(1) }                                         });
            test("smsp__warps_active.sum.peak_sustained_region.per_second",      { { NVPW_DIM_UNIT_GPC_CYCLES,  int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(1) }, { NVPW_DIM_UNIT_SECONDS, int8_t(-1) }  });
            test("smsp__warps_active.sum.per_cycle_active",                      { { NVPW_DIM_UNIT_WARPS,       int8_t(1) }                                                                             });
            test("smsp__warps_active.sum.per_cycle_elapsed",                     { { NVPW_DIM_UNIT_WARPS,       int8_t(1) }                                                                             });
            test("smsp__warps_active.sum.per_cycle_in_frame",                    { { NVPW_DIM_UNIT_WARPS,       int8_t(1) }                                                                             });
            test("smsp__warps_active.sum.per_cycle_in_region",                   { { NVPW_DIM_UNIT_WARPS,       int8_t(1) }                                                                             });
            test("smsp__warps_active.sum.per_second",                            { { NVPW_DIM_UNIT_GPC_CYCLES,  int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(1) }, { NVPW_DIM_UNIT_SECONDS, int8_t(-1)    }});
            test("smsp__warps_active.sum.pct_of_peak_sustained_active",          { { NVPW_DIM_UNIT_PERCENT,     int8_t(1) }                                                                             });
            test("smsp__warps_active.sum.pct_of_peak_sustained_elapsed",         { { NVPW_DIM_UNIT_PERCENT,     int8_t(1) }                                                                             });
            test("smsp__warps_active.sum.pct_of_peak_sustained_frame",           { { NVPW_DIM_UNIT_PERCENT,     int8_t(1) }                                                                             });
            test("smsp__warps_active.sum.pct_of_peak_sustained_region",          { { NVPW_DIM_UNIT_PERCENT,     int8_t(1) }                                                                             });
        }
        NVPW_SUBCASE("Ratios")
        {
            test("tpc__average_threads_launched_per_warp_shader_ps.max_rate",    { { NVPW_DIM_UNIT_THREADS,     int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(-1) } });
            test("tpc__average_threads_launched_per_warp_shader_ps.pct",         { { NVPW_DIM_UNIT_PERCENT,     int8_t(1) }                                      });
            test("tpc__average_threads_launched_per_warp_shader_ps.ratio",       { { NVPW_DIM_UNIT_THREADS,     int8_t(1) }, { NVPW_DIM_UNIT_WARPS, int8_t(-1) } });
        }
        NVPW_SUBCASE("Throughputs")
        {
            test("sm__throughput.avg.pct_of_peak_sustained_active",              { { NVPW_DIM_UNIT_PERCENT,     int8_t(1) } });
            test("sm__throughput.avg.pct_of_peak_sustained_elapsed",             { { NVPW_DIM_UNIT_PERCENT,     int8_t(1) } });
            test("sm__throughput.avg.pct_of_peak_sustained_frame",               { { NVPW_DIM_UNIT_PERCENT,     int8_t(1) } });
            test("sm__throughput.avg.pct_of_peak_sustained_region",              { { NVPW_DIM_UNIT_PERCENT,     int8_t(1) } });
        }
    }

    NVPW_TEST_CASE("DimUnitToString")
    {
#if defined (__aarch64__)
        const char* pChipName = "GA10B";
#else
        const char* pChipName = "TU102";
#endif
        MetricsEvaluator metricsEvaluator = CreateMetricsEvaluator(pChipName);
        auto test = [&](const std::vector<std::pair<NVPW_DimUnitName, int8_t>>& dimUnits, const std::string& expectedString) {
            std::vector<NVPW_DimUnitFactor> dimUnitFactors;
            for (const auto dimUnit : dimUnits)
            {
                dimUnitFactors.emplace_back(NVPW_DimUnitFactor{static_cast<uint32_t>(dimUnit.first), dimUnit.second});
            }
            const std::string dimUnitsStr = ToString(dimUnitFactors, [&](NVPW_DimUnitName dimUnit, bool plural) {
                return ToCString(metricsEvaluator, dimUnit, plural);
            });
            NVPW_CHECK(dimUnitsStr == expectedString);
        };

        test({ { NVPW_DIM_UNIT_SECONDS,  int8_t( 1) } }, "seconds");
        test({ { NVPW_DIM_UNIT_SECONDS,  int8_t( 2) } }, "seconds^2");
        test({ { NVPW_DIM_UNIT_SECONDS,  int8_t(-1) } }, "1 / second");
        test({ { NVPW_DIM_UNIT_SECONDS,  int8_t(-2) } }, "1 / second^2");
        test({ { NVPW_DIM_UNIT_WARPS,    int8_t( 1) }, { NVPW_DIM_UNIT_SECONDS, int8_t( 1) } }, "(warps * seconds)");
        test({ { NVPW_DIM_UNIT_WARPS,    int8_t(-1) }, { NVPW_DIM_UNIT_SECONDS, int8_t(-1) } }, "1 / (warp * second)");
        test({ { NVPW_DIM_UNIT_WARPS,    int8_t( 1) }, { NVPW_DIM_UNIT_SECONDS, int8_t(-1) } }, "warps / second");
        test({ { NVPW_DIM_UNIT_WARPS,    int8_t( 1) }, { NVPW_DIM_UNIT_SECONDS, int8_t(-1) } }, "warps / second");
        test({ {NVPW_DIM_UNIT_REGISTERS, int8_t( 1) }, { NVPW_DIM_UNIT_THREADS, int8_t( 1) }, { NVPW_DIM_UNIT_SECONDS, int8_t(-1) } }, "(registers * threads) / second");
    }

    NVPW_TEST_CASE("Description")
    {
#if defined (__aarch64__)
        const char* pChipName = "GA10B";
#else
        const char* pChipName = "TU102";
#endif
        MetricsEvaluator metricsEvaluator = CreateMetricsEvaluator(pChipName);
        auto test = [&](const char* pMetricName) {
            NVPW_INFO("pMetricName: ", pMetricName);
            NVPW_MetricType metricType = NVPW_METRIC_TYPE__COUNT;
            size_t metricIndex = (size_t)~0;
            NVPW_REQUIRE(GetMetricTypeAndIndex(metricsEvaluator, pMetricName, metricType, metricIndex));
            NVPW_REQUIRE(metricType != NVPW_METRIC_TYPE__COUNT);
            NVPW_REQUIRE(metricIndex != (size_t)~0);
            const char* pDescription = GetMetricDescription(metricsEvaluator, metricType, metricIndex);
            NVPW_CHECK(pDescription);
            if (pDescription)
            {
                NVPW_CHECK(strlen(pDescription));
            }
        };

        NVPW_SUBCASE("Counters")
        {
            test("smsp__inst_executed");
        }
        NVPW_SUBCASE("Ratios")
        {
            test("tpc__average_registers_per_thread");
        }
        NVPW_SUBCASE("Throughputs")
        {
            test("zrop__throughput");
        }
    }

    NVPW_TEST_CASE("HwUnit")
    {
#if defined (__aarch64__)
        const char* pChipName = "GA10B";
#else
        const char* pChipName = "TU102";
#endif
        MetricsEvaluator metricsEvaluator = CreateMetricsEvaluator(pChipName);
        auto test = [&](const char* pMetricName, const char* pExpectedHwUnitStr) {
            NVPW_INFO("pMetricName: ", pMetricName);
            NVPW_MetricType metricType = NVPW_METRIC_TYPE__COUNT;
            size_t metricIndex = (size_t)~0;
            NVPW_REQUIRE(GetMetricTypeAndIndex(metricsEvaluator, pMetricName, metricType, metricIndex));
            NVPW_REQUIRE(metricType != NVPW_METRIC_TYPE__COUNT);
            NVPW_REQUIRE(metricIndex != (size_t)~0);
            const char* pHwUnitStr = GetMetricHwUnitStr(metricsEvaluator, metricType, metricIndex);
            NVPW_CHECK(pHwUnitStr);
            if (pHwUnitStr)
            {
                NVPW_CHECK(strcmp(pHwUnitStr, pExpectedHwUnitStr) == 0);
            }
        };

        NVPW_SUBCASE("Counters")
        {
            test("smsp__inst_executed", "smsp");
        }
        NVPW_SUBCASE("Ratios")
        {
            test("tpc__average_registers_per_thread", "tpc");
        }
        NVPW_SUBCASE("Throughputs")
        {
            test("zrop__throughput", "zrop");
#if !defined (__aarch64__)
            test("PCI.TriageA.pcie__throughput", "pcie");
#endif
        }
    }

    NVPW_TEST_SUITE_END();

}}} // nv::perf::test
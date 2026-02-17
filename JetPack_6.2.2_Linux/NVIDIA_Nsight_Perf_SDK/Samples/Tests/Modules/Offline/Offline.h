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
#include <TestCommon.h>
#include <NvPerfVulkan.h>
#include <NvPerfMetricsConfigBuilder.h>
#include <NvPerfMetricsEvaluator.h>
#include <NvPerfReportDefinitionHAL.h>
#include <vector>

namespace nv { namespace perf { namespace test {

    inline void GetHtmlReport(const char* pChipName)
    {
        const ReportDefinition reportDefinition = PerRangeReport::GetReportDefinition(pChipName);
        NVPW_CHECK(reportDefinition.pReportHtml);
    }

    inline MetricsEvaluator CreateMetricsEvaluator(const char* pChipName)
    {
        const size_t scratchBufferSize = VulkanCalculateMetricsEvaluatorScratchBufferSize(pChipName);
        NVPW_REQUIRE(scratchBufferSize);
        std::vector<uint8_t> scratchBuffer(scratchBufferSize);
        NVPW_MetricsEvaluator* pMetricsEvaluator = VulkanCreateMetricsEvaluator(scratchBuffer.data(), scratchBuffer.size(), pChipName);
        NVPW_REQUIRE(pMetricsEvaluator);
        return MetricsEvaluator(pMetricsEvaluator, std::move(scratchBuffer));
    }

    inline void HtmlReportMetricsConfiguration(const char* pChipName)
    {
        NVPA_RawMetricsConfig* pRawMetricsConfig = profiler::VulkanCreateRawMetricsConfig(pChipName);
        NVPW_REQUIRE(pRawMetricsConfig);

        MetricsEvaluator metricsEvaluator = CreateMetricsEvaluator(pChipName);

        MetricsConfigBuilder configBuilder;
        NVPW_REQUIRE(configBuilder.Initialize(metricsEvaluator, pRawMetricsConfig, pChipName));

        const ReportDefinition reportDefinition = PerRangeReport::GetReportDefinition(pChipName);
        NVPW_REQUIRE(reportDefinition.pReportHtml);

        for (uint8_t metricTypeInt = 0; metricTypeInt < NVPW_METRIC_TYPE__COUNT; ++metricTypeInt)
        {
            const NVPW_MetricType metricType = static_cast<NVPW_MetricType>(metricTypeInt);
            NVPW_INFO("metricType: ", ToCString(metricType)); // this will only get printed if any of the below checks failed
            std::vector<NVPW_Submetric> supportedSubmetrics;
            NVPW_REQUIRE(GetSupportedSubmetrics(metricsEvaluator, metricType, supportedSubmetrics));
            NVPW_REQUIRE(supportedSubmetrics.size());

            const char* const* ppMetricNames = nullptr;
            size_t numMetrics = 0;
            if (metricType == NVPW_METRIC_TYPE_COUNTER)
            {
                ppMetricNames = reportDefinition.ppCounterNames;
                numMetrics = reportDefinition.numCounters;
            }
            else if (metricType == NVPW_METRIC_TYPE_RATIO)
            {
                ppMetricNames = reportDefinition.ppRatioNames;
                numMetrics = reportDefinition.numRatios;
            }
            else
            {
                NVPW_REQUIRE(metricType == NVPW_METRIC_TYPE_THROUGHPUT);
                ppMetricNames = reportDefinition.ppThroughputNames;
                numMetrics = reportDefinition.numThroughputs;
            }

            NVPW_MetricEvalRequest metricEvalRequest;
            metricEvalRequest.metricType = metricTypeInt;
            metricEvalRequest.rollupOp = static_cast<uint8_t>(NVPW_ROLLUP_OP_AVG);
            for (size_t metricIdx = 0; metricIdx < numMetrics; ++metricIdx)
            {
                const char* const pBaseMetricName = ppMetricNames[metricIdx];
                NVPW_INFO("pBaseMetricName: ", pBaseMetricName); // this will only get printed if any of the below checks failed
                NVPW_MetricType actualMetricType = NVPW_METRIC_TYPE__COUNT;
                size_t metricIndex = ~size_t(0);
                const bool getMetricTypeAndIndexSuccess = GetMetricTypeAndIndex(metricsEvaluator, pBaseMetricName, actualMetricType, metricIndex);
                NVPW_CHECK(getMetricTypeAndIndexSuccess);
                if (!getMetricTypeAndIndexSuccess)
                {
                    continue;
                }
                NVPW_CHECK(actualMetricType == metricType);
                NVPW_CHECK(metricIndex != ~size_t(0));
                if (actualMetricType != metricType || metricIndex == ~size_t(0))
                {
                    continue;
                }
                metricEvalRequest.metricIndex = metricIndex;
                for (NVPW_Submetric submetric : supportedSubmetrics)
                {
                    metricEvalRequest.submetric = static_cast<uint16_t>(submetric);
                    NVPW_CHECK(configBuilder.AddMetrics(&metricEvalRequest, 1));
                }
            }
        }
    }

}}} // nv::perf::test
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
#include "NvPerfReportGenerator.h"
#include <json/json.hpp>
#include <functional>
#include <algorithm>
#include <string.h>

namespace nv { namespace perf { namespace test {

    NVPW_TEST_SUITE_BEGIN("HtmlReport");

    NVPW_TEST_CASE("ChipSupport")
    {
#if defined (__aarch64__)
        NVPW_SUBCASE("GA10B")
        {
            GetHtmlReport("GA10B");
        }
#else
        NVPW_SUBCASE("TU102")
        {
            GetHtmlReport("TU102");
        }
        NVPW_SUBCASE("TU104")
        {
            GetHtmlReport("TU104");
        }
        NVPW_SUBCASE("TU106")
        {
            GetHtmlReport("TU106");
        }
        NVPW_SUBCASE("TU116")
        {
            GetHtmlReport("TU116");
        }
        NVPW_SUBCASE("TU117")
        {
            GetHtmlReport("TU117");
        }
        NVPW_SUBCASE("GA102")
        {
            GetHtmlReport("GA102");
        }
        NVPW_SUBCASE("GA103")
        {
            GetHtmlReport("GA103");
        }
        NVPW_SUBCASE("GA104")
        {
            GetHtmlReport("GA104");
        }
        NVPW_SUBCASE("GA106")
        {
            GetHtmlReport("GA106");
        }
        NVPW_SUBCASE("GA107")
        {
            GetHtmlReport("GA107");
        }
        NVPW_SUBCASE("AD102")
        {
            GetHtmlReport("AD102");
        }
        NVPW_SUBCASE("AD103")
        {
            GetHtmlReport("AD103");
        }
        NVPW_SUBCASE("AD104")
        {
            GetHtmlReport("AD104");
        }
        NVPW_SUBCASE("AD106")
        {
            GetHtmlReport("AD106");
        }
        NVPW_SUBCASE("AD107")
        {
            GetHtmlReport("AD107");
        }
#endif
    }

    NVPW_TEST_CASE("MetricsConfiguration")
    {
#if defined (__aarch64__)
        NVPW_SUBCASE("GA10B")
        {
            HtmlReportMetricsConfiguration("GA10B");
        }
#else
        NVPW_SUBCASE("TU102")
        {
            HtmlReportMetricsConfiguration("TU102");
        }
        NVPW_SUBCASE("TU116")
        {
            HtmlReportMetricsConfiguration("TU116");
        }
        NVPW_SUBCASE("GA102")
        {
            HtmlReportMetricsConfiguration("GA102");
        }
        NVPW_SUBCASE("GA103")
        {
            HtmlReportMetricsConfiguration("GA103");
        }
        NVPW_SUBCASE("GA104")
        {
            HtmlReportMetricsConfiguration("GA104");
        }
        NVPW_SUBCASE("GA106")
        {
            HtmlReportMetricsConfiguration("GA106");
        }
        NVPW_SUBCASE("GA107")
        {
            HtmlReportMetricsConfiguration("GA107");
        }
        NVPW_SUBCASE("AD102")
        {
            HtmlReportMetricsConfiguration("AD102");
        }
        NVPW_SUBCASE("AD103")
        {
            HtmlReportMetricsConfiguration("AD103");
        }
        NVPW_SUBCASE("AD104")
        {
            HtmlReportMetricsConfiguration("AD104");
        }
        NVPW_SUBCASE("AD106")
        {
            HtmlReportMetricsConfiguration("AD106");
        }
        NVPW_SUBCASE("AD107")
        {
            HtmlReportMetricsConfiguration("AD107");
        }
#endif
    }

    NVPW_TEST_CASE("JSONSyntax")
    {
        NVPW_SUBCASE("Summary")
        {
#if defined (__aarch64__)
            const char* pChipName = "GA10B";
#else
            const char* pChipName = "GA102";
#endif
            MetricsEvaluator metricsEvaluator = CreateMetricsEvaluator(pChipName);
            ReportLayout reportLayout = {};
            reportLayout.chipName = pChipName;
            ReportData reportData = {};
            std::string jsonString = SummaryReport::MakeJsonContents(metricsEvaluator, reportLayout, reportData);
            jsonString = "{" + jsonString + "}";
            auto j = nlohmann::json::parse(jsonString, nullptr, false);
            NVPW_REQUIRE_NE(j.type(), nlohmann::json::value_t::discarded);
            NVPW_CHECK_EQ(j["device"]["chipName"], pChipName);
        }
        NVPW_SUBCASE("PerRange")
        {
#if defined (__aarch64__)
            const char* pChipName = "GA10B";
#else
            const char* pChipName = "GA102";
#endif
            MetricsEvaluator metricsEvaluator = CreateMetricsEvaluator(pChipName);
            ReportLayout reportLayout = {};
            reportLayout.chipName = pChipName;
            ReportData reportData = {};
            reportData.ranges.resize(1);
            std::string jsonString = PerRangeReport::MakeJsonContents(metricsEvaluator, reportLayout, reportData, 0);
            jsonString = "{" + jsonString + "}";
            auto j = nlohmann::json::parse(jsonString, nullptr, false);
            NVPW_REQUIRE_NE(j.type(), nlohmann::json::value_t::discarded);
            NVPW_CHECK_EQ(j["device"]["chipName"], pChipName);
        }
        NVPW_SUBCASE("DoubleNumber")
        {
            NVPW_CHECK_NE(nlohmann::json::parse(R"({"a": 123.456})", nullptr, false).type(), nlohmann::json::value_t::discarded);
            NVPW_CHECK_NE(nlohmann::json::parse(R"({"b": )" + FormatJsDouble(NAN) + "}", nullptr, false).type(), nlohmann::json::value_t::discarded);
            NVPW_CHECK_NE(nlohmann::json::parse(R"({"c": )" + FormatJsDouble(INFINITY) + "}", nullptr, false).type(), nlohmann::json::value_t::discarded);
            NVPW_CHECK_NE(nlohmann::json::parse(R"({"c": )" + FormatJsDouble(-INFINITY) + "}", nullptr, false).type(), nlohmann::json::value_t::discarded);
        }
    }
    NVPW_TEST_SUITE_END();

}}} // nv::perf::test
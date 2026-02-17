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

#include <doctest/doctest.h>
#include <ostream>
#include <string>
#include <string.h>
#include <algorithm>

namespace nv { namespace perf { namespace test {

    using namespace doctest;

    struct QAReporter : public IReporter
    {
        std::ostream& os;
        const ContextOptions& options;
        std::string binaryName;

        QAReporter(const ContextOptions& contextOptions)
            : os(*contextOptions.cout)
            , options(contextOptions)
            , binaryName(contextOptions.binary_name.c_str())
        {
            const std::size_t pos = binaryName.find_first_of(".");
            if (pos != std::string::npos)
            {
                binaryName.erase(pos);
            }
        }

        void sanitize_test_name(std::string& testName) const
        {
            std::replace(testName.begin(), testName.end(), ' ', '_');
        }

        void print_test_name(const TestCaseData& testCaseData)
        {
            std::string testGroupName(testCaseData.m_test_suite);
            if (testGroupName == "")
            {
                testGroupName = binaryName;
            }
            sanitize_test_name(testGroupName);

            std::string testCaseName(testCaseData.m_name);
            sanitize_test_name(testCaseName);

            const std::string TestGroupPrefix = "PerfSDK.NvPerfUtility.";
            os << "[T:" << testCaseName.c_str() << "(" << TestGroupPrefix << testGroupName.c_str() << ")]\n";
        }

        void report_query(const QueryData& queryData) override
        {
        }

        void test_run_start() override
        {
        }

        void test_run_end(const TestRunStats& testRunStats) override
        {
        }

        void test_case_start(const TestCaseData& testCaseData) override
        {
            print_test_name(testCaseData);
        }

        void test_case_reenter(const TestCaseData& testCaseData) override
        {
        }

        void test_case_end(const CurrentTestCaseStats& currentTestCaseStats) override
        {
            if (currentTestCaseStats.failure_flags)
            {
                os << "[R:ERROR]\n";
            }
            else
            {
                os << "[R:OK]\n";
            }
        }

        void test_case_exception(const TestCaseException& testCaseException) override
        {
            os << "[R:ERROR]\n";
        }

        void subcase_start(const SubcaseSignature& subcaseSignature) override
        {
        }

        void subcase_end() override
        {
        }

        void log_assert(const AssertData& assertData) override
        {
        }

        void log_message(const MessageData& messageData) override
        {
        }

        void test_case_skipped(const TestCaseData& testCaseData) override
        {
            print_test_name(testCaseData);
            os << "[R:SKIP]\n";
        }
    };

    struct SimplifiedGTestReporter : public IReporter
    {
        std::ostream& os;
        const ContextOptions& options;
        const TestCaseData* pCurrentTestCaseData;
        std::string binaryName;

        SimplifiedGTestReporter(const ContextOptions& contextOptions)
            : os(*contextOptions.cout)
            , options(contextOptions)
            , pCurrentTestCaseData()
            , binaryName(contextOptions.binary_name.c_str())
        {
            const std::size_t pos = binaryName.find_first_of(".");
            if (pos != std::string::npos)
            {
                binaryName.erase(pos);
            }
        }

        void print_current_test_name()
        {
            const char* pTestGroupName = pCurrentTestCaseData->m_test_suite;
            if (!strcmp(pTestGroupName, ""))
            {
                pTestGroupName = binaryName.c_str();
            }
            os << pCurrentTestCaseData->m_name << "." << pTestGroupName;
        }

        void report_query(const QueryData& queryData) override
        {
        }

        void test_run_start() override
        {
            os << Color::Green << "[----------] " << Color::None << "\n";
        }

        void test_run_end(const TestRunStats& testRunStats) override
        {
            const size_t numTotalTestCases = testRunStats.numTestCases;
            const size_t numFailedTestCases = testRunStats.numTestCasesFailed;
            const size_t numPassedTestCases = testRunStats.numTestCasesPassingFilters - testRunStats.numTestCasesFailed;
            const size_t numSkippedTestCases = testRunStats.numTestCases - testRunStats.numTestCasesPassingFilters;
            os << Color::Green << "[----------] " << Color::None << "\n";
            os << Color::Green << "[==========] " << Color::None << numTotalTestCases   << " test(s) ran." << "\n";
            os << Color::Green << "[  PASSED  ] " << Color::None << numPassedTestCases  << " test(s)." << "\n";
            os << Color::Green << "[  SKIPPED ] " << Color::None << numSkippedTestCases << " test(s)." << "\n";
            if (numFailedTestCases)
            {
                os << Color::Red   << "[  FAILED  ] " << Color::None << numFailedTestCases  << " test(s)." << "\n";
            }
        }

        void test_case_start(const TestCaseData& testCaseData) override
        {
            pCurrentTestCaseData = &testCaseData;
            os << Color::Green << "[ RUN      ] " << Color::None;
            print_current_test_name();
            os << "\n";
        }

        void test_case_reenter(const TestCaseData& testCaseData) override
        {
        }

        void test_case_end(const CurrentTestCaseStats& currentTestCaseStats) override
        {
            if (currentTestCaseStats.failure_flags)
            {
                os << Color::Red << "[  FAILED  ] " << Color::None;
            }
            else
            {
                os << Color::Green << "[       OK ] " << Color::None;
            }
            print_current_test_name();
            os << " (" << size_t(currentTestCaseStats.seconds * 1000) << " ms)\n";
        }

        void test_case_exception(const TestCaseException& testCaseException) override
        {
            os << Color::Red << "[  FAILED  ] " << Color::None;
            print_current_test_name();
            os << " (??? ms)\n";
        }

        void subcase_start(const SubcaseSignature& subcaseSignature) override
        {
        }

        void subcase_end() override
        {
        }

        void log_assert(const AssertData& assertData) override
        {
        }

        void log_message(const MessageData& messageData) override
        {
        }

        void test_case_skipped(const TestCaseData& testCaseData) override
        {
            pCurrentTestCaseData = &testCaseData;
            os << Color::Green << "[ RUN      ] " << Color::None;
            print_current_test_name();
            os << "\n";
            os << Color::Green << "[  SKIPPED ] " << Color::None;
            print_current_test_name();
            os << " (0 ms)\n";
        }
    };

}}} // nv::perf::test

// use "-r=qa" to enable the qa reporter, since it doesn't log assertions, you may want to use it together with built-in reporters, e.g. "-r=qa,console"
DOCTEST_REGISTER_REPORTER("qa", 0, ::nv::perf::test::QAReporter);
// use "-r=gtest" to enable the gtest reporter, since it doesn't log assertions, you may want to use it together with built-in reporters, e.g. "-r=gtest,console"
DOCTEST_REGISTER_REPORTER("gtest", 0, ::nv::perf::test::SimplifiedGTestReporter);
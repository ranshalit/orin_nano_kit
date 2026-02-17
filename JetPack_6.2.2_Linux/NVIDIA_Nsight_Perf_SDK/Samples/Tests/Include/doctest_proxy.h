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

#ifndef DOCTEST_CONFIG_DISABLE
#define DOCTEST_CONFIG_SUPER_FAST_ASSERTS // defined for better performance - both for compilation and execution
#endif

#ifndef DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES
#define DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES
#endif

#include <doctest/doctest.h>

#ifndef NVPW_PROXY_MACROS
#define NVPW_PROXY_MACROS

#define NVPW_TEST_CASE                  DOCTEST_TEST_CASE
#define NVPW_TEST_CASE_CLASS            DOCTEST_TEST_CASE_CLASS
#define NVPW_TEST_CASE_FIXTURE          DOCTEST_TEST_CASE_FIXTURE
#define NVPW_SUBCASE                    DOCTEST_SUBCASE
#define NVPW_TEST_SUITE                 DOCTEST_TEST_SUITE
#define NVPW_TEST_SUITE_BEGIN           DOCTEST_TEST_SUITE_BEGIN
#define NVPW_TEST_SUITE_END             DOCTEST_TEST_SUITE_END
#define NVPW_INFO                       DOCTEST_INFO
#define NVPW_TEST_MESSAGE               DOCTEST_MESSAGE
#define NVPW_WARN                       DOCTEST_WARN
#define NVPW_WARN_FALSE                 DOCTEST_WARN_FALSE
#define NVPW_WARN_THROWS                DOCTEST_WARN_THROWS
#define NVPW_WARN_THROWS_AS             DOCTEST_WARN_THROWS_AS
#define NVPW_WARN_THROWS_WITH           DOCTEST_WARN_THROWS_WITH
#define NVPW_WARN_THROWS_WITH_AS        DOCTEST_WARN_THROWS_WITH_AS
#define NVPW_WARN_NOTHROW               DOCTEST_WARN_NOTHROW
#define NVPW_CHECK                      DOCTEST_CHECK
#define NVPW_CHECK_FALSE                DOCTEST_CHECK_FALSE
#define NVPW_CHECK_THROWS               DOCTEST_CHECK_THROWS
#define NVPW_CHECK_THROWS_AS            DOCTEST_CHECK_THROWS_AS
#define NVPW_CHECK_THROWS_WITH          DOCTEST_CHECK_THROWS_WITH
#define NVPW_CHECK_THROWS_WITH_AS       DOCTEST_CHECK_THROWS_WITH_AS
#define NVPW_CHECK_NOTHROW              DOCTEST_CHECK_NOTHROW
#define NVPW_CHECK_MESSAGE              DOCTEST_CHECK_MESSAGE
#define NVPW_REQUIRE                    DOCTEST_REQUIRE
#define NVPW_REQUIRE_FALSE              DOCTEST_REQUIRE_FALSE
#define NVPW_REQUIRE_THROWS             DOCTEST_REQUIRE_THROWS
#define NVPW_REQUIRE_THROWS_AS          DOCTEST_REQUIRE_THROWS_AS
#define NVPW_REQUIRE_THROWS_WITH_AS     DOCTEST_REQUIRE_THROWS_WITH_AS
#define NVPW_REQUIRE_NOTHROW            DOCTEST_REQUIRE_NOTHROW

#define NVPW_SCENARIO                   DOCTEST_SCENARIO
#define NVPW_GIVEN                      DOCTEST_GIVEN
#define NVPW_WHEN                       DOCTEST_WHEN
#define NVPW_AND_WHEN                   DOCTEST_AND_WHEN
#define NVPW_THEN                       DOCTEST_THEN
#define NVPW_AND_THEN                   DOCTEST_AND_THEN

#define NVPW_WARN_EQ                    DOCTEST_WARN_EQ
#define NVPW_CHECK_EQ                   DOCTEST_CHECK_EQ
#define NVPW_REQUIRE_EQ                 DOCTEST_REQUIRE_EQ
#define NVPW_WARN_NE                    DOCTEST_WARN_NE
#define NVPW_CHECK_NE                   DOCTEST_CHECK_NE
#define NVPW_REQUIRE_NE                 DOCTEST_REQUIRE_NE
#define NVPW_WARN_GT                    DOCTEST_WARN_GT
#define NVPW_CHECK_GT                   DOCTEST_CHECK_GT
#define NVPW_REQUIRE_GT                 DOCTEST_REQUIRE_GT
#define NVPW_WARN_LT                    DOCTEST_WARN_LT
#define NVPW_CHECK_LT                   DOCTEST_CHECK_LT
#define NVPW_REQUIRE_LT                 DOCTEST_REQUIRE_LT
#define NVPW_WARN_GE                    DOCTEST_WARN_GE
#define NVPW_CHECK_GE                   DOCTEST_CHECK_GE
#define NVPW_REQUIRE_GE                 DOCTEST_REQUIRE_GE
#define NVPW_WARN_LE                    DOCTEST_WARN_LE
#define NVPW_CHECK_LE                   DOCTEST_CHECK_LE
#define NVPW_REQUIRE_LE                 DOCTEST_REQUIRE_LE
#define NVPW_WARN_UNARY                 DOCTEST_WARN_UNARY
#define NVPW_CHECK_UNARY                DOCTEST_CHECK_UNARY
#define NVPW_REQUIRE_UNARY              DOCTEST_REQUIRE_UNARY
#define NVPW_WARN_UNARY_FALSE           DOCTEST_WARN_UNARY_FALSE
#define NVPW_CHECK_UNARY_FALSE          DOCTEST_CHECK_UNARY_FALSE
#define NVPW_REQUIRE_UNARY_FALSE        DOCTEST_REQUIRE_UNARY_FALSE

#endif // NVPW_PROXY_MACROS

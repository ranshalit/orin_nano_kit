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

#include <NvPerfInit.h>

namespace nv { namespace perf { namespace test {

    class ScopedNvPerfLogDisabler
    {
    private:
        bool m_appendToFile;
        bool m_appendToStderr;
    public:
        ScopedNvPerfLogDisabler()
        {
            m_appendToFile = nv::perf::GetLogSettingsStorage_()->appendToFile;
            m_appendToStderr = nv::perf::GetLogSettingsStorage_()->writeStderr;
            nv::perf::SetLogAppendToFile(false);
            nv::perf::UserLogEnableStderr(false);
        }
        ~ScopedNvPerfLogDisabler()
        {
            nv::perf::SetLogAppendToFile(m_appendToFile);
            nv::perf::UserLogEnableStderr(m_appendToStderr);
        }
    };

}}} // nv::perf::test
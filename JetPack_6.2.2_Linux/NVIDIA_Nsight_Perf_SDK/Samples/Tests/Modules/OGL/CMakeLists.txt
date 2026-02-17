#[[
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
]]
if(UNIX)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
endif()

set(OGL_SAMPLE_DIR ../../../OGL/gtruc/ogl-samples)
set(GLFW_SOURCE_DIR ${OGL_SAMPLE_DIR}/external/glfw-3.3.8/src)
set(GLFW_INCLUDE_DIR ${OGL_SAMPLE_DIR}/external/glfw-3.3.8/include)
set(GLFW_SOURCES
    ${GLFW_SOURCE_DIR}/context.c
    ${GLFW_SOURCE_DIR}/init.c
    ${GLFW_SOURCE_DIR}/input.c
    ${GLFW_SOURCE_DIR}/monitor.c
    ${GLFW_SOURCE_DIR}/vulkan.c
    ${GLFW_SOURCE_DIR}/window.c
)

if(WIN32)
    list(APPEND GLFW_SOURCES
        ${GLFW_SOURCE_DIR}/win32_init.c
        ${GLFW_SOURCE_DIR}/win32_joystick.c
        ${GLFW_SOURCE_DIR}/win32_monitor.c
        ${GLFW_SOURCE_DIR}/win32_time.c
        ${GLFW_SOURCE_DIR}/win32_thread.c
        ${GLFW_SOURCE_DIR}/win32_window.c
        ${GLFW_SOURCE_DIR}/wgl_context.c
        ${GLFW_SOURCE_DIR}/egl_context.c
        ${GLFW_SOURCE_DIR}/osmesa_context.c
    )
elseif (UNIX)
    list(APPEND GLFW_SOURCES
        ${GLFW_SOURCE_DIR}/x11_init.c
        ${GLFW_SOURCE_DIR}/x11_monitor.c
        ${GLFW_SOURCE_DIR}/x11_window.c
        ${GLFW_SOURCE_DIR}/xkb_unicode.c
        ${GLFW_SOURCE_DIR}/posix_time.c
        ${GLFW_SOURCE_DIR}/posix_thread.c
        ${GLFW_SOURCE_DIR}/glx_context.c
        ${GLFW_SOURCE_DIR}/egl_context.c
        ${GLFW_SOURCE_DIR}/osmesa_context.c
        ${GLFW_SOURCE_DIR}/linux_joystick.c
    )
endif()
add_library(GLFW ${GLFW_SOURCES})
target_include_directories(GLFW
    PUBLIC
        ${GLFW_INCLUDE_DIR}
        ${OGL_SAMPLE_DIR}/external
)
target_compile_definitions(GLFW PUBLIC _GLFW_USE_OPENGL GLEW_STATIC _GLFW_HAS_GLXGETPROCADDRESSARB)

if(WIN32)
    target_compile_definitions(GLFW PUBLIC _GLFW_WIN32 _GLFW_WGL)
    target_compile_options(GLFW PRIVATE /wd4457 /wd4244 /wd4152 /wd4204 /wd4456)
    target_link_libraries(GLFW PUBLIC opengl32)
elseif (UNIX)
    target_compile_definitions(GLFW PUBLIC _GLFW_X11 _GLFW_GLX)
    target_compile_options(GLFW PRIVATE -Wno-format-truncation)
    target_link_libraries(GLFW PUBLIC GL rt X11 Xxf86vm Xrandr Xi Xinerama Xcursor Threads::Threads)
endif()

set(SOURCES
    HeaderSanity/HeaderSanity_NvPerfOpenGL.cpp
    HeaderSanity/HeaderSanity_NvPerfRangeProfilerOpenGL.cpp
    HeaderSanity/HeaderSanity_NvPerfReportGeneratorOpenGL.cpp
    OpenGL_ReportGenerator.cpp
    OpenGLMain.cpp
    ${OGL_SAMPLE_DIR}/framework/glew.c
)
add_executable(NvPerfOpenGLTest
    ${SOURCES}
    ${NVPERF_FILES}
    ${NVPERF_UTILITY_FILES}
)
target_include_directories(NvPerfOpenGLTest
    PRIVATE
        ${NVPERF_TEST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(NvPerfOpenGLTest
    PRIVATE
        NvPerf
        NvPerfUtility
        GLFW
)

if(MSVC)
  target_compile_options(NvPerfOpenGLTest PRIVATE /WX)
  set_source_files_properties(${OGL_SAMPLE_DIR}/framework/glew.c PROPERTIES COMPILE_FLAGS /wd4456)
endif()

DeployNvPerf(NvPerfOpenGLTest NvPerf)
#pragma once
#include <GLFW/glfw3.h>
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
#ifdef _WIN32
// Suppress redifinition warnings
#undef APIENTRY
// undef min and max from windows.h
#define NOMINMAX
#endif
#include <NvPerfReportGeneratorOpenGL.h>
#ifdef __linux__
#include <NvPerfReportGeneratorEGL.h>
#endif
#endif

class ContextOpenGL
{
public:
	static const int contextAPI = GLFW_NATIVE_CONTEXT_API;

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
	using ReportGeneratorType = nv::perf::profiler::ReportGeneratorOpenGL;
	static NVPW_Device_ClockStatus GetDeviceClockState()
	{
		return nv::perf::OpenGLGetDeviceClockState();
	}

	static bool SetDeviceClockState(NVPW_Device_ClockSetting clockStatus)
	{
		return nv::perf::OpenGLSetDeviceClockState(clockStatus);
	}

	static bool SetDeviceClockState(NVPW_Device_ClockStatus clockStatus)
	{
		return nv::perf::OpenGLSetDeviceClockState(clockStatus);
	}
#endif
};

#ifdef __linux__
class ContextEGL
{
public:
	static const int contextAPI = GLFW_EGL_CONTEXT_API;

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
	using ReportGeneratorType = nv::perf::profiler::ReportGeneratorEGL;

	static NVPW_Device_ClockStatus GetDeviceClockState()
	{
		return nv::perf::EGLGetDeviceClockState();
	}

	static bool SetDeviceClockState(NVPW_Device_ClockSetting clockStatus)
	{
		return nv::perf::EGLSetDeviceClockState(clockStatus);
	}

	static bool SetDeviceClockState(NVPW_Device_ClockStatus clockStatus)
	{
		return nv::perf::EGLSetDeviceClockState(clockStatus);
	}
#endif
};
#endif
/*
* Copyright 2014-2023 NVIDIA Corporation.  All rights reserved.
*
* The code enclosed by #ifdef NV_PERF_ENABLE_INSTRUMENTATION ... #endif
* is licensed under the Apache License, Version 2.0 (the "License");
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
#include "test.hpp"
#include "context.hpp"
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
#ifdef _WIN32
// Suppress redifinition warnings
#undef APIENTRY
// undef min and max from windows.h
#define NOMINMAX
#endif
#include <nvperf_host_impl.h>
#include <NvPerfReportGeneratorOpenGL.h>
#ifdef __linux__
#include <NvPerfReportGeneratorEGL.h>
#endif
#endif

namespace
{
	char const* VERT_SHADER_SOURCE("gl-420/draw-base-instance.vert");
	char const* FRAG_SHADER_SOURCE("gl-420/draw-base-instance.frag");

	GLsizei const ElementCount(6);
	GLsizeiptr const ElementSize = ElementCount * sizeof(glm::uint32);
	glm::uint32 const ElementData[ElementCount] =
	{
		0, 1, 2,
		0, 2, 3
	};

	GLsizei const InstanceCount(5);

	GLsizei const VertexCount(5);
	GLsizeiptr const PositionSize = VertexCount * sizeof(glm::vec2);
	glm::vec2 const PositionData[VertexCount] =
	{
		glm::vec2( 0.0f, 0.0f),
		glm::vec2(-1.0f,-1.0f),
		glm::vec2( 1.0f,-1.0f),
		glm::vec2( 1.0f, 1.0f),
		glm::vec2(-1.0f, 1.0f)
	};

	GLsizei const ColorCount(10);
	GLsizeiptr const ColorSize = ColorCount * sizeof(glm::vec4);
	glm::vec4 const ColorData[ColorCount] =
	{
		glm::vec4(1.0f, 0.5f, 0.0f, 1.0f),
		glm::vec4(0.8f, 0.4f, 0.0f, 1.0f),
		glm::vec4(0.6f, 0.3f, 0.0f, 1.0f),
		glm::vec4(0.4f, 0.2f, 0.0f, 1.0f),
		glm::vec4(0.2f, 0.1f, 0.0f, 1.0f),
		glm::vec4(0.0f, 0.1f, 0.2f, 1.0f),
		glm::vec4(0.0f, 0.2f, 0.4f, 1.0f),
		glm::vec4(0.0f, 0.3f, 0.6f, 1.0f),
		glm::vec4(0.0f, 0.4f, 0.8f, 1.0f),
		glm::vec4(0.0f, 0.5f, 1.0f, 1.0f)
	};

	namespace buffer
	{
		enum type
		{
			POSITION,
			COLOR,
			ELEMENT,
			TRANSFORM,
			MAX
		};
	}//namespace buffer

	GLuint PipelineName(0);
	GLuint ProgramName(0);
	GLuint VertexArrayName(0);
	GLuint BufferName[buffer::MAX] = {0, 0, 0, 0};
}//namespace


template <typename context>
class sample : public framework
{
private:
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
	typename context::ReportGeneratorType m_nvperf;
	double m_nvperfWarmupTime = 0.5; // Wait 0.5s to allow the clock to stabalize before begining to profile
	NVPW_Device_ClockStatus m_clockStatus = NVPW_DEVICE_CLOCK_STATUS_UNKNOWN; // Used to restore clock state when exiting
#endif

public:
	sample(int argc, char* argv[]) :
		framework(argc, argv, "gl-420-draw-base-instance", framework::CORE, 4, 2, glm::uvec2(640, 480), - glm::vec2(-glm::pi<float>() * 0.2f),
		glm::vec2(0, 4), 2, MATCH_TEMPLATE, HEURISTIC_ALL, context::contextAPI)
	{}

private:
	bool initTest()
	{
		bool Validated(true);

		glEnable(GL_DEPTH_TEST);

		return Validated;
	}

	bool initProgram()
	{
		bool Validated(true);
	
		glGenProgramPipelines(1, &PipelineName);

		if(Validated)
		{
			compiler Compiler;
			GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, getDataDirectory() + VERT_SHADER_SOURCE, 
				"--version 420 --profile core");
			GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, getDataDirectory() + FRAG_SHADER_SOURCE,
				"--version 420 --profile core");
			Validated = Validated && Compiler.check();

			ProgramName = glCreateProgram();
			glProgramParameteri(ProgramName, GL_PROGRAM_SEPARABLE, GL_TRUE);
			glAttachShader(ProgramName, VertShaderName);
			glAttachShader(ProgramName, FragShaderName);
			glLinkProgram(ProgramName);

			Validated = Validated && Compiler.check_program(ProgramName);
		}

		if(Validated)
			glUseProgramStages(PipelineName, GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName);

		return Validated;
	}

	bool initBuffer()
	{
		bool Validated(true);

		glGenBuffers(buffer::MAX, BufferName);

		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::POSITION]);
		glBufferData(GL_ARRAY_BUFFER, PositionSize, PositionData, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::COLOR]);
		glBufferData(GL_ARRAY_BUFFER, ColorSize, ColorData, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		return Validated;
	}

	bool initVertexArray()
	{
		bool Validated(true);

		glGenVertexArrays(1, &VertexArrayName);
		glBindVertexArray(VertexArrayName);
			glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::POSITION]);
			glVertexAttribPointer(semantic::attr::POSITION, 2, GL_FLOAT, GL_FALSE, 0, 0);
			glVertexAttribDivisor(semantic::attr::POSITION, 0);
			glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::COLOR]);
			glVertexAttribPointer(semantic::attr::COLOR, 4, GL_FLOAT, GL_FALSE, 0, 0);
			glVertexAttribDivisor(semantic::attr::COLOR, 1);

			glEnableVertexAttribArray(semantic::attr::POSITION);
			glEnableVertexAttribArray(semantic::attr::COLOR);
		glBindVertexArray(0);

		return Validated;
	}

	bool begin()
	{
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		nv::perf::InitializeNvPerf();
		m_nvperf.InitializeReportGenerator();
		m_nvperf.SetFrameLevelRangeName("Frame");
		m_nvperf.SetNumNestingLevels(2);
		m_nvperf.SetMaxNumRanges(2); // "Frame" + "Draw"
#ifdef WIN32
		m_nvperf.outputOptions.directoryName = "HtmlReports\\draw-base-instance";
#else
		m_nvperf.outputOptions.directoryName = "./HtmlReports/draw-base-instance";
#endif
		m_clockStatus = context::GetDeviceClockState();
		context::SetDeviceClockState(NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_RATED_TDP);
#endif
		bool Validated(true);

		if(Validated)
			Validated = initTest();
		if(Validated)
			Validated = initProgram();
		if(Validated)
			Validated = initBuffer();
		if(Validated)
			Validated = initVertexArray();

		return Validated;
	}

	bool end()
	{

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.Reset();
		context::SetDeviceClockState(m_clockStatus);
#endif
		bool Validated(true);

		glDeleteProgramPipelines(1, &PipelineName);
		glDeleteProgram(ProgramName);
		glDeleteBuffers(buffer::MAX, BufferName);
		glDeleteVertexArrays(1, &VertexArrayName);

		return Validated;
	}

	bool render()
	{

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.OnFrameStart();
#endif

		glm::vec2 WindowSize(this->getWindowSize());

		{
			glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
			glm::mat4* Pointer = (glm::mat4*)glMapBufferRange(
				GL_UNIFORM_BUFFER, 0,	sizeof(glm::mat4),
				GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

			glm::mat4 Projection = glm::perspective(glm::pi<float>() * 0.25f, WindowSize.x / WindowSize.y, 0.1f, 100.0f);
			glm::mat4 Model = glm::mat4(1.0f);
		
			*Pointer = Projection * this->view() * Model;

			// Make sure the uniform buffer is uploaded
			glUnmapBuffer(GL_UNIFORM_BUFFER);
		}

		glViewportIndexedf(0, 0, 0, WindowSize.x, WindowSize.y);

		float Depth(1.0f);
		glClearBufferfv(GL_DEPTH, 0, &Depth);
		glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)[0]);

		glBindProgramPipeline(PipelineName);
		glBindBufferBase(GL_UNIFORM_BUFFER, semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);
		glBindVertexArray(VertexArrayName);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.PushRange("Draw");
#endif
		// Draw indexed triangle
		glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, ElementCount, GL_UNSIGNED_INT, 0, 5, 1, 5);
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.PopRange();
#endif

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.OnFrameEnd();
		if (m_nvperf.IsCollectingReport())
		{
			glfwSetWindowTitle(getWindow(), "draw-base-instance - Nsight Perf: Currently profiling the frame. HTML Report will be written to: <CWD>/HtmlReports/draw-base-instance");
		}
		else if (m_nvperf.GetInitStatus() == nv::perf::profiler::ReportGeneratorInitStatus::Succeeded)
		{
			glfwSetWindowTitle(getWindow(), "draw-base-instance - Nsight Perf: Hit <space-bar> to begin profiling.");
		}
		else
		{
			glfwSetWindowTitle(getWindow(), "draw-base-instance - Nsight Perf: Initialization failed. Please check the logs.");
		}

#endif
		return true;
	}

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
	virtual void keyPressed(uint32_t key) override
	{
		switch (key)
		{
		case GLFW_KEY_SPACE:
			{
				m_nvperf.StartCollectionOnNextFrame();
				break;
			}
		}
	}
#endif
};

int main(int argc, char* argv[])
{
	int Error = 0;
#ifdef __linux__
	printf("Usage: %s [--egl]\n", argv[0]);
	printf("%s\n", "  --egl                  Use EGL for context creation. The default is GLX on Linux.");

	bool useEGL = false;
	for (int index = 1; index < argc; ++index)
	{
		if (!strcmp(argv[index], "--egl"))
		{
			useEGL = true;
			break;
		}
	}
	if (useEGL)
	{
		printf("%s", "Using EGL for context creation.\n");
		sample<ContextEGL> Sample(argc, argv);
		Error += Sample();
	}
	else
	{
		printf("%s", "Using GLX for context creation.\n");
		sample<ContextOpenGL> Sample(argc, argv);
		Error += Sample();
	}
#else
	sample<ContextOpenGL> Sample(argc, argv);
	Error += Sample();	
#endif

	return Error;
}


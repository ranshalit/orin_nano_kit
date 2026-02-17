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
	char const* VS_SOURCE("gl-430/program-compute.vert");
	char const* FS_SOURCE("gl-430/program-compute.frag");
	char const* CS_SOURCE("gl-430/program-compute.comp");
	char const* TEXTURE_DIFFUSE("kueken7_rgba8_srgb.dds");

	GLsizei const VertexCount(8);
	GLsizeiptr const VertexSize = VertexCount * sizeof(glf::vertex_v4fv4fv4f);
	glf::vertex_v4fv4fv4f const VertexData[VertexCount] =
	{
		glf::vertex_v4fv4fv4f(glm::vec4(-1.0f,-1.0f, 0.0f, 1.0f), glm::vec4(0.0f, 1.0f, glm::vec2(0.0f)), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4( 1.0f,-1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 1.0f, glm::vec2(0.0f)), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4( 1.0f, 1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 0.0f, glm::vec2(0.0f)), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f), glm::vec4(0.0f, 0.0f, glm::vec2(0.0f)), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4(-1.0f,-1.0f, 0.0f, 1.0f), glm::vec4(0.0f, 1.0f, glm::vec2(0.0f)), glm::vec4(1.0f, 0.5f, 0.5f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4( 1.0f,-1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 1.0f, glm::vec2(0.0f)), glm::vec4(1.0f, 1.0f, 0.5f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4( 1.0f, 1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 0.0f, glm::vec2(0.0f)), glm::vec4(0.5f, 1.0f, 0.0f, 1.0f)),
		glf::vertex_v4fv4fv4f(glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f), glm::vec4(0.0f, 0.0f, glm::vec2(0.0f)), glm::vec4(0.5f, 0.5f, 1.0f, 1.0f)),

	};

	GLsizei const ElementCount(6);
	GLsizeiptr const ElementSize = ElementCount * sizeof(GLushort);
	GLushort const ElementData[ElementCount] =
	{
		0, 1, 2, 
		2, 3, 0
	};

	namespace program
	{
		enum type
		{
			GRAPHICS,
			COMPUTE,
			MAX
		};
	}//namespace program

	namespace buffer
	{
		enum type
		{
			ELEMENT,
			INPUT,
			OUTPUT,
			TRANSFORM,
			MAX
		};
	}//namespace buffer

	namespace semantics
	{
		enum type
		{
			INPUT,
			OUTPUT
		};
	}//namespace semantics
}//namespace

template <typename context>
class sample : public framework
{
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
	typename context::ReportGeneratorType m_nvperf;
	NVPW_Device_ClockStatus m_clockStatus = NVPW_DEVICE_CLOCK_STATUS_UNKNOWN; // Used to restore clock state when exiting
#endif
public:
	sample(int argc, char* argv[]) :
		framework(argc, argv, "gl-430-program-compute", framework::CORE, 4, 2, 
		glm::uvec2(640, 480), glm::vec2(0, 0), glm::vec2(0, 4), 2,
		MATCH_TEMPLATE, HEURISTIC_ALL, context::contextAPI),
		TextureName(0),
		VertexArrayName(0)
	{}

private:
	std::array<GLuint, program::MAX> PipelineName;
	std::array<GLuint, program::MAX> ProgramName;
	std::array<GLuint, buffer::MAX> BufferName;
	GLuint TextureName;
	GLuint VertexArrayName;

	bool initProgram()
	{
		bool Validated(true);

		compiler Compiler;

		if(Validated)
		{
			GLuint VertShaderName = Compiler.create(GL_VERTEX_SHADER, getDataDirectory() + VS_SOURCE);
			GLuint FragShaderName = Compiler.create(GL_FRAGMENT_SHADER, getDataDirectory() + FS_SOURCE);
			GLuint ComputeShaderName = Compiler.create(GL_COMPUTE_SHADER, getDataDirectory() + CS_SOURCE);

			ProgramName[program::GRAPHICS] = glCreateProgram();
			glProgramParameteri(ProgramName[program::GRAPHICS], GL_PROGRAM_SEPARABLE, GL_TRUE);
			glAttachShader(ProgramName[program::GRAPHICS], VertShaderName);
			glAttachShader(ProgramName[program::GRAPHICS], FragShaderName);
			glLinkProgram(ProgramName[program::GRAPHICS]);

			ProgramName[program::COMPUTE] = glCreateProgram();
			glProgramParameteri(ProgramName[program::COMPUTE], GL_PROGRAM_SEPARABLE, GL_TRUE);
			glAttachShader(ProgramName[program::COMPUTE], ComputeShaderName);
			glLinkProgram(ProgramName[program::COMPUTE]);
		}

		if(Validated)
		{
			Validated = Validated && Compiler.check();
			Validated = Validated && Compiler.check_program(ProgramName[program::GRAPHICS]);
			Validated = Validated && Compiler.check_program(ProgramName[program::COMPUTE]);
		}

		if(Validated)
		{
			glGenProgramPipelines(program::MAX, &PipelineName[0]);
			glUseProgramStages(PipelineName[program::GRAPHICS], GL_VERTEX_SHADER_BIT | GL_FRAGMENT_SHADER_BIT, ProgramName[program::GRAPHICS]);
			glUseProgramStages(PipelineName[program::COMPUTE], GL_COMPUTE_SHADER_BIT, ProgramName[program::COMPUTE]);
		}
	/*
		if(Validated)
		{
			GLint ActiveUniforms(0);
			glGetProgramInterfaceiv(ProgramName[program::GRAPHICS], GL_PROGRAM_INPUT, GL_ACTIVE_RESOURCES, &ActiveUniforms);

			for(GLint Index = 0; Index < ActiveUniforms; ++Index)
			{
				GLenum Props = GL_LOCATION;
				GLint Binding = -1;
				GLsizei Length = 0;
				std::vector<char> Name;
				Name.resize(64, '\0');

				glGetProgramResourceName(ProgramName[program::GRAPHICS], GL_PROGRAM_INPUT, Index, 64, &Length, &Name[0]);
				glGetProgramResourceiv(ProgramName[program::GRAPHICS], GL_PROGRAM_INPUT, Index, 1, &Props, sizeof(GLint), &Length, &Binding);

				continue;
				//assert(Binding == 0);
			}
		}

		if(Validated)
		{
			GLint ActiveUniforms(0);
			glGetProgramInterfaceiv(ProgramName[program::GRAPHICS], GL_UNIFORM, GL_ACTIVE_RESOURCES, &ActiveUniforms);

			for(GLint Index = 0; Index < ActiveUniforms; ++Index)
			{
				GLenum Props[2] = {GL_BUFFER_BINDING, GL_LOCATION};

				struct results
				{
					GLint Binding;
					GLint Location;
				};

				results Results = {0, 0};

				GLsizei NameLength = 0;
				GLsizei Length = 0;
				std::vector<char> Name;
				Name.resize(64, '\0');

				glGetProgramResourceName(ProgramName[program::GRAPHICS], GL_UNIFORM, Index, 64, &NameLength, &Name[0]);
				glGetProgramResourceiv(ProgramName[program::GRAPHICS], GL_UNIFORM, Index, 2, Props, sizeof(Props), &Length, (GLint*)&Results);

				continue;
				//assert(Binding == 0);
			}
		}
	*/
		return Validated;
	}

	bool initBuffer()
	{
		bool Validated(true);

		glGenBuffers(buffer::MAX, &BufferName[0]);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ElementSize, ElementData, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::INPUT]);
		glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glBindBuffer(GL_ARRAY_BUFFER, BufferName[buffer::OUTPUT]);
		glBufferData(GL_ARRAY_BUFFER, VertexSize, VertexData, GL_STATIC_COPY);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		GLint UniformBufferOffset(0);
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &UniformBufferOffset);
		GLint UniformBlockSize = glm::max(GLint(sizeof(glm::mat4)), UniformBufferOffset);

		glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
		glBufferData(GL_UNIFORM_BUFFER, UniformBlockSize, NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		return Validated;
	}

	bool initTexture()
	{
		gli::texture2d Texture(gli::load_dds((getDataDirectory() + TEXTURE_DIFFUSE).c_str()));
		assert(!Texture.empty());
		gli::gl GL(gli::gl::PROFILE_GL33);
		gli::gl::format const& Format = GL.translate(Texture.format(), Texture.swizzles());

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glGenTextures(1, &TextureName);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, TextureName);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, Format.Swizzles[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, Format.Swizzles[1]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, Format.Swizzles[2]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, Format.Swizzles[3]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(Texture.levels() - 1));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexStorage2D(GL_TEXTURE_2D, GLint(Texture.levels()), Format.Internal, GLsizei(Texture.extent().x), GLsizei(Texture.extent().y));
		for(gli::texture2d::size_type Level = 0; Level < Texture.levels(); ++Level)
		{
			glTexSubImage2D(GL_TEXTURE_2D, GLint(Level),
				0, 0, 
				GLsizei(Texture[Level].extent().x), GLsizei(Texture[Level].extent().y),
				Format.External, Format.Type,
				Texture[Level].data());
		}
	
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		return true;
	}

	bool initVertexArray()
	{
		glGenVertexArrays(1, &VertexArrayName);
		glBindVertexArray(VertexArrayName);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferName[buffer::ELEMENT]);
		glBindVertexArray(0);

		return true;
	}

	bool begin()
	{
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		nv::perf::InitializeNvPerf();
		m_nvperf.InitializeReportGenerator();
		m_nvperf.SetFrameLevelRangeName("Frame");
		m_nvperf.SetNumNestingLevels(2);
		m_nvperf.SetMaxNumRanges(3); // "Frame" + "Dispatch" + "Draw"
#ifdef WIN32
		m_nvperf.outputOptions.directoryName = "HtmlReports\\program-compute";
#else
		m_nvperf.outputOptions.directoryName = "./HtmlReports/program-compute";
#endif

		m_clockStatus =  context::GetDeviceClockState();
		context::SetDeviceClockState(NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_RATED_TDP);
#endif
		bool Validated(true);
		Validated = Validated && this->checkExtension("GL_ARB_compute_shader");

		this->logImplementationDependentLimit(GL_MAX_COMPUTE_UNIFORM_BLOCKS, "GL_MAX_COMPUTE_UNIFORM_BLOCKS");
		this->logImplementationDependentLimit(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, "GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS");
		this->logImplementationDependentLimit(GL_MAX_COMPUTE_IMAGE_UNIFORMS, "GL_MAX_COMPUTE_IMAGE_UNIFORMS");
		this->logImplementationDependentLimit(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, "GL_MAX_COMPUTE_SHARED_MEMORY_SIZE");
		this->logImplementationDependentLimit(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, "GL_MAX_COMPUTE_UNIFORM_COMPONENTS");
		this->logImplementationDependentLimit(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS, "GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS");
		this->logImplementationDependentLimit(GL_MAX_COMPUTE_ATOMIC_COUNTERS, "GL_MAX_COMPUTE_ATOMIC_COUNTERS");
		this->logImplementationDependentLimit(GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS, "GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS");
		this->logImplementationDependentLimit(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, "GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS");

		//this->logImplementationDependentLimit(GL_MAX_COMPUTE_WORK_GROUP_COUNT, "GL_MAX_COMPUTE_WORK_GROUP_COUNT");
		//this->logImplementationDependentLimit(GL_MAX_COMPUTE_WORK_GROUP_SIZE, "GL_MAX_COMPUTE_WORK_GROUP_SIZE");

		if(Validated)
			Validated = initProgram();
		if(Validated)
			Validated = initBuffer();
		if(Validated)
			Validated = initVertexArray();
		if(Validated)
			Validated = initTexture();

		return Validated;
	}

	bool end()
	{
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.Reset();
		context::SetDeviceClockState(m_clockStatus);
#endif
		glDeleteProgramPipelines(program::MAX, &PipelineName[0]);
		glDeleteProgram(ProgramName[program::GRAPHICS]);
		glDeleteProgram(ProgramName[program::COMPUTE]);
		glDeleteBuffers(buffer::MAX, &BufferName[0]);
		glDeleteTextures(1, &TextureName);
		glDeleteVertexArrays(1, &VertexArrayName);

		return true;
	}

	bool render()
	{
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.OnFrameStart();
#endif
		glm::vec2 WindowSize(this->getWindowSize());

		{
			glBindBuffer(GL_UNIFORM_BUFFER, BufferName[buffer::TRANSFORM]);
			glm::mat4* Pointer = (glm::mat4*)glMapBufferRange(GL_UNIFORM_BUFFER, 0,	sizeof(glm::mat4),
				GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

			glm::mat4 Projection = glm::perspectiveFov(glm::pi<float>() * 0.25f, WindowSize.x, WindowSize.y, 0.1f, 100.0f);
			glm::mat4 Model = glm::mat4(1.0f);
			*Pointer = Projection * this->view() * Model;

			// Make sure the uniform buffer is uploaded
			glUnmapBuffer(GL_UNIFORM_BUFFER);
		}

		glBindProgramPipeline(PipelineName[program::COMPUTE]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, semantics::INPUT, BufferName[buffer::INPUT]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, semantics::OUTPUT, BufferName[buffer::OUTPUT]);
		glBindBufferBase(GL_UNIFORM_BUFFER, semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.PushRange("Dispatch");
#endif
		glDispatchCompute(GLuint(VertexCount), 1, 1);
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.PopRange();
#endif
		glViewportIndexedf(0, 0, 0, WindowSize.x, WindowSize.y);
		glClearBufferfv(GL_COLOR, 0, &glm::vec4(1.0f)[0]);

		glBindProgramPipeline(PipelineName[program::GRAPHICS]);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, TextureName);
		glBindVertexArray(VertexArrayName);
		glBindBufferBase(GL_UNIFORM_BUFFER, semantic::uniform::TRANSFORM0, BufferName[buffer::TRANSFORM]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, semantics::INPUT, BufferName[buffer::OUTPUT]);

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.PushRange("Draw");
#endif
		glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, ElementCount, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0), 1, 0, 0);
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.PopRange();
#endif

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
		m_nvperf.OnFrameEnd();
		if (m_nvperf.IsCollectingReport())
		{
			glfwSetWindowTitle(getWindow(), "program-compute - Nsight Perf: Currently profiling the frame. HTML Report will be written to: <CWD>/HtmlReports/program-compute");
		}
		else if (m_nvperf.GetInitStatus() == nv::perf::profiler::ReportGeneratorInitStatus::Succeeded)
		{
			glfwSetWindowTitle(getWindow(), "program-compute - Nsight Perf: Hit <space-bar> to begin profiling.");
		}
		else
		{
			glfwSetWindowTitle(getWindow(), "program-compute - Nsight Perf: Initialization failed. Please check the logs.");
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

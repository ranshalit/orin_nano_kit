function(glCreateSampleGTC NAME)
	set(NVPERF_FILES)
	set(NVPERF_UTILITY_FILES)
	if(NvPerf_FOUND)
	# Add NvPerf API
		foreach(NvPerf_INCLUDE_DIR IN LISTS NvPerf_INCLUDE_DIRS)
			file(GLOB FILES "${NvPerf_INCLUDE_DIR}/*.h")
			list(APPEND NVPERF_FILES ${FILES})
		endforeach()
		source_group("NvPerf" FILES ${NVPERF_FILES})

		# Add NvPerf Utility
		file(GLOB NVPERF_UTILITY_FILES "${NvPerfUtility_INCLUDE_DIRS}/*.h")
		source_group("NvPerfUtility" FILES ${NVPERF_UTILITY_FILES})
	endif()
	set(SAMPLE_NAME ${GL_PROFILE_GTC}-${GL_VERSION_GTC}-${NAME})
	set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../data/${GL_PROFILE_GTC}-${GL_VERSION_GTC})

	foreach(FILE ${GL_SHADER_GTC})
		set(SHADER_PATH ${SHADER_PATH} ${SHADER_DIR}/${FILE})
	endforeach(FILE)

	source_group("Shader Files" FILES ${SHADER_PATH})
	
	add_executable(${SAMPLE_NAME} ${GL_PROFILE_GTC}-${GL_VERSION_GTC}-${NAME}.cpp ${SHADER_PATH} ${NVPERF_FILES} ${NVPERF_UTILITY_FILES})
	add_test(NAME ${SAMPLE_NAME} COMMAND $<TARGET_FILE:${SAMPLE_NAME}>)

	target_link_libraries(${SAMPLE_NAME} ${FRAMEWORK_NAME} ${BINARY_FILES})
	add_dependencies(${SAMPLE_NAME} glfw ${FRAMEWORK_NAME} ${COPY_BINARY})

	add_custom_command(TARGET ${SAMPLE_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
		$<TARGET_FILE_DIR:${SAMPLE_NAME}>/data/${GL_PROFILE_GTC}-${GL_VERSION_GTC}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${SHADER_PATH} $<TARGET_FILE_DIR:${SAMPLE_NAME}>/data/${GL_PROFILE_GTC}-${GL_VERSION_GTC}
	)
	if(NvPerf_FOUND)
		target_compile_definitions(${SAMPLE_NAME} PRIVATE NV_PERF_ENABLE_INSTRUMENTATION)
		target_link_libraries(${SAMPLE_NAME} NvPerf NvPerfUtility)
		DeployNvPerf(${SAMPLE_NAME} NvPerf)
	endif()
	install(TARGETS ${SAMPLE_NAME} DESTINATION .)
endfunction(glCreateSampleGTC)

set(GL_PROFILE_GTC gl)
# OpenGL 4.2 samples
set(GL_VERSION_GTC 420)

set(GL_SHADER_GTC draw-base-instance.vert draw-base-instance.frag)
glCreateSampleGTC(draw-base-instance)
# OpenGL 4.3 samples
set(GL_VERSION_GTC 430)
set(GL_SHADER_GTC program-compute.vert program-compute.frag program-compute.comp)
glCreateSampleGTC(program-compute)

add_custom_command(TARGET gl-430-program-compute POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory
	$<TARGET_FILE_DIR:gl-430-program-compute>/data
COMMAND ${CMAKE_COMMAND} -E copy_if_different
	${CMAKE_CURRENT_SOURCE_DIR}/../data/kueken7_rgba8_srgb.dds $<TARGET_FILE_DIR:gl-430-program-compute>/data
)
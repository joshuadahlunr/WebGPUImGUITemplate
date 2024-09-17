cmake_minimum_required(VERSION 3.21)
project(WebGPUImGUITemplate CXX C)

set(CMAKE_CXX_STANDARD 20)

if(EMSCRIPTEN)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_GLFW=3")
endif()

# Note: Dawn and Emscripten both provide GLFW
add_subdirectory(thirdparty/WebGPU-distribution)
add_subdirectory(thirdparty/glfw3webgpu)

set(IMGUI_DIR thirdparty/imgui)
add_library(imgui
	${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
	${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp
	# Dear ImGui files
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC webgpu glfw)
if(NOT EMSCRIPTEN)
	target_compile_definitions(imgui PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
else()
	target_link_options(imgui PUBLIC -sUSE_GLFW=3 -sUSE_WEBGPU -sASYNCIFY)
endif()

add_executable(example example.cpp)
target_link_libraries(example PUBLIC webgpu glfw3webgpu imgui)
if(EMSCRIPTEN)
	set_target_properties(example PROPERTIES SUFFIX ".html")
	target_link_options(example PUBLIC -sUSE_GLFW=3 -sUSE_WEBGPU -sASYNCIFY)
endif()
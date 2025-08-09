cmake_minimum_required (VERSION 3.24)

project("vendor")

# Includes all 3rd party libraries here

# Minhook library
message("Adding minhook...")
add_subdirectory ("minhook")

# GLFW
message("Adding GLFW...")
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(glfw)

# Dear ImGui
message("Adding ImGui...")
add_library(imgui STATIC
	${CMAKE_SOURCE_DIR}/vendor/imgui/imgui.cpp
	${CMAKE_SOURCE_DIR}/vendor/imgui/imgui_demo.cpp
	${CMAKE_SOURCE_DIR}/vendor/imgui/imgui_draw.cpp
	${CMAKE_SOURCE_DIR}/vendor/imgui/imgui_tables.cpp
	${CMAKE_SOURCE_DIR}/vendor/imgui/imgui_widgets.cpp)
target_include_directories(imgui PUBLIC ${CMAKE_SOURCE_DIR}/vendor/imgui)
target_compile_definitions(imgui PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

# Assign solution dirs
set_property(TARGET minhook PROPERTY FOLDER "lib") 

set_property(TARGET glfw PROPERTY FOLDER "vendor/glfw")
set_property(TARGET update_mappings PROPERTY FOLDER "vendor/glfw")

set_property(TARGET imgui PROPERTY FOLDER "vendor")
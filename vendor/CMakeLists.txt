cmake_minimum_required (VERSION 3.24)

project("vendor")

# Includes all 3rd party libraries here

# Minhook library
if (WIN32)
	message("Adding minhook...")
	add_subdirectory ("minhook")
endif()

# glad
message("Adding glad...")
add_library(glad STATIC
	${CMAKE_SOURCE_DIR}/vendor/glad/src/glad.c
	${CMAKE_SOURCE_DIR}/vendor/glad/include/glad/glad.h
	${CMAKE_SOURCE_DIR}/vendor/glad/include/KHR/khrplatform.h)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/vendor/glad/include)

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

# ImPlot
message("Adding ImPlot...")
add_library(implot STATIC
	${CMAKE_SOURCE_DIR}/vendor/implot/implot.cpp
	${CMAKE_SOURCE_DIR}/vendor/implot/implot_demo.cpp
	${CMAKE_SOURCE_DIR}/vendor/implot/implot_items.cpp)
target_include_directories(implot
	PUBLIC ${CMAKE_SOURCE_DIR}/vendor/imgui
	PUBLIC ${CMAKE_SOURCE_DIR}/vendor/implot)
target_compile_definitions(implot PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(implot PRIVATE imgui)

# fmt
message("Adding fmt...")
set(BUILD_SHARED_LIBS FALSE)
add_subdirectory(fmt)

# quill
message("Adding quill...")
set(QUILL_NO_EXCEPTIONS ON CACHE BOOL "Enable this option to build without exception handling support." FORCE)
set(QUILL_X86ARCH OFF CACHE BOOL "Enable x86-specific optimizations for cache coherence using _mm_prefetch, _mm_clflush, and _mm_clflushopt instructions. Ensure the target architecture must also be specified with -march=\"...\" when enabling this option." FORCE)
set(QUILL_DISABLE_NON_PREFIXED_MACROS ON CACHE BOOL "Enable this option to disable non-prefixed `LOG_*` macros, keeping only the `QUILL_LOG_*` macros to avoid conflicts with other logging libraries." FORCE)
add_subdirectory(quill)

# glaze
message ("Adding glaze...")
add_subdirectory(glaze)

# vulkan headers
message ("Adding Vulkan-Headers...")
add_subdirectory(Vulkan-Headers)

# volk (Vulkan loader)
message ("Adding volk...")
add_subdirectory(volk)

# simpleble (for base station management)

# base stations have a quirk where the hardware *LIES* about what its capable of. SimpleBLE naiively trusts
# them and doesn't let you actually talk to the base stations to turn them off!
# we need to patch SimpleBLE to remove the characteristic check before doing GATT operations to actually
# be able to do power management. this is a library design fault and hopefully will be addressed upstream.
message ("Adding SimpleBLE...")
set(SIMPLEBLE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/simpleble")
set(SIMPLEBLE_PATCHED_DIR "${CMAKE_CURRENT_BINARY_DIR}/patched_submodules/simpleble")
set(SIMPLEBLE_PATCH_FILE "${CMAKE_SOURCE_DIR}/vendor/simpleble-base-station-fix.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SIMPLEBLE_PATCH_FILE}")
file(COPY "${SIMPLEBLE_SRC_DIR}/" DESTINATION "${SIMPLEBLE_PATCHED_DIR}")
file(REMOVE "${SIMPLEBLE_PATCHED_DIR}/.git")
file(RELATIVE_PATH SIMPLEBLE_REL_DIR "${CMAKE_SOURCE_DIR}" "${SIMPLEBLE_PATCHED_DIR}")
execute_process(
    COMMAND git apply -p1 --directory=${SIMPLEBLE_REL_DIR} --verbose --no-index --ignore-space-change --ignore-whitespace --whitespace=fix "${SIMPLEBLE_PATCH_FILE}"
    WORKING_DIRECTORY "${SIMPLEBLE_PATCHED_DIR}"
    RESULT_VARIABLE PATCH_APPLY_RESULT
    OUTPUT_VARIABLE PATCH_APPLY_OUT
    ERROR_VARIABLE PATCH_APPLY_ERR
)
if(NOT PATCH_APPLY_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to apply SimpleBLE patch:\n${PATCH_APPLY_OUT}\n${PATCH_APPLY_ERR}")
endif()
add_subdirectory("${SIMPLEBLE_PATCHED_DIR}/simplecble" "${CMAKE_CURRENT_BINARY_DIR}/simplecble_build")
target_compile_definitions(simpleble PRIVATE _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
target_compile_definitions(simplecble PRIVATE _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
# make simpleble use static fmt
get_target_property(SIMPLEBLE_DEFS simpleble COMPILE_DEFINITIONS)
if(SIMPLEBLE_DEFS)
    list(REMOVE_ITEM SIMPLEBLE_DEFS "FMT_HEADER_ONLY")
    set_target_properties(simpleble PROPERTIES COMPILE_DEFINITIONS "${SIMPLEBLE_DEFS}")
endif()
target_link_libraries(simpleble PRIVATE fmt)

# Assign solution dirs
if (WIN32)
	set_property(TARGET glfw PROPERTY FOLDER "vendor/glfw")
	set_property(TARGET update_mappings PROPERTY FOLDER "vendor/glfw")

	set_property(TARGET simpleble PROPERTY FOLDER "vendor/simpleble")
	set_property(TARGET simplecble PROPERTY FOLDER "vendor/simpleble")

	set_property(TARGET minhook PROPERTY FOLDER "vendor") 
	set_property(TARGET imgui PROPERTY FOLDER "vendor")
	set_property(TARGET implot PROPERTY FOLDER "vendor")
	set_property(TARGET fmt PROPERTY FOLDER "vendor")
	set_property(TARGET quill PROPERTY FOLDER "vendor")
	set_property(TARGET glad PROPERTY FOLDER "vendor")
	set_property(TARGET volk PROPERTY FOLDER "vendor")
endif()
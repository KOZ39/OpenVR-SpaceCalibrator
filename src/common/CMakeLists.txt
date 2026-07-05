cmake_minimum_required (VERSION 3.24)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(SpaceCalibratorCommon)
message("SpaceCalibrator - Common")

function(GroupSourcesByFolder target)
    set(SOURCE_GROUP_DELIMITER "/")
    set(last_dir "")
    set(files "")

    get_target_property(sources ${target} SOURCES)

    foreach(file ${sources})
        file(RELATIVE_PATH relative_file "${PROJECT_SOURCE_DIR}" ${file})
        get_filename_component(dir "${relative_file}" PATH)
        if(NOT "${dir}" STREQUAL "${last_dir}")
            if(files)
                source_group("${last_dir}" FILES ${files})
            endif()
            set(files "")
        endif()
        set(files ${files} ${file})
        set(last_dir "${dir}")
    endforeach()

    if(files)
        source_group("${last_dir}" FILES ${files})
    endif()
endfunction()

# If not set, determine the platform architecture
if (NOT PLATFORM)
    if (CMAKE_SIZEOF_VOID_P MATCHES 8)
        set(PLATFORM 64)
    else ()
        set(PLATFORM 32)
    endif ()
endif ()
message(STATUS "Compilation set for ${PLATFORM}bits architectures.")

# Add source files
file(GLOB_RECURSE SOURCES_API     ${CMAKE_SOURCE_DIR}/src/common "*.c" "*.h" "*.hpp" "*.cpp")

foreach(SOURCE IN ITEMS ${SOURCES_API})
    get_filename_component(SOURCE_PATH "${SOURCE}" PATH)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP_PATH "${SOURCE_PATH}")
    string(REPLACE "/" "\\" GROUP_PATH "${GROUP_PATH}")
    source_group("${GROUP_PATH}" FILES "${SOURCE}")
endforeach()

add_definitions(-D_UNICODE)
add_library(SpaceCalibratorCommon STATIC ${SOURCES_API})
GroupSourcesByFolder(SpaceCalibratorCommon)

target_include_directories(SpaceCalibratorCommon
    PRIVATE ${CMAKE_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/src/common
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor/openvr/headers
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor/quill/include
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor/fmt/include
)

target_link_libraries(SpaceCalibratorCommon
    PRIVATE fmt
    PRIVATE quill
    PRIVATE Vulkan-Headers
)

target_compile_definitions(SpaceCalibratorCommon
    PRIVATE
        $<$<CONFIG:Debug>:QUILL_COMPILE_ACTIVE_LOG_LEVEL=-1>
        $<$<NOT:$<CONFIG:Debug>>:QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_INFO>
)

if(NOT MSVC)
    target_compile_options(SpaceCalibratorCommon
        PRIVATE -fno-exceptions)
endif()

# Compile definitions
target_compile_definitions(SpaceCalibratorCommon
    PRIVATE
    # If the debug configuration pass the DEBUG define to the compiler
    $<$<CONFIG:Debug>:ENABLE_ASSERT>
    PRIVATE NOMINMAX
    PRIVATE _CRT_SECURE_NO_WARNINGS
    # PRIVATE FMTQUILL_THROW="((void)0)"
    PRIVATE FMTQUILL_EXCEPTIONS=0
)

# optimisations
if (MSVC)
    target_compile_options(SpaceCalibratorCommon
        PRIVATE
            /EHsc
            $<$<CONFIG:RELEASE>:/Ob2 /Zc:inline>
            $<$<CONFIG:RELWITHDEBINFO>:/Ob2 /Zc:inline>
            $<$<CONFIG:MINSIZEREL>:/Ob2 /Zc:inline>
    )

    target_link_options(SpaceCalibratorCommon
        PRIVATE
            $<$<CONFIG:RELEASE>:/LTCG /OPT:REF /OPT:ICF>
            $<$<CONFIG:RELWITHDEBINFO>:/LTCG /OPT:REF /OPT:ICF>
            $<$<CONFIG:MINSIZEREL>:/LTCG /OPT:REF /OPT:ICF>
    )
endif()
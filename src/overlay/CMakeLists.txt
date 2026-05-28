cmake_minimum_required (VERSION 3.8)

project(SpaceCalibratorOverlay CXX)
message("SpaceCalibrator - Overlay")

if (WIN32)
    foreach(config_type RELEASE RELWITHDEBINFO MINSIZEREL)
        set(CMAKE_CXX_FLAGS_${config_type} "${CMAKE_CXX_FLAGS_${config_type}} /Ob2 /Zc:inline")
        set(CMAKE_SHARED_LINKER_FLAGS_${config_type} "${CMAKE_SHARED_LINKER_FLAGS_${config_type}} /LTCG /OPT:REF /OPT:ICF")
    endforeach()
endif()

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

set(OPENVR_LIB_DIR ${CMAKE_SOURCE_DIR}/vendor/openvr)

# If not set, determine the platform architecture
if (NOT PLATFORM)
    if (CMAKE_SIZEOF_VOID_P MATCHES 8)
        set(PLATFORM 64)
    else ()
        set(PLATFORM 32)
    endif ()
endif ()
message(STATUS "Compilation set for ${PLATFORM}bits architectures.")

# OpenVR compatibility checking
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_definitions(-DLINUX -DPOSIX)
    
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(ARCH_TARGET linux_arm64)
        add_definitions(-DLINUXARM64)
    else()
        set(ARCH_TARGET linux64)
        add_definitions(-DLINUX64)
    endif()

    if (${PLATFORM} MATCHES 32)
        message(WARNING "OpenVR x86 binaries not provided on GNU/Linux.")
    endif ()
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_MACOSX_RPATH 0)
    add_definitions(-DOSX -DPOSIX)
    set(ARCH_TARGET osx32)

elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_definitions(-D_WIN32)
    set(ARCH_TARGET win${PLATFORM})

    # Binaries path for thirdparties are not generics so we try to guess their suffixes.
    set(WINDOWS_PATH_SUFFIXES win${PLATFORM} Win${PLATFORM} x${PLATFORM})
endif ()

find_library(OPENVR_LIBRARIES
        NAMES
        openvr_api
        PATHS
        ${OPENVR_LIB_DIR}/bin
        ${OPENVR_LIB_DIR}/lib
        PATH_SUFFIXES
        osx${PLATFORM}
        linux${PLATFORM}
        win${PLATFORM}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
        )

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    enable_language("RC")
    set (WIN32_RESOURCES ${CMAKE_SOURCE_DIR}/src/overlay/SpaceCalibrator.rc)
endif()

# Add source files
file(GLOB_RECURSE SOURCES_API ${CMAKE_SOURCE_DIR}/src/overlay "*.c" "*.h" "*.hpp" "*.cpp")
file(GLOB_RECURSE SOURCES_HEADERS ${CMAKE_SOURCE_DIR}/src/common "*.h" "*.hpp")

foreach(SOURCE IN ITEMS ${SOURCES_API})
    get_filename_component(SOURCE_PATH "${SOURCE}" PATH)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP_PATH "${SOURCE_PATH}")
    string(REPLACE "/" "\\" GROUP_PATH "${GROUP_PATH}")
    source_group("${GROUP_PATH}" FILES "${SOURCE}")
endforeach()

set(IMGUI_FILES
    ${CMAKE_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_glfw.cpp)

add_executable(SpaceCalibratorOverlay ${SOURCES_API} ${SOURCES_HEADERS} ${IMGUI_FILES} ${WIN32_RESOURCES})
GroupSourcesByFolder(SpaceCalibratorOverlay)
# Disable console window
if (WIN32)
    set_target_properties(SpaceCalibratorOverlay PROPERTIES
        LINK_FLAGS_DEBUG            "/SUBSYSTEM:CONSOLE /ENTRY:wWinMainCRTStartup /NODEFAULTLIB:library /MANIFEST:NO"
        LINK_FLAGS_MINSIZEREL       "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup /NODEFAULTLIB:library /MANIFEST:NO"
        LINK_FLAGS_RELEASE          "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup /NODEFAULTLIB:library /MANIFEST:NO"
        LINK_FLAGS_RELWITHDEBINFO   "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup /NODEFAULTLIB:library /MANIFEST:NO")
endif()

target_include_directories(SpaceCalibratorOverlay
    PUBLIC ${CMAKE_SOURCE_DIR}
    PUBLIC ${CMAKE_SOURCE_DIR}/src/common
    PUBLIC ${CMAKE_SOURCE_DIR}/src/overlay
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor/openvr/headers
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor/imgui/backends
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor/quill/include
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor/fmt/include
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor/glaze/include
    PUBLIC ${CMAKE_SOURCE_DIR}/vendor/eigen
)

target_link_libraries(SpaceCalibratorOverlay
    PRIVATE SpaceCalibratorCommon
    PRIVATE imgui
    PRIVATE glad
    PRIVATE glfw
    PRIVATE fmt
    PRIVATE quill
    PRIVATE ${OPENVR_LIBRARIES})

target_compile_definitions(SpaceCalibratorOverlay
    PRIVATE IMGUI_DEFINE_MATH_OPERATORS
    PRIVATE IS_OVERLAY
)

if (WIN32)
    target_link_libraries(SpaceCalibratorOverlay
        PRIVATE opengl32.lib
        PRIVATE dwmapi.lib)

    target_compile_definitions(SpaceCalibratorOverlay
        PRIVATE NOMINMAX
        PRIVATE UNICODE
        PRIVATE _CRT_SECURE_NO_WARNINGS
    )
endif()

# Object files are too big with spacecal sooooo
if (MSVC)
    target_compile_options(SpaceCalibratorOverlay PRIVATE /bigobj)
endif ()

set(OVERLAY_BIN_DIR "$<TARGET_FILE_DIR:SpaceCalibratorOverlay>")

# Copy vrmanifest
add_custom_command(
    TARGET SpaceCalibratorOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/src/overlay/manifest.vrmanifest
    ${OVERLAY_BIN_DIR}/manifest.vrmanifest
)

# Copy dashboard icon
add_custom_command(
    TARGET SpaceCalibratorOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/src/overlay/icon.png
    ${OVERLAY_BIN_DIR}/icon.png
)

# Copy taskbar icon
add_custom_command(
    TARGET SpaceCalibratorOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/src/overlay/taskbar_icon.png
    ${OVERLAY_BIN_DIR}/taskbar_icon.png
)

# Copy LICENSE
add_custom_command(
    TARGET SpaceCalibratorOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/LICENSE
    ${OVERLAY_BIN_DIR}/LICENSE
)

# Copy NOTICE
add_custom_command(
    TARGET SpaceCalibratorOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/NOTICE
    ${OVERLAY_BIN_DIR}/README
)

# Copy openvr_api.dll
if (WIN32)
    add_custom_command(
        TARGET SpaceCalibratorOverlay POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/vendor/openvr/bin/${ARCH_TARGET}/openvr_api.dll
        ${OVERLAY_BIN_DIR}/openvr_api.dll
    )
else()
    add_custom_command(
        TARGET SpaceCalibratorOverlay POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/vendor/openvr/bin/${ARCH_TARGET}/libopenvr_api.so
        ${OVERLAY_BIN_DIR}/libopenvr_api.so
    )
endif()

# Copy assets to output folder
add_custom_command(
    TARGET SpaceCalibratorOverlay
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    ${OVERLAY_BIN_DIR}/assets
    COMMENT "Copying assets"
)

# Set as startup project
if (WIN32)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SpaceCalibratorOverlay)
    set_property(TARGET SpaceCalibratorOverlay PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${OVERLAY_BIN_DIR}")
endif()
set_target_properties(SpaceCalibratorOverlay PROPERTIES OUTPUT_NAME "SpaceCalibrator")
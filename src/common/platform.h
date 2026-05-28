#pragma once

#include <string>
#include <filesystem>

#if defined(_WIN32)
#define OS_WINDOWS 1
#define OS_LINUX 0
#elif (defined(__gnu_linux__) || defined(__linux__))
#define OS_WINDOWS 0
#define OS_LINUX 1
#else
#error "Unsupported OS"
#endif

#if defined(__clang__)
#define COMPILER_MSVC 0
#define COMPILER_GCC 0
#define COMPILER_CLANG 1
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#define COMPILER_GCC 0
#define COMPILER_CLANG 0
#elif defined(__GNUC__)
#define COMPILER_MSVC 0
#define COMPILER_GCC 1
#define COMPILER_CLANG 0
#else
#error "Unknown compiler"
#endif

// warning guards for various compilers
#if defined(__clang__)
#define BEGIN_EXTERNAL_HEADERS \
    __pragma(clang diagnostic push) \
    __pragma(clang diagnostic ignored "-Weverything") \
    __pragma(warning(push, 0))
#define END_EXTERNAL_HEADERS \
    __pragma(warning(pop)) \
    __pragma(clang diagnostic pop)
#elif defined(_MSC_VER)
#define BEGIN_EXTERNAL_HEADERS \
    __pragma(warning(push, 0)) \
    __pragma(warning(disable : 4668)) // #if FOO warns if FOO is not defined; some libs are written like that :(
#define END_EXTERNAL_HEADERS \
    __pragma(warning(pop))
#elif defined(__GNUC__)
#define BEGIN_EXTERNAL_HEADERS \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wall\"") \
    _Pragma("GCC diagnostic ignored \"-Wextra\"")
#define END_EXTERNAL_HEADERS \
    _Pragma("GCC diagnostic pop")
#else
#define BEGIN_EXTERNAL_HEADERS
#define END_EXTERNAL_HEADERS
#endif

namespace platform {
    // %APPDATA% or ~/.config
    std::filesystem::path getUserConfigDir();
    std::filesystem::path getExeDir();

    std::string getEnvVariable(const std::string& szEnvVarName);

    bool isAnotherInstanceRunning(bool& bIsRunningViaSteam);
    void shutdownCurrentInstance();

    // title -> window title in the window decoration
    // message -> text in the dialog box
    void showMessageDialog(const std::string& title, const std::string& message);

    void setThreadName(const std::string& threadName);

    // utf8 stuff for imgui -> overlay -> os interop
    
}
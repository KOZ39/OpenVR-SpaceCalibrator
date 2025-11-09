#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/Logger.h"
#include "quill/LogMacros.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"

#include <openvr_driver.h>
#include "log.h"
#include "util.h"
#include <fmt/chrono.h>

#include <filesystem>
#include <cassert>

namespace fs = ::std::filesystem;

namespace logging {
    ::quill::Logger* s_logger = nullptr;
    ::quill::Logger* s_loggerOpenVr = nullptr;
    ::quill::Logger* s_loggerIpc = nullptr;
    ::quill::Logger* s_loggerHooking = nullptr;

    class UiHaltOnErrorSink final : public quill::Sink {
    public:
        UiHaltOnErrorSink() = default;

        void write_log(quill::MacroMetadata const* /** log_metadata **/, uint64_t /** log_timestamp **/,
            std::string_view /** thread_id **/, std::string_view /** thread_name **/,
            std::string const& /** process_id **/, std::string_view /** logger_name **/,
            quill::LogLevel log_level, std::string_view /** log_level_description **/,
            std::string_view /** log_level_short_code **/,
            std::vector<std::pair<std::string, std::string>> const* /** named_args - only populated when named args in the format placeholder are used **/,
            std::string_view /** log_message **/, std::string_view log_statement) override
        {
            // Propagate error logs to the UI
            if (log_level < quill::LogLevel::Backtrace && log_level >= quill::LogLevel::Error) {
                // @TODO: propagate to ui
            }
        }

        void flush_sink() noexcept override { }
        void run_periodic_tasks() noexcept override { }
    };

    class DriverLogSink final : public quill::Sink {
    public:
        DriverLogSink() = default;

        void write_log(quill::MacroMetadata const* /** log_metadata **/, uint64_t /** log_timestamp **/,
            std::string_view /** thread_id **/, std::string_view /** thread_name **/,
            std::string const& /** process_id **/, std::string_view logger_name,
            quill::LogLevel log_level, std::string_view /** log_level_description **/,
            std::string_view /** log_level_short_code **/,
            std::vector<std::pair<std::string, std::string>> const* /** named_args - only populated when named args in the format placeholder are used **/,
            std::string_view log_message, std::string_view log_statement) override
        {
            // Propagate error logs to the steamVR
            std::string szLogLevel;
            switch (log_level) {
            case quill::LogLevel::TraceL3:
                szLogLevel = "TraceL3";
            case quill::LogLevel::TraceL2:
                szLogLevel = "TraceL2";
            case quill::LogLevel::TraceL1:
                szLogLevel = "TraceL1";
            case quill::LogLevel::Debug:
                szLogLevel = "Debug";
            case quill::LogLevel::Info:
                szLogLevel = "Info";
            case quill::LogLevel::Notice:
                szLogLevel = "Notice";
            case quill::LogLevel::Warning:
                szLogLevel = "Warning";
            case quill::LogLevel::Error:
                szLogLevel = "Error";
            case quill::LogLevel::Critical:
                szLogLevel = "Critical";
            case quill::LogLevel::Backtrace:
                szLogLevel = "Backtrace";
            case quill::LogLevel::None:
                szLogLevel = "None";
            default:
                szLogLevel = fmt::format("LogLevel_{}", (uint32_t)log_level);
            }

            std::string vrLogMsg = fmt::format("[{} : {}] {}", logger_name, szLogLevel, log_message);
            vr::VRDriverLog()->Log(vrLogMsg.c_str());
        }

        void flush_sink() noexcept override { }
        void run_periodic_tasks() noexcept override { }
    };

    bool IsFileOlderThan30Days(int year, int month, int day, int hour, int minute, int second)
    {
        std::tm time = std::tm{ second, minute, hour, day, month - 1, year - 1900 };
        auto fileTime = std::chrono::system_clock::from_time_t(std::mktime(&time));
        auto currentTime = std::chrono::system_clock::now();
        auto fileAge = currentTime - fileTime;
        return fileAge > std::chrono::days(CLEAR_LOG_FILES_AFTER_DAYS);
    }

    bool ParseLogFilename(const std::string& filename, int& year, int& month, int& day, int& hour, int& minute, int& second)
    {
        if (filename.size() != 27) {
            return false;
        }

        std::stringstream ss(filename.substr(4, 23));
        ss >> year;
        ss.ignore(1);
        ss >> month;
        ss.ignore(1);
        ss >> day;
        ss.ignore(1);
        ss >> hour;
        ss.ignore(1);
        ss >> minute;
        ss.ignore(1);
        ss >> second;

        return !ss.fail();
    }

    void DeleteOldLogFiles(const std::filesystem::path& directoryPath)
    {
        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            if (entry.is_regular_file() && entry.path().filename() != "log_latest.log") {
                std::string fileName = entry.path().filename().string();
                int year, month, day, hour, minute, second;

                if (fileName.substr(0, 4) == "log_" && ParseLogFilename(fileName, year, month, day, hour, minute, second) && IsFileOlderThan30Days(year, month, day, hour, minute, second)) {
                    std::filesystem::remove(entry.path());
                }
            }
        }
    }

// Helper for declaring a logger
#ifndef _DEBUG
#define MAKE_LOGGER_OVERLAY(title)                                              \
    quill::Frontend::create_or_get_logger(title,                                \
    {                                                                           \
        quill::Frontend::create_or_get_sink<quill::FileSink>(logFilePath),      \
        quill::Frontend::create_or_get_sink<quill::FileSink>(logFilePath2),     \
        quill::Frontend::create_or_get_sink<UiHaltOnErrorSink>("halt_on_error") \
    });
#define MAKE_LOGGER_DRIVER(title)                                               \
    quill::Frontend::create_or_get_logger(title,                                \
    {                                                                           \
        quill::Frontend::create_or_get_sink<quill::FileSink>(logFilePath),      \
        quill::Frontend::create_or_get_sink<quill::FileSink>(logFilePath2),     \
        quill::Frontend::create_or_get_sink<DriverLogSink>("driverlog")         \
    });
#else
#define MAKE_LOGGER_OVERLAY(title)                                              \
    quill::Frontend::create_or_get_logger(title,                                \
    {                                                                           \
        quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console"),     \
        quill::Frontend::create_or_get_sink<quill::FileSink>(logFilePath),      \
        quill::Frontend::create_or_get_sink<quill::FileSink>(logFilePath2),     \
        quill::Frontend::create_or_get_sink<UiHaltOnErrorSink>("halt_on_error") \
    });
#define MAKE_LOGGER_DRIVER(title)                                               \
    quill::Frontend::create_or_get_logger(title,                                \
    {                                                                           \
        quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console"),     \
        quill::Frontend::create_or_get_sink<quill::FileSink>(logFilePath),      \
        quill::Frontend::create_or_get_sink<quill::FileSink>(logFilePath2),     \
        quill::Frontend::create_or_get_sink<DriverLogSink>("driverlog")         \
    });
#endif

    void Init(bool isOverlay)
    {
        assert(s_logger == nullptr);
        assert(s_loggerOpenVr == nullptr);

        quill::Backend::start();

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time);

        const char* logType = isOverlay ? "overlay" : "driver";

        std::string logFilePath = (util::getSpaceCalibratorLogsDir() / fmt::format("log_{}_{:%Y_%m_%d_%H_%M_%S}.log", logType, tm)).string();
        std::string logFilePath2 = (util::getSpaceCalibratorLogsDir() /  fmt::format("log_{}_latest.log", logType)).string();
        DeleteOldLogFiles(util::getSpaceCalibratorLogsDir());

        if (isOverlay) {
            s_logger = MAKE_LOGGER_OVERLAY("overlay");
            s_loggerOpenVr = MAKE_LOGGER_OVERLAY("openvr");
            s_loggerIpc = MAKE_LOGGER_OVERLAY("ipc");
            s_loggerHooking = MAKE_LOGGER_OVERLAY("hooking");
        } else {
            s_logger = MAKE_LOGGER_DRIVER("overlay");
            s_loggerOpenVr = MAKE_LOGGER_DRIVER("openvr");
            s_loggerIpc = MAKE_LOGGER_DRIVER("ipc");
            s_loggerHooking = MAKE_LOGGER_DRIVER("hooking");
        }
    }
}
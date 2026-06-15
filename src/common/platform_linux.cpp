#include "platform.h"

#if OS_LINUX
#include "log.h"
#include "constants.h"

#include <stdlib.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

namespace platform {
    constexpr int INVALID_FD = -1;

    std::filesystem::path getUserConfigDir() {
        // based on XDG base dir spec for XDG_DATA_HOME
        // https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
        
        const char* szXdgConfigHome = getenv("XDG_DATA_HOME");

        if (szXdgConfigHome != nullptr && szXdgConfigHome[0] != '\0') {
            return std::filesystem::path(szXdgConfigHome);
        }

        szXdgConfigHome = getenv("HOME");
        if (szXdgConfigHome != nullptr && szXdgConfigHome[0] != '\0') {
            return std::filesystem::path(szXdgConfigHome) / ".local" / "share";
        }

        struct passwd* user_db_entry = getpwuid(getuid());
        if (user_db_entry) {
            szXdgConfigHome = user_db_entry->pw_dir;
        }
        if (szXdgConfigHome != nullptr && szXdgConfigHome[0] != '\0') {
            return std::filesystem::path(szXdgConfigHome) / ".local" / "share";
        }

        return std::filesystem::path();
    }

    std::filesystem::path getRuntimeDir() {
        // based on XDG base dir spec for XDG_RUNTIME_DIR
        // https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html

        const char* szXdgRuntimeDir = getenv("XDG_RUNTIME_DIR");
        if (szXdgRuntimeDir != nullptr && szXdgRuntimeDir[0] != '\0') {
            return std::filesystem::path(szXdgRuntimeDir);
        }

        LOG_WARN("XDG_RUNTIME_DIR is not set, falling back to /tmp/spacecalibrator-runtime-{}", getuid());

        std::filesystem::path fallback = std::filesystem::path("/tmp") / ("spacecalibrator-runtime-" + std::to_string(getuid()));

        std::error_code ec;
        std::filesystem::create_directories(fallback, ec);
        if (ec) {
            LOG_ERROR("Failed to create runtime dir fallback {}: {}", fallback.string(), ec.message());
            return std::filesystem::path();
        }

        errno = 0;
        if (chmod(fallback.c_str(), 0700) != 0) {
            LOG_ERROR("Failed to set permissions on runtime dir fallback {}: {}", fallback.string(), strerror(errno));
        }

        return fallback;
    }

    std::filesystem::path getExeDir() {
        std::error_code ec;
        std::filesystem::path exePath = std::filesystem::canonical("/proc/self/exe", ec);
        if (ec) {
            return std::filesystem::path();
        }
        return exePath.parent_path();
    }

    std::string getEnvVariable(const std::string& szEnvVarName) {
        const char* szEnvVar = std::getenv(szEnvVarName.c_str());
        return szEnvVar ? std::string(szEnvVar) : "";
    }

    constexpr const char* s_STEAM_MUTEX_KEY = "/MUTEX__SpaceCalibrator_Steam.lock";
    int s_hSteamMutex = INVALID_FD; // flock on linux
    bool s_isGitHubVersionInstalled = false;

    bool isAnotherInstanceRunning(bool& bIsRunningViaSteam) {
        bIsRunningViaSteam = false;
        std::string szSteamAppIdEnv = getEnvVariable("SteamAppId");
        if (spacecal::c_SPACE_CALIBRATOR_STEAM_APP_ID == szSteamAppIdEnv ||
            spacecal::c_STEAMVR_STEAM_APP_ID == szSteamAppIdEnv) {
            // We got launched via the Steam client UI.
            bIsRunningViaSteam = true;
            
            std::filesystem::path szRuntimeDir = getRuntimeDir();
            std::string szLockPath = (szRuntimeDir / "spacecalibrator_steam.lock").string();

            s_hSteamMutex = open(szLockPath.c_str(), O_CREAT | O_RDWR, 0666);
            if (s_hSteamMutex == INVALID_FD) {
                return false; 
            }

            errno = 0;
            if (flock(s_hSteamMutex, LOCK_EX | LOCK_NB) == INVALID_FD) {
                if (errno == EWOULDBLOCK) {
                    close(s_hSteamMutex);
                    s_hSteamMutex = INVALID_FD;
                    return true;
                }
            }
            return false;
        }

        return false;
    }
    void shutdownCurrentInstance() {
        if (s_hSteamMutex != INVALID_FD) {
            flock(s_hSteamMutex, LOCK_UN);
            close(s_hSteamMutex);
            s_hSteamMutex = INVALID_FD;
        }
    }

    void showMessageDialog(const std::string& title, const std::string& message) {
        LOG_INFO("displaying dialog [{}]: {}", title, message);

        pid_t pid = fork();

        if (pid == 0) {
            // child process

            // redirect stderr to /dev/null to keep the console clean if tools are missing
            int nullFd = open("/dev/null", O_WRONLY);
            dup2(nullFd, STDERR_FILENO);
            close(nullFd);

            // try invoking cli tools which provide a gui
            // order of choice -> KDE -> GNOME -> notification
            char* kdialogArgs[] = {
                (char*)"kdialog",
                (char*)"--title", (char*)title.c_str(),
                (char*)"--error", (char*)message.c_str(),
                nullptr
            };
            execvp(kdialogArgs[0], kdialogArgs);

            char* zenityArgs[] = {
                (char*)"zenity",
                (char*)"--error",
                (char*)"--title", (char*)title.c_str(),
                (char*)"--text", (char*)message.c_str(),
                (char*)"--width=300",
                nullptr
            };
            execvp(zenityArgs[0], zenityArgs);

            char* notifySendArgs[] = {
                (char*)"notify-send",
                (char*)title.c_str(),
                (char*)message.c_str(),
                nullptr
            };
            execvp(notifySendArgs[0], notifySendArgs);

            // terminate if we failed to switch to any of the above tools
            _exit(1);
        }
        else if (pid > 0) {
            // parent process - wait for the user to dismiss the dialog
            int status = 0;
            waitpid(pid, &status, 0);
        } else {
            // fork failed
            LOG_CRITICAL("Failed to fork process for message dialog.");
        }
    }

    void setThreadName(const std::string& threadName) {
        // thread name has max 16 chars
        // https://man7.org/linux/man-pages/man3/pthread_setname_np.3.html
        std::string truncatedName = threadName.substr(0, 15);
        pthread_setname_np(pthread_self(), truncatedName.c_str());
    }
}
#endif
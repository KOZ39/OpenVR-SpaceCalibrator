#include <stdio.h>
#include <windows.h>
#include "util.h"
#include "log.h"
#include "window.h"
#include "platform.h"

extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    // Space Calibrator has to be single instance to work well with Steam
    bool bIsRunningViaSteam = false;
    if (platform::isAnotherInstanceRunning(bIsRunningViaSteam)) {
        // another instance is running! exit!
        return 0;
    }

    util::init();
    logging::Init(/* isOverlay */ true);

    spacecal::Window* theWindow = new spacecal::Window;
    if (!theWindow) {
        platform::showMessageDialog("Failed to start Space Calibrator Nova", "Couldn't allocate enough memory for the window!");
        LOG_FATAL("Failed to allocate memory for the main window!");
        return -1;
    }

    if (!theWindow->CreateNativeWindow()) {
        platform::showMessageDialog("Failed to start Space Calibrator Nova", "Failed to initialise the overlay window!");
        LOG_FATAL("Failed to create native window");
        return -1;
    }

    LOG_INFO("Started Space Calibrator Nova!");

    theWindow->RunLoop();

    theWindow->Shutdown();
    delete theWindow;

    // releases global mutex, ie allows other instances to run
    platform::shutdownCurrentInstance();

    return 0;
}
#include <stdio.h>
#include <windows.h>
#include "util.h"
#include "window.h"
#include "platform.h"
#include "configuration.h"
#include "calibration.h"
#include "localisation.h"
#include "vr_core.h"
#include "log.h"

// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
// http://developer.amd.com/community/blog/2015/10/02/amd-enduro-system-for-developers/
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

    // Setup config manager
    spacecal::ConfigurationManager configManager;
    configManager.init();

    // Load locale strings
    spacecal::LocalisationManager localisationManager;
    localisationManager.init();

    spacecal::Window* theWindow = new spacecal::Window;
    if (!theWindow) {
        platform::showMessageDialog("An error occured initialising Space Calibrator Nova", "Couldn't allocate enough memory for the window!");
        LOG_FATAL("Failed to allocate memory for the main window!");
        return -1;
    }

    if (!theWindow->CreateNativeWindow()) {
        LOG_FATAL("Failed to create native window");
        return -1;
    }

    // Init SteamVR
    spacecal::VRState vrState;
    if (!vrState.init()) {
        // @TODO: Present error to user in friendly way
        LOG_CRITICAL("Failed to initialise VRState D:");
    }

    // init calibration manager or else instance will be nullptr
    spacecal::CalibrationManager* pCalibrationManager = new spacecal::CalibrationManager;

    LOG_INFO("Started Space Calibrator Nova!");

    theWindow->RunLoop();

    // close window and save settings to disk
    theWindow->Shutdown();
    spacecal::ConfigurationManager::getInstance()->saveConfiguration();

    delete pCalibrationManager;
    delete theWindow;

    // releases global mutex, ie allows other instances to run
    platform::shutdownCurrentInstance();

    return 0;
}
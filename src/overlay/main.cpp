#include <stdio.h>
#include <windows.h>
#include "util.h"
#include "log.h"
#include "window.h"

extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    util::init();
    logging::Init(/* isOverlay */ true);

    spacecal::Window* theWindow = new spacecal::Window;
    if (!theWindow) {
        // @TODO: MessageBox
        return -1;
    }

    if (!theWindow->CreateNativeWindow()) {
        return -1;
    }

    LOG_INFO("Started Space Calibrator Nova!");

    theWindow->RunLoop();

    theWindow->Shutdown();
    delete theWindow;
    return 0;
}
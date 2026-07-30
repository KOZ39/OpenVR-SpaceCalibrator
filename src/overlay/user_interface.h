#pragma once

#include <imgui.h>
#include <implot.h>
#include <IconsMaterialSymbols.h>

namespace spacecal {

    namespace Colors {
        extern ImVec4 Amber;
        extern ImVec4 Yellow;
        extern ImVec4 Green;
        extern ImVec4 Purple;
        extern ImVec4 Gray;
    }

    // state for the UI
    struct UserInterfaceState_t {
        bool bIsRunningInOverlay = false;
        bool bIsSettingsAdvanced = false;
        bool bCursorOverriddenThisFrame = false;
        bool bBaseStationPowerManagementEnabled = false;
        bool bBaseStationPowerManagementOnStartup = false;
        bool bBaseStationPowerManagementOnShutdown = false;
        bool bBaseStationPowerManagementOffModeIsSleep = true;
        size_t dwSelectedCalibrationIndex = 0; // selected calibration in the ui
        size_t dwSelectedUiPage = 0; // selected vertical tab

        // @TODO: remove
        char fooText[512] = {};
    };

    struct SpaceCalibratorVerticalTab_t {
        const char* szLocaleKey = nullptr;
        const char* szIcon = nullptr;
        void (*fnDrawTab)(double currentTime) = nullptr;
    };

    // Draws the user interface with ImGUI
    void drawInterface(bool isOverlay, double currentTime);
}
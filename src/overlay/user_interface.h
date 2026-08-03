#pragma once

#include <vector>
#include <string>
#include <imgui.h>
#include <implot.h>
#include <implot3d.h>
#include <IconsMaterialSymbols.h>

namespace spacecal {

    namespace Colors {
        extern ImVec4 Amber;
        extern ImVec4 Yellow;
        extern ImVec4 Green;
        extern ImVec4 Purple;
        extern ImVec4 Gray;
    }

    struct UserInterface_BaseStationState_t {
        bool bIsEditing = false;
        std::string szBaseStationId; // HTC BS XXXXXX or LHB-XXXXXXXX
        std::string szNickname; // user string or empty
    };

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

        // flat list cuz realistically we won't have enough elements to warrant the overhead of a hashmap
        std::vector<UserInterface_BaseStationState_t> aBaseStations;
        bool bNicknamesLoaded = false;
    };

    struct SpaceCalibratorVerticalTab_t {
        const char* szLocaleKey = nullptr;
        const char* szIcon = nullptr;
        void (*fnDrawTab)(double currentTime) = nullptr;
    };

    // Draws the user interface with ImGUI
    void drawInterface(bool isOverlay, double currentTime);
}
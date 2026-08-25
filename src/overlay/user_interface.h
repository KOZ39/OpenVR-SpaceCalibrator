#pragma once

#include "renderer/renderer.h"
#include <IconsMaterialSymbols.h>
#include <imgui.h>
#include <implot.h>
#include <implot3d.h>
#include <inttypes.h>
#include <string>
#include <vector>

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

enum ELearnPage_t {
    LearnPage_Home = 0,
    LearnPage_Standard,
    LearnPage_Continuous,
    LearnPage_BaseStations,
    LearnPage_UITour,
    LearnPage_Unknown,
};

enum EImageId_t {
    EImageId_LearnStandard_CalibrateDiagram,
    EImageId_LearnContinuous_Mounting,
    EImageId_LearnBaseStation_10_Front,
    EImageId_LearnBaseStation_10_Channel,
    EImageId_LearnBaseStation_20,
    EImageId_LearnUI_Unk0,
    EImageId_LearnUI_Unk1,
    EImageId_Learn_Preview_Standard,
    EImageId_Learn_Preview_Continuous,
    EImageId_Learn_Preview_BaseStations,
    EImageId_Learn_Preview_UITour,
    EImageId_Count,
};

// state for the UI
struct UserInterfaceState_t {
    bool bIsRunningInOverlay = false;
    bool bIsSettingsAdvanced = false;
    bool bIgnoreStageTrackingWarning = false;
    bool bCursorOverriddenThisFrame = false;
    bool bBaseStationPowerManagementEnabled = false;
    bool bBaseStationPowerManagementOnStartup = false;
    bool bBaseStationPowerManagementOnShutdown = false;
    bool bBaseStationPowerManagementOffModeIsSleep = true;
    size_t dwSelectedCalibrationIndex = 0; // selected calibration in the ui
    size_t dwSelectedUiPage = 0; // selected vertical tab
    uint32_t dwSelectedLearnPage = LearnPage_Home; // selected learn page

    // flat list cuz realistically we won't have enough elements to warrant the overhead of a hashmap
    bool bNicknamesLoaded = false;
    std::vector<UserInterface_BaseStationState_t> aBaseStations;

    renderer::TextureData_t textures[EImageId_Count] = {};
};

struct SpaceCalibratorVerticalTab_t {
    const char* szLocaleKey = nullptr;
    const char* szIcon = nullptr;
    void (*fnDrawTab)(double currentTime) = nullptr;
    bool bIsAdvancedTab = false;
};

// Draws the user interface with ImGUI
void drawInterface(bool isOverlay, double currentTime);
void cleanupInterface();
}
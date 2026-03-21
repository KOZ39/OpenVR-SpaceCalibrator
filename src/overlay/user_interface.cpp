#include "user_interface.h"
#include "imgui_extensions.h"
#include "constants.h"
#include "localisation.h"
#include "calibration.h"

namespace spacecal {

    bool bIsRunningInOverlay = false;

    inline std::string getTrackingSystemFriendlyName(const std::string& szTrackingSystemName) {

        // maps tracking system name to a unique entry per tracking device
        if (szTrackingSystemName == "lighthouse") {
            return "SteamVR Tracking"; // @NOTE: should this be localised?
        }
        if (szTrackingSystemName == "oculus") {
            // @TODO: if vd then set as virtual desktop
            // return "Virtual Desktop"; // @NOTE: should this be localised?
        }

        return szTrackingSystemName;
    }

    inline void BuildVersionInfo() {
        ImGui::SetNextWindowPos(ImVec2(10.0f, ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing()));
        ImGui::BeginChild("spacecal_version_box", ImVec2(ImGui::GetWindowWidth() - 20.0f, ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_None);
        if (bIsRunningInOverlay) {
            ImGui::Text("%s", LOCALE_FORMAT("app_title_vr", "Space Calibrator Nova", SPACECAL_VERSION_STRING).c_str()); // Space Calibrator 2.0.0 - close VR overlay to use mouse
        } else {
            ImGui::Text("%s", LOCALE_FORMAT("app_title", "Space Calibrator Nova", SPACECAL_VERSION_STRING).c_str()); // Space Calibrator 2.0.0
        }
        ImGui::EndChild();
    }

    inline void BuildTrackingSystemSelection(spacecal::TrackingSystemCalibration& calibration) {
        if (VRState::getInstance()->getTrackingSystemCount() == 0) {
            ImGui::Text("%s", LOCALE_GET("tracking_system_no_systems").c_str()); // No tracked devices present. Please turn on a device to continue.
            return;
        }

        ImGuiStyle& style = ImGui::GetStyle();
        float paneWidth = ImGui::GetContentRegionAvail().x / 2 - style.FramePadding.x;

        ImGui::TextWithWidth("ReferenceSystemLabel", LOCALE_GET("reference_space").c_str(), paneWidth);
        ImGui::SameLine();
        ImGui::TextWithWidth("TargetSystemLabel", LOCALE_GET("target_space").c_str(), paneWidth);

        int currentReferenceSystem = -1;
        int currentTargetSystem = -1;
        int firstReferenceSystemNotTargetSystem = -1;

        // build ui table for ref tracking systems
        std::vector<const char*> referenceSystems;
        std::vector<const char*> referenceSystemsUi;
        for (size_t i = 0; i < VRState::getInstance()->getTrackingSystemCount(); i++) {
            const std::string szTrackingSystemName = VRState::getInstance()->getTrackingSystem(i);
            const std::string szTrackingSystemUiName = getTrackingSystemFriendlyName(szTrackingSystemName);

            if (szTrackingSystemName == calibration.referenceDevice.trackingSystem)
                currentReferenceSystem = (int)referenceSystems.size();
            else if (firstReferenceSystemNotTargetSystem == -1 && szTrackingSystemName != calibration.targetDevice.trackingSystem)
                firstReferenceSystemNotTargetSystem = (int)referenceSystems.size();

            referenceSystems.push_back(szTrackingSystemName.c_str());
            referenceSystemsUi.push_back(szTrackingSystemUiName.c_str());
        }

        ImGui::PushItemWidth(paneWidth);
        ImGui::Combo("##ReferenceTrackingSystem", &currentReferenceSystem, &referenceSystemsUi[0], (int)referenceSystemsUi.size());

        // if we have an entry selected assign to reference
        if (currentReferenceSystem != -1 && currentReferenceSystem < (int)referenceSystems.size()) {
            calibration.referenceDevice.trackingSystem = std::string(referenceSystems[currentReferenceSystem]);
            if (calibration.referenceDevice.trackingSystem == calibration.targetDevice.trackingSystem)
                calibration.targetDevice.trackingSystem = "";
        }

        // target tracking system list is tracking system list EXCLUDING reference tracking system. one may NOT calibrate two devices of the same tracking system!
        std::vector<const char*> targetSystems;
        std::vector<const char*> targetSystemsUi;
        for (size_t i = 0; i < VRState::getInstance()->getTrackingSystemCount(); i++) {
            const std::string szTrackingSystemName = VRState::getInstance()->getTrackingSystem(i);
            if (szTrackingSystemName != calibration.referenceDevice.trackingSystem)
            {
                if (szTrackingSystemName != "" && szTrackingSystemName == calibration.targetDevice.trackingSystem)
                    currentTargetSystem = (int)targetSystems.size();
                targetSystems.push_back(szTrackingSystemName.c_str());
                targetSystemsUi.push_back(getTrackingSystemFriendlyName(szTrackingSystemName).c_str());
            }
        }

        ImGui::SameLine();
        ImGui::Combo("##TargetTrackingSystem", &currentTargetSystem, &targetSystemsUi[0], (int)targetSystemsUi.size());

        // if we have an entry selected assign to target
        if (currentTargetSystem != -1 && currentTargetSystem < targetSystems.size())
            calibration.targetDevice.trackingSystem = std::string(targetSystems[currentTargetSystem]);

        ImGui::PopItemWidth();
    }

    inline bool isSpacecalRunningStateOk() {
        return VRState::getInstance()->isSteamVrAvailable() && CalibrationManager::getInstance()->getIpcClient().IsConnected();
    }

    inline void drawTroubleshootView() {
        if (!VRState::getInstance()->isSteamVrAvailable()) {
            ImGui::Text("%s", LOCALE_GET("steamvr_unavailable").c_str());
            vr::EVRInitError eVrErr = VRState::getInstance()->getVrInitError();
            switch (eVrErr) {
            case vr::EVRInitError::VRInitError_Driver_WirelessHmdNotConnected:
                ImGui::Text("%s", LOCALE_GET("vr_troubleshooting_connect_steamlink").c_str());
                break;
            case vr::EVRInitError::VRInitError_Init_HmdNotFound:
                ImGui::Text("%s", LOCALE_GET("vr_troubleshooting_hmd_not_found").c_str());
                break;
            default:
                // need to pass by ref, cant pass as literal value
                uint32_t dwVrErr = (uint32_t)VRState::getInstance()->getVrInitError();
                ImGui::Text("%s", LOCALE_FORMAT("vr_troubleshooting_generic", dwVrErr, eVrErr).c_str());
                break;
            }
        } else if (CalibrationManager::getInstance()->getIpcClient().IsConnected()) {
            ImGui::Text("%s", LOCALE_GET("ipc_unavailable").c_str());
        }
    }

    void DrawInterface(bool isOverlay) {
        bIsRunningInOverlay = isOverlay;
        auto& io = ImGui::GetIO();

        // disable ctrl + tab, pointless in a VR overlay https://github.com/ocornut/imgui/issues/7987
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Tab, ImGuiInputFlags_RouteGlobal);

        constexpr ImGuiWindowFlags k_bareWindowFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoCollapse;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

        ImGui::Begin("Space Calibrator", nullptr, k_bareWindowFlags);

        // @TODO: Build ui

        if (!isSpacecalRunningStateOk()) {
            drawTroubleshootView();
        } else {
            const size_t dwNumCalibrations = spacecal::CalibrationManager::getInstance()->getCalibrationCount();

            for (size_t i = 0; i < dwNumCalibrations; i++) {
                spacecal::TrackingSystemCalibration& calibration = spacecal::CalibrationManager::getInstance()->getCalibration(i);
                if (calibration.hmdIsInSameTrackingSystem) {
                    // @TODO: hmd identification
                }
                else {
                    // @TODO: idfk
                }

                // @TODO: build ui
                BuildTrackingSystemSelection(calibration);
            }
        }

        BuildVersionInfo();
        ImGui::End();
    }
}
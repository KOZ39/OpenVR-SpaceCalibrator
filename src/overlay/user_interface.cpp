#include "user_interface.h"
#include "imgui_extensions.h"
#include "constants.h"
#include "localisation.h"
#include "calibration.h"

namespace spacecal {

    bool bIsRunningInOverlay = false;

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

    inline void BuildTrackingSystemSelection() {
        if (VRState::getInstance()->getTrackingSystemCount() == 0)
        {
            ImGui::Text("%s", LOCALE_GET("tracking_system_no_systems").c_str()); // No tracked devices present. Please turn on a device to continue.
            return;
        }

        ImGuiStyle& style = ImGui::GetStyle();
        float paneWidth = ImGui::GetContentRegionAvail().x / 2 - style.FramePadding.x;

        ImGui::TextWithWidth("ReferenceSystemLabel", LOCALE_GET("reference_space").c_str(), paneWidth);
        ImGui::SameLine();
        ImGui::TextWithWidth("TargetSystemLabel", LOCALE_GET("target_space").c_str(), paneWidth);
    }

    inline bool isSpacecalRunningStateOk() {
        return VRState::getInstance()->isSteamVrAvailable() && CalibrationManager::getInstance()->getIpcClient().IsConnected();
    }

    inline void drawTroubleshootView() {
        if (!VRState::getInstance()->isSteamVrAvailable()) {
            ImGui::Text("%s", LOCALE_GET("steamvr_unavailable").c_str());
            switch (VRState::getInstance()->getVrInitError()) {
            case vr::EVRInitError::VRInitError_Driver_WirelessHmdNotConnected:
                ImGui::Text("%s", LOCALE_GET("vr_troubleshooting_connect_steamlink").c_str());
                break;
            default:
                ImGui::Text("%s", LOCALE_GET("vr_troubleshooting_generic").c_str());
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
                BuildTrackingSystemSelection();
            }
        }

        BuildVersionInfo();
        ImGui::End();
    }
}
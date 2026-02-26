#include "user_interface.h"
#include "constants.h"
#include "localisation.h"

namespace spacecal {

    bool bIsRunningInOverlay = false;

    inline void BuildVersionInfo() {
        ImGui::SetNextWindowPos(ImVec2(10.0f, ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing()));
        ImGui::BeginChild("spacecal_version_box", ImVec2(ImGui::GetWindowWidth() - 20.0f, ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_None);
        if (bIsRunningInOverlay) {
            ImGui::Text("%s", LOCALE_FORMAT("app_title_vr", "Space Calibrator Nova", SPACECAL_VERSION_STRING).c_str());
        } else {
            ImGui::Text("%s", LOCALE_FORMAT("app_title", "Space Calibrator Nova", SPACECAL_VERSION_STRING).c_str());
        }
        ImGui::EndChild();
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

        BuildVersionInfo();
        ImGui::End();
    }
}
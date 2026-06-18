#include "user_interface.h"
#include "imgui.h"
#include "imgui_extensions.h"
#include "constants.h"
#include "localisation.h"
#include "calibration.h"
#include "configuration.h"

namespace spacecal {

    bool bIsRunningInOverlay = false;

    inline std::string getTrackingSystemFriendlyName(const std::string& szTrackingSystemName) {

        // maps tracking system name to a unique entry per tracking device
        if (szTrackingSystemName == "lighthouse") {
            return "SteamVR Tracking"; // @NOTE: should this be localised?
        }
        if (szTrackingSystemName == "cv") {
            return "Steam Frame"; // @NOTE: should this be localised?
        }
        if (szTrackingSystemName == "oculus") {
            // @TODO: if vd then set as virtual desktop
            // return "Virtual Desktop"; // @NOTE: should this be localised?
        }

        return szTrackingSystemName;
    }

    inline void buildVersionInfo() {
        ImGui::SetNextWindowPos(ImVec2(10.0f, ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing()));
        ImGui::BeginChild("spacecal_version_box", ImVec2(ImGui::GetWindowWidth() - 20.0f, ImGui::GetFrameHeightWithSpacing() * 2), ImGuiChildFlags_None);
        if (bIsRunningInOverlay) {
            ImGui::TextUnformatted(LOCALE_FORMAT("app_title_vr", "Space Calibrator Nova", SPACECAL_VERSION_STRING).c_str()); // Space Calibrator 2.0.0 - close VR overlay to use mouse
        } else {
            ImGui::TextUnformatted(LOCALE_FORMAT("app_title", "Space Calibrator Nova", SPACECAL_VERSION_STRING).c_str()); // Space Calibrator 2.0.0
        }
        ImGui::EndChild();
    }

    inline void buildTrackingSystemSelection(spacecal::TrackingSystemCalibration& calibration) {
        if (VRState::getInstance()->getTrackingSystemCount() == 0) {
            ImGui::TextUnformatted(LOCALE_GET("tracking_system_no_systems").c_str()); // No tracked devices present. Please turn on a device to continue.
            return;
        }

        ImGuiStyle& style = ImGui::GetStyle();
        float paneWidth = ImGui::GetContentRegionAvail().x / 2 - style.FramePadding.x;

        ImGui::HeadingWithWidth("ReferenceSystemLabel", LOCALE_GET("reference_device").c_str(), paneWidth);
        ImGui::SameLine();
        ImGui::HeadingWithWidth("TargetSystemLabel", LOCALE_GET("target_device").c_str(), paneWidth);

        ImGui::TextWrappedDisabledWithWidth("ReferenceSystemDescription", LOCALE_GET("reference_device_description").c_str(), paneWidth);
        ImGui::SameLine();
        ImGui::TextWrappedDisabledWithWidth("TargetSystemDescription", LOCALE_GET("target_device_description").c_str(), paneWidth);

        ImGui::PushItemWidth(paneWidth);

        std::string refPreview = calibration.referenceDevice.trackingSystem.empty() ? LOCALE_GET("select_reference_device") : getTrackingSystemFriendlyName(calibration.referenceDevice.trackingSystem);
        if (ImGui::BeginCombo("##ReferenceTrackingSystem", refPreview.c_str())) {
            for (size_t i = 0; i < VRState::getInstance()->getTrackingSystemCount(); i++) {
                const std::string& szTrackingSystemName = VRState::getInstance()->getTrackingSystem(i);
                const std::string szFriendlyName = getTrackingSystemFriendlyName(szTrackingSystemName);

                bool isSelected = (calibration.referenceDevice.trackingSystem == szTrackingSystemName);
                if (ImGui::Selectable(szFriendlyName.c_str(), isSelected)) {
                    calibration.referenceDevice.trackingSystem = szTrackingSystemName;
                    // if ref space is now the target space, target should be unset
                    // @TODO: should we pick the first tracking system that makes sense instead?
                    if (calibration.referenceDevice.trackingSystem == calibration.targetDevice.trackingSystem)
                        calibration.targetDevice.trackingSystem = "";
                }

                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        // target tracking system list is tracking system list EXCLUDING reference tracking system. one may NOT calibrate two devices of the same tracking system!
        std::string targetPreview = calibration.targetDevice.trackingSystem.empty() ? LOCALE_GET("select_target_device") : getTrackingSystemFriendlyName(calibration.targetDevice.trackingSystem);
        if (ImGui::BeginCombo("##TargetTrackingSystem", targetPreview.c_str())) {
            for (size_t i = 0; i < VRState::getInstance()->getTrackingSystemCount(); i++) {
                const std::string& szTrackingSystemName = VRState::getInstance()->getTrackingSystem(i);

                // target cannot be the reference space
                if (szTrackingSystemName == calibration.referenceDevice.trackingSystem)
                    continue;

                const std::string szFriendlyName = getTrackingSystemFriendlyName(szTrackingSystemName);
                bool isSelected = (calibration.targetDevice.trackingSystem == szTrackingSystemName);

                if (ImGui::Selectable(szFriendlyName.c_str(), isSelected)) {
                    calibration.targetDevice.trackingSystem = szTrackingSystemName;
                }

                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();
    }

    inline void buildDeviceSelection(spacecal::TrackingSystemCalibration& calibration, spacecal::CalibrationDevice& device) {
        vr::TrackedDeviceIndex_t selected = device.deviceId;

        // check if the selected device was disconnected or is not present
        if (selected != vr::k_unTrackedDeviceIndexInvalid) {
            bool matched = false;
            for (size_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                auto vrDevice = VRState::getInstance()->getVrDevice(i);
                if (!vrDevice.bIsConnected)
                    continue;
                if (vrDevice.szTrackingSystemId != device.trackingSystem)
                    continue;
                if (vrDevice.eDeviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_TrackingReference)
                    continue;

                if (selected == vrDevice.dwDeviceIndex) {
                    matched = true;
                    break;
                }
            }

            if (!matched) {
                // Device is no longer present.
                selected = vr::k_unTrackedDeviceIndexInvalid;
            }
        }

        bool standby = calibration.state == CalibrationState::CONTINUOUS_IDLE;

        // select the left controller, or the first device that makes sense to select
        if (selected == vr::k_unTrackedDeviceIndexInvalid && !standby) {
            for (size_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                auto vrDevice = VRState::getInstance()->getVrDevice(i);
                if (!vrDevice.bIsConnected)
                    continue;
                if (vrDevice.szTrackingSystemId != device.trackingSystem)
                    continue;
                if (vrDevice.eDeviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_TrackingReference)
                    continue;

                if (vrDevice.eControllerRole == vr::ETrackedControllerRole::TrackedControllerRole_LeftHand) {
                    selected = vrDevice.dwDeviceIndex;
                    break;
                }
            }

            if (selected == vr::k_unTrackedDeviceIndexInvalid) {
                for (size_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                    auto vrDevice = VRState::getInstance()->getVrDevice(i);
                    if (!vrDevice.bIsConnected)
                        continue;
                    if (vrDevice.szTrackingSystemId != device.trackingSystem)
                        continue;
                    if (vrDevice.eDeviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_Invalid)
                        continue;
                    if (vrDevice.eDeviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_TrackingReference)
                        continue;

                    selected = vrDevice.dwDeviceIndex;
                    break;
                }
            }
        }

        uint64_t iterator = 0;
        if (selected == vr::k_unTrackedDeviceIndexInvalid && standby) {
            bool present = false;
            for (size_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                auto vrDevice = VRState::getInstance()->getVrDevice(i);
                if (!vrDevice.bIsConnected)
                    continue;
                if (vrDevice.szTrackingSystemId != device.trackingSystem)
                    continue;
                if (vrDevice.eDeviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_TrackingReference)
                    continue;

                if (device.deviceModel != vrDevice.szModel) continue;
                if (device.deviceSerialNumber != vrDevice.szSerial) continue;

                present = true;
                break;
            }

            if (!present) {
                auto ghostLabel = fmt::format("< {} | {} >", device.deviceModel, device.deviceSerialNumber);
                std::string uniqueId = fmt::format("{}_pass0_{}", ghostLabel, iterator);
                iterator++;
                ImGui::PushID(uniqueId.c_str());
                ImGui::Selectable(ghostLabel.c_str(), true);
                ImGui::PopID();
            }
        }

        iterator = 0;

        for (size_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
            auto vrDevice = VRState::getInstance()->getVrDevice(i);
            if (!vrDevice.bIsConnected)
                continue;
            if (vrDevice.szTrackingSystemId != device.trackingSystem)
                continue;
            if (vrDevice.eDeviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_TrackingReference)
                continue;

            auto label = fmt::format("{} | {}", vrDevice.szModel, vrDevice.szSerial);
            std::string uniqueId = fmt::format("{}_pass1_{}", label, iterator);
            iterator++;
            ImGui::PushID(uniqueId.c_str());
            // @TODO: somehow i need to add icons to this
            if (ImGui::Selectable(label.c_str(), selected == vrDevice.dwDeviceIndex)) {
                selected = vrDevice.dwDeviceIndex;
            }
            ImGui::PopID();
        }

        if (selected != device.deviceId) {
            auto vrDevice = VRState::getInstance()->getVrDevice(selected);
            device.deviceId = selected;
            device.trackingSystem = vrDevice.szTrackingSystemId;
            device.deviceModel = vrDevice.szModel;
            device.deviceSerialNumber = vrDevice.szSerial;
        }
    }

    inline void buildDeviceSelection(spacecal::TrackingSystemCalibration& calibration) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec2 paneSize(ImGui::GetWindowContentRegionWidth() / 2 - style.FramePadding.x, ImGui::GetTextLineHeightWithSpacing() * 5 + style.ItemSpacing.y * 4);

        ImGui::BeginChild("left device pane", paneSize, ImGuiChildFlags_Borders);
        buildDeviceSelection(calibration, calibration.referenceDevice);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("right device pane", paneSize, ImGuiChildFlags_Borders);
        buildDeviceSelection(calibration, calibration.targetDevice);
        ImGui::EndChild();

        if (ImGui::Button(LOCALE_GET("identify_selected_devices").c_str(), ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeightWithSpacing() + 4.0f))) {
            // @TODO: non-blocking ; blocks for 500ms rn :(
            for (size_t i = 0; i < 100; ++i) {
                VRState::getInstance()->identifyDevice(calibration.targetDevice.deviceId);
                VRState::getInstance()->identifyDevice(calibration.referenceDevice.deviceId);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }

    inline bool isSpacecalRunningStateOk() {
        return VRState::getInstance()->isSteamVrAvailable() && CalibrationManager::getInstance()->getIpcClient().IsConnected();
    }

    inline void buildStandardCalibrationMenu(spacecal::TrackingSystemCalibration& calibration) {
        auto& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::NewLine();

        if (calibration.state == CalibrationState::NONE)
        {
            if (calibration.isValidCalibration() && !calibration.isActive)
            {
                std::string szTrackingSystemUiName = getTrackingSystemFriendlyName(calibration.referenceDevice.trackingSystem);
                ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1), "%s", LOCALE_FORMAT("calibration_error_reference_hmd_missing", szTrackingSystemUiName).c_str());
                ImGui::NewLine();
            }

            float width = ImGui::GetWindowContentRegionWidth() - style.FramePadding.x * 2.0f;
            float scale = 1.0f / 2.0f;
            if (calibration.isValidCalibration())
            {
                width -= style.FramePadding.x * 4.0f;
                scale = 1.0f / 4.0f;
            }

            if (ImGui::Button(LOCALE_GET("calibration_action_start").c_str(), ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
            {
                // ImGui::OpenPopup("calibration_progress");
                calibration.start();
            }

            ImGui::SameLine();
            if (ImGui::Button(LOCALE_GET("calibration_action_continuous").c_str(), ImVec2(width * scale, ImGui::GetTextLineHeight() * 2))) {
                calibration.startContinuous();
            }

            if (calibration.isValidCalibration())
            {
                // ImGui::SameLine();
                // if (ImGui::Button(LOCALE_GET("calibration_action_edit").c_str(), ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
                // {
                //     calibration.state = CalibrationState::EDITING;
                // }

                ImGui::SameLine();
                if (ImGui::Button(LOCALE_GET("calibration_action_clear").c_str(), ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
                {
                    calibration.reset();
                    // spacecal::ConfigurationManager::getInstance()->updateCalibration(calibration);
                    // spacecal::ConfigurationManager::getInstance()->saveConfiguration();
                }
            }

#if 0
            width = ImGui::GetWindowContentRegionWidth();
            scale = 1.0f;
            if (calibration.chaperone.valid)
            {
                width -= style.FramePadding.x * 2.0f;
                scale = 0.5;
            }

            ImGui::NewLine();
            if (ImGui::Button("Copy Chaperone Bounds to profile", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
            {
                LoadChaperoneBounds();
                SaveProfile(calibration);
            }

            if (calibration.chaperone.valid)
            {
                ImGui::SameLine();
                if (ImGui::Button("Paste Chaperone Bounds", ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
                {
                    ApplyChaperoneBounds();
                }

                if (ImGui::Checkbox(" Paste Chaperone Bounds automatically when geometry resets", &calibration.chaperone.autoApply))
                {
                    SaveProfile(calibration);
                }
            }
#endif
            ImGui::Checkbox("DEBUG: relative transform. RECALIBRATE TO APPLY", &calibration.isRelativeCalibration);

            ImGui::NewLine();
            auto speed = calibration.calibrationSpeed;

            ImGui::Columns(4, nullptr, false);
            ImGui::TextUnformatted(LOCALE_GET("calibration_speed").c_str());

            ImGui::NextColumn();
            if (ImGui::RadioButton(LOCALE_GET("calibration_speed_fast").c_str(), speed == CalibrationSpeed::FAST))
                calibration.calibrationSpeed = CalibrationSpeed::FAST;

            ImGui::NextColumn();
            if (ImGui::RadioButton(LOCALE_GET("calibration_speed_slow").c_str(), speed == CalibrationSpeed::SLOW))
                calibration.calibrationSpeed = CalibrationSpeed::SLOW;

            ImGui::NextColumn();
            if (ImGui::RadioButton(LOCALE_GET("calibration_speed_very_slow").c_str(), speed == CalibrationSpeed::VERY_SLOW))
                calibration.calibrationSpeed = CalibrationSpeed::VERY_SLOW;

            ImGui::Columns(1);
        }
#if 0
        else if (calibration.state == CalibrationState::EDITING)
        {
            BuildProfileEditor();

            if (ImGui::Button("Save Profile", ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeight() * 2)))
            {
                SaveProfile(calibration);
                calibration.state = CalibrationState::NONE;
            }
        }
#endif
        else if (calibration.isContinuousCalibration()) {

            if (calibration.isValidCalibration() && !calibration.isActive)
            {
                std::string szTrackingSystemUiName = getTrackingSystemFriendlyName(calibration.referenceDevice.trackingSystem);
                ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1), "%s", LOCALE_FORMAT("calibration_error_reference_hmd_missing", szTrackingSystemUiName).c_str());
                ImGui::NewLine();
            }

            float width = ImGui::GetWindowContentRegionWidth() - style.FramePadding.x * 2.0f;
            float scale = 1.0f / 2.0f;
            if (calibration.isValidCalibration())
            {
                width -= style.FramePadding.x * 4.0f;
                scale = 1.0f / 4.0f;
            }

            if (ImGui::Button(LOCALE_GET("calibration_action_start").c_str(), ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
            {
                calibration.start();
            }

            ImGui::SameLine();
            if (ImGui::Button(LOCALE_GET("calibration_action_continuous").c_str(), ImVec2(width * scale, ImGui::GetTextLineHeight() * 2))) {
                calibration.startContinuous();
            }

            if (calibration.isValidCalibration())
            {
                ImGui::SameLine();
                if (ImGui::Button(LOCALE_GET("calibration_action_clear").c_str(), ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
                {
                    calibration.reset();
                }
            }

            // continuous settings
            if (calibration.isContinuousCalibration()) {
                if (ImGui::Checkbox(LOCALE_GET("continuous_hide_tracker").c_str(), &calibration.hideContinuousTracker)) {
                    CalibrationManager::getInstance()->saveConfig();
                }
            }

            if (ImGui::Checkbox("DEBUG: relative transform. RECALIBRATE TO APPLY", &calibration.isRelativeCalibration)) {
                calibration.forceNextCalibration();
            }

            ImGui::NewLine();
            auto speed = calibration.calibrationSpeed;

            ImGui::Columns(4, nullptr, false);
            ImGui::TextUnformatted(LOCALE_GET("calibration_speed").c_str());

            ImGui::NextColumn();
            if (ImGui::RadioButton(LOCALE_GET("calibration_speed_fast").c_str(), speed == CalibrationSpeed::FAST)) {
                calibration.calibrationSpeed = CalibrationSpeed::FAST;
                calibration.clearSamples();
            }

            ImGui::NextColumn();
            if (ImGui::RadioButton(LOCALE_GET("calibration_speed_slow").c_str(), speed == CalibrationSpeed::SLOW)) {
                calibration.calibrationSpeed = CalibrationSpeed::SLOW;
                calibration.clearSamples();
            }

            ImGui::NextColumn();
            if (ImGui::RadioButton(LOCALE_GET("calibration_speed_very_slow").c_str(), speed == CalibrationSpeed::VERY_SLOW)) {
                calibration.calibrationSpeed = CalibrationSpeed::VERY_SLOW;
                calibration.clearSamples();
            }

            ImGui::Columns(1);

            ImGui::TextUnformatted(LOCALE_GET("calibration_info_move_around_unsifficient_samples").c_str());
            ImGui::Button(LOCALE_GET("calibration_progress_placeholder").c_str(), ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeight() * 2));
            float fCalibrationProgressPercent = calibration.getCalibrationProgress() * 100.0f;
            ImGui::ProgressBar(calibration.getCalibrationProgress(), ImVec2(-FLT_MIN, 0), fmt::format("{:.2f}%", fCalibrationProgressPercent).c_str());
        }
        else
        {
            ImGui::TextUnformatted(LOCALE_GET("calibration_info_move_around_unsifficient_samples").c_str());
            ImGui::Button(LOCALE_GET("calibration_progress_placeholder").c_str(), ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeight() * 2));
            float fCalibrationProgressPercent = calibration.getCalibrationProgress() * 100.0f;
            ImGui::ProgressBar(calibration.getCalibrationProgress(), ImVec2(-FLT_MIN, 0), fmt::format("{:.2f}%", fCalibrationProgressPercent).c_str());
        }
    }

    inline void drawTroubleshootView() {
        if (!VRState::getInstance()->isSteamVrAvailable()) {
            ImGui::TextUnformatted(LOCALE_GET("steamvr_unavailable").c_str());
            vr::EVRInitError eVrErr = VRState::getInstance()->getVrInitError();
            switch (eVrErr) {
            case vr::EVRInitError::VRInitError_Driver_WirelessHmdNotConnected:
                ImGui::TextUnformatted(LOCALE_GET("vr_troubleshooting_connect_steamlink").c_str());
                break;
            case vr::EVRInitError::VRInitError_Init_HmdNotFound:
                ImGui::TextUnformatted(LOCALE_GET("vr_troubleshooting_hmd_not_found").c_str());
                break;
            default:
                // need to pass by ref, cant pass as literal value
                uint32_t dwVrErr = (uint32_t)eVrErr;
                ImGui::TextUnformatted(LOCALE_FORMAT("vr_troubleshooting_generic", dwVrErr, eVrErr).c_str());
                break;
            }
        } else if (CalibrationManager::getInstance()->getIpcClient().IsConnected()) {
            ImGui::TextUnformatted(LOCALE_GET("ipc_unavailable").c_str());
        }
    }

    inline void buildLocaleSelector() {
        ImGui::TextUnformatted(LOCALE_GET("settings_locale").c_str());
        ImGui::SameLine();

        std::string& szLocale = ConfigurationManager::getInstance()->getConfiguration()->uiLocale;
        Locale eLocale = LocalisationManager::getInstance()->getLocaleFromRegionString(szLocale);
        std::string selectedLocaleNativeName = LocalisationManager::getInstance()->getNativeTongueLocaleName(eLocale);

        std::string selectedLocale = fmt::format("languages_{}", szLocale);
        std::string displaySelectedLocaleName = fmt::format("{} ({})", LOCALE_GET(selectedLocale), selectedLocaleNativeName);

        if (ImGui::BeginCombo("##LocalePicker", displaySelectedLocaleName.c_str())) {
            for (size_t i = 0; i < (size_t)Locale::Count; i++) {
                const Locale eThisLocale = (Locale)i;
                std::string thisLocaleId = LocalisationManager::getInstance()->getLocaleAsRegionString(eThisLocale);
                std::string thisLocaleString = fmt::format("languages_{}", thisLocaleId);

                std::string friendlyLocaleString = LOCALE_GET(thisLocaleString);
                std::string localeNativeName = LocalisationManager::getInstance()->getNativeTongueLocaleName(eThisLocale);

                // German (Deustche)
                std::string displayLocaleName = fmt::format("{} ({})", friendlyLocaleString, localeNativeName);

                bool isSelected = (eLocale == eThisLocale);

                if (ImGui::Selectable(displayLocaleName.c_str(), isSelected)) {
                    szLocale = thisLocaleId;
                    // apply locale to UI
                    LocalisationManager::getInstance()->setLocale(eThisLocale);
                    ConfigurationManager::getInstance()->saveConfiguration();
                }

                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TextDisabled("Note: does practically nothing right now!");

        // @HACK: temp for testing if this unfucks overlay input lmao
        static char fooText[1024 * 8] = {};
        ImGui::InputText("test textbox (keyboard shouldnt be corrupt)", fooText, sizeof(fooText));
    }

    inline void buildSettingsView() {
        ImGui::TextUnformatted(LOCALE_GET("settings_title").c_str());
        ImGui::TextDisabled("This layout is temporary.");
        buildLocaleSelector();
    }

    void drawInterface(bool isOverlay) {
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
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

        ImGui::Begin("Space Calibrator", nullptr, k_bareWindowFlags);

        // @TODO: Build ui

        if (!isSpacecalRunningStateOk()) {
            drawTroubleshootView();
        } else {
            const size_t dwNumCalibrations = spacecal::CalibrationManager::getInstance()->getCalibrationCount();

            if (dwNumCalibrations == 0) {
                // @TODO: no calibrations edge case handling
            }

            for (size_t i = 0; i < dwNumCalibrations; i++) {
                spacecal::TrackingSystemCalibration& calibration = spacecal::CalibrationManager::getInstance()->getCalibration(i);
                if (calibration.hmdIsInReferenceTrackingSystem) {
                    // @TODO: hmd identification
                }
                else {
                    // @TODO: idfk
                }

                // @TODO: build ui
                buildTrackingSystemSelection(calibration);
                buildDeviceSelection(calibration);
                buildStandardCalibrationMenu(calibration);
            }
        }

        buildSettingsView();

        buildVersionInfo();
        ImGui::End();
    }
}
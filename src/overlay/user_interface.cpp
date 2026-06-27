#include "user_interface.h"
#include "log.h"
#include "imgui.h"
#include "imgui_extensions.h"
#include "constants.h"
#include "localisation.h"
#include "calibration.h"
#include "configuration.h"

namespace spacecal {

    UserInterfaceState_t g_state = {};

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
        if (g_state.bIsRunningInOverlay) {
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
        float paneWidth = (ImGui::GetContentRegionAvail().x - style.FramePadding.x) / 2;

        ImGui::HeadingWithWidth(LOCALE_GET("reference_device").c_str(), paneWidth);
        ImGui::SameLine(paneWidth + style.FramePadding.x * 3.5);
        ImGui::HeadingWithWidth(LOCALE_GET("target_device").c_str(), paneWidth);

        ImGui::TextWrappedDisabledWithWidth(LOCALE_GET("reference_device_description").c_str(), paneWidth);
        ImGui::SameLine();
        ImGui::TextWrappedDisabledWithWidth(LOCALE_GET("target_device_description").c_str(), paneWidth);

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

        if (ImGui::Button(LOCALE_GET("identify_auto_detect_devices").c_str(), ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeightWithSpacing() + 4.0f))) {
            LOG_WARN("identify_auto_detect_devices:: Not implemented");
        }
    }

    inline bool isSpacecalRunningStateOk() {
        return VRState::getInstance()->isSteamVrAvailable() && CalibrationManager::getInstance()->getIpcClient().IsConnected();
    }

    inline void buildCalibrationCommonControls(spacecal::TrackingSystemCalibration& calibration) {
        auto& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::NewLine();

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

            if (ImGui::Checkbox("DEBUG: relative transform. RECALIBRATE TO APPLY", &calibration.isRelativeCalibration)) {
                calibration.forceNextCalibration();
            }
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

        if (g_state.bIsSettingsAdvanced) {
            ImGui::Checkbox(LOCALE_GET("settings_calibrate_motion_vectors").c_str(), &calibration.calibrateMotionVectors);
            ImGui::TextDisabled("%s", LOCALE_GET("settings_calibrate_motion_vectors_description").c_str());
        }

        if (calibration.isCalibrating()) {
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
        } else if (!CalibrationManager::getInstance()->getIpcClient().IsConnected()) {
            ImGui::TextUnformatted(LOCALE_GET("ipc_unavailable").c_str());
        } else {
            ImGui::TextUnformatted("Something went catastrophically wrong! Please report this to the developer either on GitHub or Steam Discussions so that this can be addressed.");
        }
    }

    inline void buildLocaleSelector() {
        ImGui::TextUnformatted(LOCALE_GET("settings_locale").c_str());
        ImGui::SameLine();

        std::string& szLocale = ConfigurationManager::getInstance()->getConfiguration()->ui_locale;
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
    }

    void page_calibration() {
        ImGui::TextHeading("%s", LOCALE_GET("calibration_title").c_str());
        ImGui::Separator();

        if (!isSpacecalRunningStateOk()) {
            drawTroubleshootView();
        }
        else {
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
                if (!calibration.isContinuousCalibration()) {
                    if (calibration.state == CalibrationState::EDITING) {
                        // BuildProfileEditor();
                        // 
                        // if (ImGui::Button("Save Profile", ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetTextLineHeight() * 2)))
                        // {
                        //     SaveProfile(calibration);
                        //     calibration.state = CalibrationState::NONE;
                        // }
                    }
                }
                buildCalibrationCommonControls(calibration);
            }
        }
    }
    
    void page_graphs() {
        ImGui::TextHeading("%s", "Graphs");
        ImGui::TextDisabled("COMING LATER");
    }

    void page_settings() {
        ImGui::TextHeading("%s", LOCALE_GET("settings_title").c_str());

        // advanced settings
        if (ImGui::Checkbox(LOCALE_GET("settings_advanced").c_str(), &g_state.bIsSettingsAdvanced)) {
            ConfigurationManager::getInstance()->getConfiguration()->advanced_settings = g_state.bIsSettingsAdvanced;
            ConfigurationManager::getInstance()->saveConfiguration();
        }
        ImGui::TextDisabled("%s", LOCALE_GET("settings_advanced_description").c_str());

        buildLocaleSelector();
    }

    void page_base_station_management() {
        ImGui::TextHeading("%s", "Base Station Management");
        ImGui::TextDisabled("COMING LATER");
    }

    inline const char* get_tracking_result_name(vr::ETrackingResult result) {
        switch (result) {
        case vr::TrackingResult_Uninitialized:          return "Uninitialized";
        case vr::TrackingResult_Calibrating_InProgress: return "Calibrating (In Progress)";
        case vr::TrackingResult_Calibrating_OutOfRange: return "Calibrating (Out of Range)";
        case vr::TrackingResult_Running_OK:             return "Running (OK)";
        case vr::TrackingResult_Running_OutOfRange:     return "Running (Out of Range)";
        case vr::TrackingResult_Fallback_RotationOnly:  return "Fallback (Rotation Only)";
        default:                                        return "Unknown";
        }
    }

    inline void debug_driver_pose_viewer(const char* label, const vr::DriverPose_t& pose) {

#define SHOW_BOOL_STATUS(label, value)                                                  \
    ImGui::Text(label ":");                                                             \
    ImGui::SameLine();                                                                  \
    if (value) {                                                                        \
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "TRUE");                     \
    } else {                                                                            \
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "FALSE");                    \
    }

        if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::PushID(label);
            if (ImGui::CollapsingHeader("Metadata", ImGuiTreeNodeFlags_DefaultOpen)) {
                SHOW_BOOL_STATUS("Device Connected", pose.deviceIsConnected);
                SHOW_BOOL_STATUS("Pose Is Valid", pose.poseIsValid);
                SHOW_BOOL_STATUS("Will Drift In Yaw", pose.willDriftInYaw);
                SHOW_BOOL_STATUS("Should Apply Head", pose.shouldApplyHeadModel);

                ImGui::Separator();

                ImGui::Text("Tracking Result:"); ImGui::SameLine();
                if (pose.result == vr::TrackingResult_Running_OK) {
                    ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "%s", get_tracking_result_name(pose.result));
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "%s", get_tracking_result_name(pose.result));
                }

                ImGui::Text("Prediction Time: %f s", pose.poseTimeOffset);
            }
            if (ImGui::CollapsingHeader("Motion data")) {
                ImGui::Text("Position:     %.4f, %.4f, %.4f", pose.vecPosition[0], pose.vecPosition[1], pose.vecPosition[2]);
                ImGui::Text("Velocity:     %.4f, %.4f, %.4f", pose.vecVelocity[0], pose.vecVelocity[1], pose.vecVelocity[2]);
                ImGui::Text("Accel:        %.4f, %.4f, %.4f", pose.vecAcceleration[0], pose.vecAcceleration[1], pose.vecAcceleration[2]);
                ImGui::Separator();
                ImGui::Text("Ang Velocity: %.4f, %.4f, %.4f", pose.vecAngularVelocity[0], pose.vecAngularVelocity[1], pose.vecAngularVelocity[2]);
                ImGui::Text("Ang Accel:    %.4f, %.4f, %.4f", pose.vecAngularAcceleration[0], pose.vecAngularAcceleration[1], pose.vecAngularAcceleration[2]);
                ImGui::Separator();
                ImGui::Text("Quat (WXYZ):  %.4f, %.4f, %.4f, %.4f", pose.qRotation.w, pose.qRotation.x, pose.qRotation.y, pose.qRotation.z);
            }
            if (ImGui::CollapsingHeader("Coordinate Transforms")) {
                ImGui::TextDisabled("World From Driver-Space");
                ImGui::Text("Translation:  %.4f, %.4f, %.4f", pose.vecWorldFromDriverTranslation[0], pose.vecWorldFromDriverTranslation[1], pose.vecWorldFromDriverTranslation[2]);
                ImGui::Text("Quat (WXYZ):  %.4f, %.4f, %.4f, %.4f", pose.qWorldFromDriverRotation.w, pose.qWorldFromDriverRotation.x, pose.qWorldFromDriverRotation.y, pose.qWorldFromDriverRotation.z);

                ImGui::Separator();

                ImGui::TextDisabled("Driver From Head-Space");
                ImGui::Text("Translation:  %.4f, %.4f, %.4f", pose.vecDriverFromHeadTranslation[0], pose.vecDriverFromHeadTranslation[1], pose.vecDriverFromHeadTranslation[2]);
                ImGui::Text("Quat (WXYZ):  %.4f, %.4f, %.4f, %.4f", pose.qDriverFromHeadRotation.w, pose.qDriverFromHeadRotation.x, pose.qDriverFromHeadRotation.y, pose.qDriverFromHeadRotation.z);
            }
            ImGui::PopID();
            ImGui::Unindent();
        }
#undef SHOW_BOOL_STATUS
    }
    
    void page_debug() {
        ImGui::TextHeading("%s", "Debug");

        // @TODO: driverpose_t view
        spacecal::TrackingSystemCalibration& calibration = spacecal::CalibrationManager::getInstance()->getCalibration(g_state.dwSelectedCalibrationIndex);
        if (calibration.referenceDevice.deviceId < vr::k_unMaxTrackedDeviceCount) {
            debug_driver_pose_viewer("Reference device DriverPose_t", CalibrationManager::getInstance()->m_poses[calibration.referenceDevice.deviceId]);
        }
        if (calibration.targetDevice.deviceId < vr::k_unMaxTrackedDeviceCount) {
            debug_driver_pose_viewer("Target device DriverPose_t", CalibrationManager::getInstance()->m_poses[calibration.targetDevice.deviceId]);
        }

        // @HACK: temp for testing if this unfucks overlay input lmao
        ImGui::InputText("test textbox (keyboard shouldnt be corrupt)", g_state.fooText, sizeof(g_state.fooText));
    }

    void page_about() {
        ImGui::TextHeading("%s", "About");
        ImGui::TextDisabled("COMING LATER");
    }

    // UI CORE LAYOUT

    SpaceCalibratorVerticalTab_t g_spaceCalUiTabs[] = {
        { .szLocaleKey = "tab_page_calibration", .fnDrawTab = page_calibration, },
        { .szLocaleKey = "tab_page_graphs", .fnDrawTab = page_graphs, },
        { .szLocaleKey = "tab_page_base_station_management", .fnDrawTab = page_base_station_management, },
        { .szLocaleKey = "tab_page_settings", .fnDrawTab = page_settings, },
#if _DEBUG || 1 // @TODO: reserved exclusively for debug mode, may make sense to enable with a flag?
        { .szLocaleKey = "tab_page_debug", .fnDrawTab = page_debug, },
#endif
        { .szLocaleKey = "tab_page_about", .fnDrawTab = page_about, },
    };
    constexpr size_t k_SIZE_SPACECAL_UI_TABS = sizeof(g_spaceCalUiTabs) / sizeof(g_spaceCalUiTabs[0]);

    inline bool verticalTab(const SpaceCalibratorVerticalTab_t& tabData, bool selected, const ImVec2& size) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(tabData.szLocaleKey);

        ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        constexpr ImU32 k_TRANSPARENT = IM_COL32(0, 0, 0, 0);
        constexpr float k_IndicatorWidth = 4.0f; 
        const float k_LeftTextMargin  = style.FramePadding.x * 2 + k_IndicatorWidth; 

        if (selected || hovered) {
            ImU32 col_bg_max = k_TRANSPARENT;
            ImU32 col_bg_min = k_TRANSPARENT;
            if (selected) {
                col_bg_max = ImGui::GetColorU32(ImGuiCol_HeaderActive);
                col_bg_max = IM_COL32_SET_ALPHA(col_bg_max, 90); // 35% opacity
                col_bg_min = IM_COL32_SET_ALPHA(col_bg_max, 27); // 10% opacity
            } else if (hovered) {
                col_bg_max = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
                col_bg_max = IM_COL32_SET_ALPHA(col_bg_max, 45); // 17.5% opacity
                col_bg_min = IM_COL32_SET_ALPHA(col_bg_max, 14); // 5% opacity
            }
            
            window->DrawList->AddRectFilledMultiColor(bb.Min, bb.Max, col_bg_max, col_bg_min, col_bg_min, col_bg_max);
        }

        if (selected) {
            ImVec2 line_min = bb.Min;
            ImVec2 line_max = ImVec2(bb.Min.x + k_IndicatorWidth, bb.Max.y);
            ImU32 col_line = ImGui::GetColorU32(ImGuiCol_SliderGrab); 

            window->DrawList->AddRectFilled(line_min, line_max, col_line, style.FrameRounding, ImDrawFlags_RoundCornersLeft);
        }

        if (!selected && hovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        std::string textStr = LOCALE_GET(tabData.szLocaleKey);
        ImVec2 textSize = ImGui::CalcTextSize(textStr.c_str(), nullptr, true);
        float text_y_offset = (size.y - textSize.y) * 0.5f;
        float final_text_padding_x = k_LeftTextMargin + (selected ? k_IndicatorWidth : 0.0f);
        ImVec2 text_pos = ImVec2(bb.Min.x + final_text_padding_x, bb.Min.y + text_y_offset);

        ImGui::RenderText(text_pos, textStr.c_str());

        return pressed;
    }

    inline void drawMainView() {
        // @TODO: temp hardcode, move to header or some ui_config.h idk
        const float k_SIDEBAR_WIDTH = 180.0f;
        const float k_SIDEBAR_TAB_HEIGHT = 40.0f;
        const float k_CONTENT_AREA_PADDING = 5.0f;

        // sidebar
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild("Sidebar", ImVec2(k_SIDEBAR_WIDTH, 0), ImGuiChildFlags_None);
        ImGui::Spacing(); 

        for (size_t i = 0; i < k_SIZE_SPACECAL_UI_TABS; ++i) {
            ImGui::PushID((int)i);

            float itemWidth = ImGui::GetContentRegionAvail().x;

            if (verticalTab(g_spaceCalUiTabs[i], g_state.dwSelectedUiPage == i, ImVec2(itemWidth, k_SIDEBAR_TAB_HEIGHT))) {
                g_state.dwSelectedUiPage = i;
            }

            ImGui::PopID();
            ImGui::Spacing();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::SameLine();

        // content
        ImGui::BeginChild("ContentArea", ImVec2(0, 0), ImGuiChildFlags_None);

        ImGui::Dummy(ImVec2(0, k_CONTENT_AREA_PADDING)); 
        ImGui::Indent(k_CONTENT_AREA_PADDING);

        if (g_state.dwSelectedUiPage < k_SIZE_SPACECAL_UI_TABS) {
            g_spaceCalUiTabs[g_state.dwSelectedUiPage].fnDrawTab();
        }

        ImGui::Unindent(k_CONTENT_AREA_PADDING);
        ImGui::EndChild();
    }

    // UI ENTRY POINT

    void drawInterface(bool isOverlay) {
        g_state.bIsRunningInOverlay = isOverlay;
        g_state.bIsSettingsAdvanced = ConfigurationManager::getInstance()->getConfiguration()->advanced_settings;
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

        drawMainView();

        buildVersionInfo();
        ImGui::End();
    }
}
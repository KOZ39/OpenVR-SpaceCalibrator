#include "user_interface.h"
#include "log.h"
#include "imgui.h"
#include "imgui_extensions.h"
#include "constants.h"
#include "localisation.h"
#include "calibration.h"
#include "configuration.h"
#include "util.h"
#include "base_station_management.h"

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

        if (selected != device.deviceId && selected < vr::k_unMaxTrackedDeviceCount) {
            auto vrDevice = VRState::getInstance()->getVrDevice(selected);
            device.deviceId = selected;
            device.trackingSystem = vrDevice.szTrackingSystemId;
            device.deviceModel = vrDevice.szModel;
            device.deviceSerialNumber = vrDevice.szSerial;
        }
    }

    inline void buildDeviceSelection(spacecal::TrackingSystemCalibration& calibration) {
        if (VRState::getInstance()->getTrackingSystemCount() == 0) {
            ImGui::TextUnformatted(LOCALE_GET("tracking_system_no_systems").c_str()); // No tracked devices present. Please turn on a device to continue.
            return;
        }

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec2 paneSize(
            (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x - ImGui::k_BORDER_WIDTH * 4.0f) / 2.0f,
            ImGui::GetTextLineHeightWithSpacing() * 5.0f + style.ItemSpacing.y * 8.0f + ImGui::GetTextLineHeight() * 3.0f + style.FramePadding.y * 2.0f
        );

        ImGui::BeginCard("left device pane", paneSize, ImGuiChildFlags_Borders);
        {
            ImGui::TextHeading(LOCALE_GET("reference_device").c_str());
            ImGui::TextWrappedDisabled(LOCALE_GET("reference_device_description").c_str());

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
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

            buildDeviceSelection(calibration, calibration.referenceDevice);
            ImGui::PopItemWidth();
        }
        ImGui::EndCard();

        ImGui::SameLine();

        ImGui::BeginCard("right device pane", paneSize, ImGuiChildFlags_Borders);
        {
            ImGui::TextHeading(LOCALE_GET("target_device").c_str());
            ImGui::TextWrappedDisabled(LOCALE_GET("target_device_description").c_str());

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
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

            buildDeviceSelection(calibration, calibration.targetDevice);
            ImGui::PopItemWidth();
        }
        ImGui::EndCard();

        if (ImGui::Button(LOCALE_GET("identify_selected_devices").c_str(), ImVec2(paneSize.x, ImGui::GetTextLineHeightWithSpacing() + 4.0f))) {
            // @TODO: non-blocking ; blocks for 500ms rn :(
            for (size_t i = 0; i < 100; ++i) {
                VRState::getInstance()->identifyDevice(calibration.targetDevice.deviceId);
                VRState::getInstance()->identifyDevice(calibration.referenceDevice.deviceId);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        ImGui::SameLine();

        if (ImGui::Button(LOCALE_GET("identify_auto_detect_devices").c_str(), ImVec2(paneSize.x, ImGui::GetTextLineHeightWithSpacing() + 4.0f))) {
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

        float width = ImGui::GetContentRegionAvail().x - style.FramePadding.x * 2.0f;
        float scale = 1.0f / 2.0f;
        if (calibration.isValidCalibration())
        {
            width -= style.FramePadding.x * 4.0f;
            scale = 1.0f / 4.0f;
        }

        if (ImGui::IconButton(ICON_MS_PLAY_ARROW, LOCALE_GET("calibration_action_start").c_str(), ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
        {
            calibration.start();
        }

        ImGui::SameLine();
        if (ImGui::IconButton(ICON_MS_SYNC, LOCALE_GET("calibration_action_continuous").c_str(), ImVec2(width * scale, ImGui::GetTextLineHeight() * 2))) {
            calibration.startContinuous();
        }

        if (calibration.isValidCalibration())
        {
            ImGui::SameLine();
            if (ImGui::IconButton(ICON_MS_CLOSE, LOCALE_GET("calibration_action_clear").c_str(), ImVec2(width * scale, ImGui::GetTextLineHeight() * 2)))
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
        ImGui::TextUnformatted(fmt::format("{}  {}", ICON_MS_SPEED, LOCALE_GET("calibration_speed").c_str()).c_str());

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
            ImGui::TextWrapped("%s", LOCALE_GET("calibration_info_move_around_unsifficient_samples").c_str());
            ImGui::Button(LOCALE_GET("calibration_progress_placeholder").c_str(), ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 2));
            float fCalibrationProgressPercent = calibration.getCalibrationProgress() * 100.0f;
            ImGui::ProgressBar(calibration.getCalibrationProgress(), ImVec2(-FLT_MIN, 0), fmt::format("{:.2f}%", fCalibrationProgressPercent).c_str());
        } else if (calibration.isContinuousCalibration()) {
            ImGui::TextWrapped("%s", LOCALE_GET("calibration_info_continuous_progress").c_str());
            ImGui::Button(LOCALE_GET("calibration_progress_placeholder").c_str(), ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 2));
            float fCalibrationProgressPercent = calibration.getCalibrationProgress() * 100.0f;
            ImGui::ProgressBar(calibration.getCalibrationProgress(), ImVec2(-FLT_MIN, 0), fmt::format("{:.2f}%", fCalibrationProgressPercent).c_str());
        }
    }

    inline void drawTroubleshootView() {
        if (!VRState::getInstance()->isSteamVrAvailable()) {
            ImGui::TextWrapped("%s", LOCALE_GET("steamvr_unavailable").c_str());
            vr::EVRInitError eVrErr = VRState::getInstance()->getVrInitError();
            switch (eVrErr) {
            case vr::EVRInitError::VRInitError_Driver_WirelessHmdNotConnected:
                ImGui::TextWrapped("%s", LOCALE_GET("vr_troubleshooting_connect_steamlink").c_str());
                break;
            case vr::EVRInitError::VRInitError_Init_HmdNotFound:
                ImGui::TextWrapped("%s", LOCALE_GET("vr_troubleshooting_hmd_not_found").c_str());
                break;
            default:
                // need to pass by ref, cant pass as literal value
                uint32_t dwVrErr = (uint32_t)eVrErr;
                ImGui::TextUnformatted(LOCALE_FORMAT("vr_troubleshooting_generic", dwVrErr, eVrErr).c_str());
                break;
            }
        } else if (!CalibrationManager::getInstance()->getIpcClient().IsConnected()) {
            ImGui::TextWrapped("%s", LOCALE_GET("ipc_unavailable").c_str());
        } else {
            ImGui::TextWrapped("%s", LOCALE_GET("error_unknown_catastrophic").c_str());
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

    struct PlotUiState {
        double referenceTime = 0.0;
        double lastMouseX = -HUGE_VAL;
        bool wasHoveredThisFrame = false;
        
        std::vector<double> calAppliedTimeBuffer;
        std::vector<double> calByRelPoseTimeBuffer;
        std::vector<int> currentGraphIndexes;
        ImPlotColormap axisVarianceColormap = 0;
        bool colormapInitialized = false;
    };

    PlotUiState g_PlotUiState;

    constexpr size_t k_INVALID_OFFSET = (size_t)(-1);

    enum class SeriesType {
        Scalar,
        Vector3d,
        CustomFunc
    };

    struct GraphConfig {
        const char* menuName = nullptr;
        const char* menuId = nullptr;
        const char* szAxisUnits = nullptr;
        SeriesType type = SeriesType::Scalar;
        size_t offset = k_INVALID_OFFSET;
        /**
         * weird graph rendering (eg fills or a constant line). this renders inside an active implot context so dont call implot::begin or end
         *
         * @param render_sample_count   the amount of samples we are rendering for this graph
         * @param baseSpec              implot rendering params
         */
        void (*customRenderFunc)(const CalibrationErrorMetrics& metrics, int render_sample_count, const ImPlotSpec& baseSpec) = nullptr;
        double yLimit = 0.0;
        double xScale = 1.0; // used for eg zooming in to specific graphs like velocity
        ImPlotAxisFlags yFlags = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit;

        // ptr to next graph config to draw in the ui
        const GraphConfig* pNext = nullptr;
    };

    void addApplyTicks() {
        ImPlotSpec defaultSpec;

        if (g_PlotUiState.calAppliedTimeBuffer.empty()) {
            double x = -HUGE_VAL;
            ImPlot::PlotInfLines("##CalibrationAppliedTime", &x, 1, defaultSpec);
        } else {
            ImPlot::PlotInfLines("##CalibrationAppliedTime", g_PlotUiState.calAppliedTimeBuffer.data(), static_cast<int>(g_PlotUiState.calAppliedTimeBuffer.size()), defaultSpec);
        }

        if (g_PlotUiState.calByRelPoseTimeBuffer.empty()) {
            double x = -HUGE_VAL;
            ImPlot::PlotInfLines("##CalibrationAppliedTimeByRelPose", &x, 1, defaultSpec);
        } else {
            ImPlot::PlotInfLines("##CalibrationAppliedTimeByRelPose", g_PlotUiState.calByRelPoseTimeBuffer.data(), static_cast<int>(g_PlotUiState.calByRelPoseTimeBuffer.size()), defaultSpec);
        }

        ImPlotSpec tagSpec;
        tagSpec.LineColor = ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
        ImPlot::PlotInfLines("##TagLine", &g_PlotUiState.lastMouseX, 1, tagSpec);

        if (ImPlot::IsPlotHovered()) {
            g_PlotUiState.lastMouseX = ImPlot::GetPlotMousePos().x;
            g_PlotUiState.wasHoveredThisFrame = true;
        }
    }

    void drawGraphSeriesChain(const GraphConfig* cfg, const char* pMetricsAddress, const CalibrationErrorMetrics& metrics, int count) {
        ImPlotSpec spec;

        static thread_local std::vector<TimeSeries<double>::TimeSeriesSample> scalarScratch;
        static thread_local std::vector<TimeSeries<Eigen::Vector3d>::TimeSeriesSample> vec3Scratch;

        while (cfg != nullptr) {
            if (cfg->type == SeriesType::CustomFunc && cfg->customRenderFunc != nullptr) {
                cfg->customRenderFunc(metrics, count, spec);
            }
            else if (cfg->type == SeriesType::Scalar) {
                const auto& ts = *reinterpret_cast<const TimeSeries<double>*>(pMetricsAddress + cfg->offset);
                if (ts.size() > 0) {
                    scalarScratch.clear();
                    scalarScratch.reserve(ts.size());
                    for (int i = 0; i < ts.size(); i++) {
                        scalarScratch.push_back({ ts[i].time - g_PlotUiState.referenceTime, ts[i].value });
                    }
                    spec.Stride = static_cast<int>(sizeof(scalarScratch[0]));
                    ImPlot::PlotLine(cfg->menuName, &scalarScratch[0].time, &scalarScratch[0].value, static_cast<int>(scalarScratch.size()), spec);
                }
            }
            else if (cfg->type == SeriesType::Vector3d) {
                const auto& ts = *reinterpret_cast<const TimeSeries<Eigen::Vector3d>*>(pMetricsAddress + cfg->offset);
                if (ts.size() > 0) {
                    vec3Scratch.clear();
                    vec3Scratch.reserve(ts.size());
                    for (int i = 0; i < ts.size(); i++) {
                        vec3Scratch.push_back({ ts[i].time - g_PlotUiState.referenceTime, ts[i].value });
                    }
                    spec.Stride = static_cast<int>(sizeof(vec3Scratch[0]));
                    std::string labelX = fmt::format("{}.X", cfg->menuName);
                    std::string labelY = fmt::format("{}.Y", cfg->menuName);
                    std::string labelZ = fmt::format("{}.Z", cfg->menuName);

                    ImPlot::PlotLine(labelX.c_str(), &vec3Scratch[0].time, &vec3Scratch[0].value.x(), static_cast<int>(vec3Scratch.size()), spec);
                    ImPlot::PlotLine(labelY.c_str(), &vec3Scratch[0].time, &vec3Scratch[0].value.y(), static_cast<int>(vec3Scratch.size()), spec);
                    ImPlot::PlotLine(labelZ.c_str(), &vec3Scratch[0].time, &vec3Scratch[0].value.z(), static_cast<int>(vec3Scratch.size()), spec);

                }
            }
            cfg = cfg->pNext;
        }
    }

    void drawComposedPlot(double timeSpan, const GraphConfig* rootCfg, const CalibrationErrorMetrics& metrics) {
        if (ImPlot::BeginPlot(rootCfg->menuId, ImVec2(-1, 0), ImPlotFlags_None)) {
            ImPlot::SetupAxes(nullptr, rootCfg->szAxisUnits, 0, rootCfg->yFlags);
            ImPlot::SetupAxisLimits(ImAxis_X1, -timeSpan * rootCfg->xScale, 0, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, rootCfg->yLimit, ImGuiCond_Appearing);

            addApplyTicks();

            const char* pMetricsAddress = reinterpret_cast<const char*>(&metrics);
            int maxCount = metrics.axisIndependence.size();

            drawGraphSeriesChain(rootCfg, pMetricsAddress, metrics, maxCount);

            ImPlot::EndPlot();
        }
    }

    void drawAxisVariancePlot(const CalibrationErrorMetrics& metrics, int render_sample_count, const ImPlotSpec& baseSpec) {
        if (!g_PlotUiState.colormapInitialized) {
            g_PlotUiState.colormapInitialized = true;
            ImVec4 colors[] = { ImPlot::GetColormapColor(0), ImPlot::GetColormapColor(1), {1,0,0,1}, {0,1,0,1}, {0.5,0.5,0.5,1} };
            g_PlotUiState.axisVarianceColormap = ImPlot::AddColormap("AxisVarianceColormap", colors, std::size(colors));
        }

        const auto& ts = metrics.axisIndependence;
        if (ts.size() <= 0) return;

        static thread_local std::vector<TimeSeries<double>::TimeSeriesSample> scratch;
        scratch.clear();
        scratch.reserve(ts.size());
        for (int i = 0; i < ts.size(); i++) {
            scratch.push_back({ ts[i].time - g_PlotUiState.referenceTime, ts[i].value });
        }

        ImPlot::PushColormap(g_PlotUiState.axisVarianceColormap);
        const auto& rawData = ts.data();
        int stride = (int) (sizeof(scratch[0]));

        std::vector<double> threshLine(render_sample_count, k_MAX_AXIS_VARIANCE_THRESHOLD);
        std::vector<double> zeroLine(render_sample_count, 0.0);

        ImPlotSpec lowSpec = baseSpec;
        lowSpec.LineColor = ImVec4(1, 0, 0, 1); 
        lowSpec.FillAlpha = 0.5f;
        lowSpec.Stride = stride;
        
        ImPlotSpec highSpec = baseSpec;
        highSpec.LineColor = ImVec4(0, 1, 0, 1);
        highSpec.FillAlpha = 0.5f;
        highSpec.Stride = stride;

        ImPlot::PlotShaded("##VarianceLow", &scratch[0].time, &scratch[0].value, zeroLine.data(), (int) zeroLine.size(), lowSpec);
        ImPlot::PlotShaded("##VarianceHigh", &scratch[0].time, &scratch[0].value, threshLine.data(), (int) threshLine.size(), highSpec);
        ImPlot::PopColormap(1);
    }

    // these are defomed outside the array for the linked list to work

    // axis variance needs custom func for the draw line
    static const GraphConfig g_varLineNode    { "Datapoint",      "##Axis variance", nullptr, SeriesType::Scalar,     offsetof(CalibrationErrorMetrics, axisIndependence),    nullptr,              0.003, 1.0, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit,   /* .pNext = */ nullptr           };
    static const GraphConfig g_varianceRoot   { "Axis Variance",  "##Axis variance", nullptr, SeriesType::CustomFunc, 0,                                                      drawAxisVariancePlot, 0.003, 1.0, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit,   /* .pNext = */ &g_varLineNode    };

    // temp for seeing how bad latency really is
    static const GraphConfig g_referencePoseVelocity    { "Reference Velocity", "##hmdVelocity", "m/s", SeriesType::Scalar,       offsetof(CalibrationErrorMetrics, pose_ref_velocity_mag),   nullptr,    0.003, 0.05, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit,   /* .pNext = */ nullptr };
    static const GraphConfig g_targetPoseVelocity       { "Target Velocity",    "##targetVelocity", "m/s", SeriesType::Scalar,    offsetof(CalibrationErrorMetrics, pose_tgt_velocity_mag),   nullptr,    0.003, 0.05, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit,   /* .pNext = */ &g_referencePoseVelocity };

    const GraphConfig g_graph_data[] = {
        g_varianceRoot,
        g_targetPoseVelocity,
        { "Offset: Raw Computed",           "##posOffsetRawComputed",   "mm",   SeriesType::Vector3d,   offsetof(CalibrationErrorMetrics, posOffset_rawComputed),   nullptr,    200.0 },
        { "Offset: Current Calibration",    "##posOffsetCurrentCal",    "mm",   SeriesType::Vector3d,   offsetof(CalibrationErrorMetrics, posOffset_currentCal),    nullptr,    200.0 },
        { "Offset: RMS error",              "##posOffsetRMSError",      "mm",   SeriesType::Vector3d,   offsetof(CalibrationErrorMetrics, posOffset_rmsError),      nullptr,    200.0 },
        { "Retargeting RMS Error",          "##rmsError",               "ms",   SeriesType::Scalar,     offsetof(CalibrationErrorMetrics, rmsError),                nullptr,    200.0 },
        { "Processing time",                "##Computation Time",       "ms",   SeriesType::Scalar,     offsetof(CalibrationErrorMetrics, computationTime),         nullptr,    200.0 },
    };

    constexpr size_t k_NUM_GRAPHS = ARRAY_SIZE(g_graph_data);

    void draw_error_graphs(const std::string& targetDeviceName, double timeSpan, double currentTimestamp, const CalibrationErrorMetrics& metrics) {
        constexpr int k_ROWS = 2;
        constexpr int k_COLS = (k_NUM_GRAPHS + k_ROWS - 1) / k_ROWS;
        constexpr int k_TOTAL_PLOTS = k_ROWS * k_COLS;

        if (g_PlotUiState.currentGraphIndexes.size() < k_TOTAL_PLOTS) {
            while (g_PlotUiState.currentGraphIndexes.size() < k_TOTAL_PLOTS) {
                g_PlotUiState.currentGraphIndexes.push_back(static_cast<int>(g_PlotUiState.currentGraphIndexes.size()) % (int)k_NUM_GRAPHS);
            }
        }

        g_PlotUiState.wasHoveredThisFrame = false;
        auto avail = ImGui::GetContentRegionAvail();
        auto bgCol = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);

        ImGui::PushStyleColor(ImGuiCol_TableRowBg, bgCol);
        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, bgCol);
        ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4(0, 0, 0, 0));

        std::string childId = fmt::format("##MetricsPanel_{}", targetDeviceName);
        if (!ImGui::BeginChild(childId.c_str(), avail, false, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing)) {
            ImGui::EndChild();
            ImPlot::PopStyleColor(1);
            ImGui::PopStyleColor(2);
            return;
        }

        std::string tableId = fmt::format("##MetricsTable_{}", targetDeviceName);
        if (!ImGui::BeginTable(tableId.c_str(), k_COLS, ImGuiTableFlags_RowBg)) {
            ImGui::EndChild();
            ImPlot::PopStyleColor(1);
            ImGui::PopStyleColor(2);
            return;
        }

        g_PlotUiState.referenceTime = currentTimestamp;
        
        g_PlotUiState.calAppliedTimeBuffer.clear();
        g_PlotUiState.calByRelPoseTimeBuffer.clear();
        for (const auto& sample : metrics.calibrationApplied.data()) {
            if (sample.value) g_PlotUiState.calAppliedTimeBuffer.push_back(sample.time - g_PlotUiState.referenceTime);
            else              g_PlotUiState.calByRelPoseTimeBuffer.push_back(sample.time - g_PlotUiState.referenceTime);
        }

        const char* pMetricsAddress = reinterpret_cast<const char*>(&metrics);

        for (int r = 0; r < k_ROWS; r++) {
            ImGui::TableNextRow();
            for (int c = 0; c < k_COLS; c++) {
                int idx = r * k_COLS + c;
                if (idx >= k_NUM_GRAPHS) {
                    continue;
                }

                ImGui::TableSetColumnIndex(c);
                ImGui::PushID(idx);

                ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
                if (ImGui::BeginCombo("", g_graph_data[g_PlotUiState.currentGraphIndexes[idx]].menuName, 0)) {
                    for (int j = 0; j < k_NUM_GRAPHS; j++) {
                        bool isSelected = (j == g_PlotUiState.currentGraphIndexes[idx]);
                        if (ImGui::Selectable(g_graph_data[j].menuName, isSelected)) {
                            g_PlotUiState.currentGraphIndexes[idx] = j;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                const auto& rootCfg = g_graph_data[g_PlotUiState.currentGraphIndexes[idx]];
                drawComposedPlot(timeSpan, &rootCfg, metrics);

                ImGui::PopID();
            }
        }
        
        ImGui::EndTable();
        ImGui::EndChild();

        ImPlot::PopStyleColor(1);
        ImGui::PopStyleColor(2);

        if (!g_PlotUiState.wasHoveredThisFrame) {
            g_PlotUiState.lastMouseX = -HUGE_VAL;
        }
    }

    void page_calibration(double currentTime) {
        ImGui::TextTitle("%s", LOCALE_GET("calibration_title").c_str());
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

                buildDeviceSelection(calibration);
                if (!calibration.isContinuousCalibration()) {
                    if (calibration.state == CalibrationState::EDITING) {
                        // BuildProfileEditor();
                        // 
                        // if (ImGui::Button("Save Profile", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 2)))
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
    
    void page_graphs(double currentTime) {
        ImGui::TextTitle("%s", LOCALE_GET("graphs_title").c_str());

        // @TODO: numbering or smth
        spacecal::TrackingSystemCalibration& calibration = spacecal::CalibrationManager::getInstance()->getCalibration(0);
        draw_error_graphs(calibration.referenceDevice.trackingSystem, k_METRIC_HISTORY_TIMESPAN, currentTime, calibration.errorMetrics);
    }

    void page_settings(double currentTime) {
        ImGui::TextTitle("%s", LOCALE_GET("settings_title").c_str());

        // advanced settings
        if (ImGui::CheckboxWithDescription(LOCALE_GET("settings_advanced").c_str(), &g_state.bIsSettingsAdvanced, LOCALE_GET("settings_advanced_description").c_str())) {
            ConfigurationManager::getInstance()->getConfiguration()->advanced_settings = g_state.bIsSettingsAdvanced;
            ConfigurationManager::getInstance()->saveConfiguration();
        }

        buildLocaleSelector();

        // base station power management
        ImGui::BeginCard("base_station_power_management");
        {
            ImGui::TextHeading(LOCALE_GET("base_station_power_management").c_str());

            const char* bsPowerEnableDescKey = g_state.bBaseStationPowerManagementOffModeIsSleep
                ? "base_stations_power_mgmt_enable_description_sleep"
                : "base_stations_power_mgmt_enable_description_standby";

            if (ImGui::CheckboxWithDescription(LOCALE_GET("base_stations_power_mgmt_enable").c_str(), &g_state.bBaseStationPowerManagementEnabled, LOCALE_GET(bsPowerEnableDescKey).c_str())) {
                ConfigurationManager::getInstance()->getConfiguration()->base_stations.auto_power_management_enabled = g_state.bBaseStationPowerManagementEnabled;
                ConfigurationManager::getInstance()->saveConfiguration();
            }

            if (ImGui::CheckboxWithDescription(LOCALE_GET("base_stations_power_mgmt_on_startup").c_str(), &g_state.bBaseStationPowerManagementOnStartup, LOCALE_GET("base_stations_power_mgmt_on_startup_description").c_str())) {
                ConfigurationManager::getInstance()->getConfiguration()->base_stations.auto_turn_on_during_startup = g_state.bBaseStationPowerManagementOnStartup;
                ConfigurationManager::getInstance()->saveConfiguration();
            }

            if (ImGui::CheckboxWithDescription(LOCALE_GET("base_stations_power_mgmt_on_shutdown").c_str(), &g_state.bBaseStationPowerManagementOnShutdown, LOCALE_GET("base_stations_power_mgmt_on_shutdown_description").c_str())) {
                ConfigurationManager::getInstance()->getConfiguration()->base_stations.auto_turn_off_during_shutdown = g_state.bBaseStationPowerManagementOnShutdown;
                ConfigurationManager::getInstance()->saveConfiguration();
            }

            if (ImGui::RadioButtonWithDescription(LOCALE_GET("base_stations_power_mgmt_standby").c_str(), !g_state.bBaseStationPowerManagementOffModeIsSleep, LOCALE_GET("base_stations_power_mgmt_standby_description").c_str())) {
                ConfigurationManager::getInstance()->getConfiguration()->base_stations.off_should_use_standby = true;
                ConfigurationManager::getInstance()->saveConfiguration();
            }

            if (ImGui::RadioButtonWithDescription(LOCALE_GET("base_stations_power_mgmt_sleep").c_str(), g_state.bBaseStationPowerManagementOffModeIsSleep, LOCALE_GET("base_stations_power_mgmt_sleep_description").c_str())) {
                ConfigurationManager::getInstance()->getConfiguration()->base_stations.off_should_use_standby = false;
                ConfigurationManager::getInstance()->saveConfiguration();
            }

            ImGui::TextWrapped(LOCALE_GET("base_stations_power_mgmt_warning").c_str());
        }
        ImGui::EndCard();
    }

    void page_base_station_management(double currentTime) {
        ImGui::TextTitle("%s", LOCALE_GET("base_stations_title").c_str());

        if (!bluetooth::is_bluetooth_available()) {
            ImGui::TextDisabled(LOCALE_GET("base_stations_no_bluetooth").c_str());
            return;
        }

        // bt avail
        size_t dwBaseStationCount = bluetooth::get_base_station_count();

        if (dwBaseStationCount > 0) {
            if (ImGui::Button(LOCALE_GET("base_stations_action_wake_all").c_str())) {
                bluetooth::set_all_base_station_power_state(bluetooth::PowerState_Awake_From_Standby);
            }
            ImGui::SameLine();
            if (ImGui::Button(LOCALE_GET("base_stations_action_sleep_all").c_str())) {
                bluetooth::set_all_base_station_power_state(bluetooth::PowerState_Sleep);
            }
            ImGui::SameLine();
            if (ImGui::Button(LOCALE_GET("base_stations_action_fix_collisions").c_str())) {
                bluetooth::auto_assign_base_station_channels();
            }

            if (bluetooth::do_base_station_channels_collide()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCALE_GET("base_stations_warning_collision").c_str());
            }
            ImGui::Separator();
        }

        if (dwBaseStationCount < 1) {
            ImGui::TextDisabled(LOCALE_GET("base_stations_none_found").c_str());
        }
        else {
            for (size_t i = 0; i < dwBaseStationCount; i++) {
                auto base_station = bluetooth::get_base_station(i);

                ImGui::PushID(static_cast<int>(i));
                ImGui::BeginGroup();

                const char* typeStr = (base_station.eType == bluetooth::BaseStationType_10) ? "1.0" : "2.0";
                std::string szBaseStationName = base_station.szSerialNumber;
                std::string headerText = LOCALE_FORMAT("base_stations_header_info", typeStr, szBaseStationName);
                ImGui::Text("%s", headerText.c_str());

                ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
                switch (base_station.powerState) {
                case bluetooth::PowerState_Sleep:
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", LOCALE_GET("base_stations_state_sleep").c_str());
                    break;
                case bluetooth::PowerState_Standby:
                    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.1f, 1.0f), "%s", LOCALE_GET("base_stations_state_standby").c_str());
                    break;
                case bluetooth::PowerState_Awake_From_Sleep:
                case bluetooth::PowerState_Awake_From_Standby:
                case bluetooth::PowerState_Awake_TooOldFirmware:
                    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "%s", LOCALE_GET("base_stations_state_active").c_str());
                    break;
                default:
                    ImGui::TextDisabled("%s", LOCALE_GET("base_stations_state_unknown").c_str());
                    break;
                }

                ImGui::Spacing();

                if (base_station.eType == bluetooth::BaseStationType_20) {
                    ImGui::Text("%s", LOCALE_GET("base_stations_channel_select_label").c_str());

                    for (int row = 0; row < 2; ++row) {
                        for (int col = 0; col < 8; ++col) {
                            uint8_t targetChannel = static_cast<uint8_t>((row * 8) + col + 1);

                            std::string channelBtnLabel = fmt::format("{}", targetChannel);

                            bool isCurrent = (base_station.channel == targetChannel);
                            if (isCurrent) {
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                            }
                            else {
                                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_Button]);
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
                            }

                            if (ImGui::Button(channelBtnLabel.c_str(), ImVec2(25.0f, 25.0f))) {
                                bluetooth::set_base_station_channel(i, targetChannel);
                            }

                            ImGui::PopStyleColor(2);

                            if (col < 7) {
                                ImGui::SameLine();
                            }
                        }
                    }
                    ImGui::Spacing();
                }

                if (ImGui::Button(LOCALE_GET("base_stations_btn_wake").c_str())) {
                    bluetooth::set_base_station_power_state(i, bluetooth::PowerState_Awake_From_Standby);
                }
                ImGui::SameLine();
                if (ImGui::Button(LOCALE_GET("base_stations_btn_standby").c_str())) {
                    bluetooth::set_base_station_power_state(i, bluetooth::PowerState_Standby);
                }
                ImGui::SameLine();
                if (ImGui::Button(LOCALE_GET("base_stations_btn_sleep").c_str())) {
                    bluetooth::set_base_station_power_state(i, bluetooth::PowerState_Sleep);
                }

                ImGui::EndGroup();

                if (i < dwBaseStationCount - 1) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                ImGui::PopID();
            }
        }
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
    
    void page_debug(double currentTime) {
        ImGui::TextTitle("%s", LOCALE_GET("tab_page_debug").c_str());

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

    void page_about(double currentTime) {
        ImGui::TextTitle("%s", LOCALE_GET("about_title").c_str());

        ImGui::Text(LOCALE_GET("about_description").c_str());
        ImGui::Text(LOCALE_GET("about_view_source_info").c_str());

        ImGui::TextHeading(LOCALE_GET("about_contributors_title").c_str());
        ImGui::Text(LOCALE_GET("about_contributors_description").c_str());
        ImGui::Text("pushrax");
        ImGui::Text("bd_");
        ImGui::Text("ArticFox");
        ImGui::Text("hekky");
        ImGui::Text("pimaker");

        if (ImGui::Button(LOCALE_GET("about_link_github").c_str())) {
            platform::launchWebpage("https://github.com/hyblocker/OpenVR-SpaceCalibrator");
        }
        
        if (ImGui::Button(LOCALE_GET("about_link_discord").c_str())) {
            platform::launchWebpage("https://discord.gg/YWN7Z9T8DP");
        }
        
        if (ImGui::Button(LOCALE_GET("about_link_logs_dir").c_str())) {
            platform::launchDirInFileBrowser(util::getSpaceCalibratorLogsDir());
        }

        // ImGui::InputTextMultiline();

        ImGui::TextHeading(LOCALE_GET("about_licenses_title").c_str());
        ImGui::Text(LOCALE_GET("about_licenses_description").c_str());
        ImGui::TextDisabled("LICENSES HERE");
    }

    // UI CORE LAYOUT

    SpaceCalibratorVerticalTab_t g_spaceCalUiTabs[] = {
        { .szLocaleKey = "tab_page_calibration", .szIcon = ICON_MS_TARGET, .fnDrawTab = page_calibration, },
        { .szLocaleKey = "tab_page_graphs", .szIcon = ICON_MS_STACKED_LINE_CHART, .fnDrawTab = page_graphs, },
        { .szLocaleKey = "tab_page_base_station_management", .szIcon = ICON_MS_SENSORS, .fnDrawTab = page_base_station_management, }, // ICON_MS_CELL_TOWER
        { .szLocaleKey = "tab_page_settings", .szIcon = ICON_MS_SETTINGS, .fnDrawTab = page_settings, },
#if _DEBUG || 1 // @TODO: reserved exclusively for debug mode, may make sense to enable with a flag?
        { .szLocaleKey = "tab_page_debug", .szIcon = ICON_MS_TERMINAL, .fnDrawTab = page_debug, },
#endif
        { .szLocaleKey = "tab_page_about", .szIcon = ICON_MS_INFO, .fnDrawTab = page_about, },
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
        constexpr float k_IconTextGap = 8.0f;
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
            
            ImVec2 startPos = bb.Min;
            if (selected) {
                startPos.x += k_IndicatorWidth;
            }
            window->DrawList->AddRectFilledMultiColor(startPos, bb.Max, col_bg_max, col_bg_min, col_bg_min, col_bg_max);
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

        float final_text_offset_x = k_LeftTextMargin;

        // icon rendering
        ImVec2 iconSize = ImGui::CalcTextSize(tabData.szIcon, nullptr, true);
        float icon_y_offset = (size.y - iconSize.y) * 0.5f;
        ImVec2 icon_pos = ImVec2(bb.Min.x + final_text_offset_x, bb.Min.y + icon_y_offset);
        ImGui::RenderText(icon_pos, tabData.szIcon);
        final_text_offset_x += iconSize.x + k_IconTextGap;

        // text rendering
        std::string textStr = LOCALE_GET(tabData.szLocaleKey);
        ImVec2 textSize = ImGui::CalcTextSize(textStr.c_str(), nullptr, true);
        float text_y_offset = (size.y - textSize.y) * 0.5f;
        ImVec2 text_pos = ImVec2(bb.Min.x + final_text_offset_x, bb.Min.y + text_y_offset);
        ImGui::RenderText(text_pos, textStr.c_str());

        return pressed;
    }

    inline void drawMainView(double currentTime) {
        // @TODO: temp hardcode, move to header or some ui_config.h idk
        const float k_SIDEBAR_WIDTH = 180.0f;
        const float k_SIDEBAR_TAB_HEIGHT = 48.0f;
        const float k_CONTENT_AREA_PADDING = 5.0f;

        // sidebar
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
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
        ImGui::PopStyleVar(2);

        ImGui::SameLine();

        // content
        ImGui::BeginChild("ContentArea", ImVec2(0, ImGui::GetFrameHeightWithSpacing() * -2.0f), ImGuiChildFlags_None);

        ImGui::Dummy(ImVec2(0, k_CONTENT_AREA_PADDING)); 
        ImGui::Indent(k_CONTENT_AREA_PADDING);

        if (g_state.dwSelectedUiPage < k_SIZE_SPACECAL_UI_TABS) {
            g_spaceCalUiTabs[g_state.dwSelectedUiPage].fnDrawTab(currentTime);
        }

        ImGui::Unindent(k_CONTENT_AREA_PADDING);
        ImGui::EndChild();
    }

    // UI ENTRY POINT

    void drawInterface(bool isOverlay, double currentTime) {
        g_state.bIsRunningInOverlay = isOverlay;
        g_state.bIsSettingsAdvanced = ConfigurationManager::getInstance()->getConfiguration()->advanced_settings;
        g_state.bBaseStationPowerManagementEnabled = ConfigurationManager::getInstance()->getConfiguration()->base_stations.auto_power_management_enabled;
        g_state.bBaseStationPowerManagementOnStartup = ConfigurationManager::getInstance()->getConfiguration()->base_stations.auto_turn_on_during_startup;
        g_state.bBaseStationPowerManagementOnShutdown = ConfigurationManager::getInstance()->getConfiguration()->base_stations.auto_turn_off_during_shutdown;
        g_state.bBaseStationPowerManagementOffModeIsSleep = !ConfigurationManager::getInstance()->getConfiguration()->base_stations.off_should_use_standby;
        auto& io = ImGui::GetIO();

        // disable ctrl + tab, pointless in a VR overlay https://github.com/ocornut/imgui/issues/7987
#if !defined(IMGUI_USE_DEBUG_WINDOW)
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Tab, ImGuiInputFlags_RouteGlobal);
#endif // IMGUI_USE_DEBUG_WINDOW

        constexpr ImGuiWindowFlags k_bareWindowFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoCollapse
#if !defined(IMGUI_USE_DEBUG_WINDOW)
            | ImGuiWindowFlags_NoNavFocus
#endif // IMGUI_USE_DEBUG_WINDOW
            ;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

        ImGui::Begin("Space Calibrator", nullptr, k_bareWindowFlags);

        drawMainView(currentTime);

        buildVersionInfo();
        ImGui::End();

#if defined(IMGUI_USE_DEBUG_WINDOW)
        ImGui::ShowDemoWindow();
#endif // IMGUI_USE_DEBUG_WINDOW
    }
}
#include "vr_core.h"
#include "util.h"
#include "log.h"
#include "constants.h"
#include "platform.h"
#include <fmt/format.h>

namespace spacecal {

    // @FIXME: Should this be behind an advanced or obscure toggle?
    // these tracking systems are explicitly hidden in Space Calibrator, as it does not make sense for Space Calibrator to "calibrate" such trackers
    constexpr const char* k_IGNORED_TRACKING_SYSTEMS[] = {
        "null", // only actual hardware is supported, null driver is a debug device and will not be accepted with space calibrator
        "standable", // virtual trackers will likely interfere with space calibrator
    };

    VRState* VRState::s_instance = nullptr;

    bool VRState::init() {
        if (s_instance != nullptr) {
            LOG_FATAL("Tried creating VRState more than once! Breaking singleton. Aborting...");
            return false;
        }
        s_instance = this;
        m_bIsSteamVrAvailable = false;
        m_aTrackingSystems.reserve(8); // conservative amount, should account for 99.9% of cases with ease

        m_eVrInitError = vr::VRInitError_None;
        vr::VR_Init(&m_eVrInitError, vr::VRApplication_Overlay);
        if (m_eVrInitError != vr::VRInitError_None) {
            auto szError = vr::VR_GetVRInitErrorAsEnglishDescription(m_eVrInitError);
            LOG_OPENVR_CRITICAL("vr::VR_Init failed, got {}", szError);
            platform::showMessageDialog(
                fmt::format("Error initialising SteamVR: {}", szError),
                "An error occured initialising Space Calibrator Nova"
            );
            return false;
        }

        // ensure the interfaces we use are valid and correct
        if (!vr::VR_IsInterfaceVersionValid(vr::IVRSystem_Version)) {
            LOG_OPENVR_CRITICAL("OpenVR interface vr::IVRSystem version is invalid! Aborting...");
            return false;
        }
        else if (!vr::VR_IsInterfaceVersionValid(vr::IVRSettings_Version)) {
            LOG_OPENVR_CRITICAL("OpenVR interface vr::IVRSettings version is invalid! Aborting...");
            return false;
        }
        else if (!vr::VR_IsInterfaceVersionValid(vr::IVROverlay_Version)) {
            LOG_OPENVR_CRITICAL("OpenVR interface vr::IVROverlay version is invalid! Aborting...");
            return false;
        }

        // create overlay
        if (!vr::VROverlay() || m_overlayMainHandle) {
            return false;
        }

        vr::VROverlayError error = vr::VROverlay()->CreateDashboardOverlay(
            c_OPENVR_APPLICATION_KEY, "Space Calibrator",
            &m_overlayMainHandle, &m_overlayThumbnailHandle
        );

        if (error == vr::VROverlayError_KeyInUse) {
            LOG_OPENVR_CRITICAL("Another instance of Space Calibrator is already running");
            platform::showMessageDialog("Another instance of Space Calibrator is already running", "An error occured initialising Space Calibrator Nova");
            return false;
        } else if (error != vr::VROverlayError_None) {
            LOG_OPENVR_CRITICAL("Error creating VR overlay: {}", vr::VROverlay()->GetOverlayErrorNameFromEnum(error));
            platform::showMessageDialog(
                fmt::format("Error creating VR overlay: {}", vr::VROverlay()->GetOverlayErrorNameFromEnum(error)),
                "An error occured initialising Space Calibrator Nova"
            );
            return false;
        }

        vr::VROverlay()->SetOverlayWidthInMeters(m_overlayMainHandle, 3.0f);
        vr::VROverlay()->SetOverlayInputMethod(m_overlayMainHandle, vr::VROverlayInputMethod_Mouse);
        vr::VROverlay()->SetOverlayFlag(m_overlayMainHandle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);

        std::string iconPath = fmt::format("{}/icon.png", util::getSpaceCalibratorInstallDir().string());
        vr::VROverlay()->SetOverlayFromFile(m_overlayThumbnailHandle, iconPath.c_str());

        // @TODO: Non-steam stuff

        m_bIsSteamVrAvailable = true;
        return true;
    }

    vr::ETrackedPropertyError VRState::getSteamVrPropString(const vr::TrackedDeviceIndex_t deviceId, vr::ETrackedDeviceProperty deviceProperty, std::string& string) const {
        char buffer[vr::k_unMaxPropertyStringSize] = {};
        vr::ETrackedPropertyError err = vr::TrackedProp_Success;
        vr::VRSystem()->GetStringTrackedDeviceProperty(deviceId, deviceProperty, buffer, vr::k_unMaxPropertyStringSize, &err);
        if (err == vr::TrackedProp_Success) {
            string = std::string(buffer);
        }
        return err;
    }

    void VRState::updateSteamVRDevice(const vr::TrackedDeviceIndex_t deviceId) {
        vr::ETrackedDeviceClass deviceClass = vr::VRSystem()->GetTrackedDeviceClass(deviceId);

        // we dont care about these devices types
        if (deviceClass == vr::TrackedDeviceClass_Invalid // Unset
            || deviceClass == vr::TrackedDeviceClass_TrackingReference // Base Stations
            || deviceClass == vr::TrackedDeviceClass_DisplayRedirect) // vr::IVRVirtualDisplay
            return;

        vr::ETrackedPropertyError err = vr::TrackedProp_Success;
        std::string szTrackingSystem;
        err = getSteamVrPropString(deviceId, vr::Prop_TrackingSystemName_String, szTrackingSystem);

        // only log if its failed and not unset / not avail rn
        if (err != vr::TrackedProp_Success) {
            if (err != vr::TrackedProp_UnknownProperty &&
                err != vr::TrackedProp_NotYetAvailable) {
                LOG_OPENVR_WARN("Failed to get tracking system string for device with id {}, got error {} ({})", deviceId, err, (uint32_t)err);
            }
            return;
        }

        // if the tracking system should be ignored, ignore it
        for (size_t i = 0; i < std::size(k_IGNORED_TRACKING_SYSTEMS); i++) {
            if (strcmp(szTrackingSystem.c_str(), k_IGNORED_TRACKING_SYSTEMS[i]) == 0) {
                return;
            }
        }

        // QUIRKS! ( a bunch of hardware lies about what it is :c )
        {
            // Check if the current HMD is a Pimax crystal
            if (deviceClass == vr::TrackedDeviceClass_HMD && szTrackingSystem == "aapvr") {
                // HMD is a Pimax HMD
                vr::HmdMatrix34_t eyeToHeadLeft = vr::VRSystem()->GetEyeToHeadTransform(vr::Eye_Left);
                // Crystal's projection matrix is constant 0s or 1s except for [0][3], which stores the IPD offset from the nose
                bool isCrystalHmd =
                    eyeToHeadLeft.m[0][0] == 1 && eyeToHeadLeft.m[0][1] == 0 && eyeToHeadLeft.m[0][2] == 0 &&                     // IPD
                    eyeToHeadLeft.m[1][0] == 0 && eyeToHeadLeft.m[1][1] == 1 && eyeToHeadLeft.m[1][2] == 0 && eyeToHeadLeft.m[1][3] == 0 &&
                    eyeToHeadLeft.m[2][0] == 0 && eyeToHeadLeft.m[2][1] == 0 && eyeToHeadLeft.m[2][2] == 1 && eyeToHeadLeft.m[2][3] == 0;

                if (isCrystalHmd) {
                    // Move it outside the aapvr system ; we treat aapvr as if it were lighthouse
                    szTrackingSystem = "Pimax Crystal HMD";
                }
            } else if (deviceClass == vr::TrackedDeviceClass_Controller && szTrackingSystem == "oculus") {
                std::string renderModel;
                std::string connectedWirelessDongle;
                err = getSteamVrPropString(deviceId, vr::Prop_RenderModelName_String, renderModel);
                err = getSteamVrPropString(deviceId, vr::Prop_ConnectedWirelessDongle_String, connectedWirelessDongle);

                // Check if the controller claims its an oculus controller but also pimax
                if (renderModel.find("{aapvr}") != std::string::npos &&
                    renderModel.find("crystal") != std::string::npos &&
                    connectedWirelessDongle.find("lighthouse") != std::string::npos) {
                    szTrackingSystem = "Pimax Crystal Controllers";
                }
            } else if (deviceClass == vr::TrackedDeviceClass_HMD && szTrackingSystem == "oculus") {
                // Possibly Virtual Desktop on a non Meta HMD
                // @TODO: figure out how to determine what the HMD ACTUALLY is in such a scenario
            }
        }

        // track new tracking systems, prioritise HMD one at front of list
        {
            auto existing = std::find(m_aTrackingSystems.begin(), m_aTrackingSystems.end(), szTrackingSystem);
            if (existing != m_aTrackingSystems.end()) {
                if (deviceClass == vr::TrackedDeviceClass_HMD) {
                    m_aTrackingSystems.erase(existing);
                    m_aTrackingSystems.insert(m_aTrackingSystems.begin(), szTrackingSystem);
                }
            } else {
                m_aTrackingSystems.push_back(szTrackingSystem);
            }
        }

        // update tracking state
        std::string szDeviceModel;
        std::string szDeviceSerial;
        err = getSteamVrPropString(deviceId, vr::Prop_ModelNumber_String, szDeviceModel);
        err = getSteamVrPropString(deviceId, vr::Prop_SerialNumber_String, szDeviceSerial);
        vr::ETrackedControllerRole controllerRole = (vr::ETrackedControllerRole)vr::VRSystem()->GetInt32TrackedDeviceProperty(deviceId, vr::Prop_ControllerRoleHint_Int32, &err);
        bool isConnected = vr::VRSystem()->IsTrackedDeviceConnected(deviceId);

        m_aDevices[deviceId] = {
            .bIsConnected = isConnected,
            .dwDeviceIndex = deviceId,
            .eControllerRole = controllerRole,
            .eDeviceClass = deviceClass,
            .szTrackingSystemId = szTrackingSystem,
            .szModel = szDeviceModel,
            .szSerial = szDeviceSerial,
        };
    }

    void VRState::updateVrState() {

        if (!m_bIsSteamVrAvailable)
            return;

        if (m_aTrackingSystems.size() == 0 || m_bStateDirty) {
            // fresh poll, go through everything because we're in a fresh state
            for (vr::TrackedDeviceIndex_t id = 0; id < vr::k_unMaxTrackedDeviceCount; ++id) {
                if (vr::VRSystem()->IsTrackedDeviceConnected(id)) {
                    updateSteamVRDevice(id);
                } else {
                    m_aDevices[id].bIsConnected = false;
                }
            }

            m_bStateDirty = false;
        }

        vr::VREvent_t vrEvent = {};
        while (vr::VRSystem()->PollNextEvent(&vrEvent, sizeof(vrEvent))) {
            switch (vrEvent.eventType) {
                // @TODO: Handle these??
            case vr::EVREventType::VREvent_TrackedDeviceActivated:
            case vr::EVREventType::VREvent_TrackedDeviceDeactivated:
            case vr::EVREventType::VREvent_TrackedDeviceUpdated:
            case vr::EVREventType::VREvent_TrackedDeviceRoleChanged:
                updateSteamVRDevice(vrEvent.trackedDeviceIndex);
                break;

                // @TODO: inform the calibration algorithm about this state? need to test
            case vr::EVREventType::VREvent_EnterStandbyMode:
                break;
            case vr::EVREventType::VREvent_LeaveStandbyMode:
                break;
            }
        }
    }

    bool VRState::isHmdVirtualDesktop() const {
        // VD sets ResourceRoot as "virtualdesktop" on the HMD device
        std::string szResourceRoot;
        vr::ETrackedPropertyError err = getSteamVrPropString(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_ResourceRoot_String, szResourceRoot);
        if (err == vr::ETrackedPropertyError::TrackedProp_Success) {
            if (szResourceRoot == "virtualdesktop") {
                return true;
            }
        }

        return false;
    }

    const VRDevice_t VRState::findVrDevice(const std::string& trackingSystem, const std::string& model, const std::string& serial) const {

        // Find the device with the matching tracking system, model and serial
        for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
            const auto& device = m_aDevices[i];

            if (device.bIsConnected) {
                continue;
            }

            uint8_t matches = 0;

            if (device.szModel == model) {
                matches++;
            }
            if (device.szSerial == serial) {
                matches++;
            }

            // Only return if:
            //   - The tracking system is identical
            //   - If the device is not a HMD, the model and serial number ALSO are identical.
            //   - If the device is the HMD, the model OR serial number are also identical.
            // 
            // This handles an edge case of some device drivers being poorly developed or misbehaving, returning bad data to SteamVR and in turn, Space Calibrator
            // e.g.   SteamLink sometimes reports the Quest Pro as either "Oculus Quest Pro" or "Oculus Quest2" as it's model string
            //        It still reports the serial number correctly however as "VRLINKHMDQUESTPRO"!
            if (device.szTrackingSystemId == trackingSystem &&
                ((matches == 2 && device.eDeviceClass != vr::TrackedDeviceClass::TrackedDeviceClass_HMD) ||
                    (matches >= 1 && device.eDeviceClass == vr::TrackedDeviceClass::TrackedDeviceClass_HMD))) {
                return device;
            }
        }

        return {};
    }

    const VRDevice_t VRState::getVrDevice(const size_t index) const {
        if (0 <= index && index < vr::k_unMaxTrackedDeviceCount) {
            return m_aDevices[index];
        }
        ASSERT(0 <= index && index < vr::k_unMaxTrackedDeviceCount, "Invalid index, out of bounds read!");
        return {};
    }

    void VRState::identifyDevice(const vr::TrackedDeviceIndex_t deviceId) const {
        // @TODO: implement another method of identifying the selected device! not everything has LEDs or haptic motors, and viewing LEDs is challenging in VR!
        vr::VRSystem()->TriggerHapticPulse(deviceId, 0, 2000);
    }
}
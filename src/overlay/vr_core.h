#pragma once

#include <openvr.h>
#include <vector>
#include <string>

namespace spacecal {
    struct VRDevice_t {
        vr::TrackedDeviceIndex_t dwDeviceIndex = vr::k_unTrackedDeviceIndexInvalid;
        vr::ETrackedControllerRole eControllerRole = vr::ETrackedControllerRole::TrackedControllerRole_Invalid;
        vr::TrackedDeviceClass eDeviceClass = vr::TrackedDeviceClass::TrackedDeviceClass_Invalid;
        std::string szTrackingSystemId;
        std::string szModel;
        std::string szSerial;
    };

    // wraps SteamVR stuff, and keeps track of connected devices
    class VRState {
    public:
        VRState();

        bool init();

        // called every frame, updates m_aDevices and m_aTrackingSystems
        void updateVrState();
        
        // @TODO: Make trackingSystem of type OpenvrTrackingSystemHandle_t
        const VRDevice_t findVrDevice(const std::string& trackingSystem, const std::string& model, const std::string& serial) const;

        const VRDevice_t getVrDevice(const size_t index) const;
        [[nodiscard]] size_t getTrackingSystemCount() const { return m_aTrackingSystems.size(); }

    private:
        vr::ETrackedPropertyError getSteamVrPropString(const vr::TrackedDeviceIndex_t deviceId, vr::ETrackedDeviceProperty deviceProperty, std::string& string);
        void updateSteamVRDevice(const vr::TrackedDeviceIndex_t deviceId);

    private:
        bool m_bStateDirty = true;
        std::vector<VRDevice_t> m_aDevices;
        std::vector<std::string> m_aTrackingSystems;
    };
}
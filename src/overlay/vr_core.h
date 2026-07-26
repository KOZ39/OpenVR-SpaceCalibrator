#pragma once

#include <openvr.h>
#include <vector>
#include <string>
#include "protocol.h"
#include "ipc_client.h"

namespace spacecal {
    struct VRDevice_t {
        bool bIsConnected = false;
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
        bool init();

        // called every frame, updates m_aDevices and m_aTrackingSystems
        void updateVrState();
        void debugListDevices() const;
        const VRDevice_t findVrDevice(const std::string& trackingSystem, const std::string& model, const std::string& serial) const;

        const VRDevice_t getVrDevice(const size_t index) const;
        [[nodiscard]] size_t getTrackingSystemCount() const { return m_aTrackingSystems.size(); }
        [[nodiscard]] const std::string& getTrackingSystem(size_t index) const { return m_aTrackingSystems[index]; }
        [[nodiscard]] vr::VROverlayHandle_t getOverlayHandle() const { return m_overlayMainHandle; }
        [[nodiscard]] vr::VROverlayHandle_t getOverlayThumbnailHandle() const { return m_overlayThumbnailHandle; }
        [[nodiscard]] static inline VRState* getInstance() { return s_instance; }

        void identifyDevice(const vr::TrackedDeviceIndex_t deviceId) const;
        vr::ETrackedPropertyError getSteamVrPropString(const vr::TrackedDeviceIndex_t deviceId, vr::ETrackedDeviceProperty deviceProperty, std::string& string) const;
        [[nodiscard]] bool isHmdVirtualDesktop() const;
        [[nodiscard]] inline bool isSteamVrAvailable() const { return m_bIsSteamVrAvailable; }
        [[nodiscard]] inline vr::EVRInitError getVrInitError() const { return m_eVrInitError; }
        [[nodiscard]] inline ipc::protocol::SharedData_HmdMetadata getHmdMeta() const { return m_hmdMetadata; }
    private:
        bool updateSteamVRDevice(const vr::TrackedDeviceIndex_t deviceId);
        void invalidateAllSamples(); // private helper

    private:
        static VRState* s_instance;
        bool m_bIsSteamVrAvailable = true;
        bool m_bStateDirty = true;
        vr::EVRInitError m_eVrInitError = vr::EVRInitError::VRInitError_None;
        VRDevice_t m_aDevices[vr::k_unMaxTrackedDeviceCount] = {};
        vr::VROverlayHandle_t m_overlayMainHandle = vr::k_ulOverlayHandleInvalid;
        vr::VROverlayHandle_t m_overlayThumbnailHandle = vr::k_ulOverlayHandleInvalid;
        ipc::protocol::SharedData_HmdMetadata m_hmdMetadata = {};
        ipc::protocol::SharedData_HmdMetadata m_lastHmdMetaState = {};
        std::vector<std::string> m_aTrackingSystems;

        friend class ::ipc::IpcClient; // for m_hmdMetadata
    };
}
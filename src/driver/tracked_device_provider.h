#pragma once

#include <openvr_driver.h>
#include "ipc_server.h"
#include <chrono>

namespace spacecal {

    struct DeviceTransformation_t {
        bool hideDevice = false;
        std::chrono::high_resolution_clock::time_point last_poll;

    };

    class ServerTrackedDeviceProvider : public vr::IServerTrackedDeviceProvider {
    public:
        vr::EVRInitError Init(vr::IVRDriverContext* pDriverContext) override;
        void Cleanup() override;
        const char* const* GetInterfaceVersions() override;
        void RunFrame() override;
        bool ShouldBlockStandbyMode() override;
        void EnterStandby() override;
        void LeaveStandby() override;

        void BlendTransform(const DeviceTransformation_t device) const;
        bool HandleDevicePoseUpdated(vr::TrackedDeviceIndex_t unWhichDevice, const vr::DriverPose_t& newPose);

    private:
        ipc::Server m_ipcServer;
    };
}
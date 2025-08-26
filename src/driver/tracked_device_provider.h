#pragma once

#include <openvr_driver.h>
#include "ipc_server.h"

namespace spacecal {
    class ServerTrackedDeviceProvider : public vr::IServerTrackedDeviceProvider {
	public:
		vr::EVRInitError Init(vr::IVRDriverContext* pDriverContext) override;
		void Cleanup() override;
		const char* const* GetInterfaceVersions() override;
		void RunFrame() override;
		bool ShouldBlockStandbyMode() override;
		void EnterStandby() override;
		void LeaveStandby() override;

		bool HandleDevicePoseUpdated(uint32_t unWhichDevice, const vr::DriverPose_t& newPose);

	private:
		ipc::Server m_ipcServer;
    };
}
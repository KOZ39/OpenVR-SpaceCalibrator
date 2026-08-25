#pragma once

#include "ipc.h"
#include "protocol.h"


namespace ipc {
class IpcClient {
public:
    bool Connect();
    void Shutdown();

    inline const bool IsConnected() const { return m_connected; }

    void SetDeviceTransform(protocol::Command_SetDeviceTransform_t deviceTransform);
    void SetAlignmentSpeed(protocol::Command_SetAlignmentSpeedParams_t alignmentParams);
    void ResetCalibration();
    void RequestVirtualDesktopProps();
    void PollPoses();

private:
    bool m_connected = false;
    ::IpcHandle_t m_hIpc = k_hInvalidIpcHandle;
    ::IpcOperation_t m_poseDataOperation;
    ::IpcOperation_t m_deviceTransformOperation;
    ::IpcOperation_t m_hmdMetaOperation;

    ipc::protocol::Command_SetDeviceTransform_t m_transforms[vr::k_unMaxTrackedDeviceCount] = {};

    static const IpcFunction_t m_funcs[];

    friend class VRState; // for m_poses
};
}
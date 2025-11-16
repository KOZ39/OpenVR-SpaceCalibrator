#pragma once

#include "protocol.h"
#include "ipc.h"

namespace spacecal {
    class ServerTrackedDeviceProvider;
}

namespace ipc {
class Server {
public:
    bool Connect(spacecal::ServerTrackedDeviceProvider* driver);
    void Shutdown();

    void UpdatePose(vr::TrackedDeviceIndex_t unWhichDevice, const vr::DriverPose_t& pose);

private:
    static void Callback_Handshake(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata);
    static void Callback_SetDeviceTransform(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata);
    static void Callback_SetAlignmentSpeedParams(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata);
    static void Callback_ResetCalibration(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata);

private:
    bool m_connected = false;
    ::IpcHandle_t m_hIpc = k_hInvalidIpcHandle;
    ::IpcOperation_t m_poseDataOperation;
    spacecal::ServerTrackedDeviceProvider* m_driver = nullptr;

    static const ::IpcFunction_t m_funcs[];
};
}
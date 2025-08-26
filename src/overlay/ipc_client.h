#pragma once

#include "protocol.h"
#include "ipc.h"

namespace ipc {
    class Client {
    public:
        bool Connect();
        void Shutdown();

        const bool IsConnected() const { return m_connected; }

        void SetDeviceTransform(protocol::Command_SetDeviceTransform_t deviceTransform);
        void SetAlignmentSpeed(protocol::Command_SetAlignmentSpeedParams_t alignmentParams);
        void ResetCalibration();

    private:
        bool m_connected = false;
        IpcHandle_t m_hIpc = k_hInvalidIpcHandle;

        static const IpcFunction_t m_funcs[];
    };
}
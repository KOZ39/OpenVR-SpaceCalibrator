#define IPC_IMPLEMENTATION
#include "ipc_client.h"

namespace ipc {
    const IpcFunction_t Client::m_funcs[] = {
        {
            .commandType = protocol::IPC_COMMAND_HANDSHAKE,
            .szFunctionName = "SpaceCalibratorNova_Handshake",
        },
        {
            .commandType = protocol::IPC_COMMAND_SET_DEVICE_TRANSFORM,
            .szFunctionName = "SpaceCalibratorNova_SetDeviceTransform",
        },
        {
            .commandType = protocol::IPC_COMMAND_SET_ALIGNMENT_SPEED_PARAMS,
            .szFunctionName = "SpaceCalibratorNova_SetAlignmentSpeedParams",
        },
        {
            .commandType = protocol::IPC_COMMAND_RESET_CALIBRATION,
            .szFunctionName = "SpaceCalibratorNova_ResetCalibration",
        },
    };

    bool Client::Connect() {
        m_hIpc = ipc_client_init({
            .szSharedMemoryName = protocol::k_szIpcIdentifier,
            .dwSharedBufferSizeBytes = protocol::k_unSharedMemoryElementCount,
            .aFunctions = m_funcs,
            .dwFunctionCount = _countof(m_funcs),
            .aOperations = nullptr,
            .dwOperationCount = 0,
            .userData = this,
        });

        // send handshake to driver
        protocol::Command_Handshake_t handshakeArgs;
        ipc_client_dispatch_function(m_hIpc, protocol::IPC_COMMAND_HANDSHAKE, &handshakeArgs, sizeof(handshakeArgs));

        m_connected = m_hIpc != k_hInvalidIpcHandle;

        return m_connected;
    }

    void Client::Shutdown() {
        ipc_client_shutdown(&m_hIpc);
    }

    void Client::SetDeviceTransform(protocol::Command_SetDeviceTransform_t deviceTransform) {
        ipc_client_dispatch_function(m_hIpc, protocol::IPC_COMMAND_SET_DEVICE_TRANSFORM, &deviceTransform, sizeof(deviceTransform));
    }
    void Client::SetAlignmentSpeed(protocol::Command_SetAlignmentSpeedParams_t alignmentParams) {
        ipc_client_dispatch_function(m_hIpc, protocol::IPC_COMMAND_SET_ALIGNMENT_SPEED_PARAMS, &alignmentParams, sizeof(alignmentParams));
    }
    void Client::ResetCalibration() {
        ipc_client_dispatch_function(m_hIpc, protocol::IPC_COMMAND_RESET_CALIBRATION, nullptr, 0);
    }
}
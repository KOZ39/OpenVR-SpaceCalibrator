#define IPC_IMPLEMENTATION
#include "ipc_server.h"
#include "log.h"
#include "tracked_device_provider.h"

namespace ipc {
    const ::IpcFunction_t Server::m_funcs[] = {
        {
            .commandType = protocol::IPC_COMMAND_HANDSHAKE,
            .szFunctionName = "SpaceCalibratorNova_Handshake",
            .callback = &Server::Callback_Handshake,
        },
        {
            .commandType = protocol::IPC_COMMAND_SET_DEVICE_TRANSFORM,
            .szFunctionName = "SpaceCalibratorNova_SetDeviceTransform",
            .callback = &Server::Callback_SetDeviceTransform,
        },
        {
            .commandType = protocol::IPC_COMMAND_SET_ALIGNMENT_SPEED_PARAMS,
            .szFunctionName = "SpaceCalibratorNova_SetAlignmentSpeedParams",
            .callback = &Server::Callback_SetAlignmentSpeedParams,
        },
        {
            .commandType = protocol::IPC_COMMAND_RESET_CALIBRATION,
            .szFunctionName = "SpaceCalibratorNova_ResetCalibration",
            .callback = &Server::Callback_ResetCalibration,
        },
    };

    void Server::Callback_Handshake(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata) {
        if (pArguments) {
            protocol::Command_Handshake_t* pHandshakeParams = reinterpret_cast<protocol::Command_Handshake_t*>(pArguments);
            Server* pThis = reinterpret_cast<Server*>(userdata);
#if _WIN32
            // assign thread name on first user-controlled invocation to it
            SetThreadDescription(GetCurrentThread(), L"IPC Thread");
#endif

            if (pHandshakeParams->version != protocol::IPC_PROTOCOL_CURRENT) {
                if (pHandshakeParams->version < protocol::IPC_PROTOCOL_CURRENT) {
                    // overlay too old
                    LOG_IPC_CRITICAL("The overlay is older than the driver! Expected IPC version {}, got {}.", protocol::IPC_PROTOCOL_CURRENT, pHandshakeParams->version);
                } else {
                    // overlay too new
                    LOG_IPC_CRITICAL("The overlay is newer than the driver! Expected IPC version {}, got {}.", protocol::IPC_PROTOCOL_CURRENT, pHandshakeParams->version);
                }
            }

            LOG_IPC_INFO("Overlay connected. Using IPC {}", pHandshakeParams->version);
        }
    }

    void Server::Callback_SetDeviceTransform(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata) {
        if (pArguments) {
            protocol::Command_SetDeviceTransform_t* pDeviceTransform = reinterpret_cast<protocol::Command_SetDeviceTransform_t*>(pArguments);
            Server* pThis = reinterpret_cast<Server*>(userdata);

            LOG_IPC_INFO("Server::Callback_SetDeviceTransform");
        }
    }

    void Server::Callback_SetAlignmentSpeedParams(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata) {
        if (pArguments) {
            protocol::Command_SetAlignmentSpeedParams_t* pAlignmentParams = reinterpret_cast<protocol::Command_SetAlignmentSpeedParams_t*>(pArguments);
            Server* pThis = reinterpret_cast<Server*>(userdata);

            LOG_IPC_INFO("Server::Callback_SetAlignmentSpeedParams");
        }
    }

    void Server::Callback_ResetCalibration(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata) {
        if (pArguments) {
            Server* pThis = reinterpret_cast<Server*>(userdata);

            LOG_IPC_INFO("Server::Callback_ResetCalibration");
        }
    }

    bool Server::Connect(spacecal::ServerTrackedDeviceProvider* driver) {
        m_hIpc = ::ipc_server_init({
            .szSharedMemoryName = protocol::k_szIpcIdentifier,
            .dwSharedBufferSizeBytes = protocol::k_unSharedMemoryElementCount,
            .aFunctions = m_funcs,
            .dwFunctionCount = _countof(m_funcs),
            .aOperations = nullptr,
            .dwOperationCount = 0,
            .userData = this,
        });

        m_driver = driver;

        return m_hIpc != k_hInvalidIpcHandle;
    }

    void Server::Shutdown() {
        ::ipc_server_shutdown(&m_hIpc);
    }
}
#include "openvr_driver.h"
#define IPC_IMPLEMENTATION
#include "ipc_server.h"
#include "log.h"
#include "tracked_device_provider.h"
#include "platform.h"

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
        {
            .commandType = protocol::IPC_COMMAND_REQUEST_VIRTUAL_DESKTOP_PROPS,
            .szFunctionName = "SpaceCalibratorNova_RequestVirtualDesktopProps",
            .callback = &Server::Callback_RequestVirtualDesktopProps,
        },
    };

    void Server::Callback_Handshake(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata) {
        if (pArguments) {
            protocol::Command_Handshake_t* pHandshakeParams = reinterpret_cast<protocol::Command_Handshake_t*>(pArguments);
            Server* pThis = reinterpret_cast<Server*>(userdata);

            platform::setThreadName("IPC Thread");

            if (pHandshakeParams->version != protocol::IPC_PROTOCOL_CURRENT) {
                if (pHandshakeParams->version < protocol::IPC_PROTOCOL_CURRENT) {
                    // overlay too old
                    LOG_IPC_CRITICAL("The overlay is older than the driver! Expected IPC version {}, got {}.", protocol::IPC_PROTOCOL_CURRENT, pHandshakeParams->version);
                } else {
                    // overlay too new
                    LOG_IPC_CRITICAL("The overlay is newer than the driver! Expected IPC version {}, got {}.", protocol::IPC_PROTOCOL_CURRENT, pHandshakeParams->version);
                }
                // @TODO: invoke function in client telling it server ipc version to verify version and refuse interop
            } else {
                LOG_IPC_INFO("Overlay connected. Using IPC {}", pHandshakeParams->version);
            }
        }
    }

    void Server::Callback_SetDeviceTransform(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata) {
        if (pArguments) {
            protocol::Command_SetDeviceTransform_t* pDeviceTransform = reinterpret_cast<protocol::Command_SetDeviceTransform_t*>(pArguments);
            Server* pThis = reinterpret_cast<Server*>(userdata);
            if (pThis && pDeviceTransform) {
                pThis->m_driver->SetDeviceTransform(*pDeviceTransform);
            } else {
                LOG_IPC_ERROR("Server::Callback_SetDeviceTransform failed due to nullptr");
            }
        }
    }

    void Server::Callback_SetAlignmentSpeedParams(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata) {
        if (pArguments) {
            protocol::Command_SetAlignmentSpeedParams_t* pAlignmentParams = reinterpret_cast<protocol::Command_SetAlignmentSpeedParams_t*>(pArguments);
            Server* pThis = reinterpret_cast<Server*>(userdata);
            if (pThis && pAlignmentParams) {
                pThis->m_driver->SetAlignmentSpeedParams(*pAlignmentParams);
            } else {
                LOG_IPC_ERROR("Server::Callback_SetAlignmentSpeedParams failed due to nullptr");
            }
        }
    }

    void Server::Callback_ResetCalibration(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata) {
        if (pArguments) {
            Server* pThis = reinterpret_cast<Server*>(userdata);
            pThis->m_driver->ResetCalibration();

            // LOG_IPC_INFO("Server::Callback_ResetCalibration");
        }
    }
    
    void Server::Callback_RequestVirtualDesktopProps(::IpcCommandType_t cmdType, ::IpcHandle_t hIpcServer, void* pArguments, void* userdata) {
        if (pArguments) {
            Server* pThis = reinterpret_cast<Server*>(userdata);
            pThis->m_driver->RequestVirtualDesktopProps();

            // LOG_IPC_INFO("Server::Callback_RequestVirtualDesktopProps");
        }
    }

    void Server::UpdatePose(vr::TrackedDeviceIndex_t unWhichDevice, const vr::DriverPose_t& pose) {
        if (unWhichDevice < 0 || unWhichDevice >= vr::k_unMaxTrackedDeviceCount) {
            LOG_IPC_FATAL("Attempted to update shared pose buffer, but got invalid device index {}...", unWhichDevice);
            return;
        }
        m_driver->m_poses[unWhichDevice] = pose;
        if (!ipc_server_write_shared_memory(m_hIpc, m_poseDataOperation, m_driver->m_poses, sizeof(m_driver->m_poses))) {
            LOG_IPC_ERROR("Tried updating shared pose buffer, but operation is invalid!");
        }
        if (unWhichDevice == vr::k_unTrackedDeviceIndex_Hmd) {
            if (!ipc_server_write_shared_memory(m_hIpc, m_hmdMetaOperation, &m_driver->m_hmdMetaData, sizeof(m_driver->m_hmdMetaData))) {
                LOG_IPC_ERROR("Tried updating HMD metadata, but operation is invalid!");
            }
        }
        ipc_client_read_shared_memory(m_hIpc, m_deviceTransformOperation, &m_driver->m_transforms, sizeof(m_driver->m_transforms));
    }

    bool Server::Connect(spacecal::ServerTrackedDeviceProvider* driver) {
        m_hIpc = ::ipc_server_init({
            .szSharedMemoryName = protocol::k_szIpcIdentifier,
            .dwSharedBufferSizeBytes = protocol::k_unSharedMemoryElementSize,
            .aFunctions = m_funcs,
            .dwFunctionCount = _countof(m_funcs),
            .aOperations = nullptr,
            .dwOperationCount = 0,
            .userData = this,
        });

        if (m_hIpc == k_hInvalidIpcHandle) {
            LOG_IPC_ERROR("Failed to initialise IPC server!");
        }

        m_poseDataOperation = {
            .szIdentifier = "SpaceCalibratorNova_PoseSharedBuffer",
            .dwSharedMemoryOffset = 0,
        };
        m_deviceTransformOperation = {
            .szIdentifier = "SpaceCalibratorNova_DeviceTransformsSharedBuffer",
            .dwSharedMemoryOffset = protocol::k_unSharedMemoryPoseSize,
        };
        m_hmdMetaOperation = {
            .szIdentifier = "SpaceCalibratorNova_HmdMetaData",
            .dwSharedMemoryOffset = protocol::k_unSharedMemoryPoseSize + protocol::k_unSharedMemoryDeviceTransformSize,
        };

        if (!ipc_server_register_operation(m_hIpc, &m_poseDataOperation)) {
            LOG_IPC_ERROR("Failed to register pose data shared memory operation!");
        }

        if (!ipc_server_register_operation(m_hIpc, &m_deviceTransformOperation)) {
            LOG_IPC_ERROR("Failed to register device transform shared memory operation!");
        }

        if (!ipc_server_register_operation(m_hIpc, &m_hmdMetaOperation)) {
            LOG_IPC_ERROR("Failed to register hmd metadata shared memory operation!");
        }

        m_driver = driver;

        return m_hIpc != k_hInvalidIpcHandle;
    }

    void Server::Shutdown() {
        ::ipc_server_shutdown(&m_hIpc);
    }
}
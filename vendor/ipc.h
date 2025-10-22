#ifndef HEKKY_IPC_INC
#define HEKKY_IPC_INC
#include <inttypes.h>

// IPC.h - Single header IPC library for Windows
// v1.0 - (C) Hekky, under MIT license.
// 
//  Concepts:
//    - Shared / streaming memory :: A block of memory thats shared between programs. This is usually filled with data such that one
//                                   process is continuously writing TO this buffer and other processes simply READ from it.
//    - Operation :: An operation reads / writes to the shared memory buffer. We use the nomenclature of operation to avoid race
//                   conditions, each unique condition has its own mutex and event pair.
//    - Command   :: Effectively a function invocation. We use comamnd IDs to identify commands to jump to the desired callback on the server.
//  
// Usage:
//     #define IPC_IMPLEMENTATION
//     #include "ipc.h"
// 
// Platforms:
//     - Windows
//     - Linux (unsupported but eventually yes!)
// 
// Design:
//   - cmds are numeric events. i recommend making an enum for them for better readibility.
//   - data is POD shared via shared memory.
//   - each cmd should have a command associated with it, but isnt necessary.
//   - a streaming buffer is mainainted too, allocated at init time though.
//   - allocate a "stack" for event arguments, and then enough data for your streams. you can use void* to access the data directly.
//
//   Shared memory layout:
// 
//    o-----------------o
//    |    cmd_type     |
//    o-----------------o
//    |                 |
//    |  Command_XXX_t  |  -> function stack effectively
//    |                 |
//    |-----------------o
//    |                 |
//    |    Streaming    |  -> shared memory for streaming operations
//    |      buffer     |
//    |                 |
//    o-----------------o
//

typedef void* IpcHandle_t;
static const IpcHandle_t k_hInvalidIpcHandle = ((IpcHandle_t)(-1));
typedef uint64_t IpcCommandType_t;
typedef void* IpcNativeHandle_t;
typedef void (*pfnIpcFunctionCallback_t)(IpcCommandType_t cmdType, IpcHandle_t hIpcServer, void* pArguments, void* userdata);

struct IpcFunction_t {
    IpcCommandType_t commandType = 0;
    const char* szFunctionName = nullptr;
    pfnIpcFunctionCallback_t callback = nullptr; // required on server, ignored on client
};

struct IpcOperation_t {
    const char* szIdentifier = nullptr; // unique name, passed to OS for names
    size_t dwSharedMemoryOffset = 0; // offset of data to write in shared memory
    IpcNativeHandle_t hNativeMutex = 0; // on windows, HANDLE to mutex
    IpcNativeHandle_t hNativeEvent = 0; // on windows, HANDLE to mutex
};

struct IpcCreateArguments_t {
    const char* szSharedMemoryName = nullptr; // name used for the shared memory
    size_t dwSharedBufferSizeBytes = 0; // size in bytes of the shared memory thats not used for function invocation. function invocation reserves 4KB of data.
    const IpcFunction_t* aFunctions = nullptr;
    size_t dwFunctionCount = 0;
    IpcOperation_t* aOperations = nullptr;
    size_t dwOperationCount = 0;
    void* userData = nullptr; // passed to userdata in callbacks
};

IpcHandle_t ipc_server_init(IpcCreateArguments_t args);
IpcHandle_t ipc_client_init(IpcCreateArguments_t args);
bool ipc_server_shutdown(IpcHandle_t* hIpcServer);
bool ipc_client_shutdown(IpcHandle_t* hIpcClient);

/* Invokes a function from the client with id unCmdType with arguments pArgs */
bool ipc_client_dispatch_function(IpcHandle_t hIpcClient, IpcCommandType_t unCmdType, void* pArgs, size_t dwArgsSize);

/* Registers the IpcOperation with the IpcServer. Must be invoked before you can use read/write shared memory. */
bool ipc_server_register_operation(IpcHandle_t hIpcServer, IpcOperation_t* ipcOperation);
/* Writes the given memory into the shared memory at the provided offset. Think of this as a memcpy with extra steps. */
bool ipc_server_write_shared_memory(IpcHandle_t hIpcServer, IpcOperation_t ipcOperation, void* pSrc, size_t destSize);
/* Reads the given memory from the shared memory at the provided offset. Think of this as a memcpy with extra steps. */
bool ipc_client_read_shared_memory(IpcHandle_t hIpcClient, IpcOperation_t ipcOperation, void* pDest, size_t destSize);

#ifdef IPC_IMPLEMENTATION

#include <stdio.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>

#ifndef IPC_FUNCTION_ARGS_TOTAL_SIZE
// 16KB
#define IPC_FUNCTION_ARGS_TOTAL_SIZE (1024*16)
#endif // IPC_FUNCTION_ARGS_TOTAL_SIZE

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOVIRTUALKEYCODES
#define NOVIRTUALKEYCODES
#endif // NOVIRTUALKEYCODES
#ifndef NOSOUND
#define NOSOUND
#endif // NOSOUND
#ifndef NOICONS
#define NOICONS
#endif // NOICONS
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <windows.h>
#endif

struct __internal__IpcFunction_t {
    IpcCommandType_t unCmdType = 0;           // the command ID for this function
    pfnIpcFunctionCallback_t pfnCallback = 0; // callback to be invoked by IPC server
};

struct __internal__IpcHandle_t {
    // generally so few that theyd fit in cache so who cares
    std::vector<__internal__IpcFunction_t> registed_functions = {};
    std::vector<IpcOperation_t> registed_operations = {};
    size_t nextFunctionOffset = 0;
    const char* szSharedMemoryName = nullptr;
    IpcNativeHandle_t hSharedMemory = NULL; // on windows, HANDLE to filemapping
    void* pSharedMemory = nullptr; // on windows, void* to the actual memory
    IpcNativeHandle_t hNativeMutex = 0; // on windows, HANDLE to mutex
    IpcNativeHandle_t hNativeEvent = 0; // on windows, HANDLE to mutex
    std::thread poll_thread;
    std::atomic<bool> is_running{ false };
    void* userData = nullptr;
};

void __internal_ipc_server_poll_requests(IpcHandle_t hIpcServer) {
    __internal__IpcHandle_t* handleInternal = (__internal__IpcHandle_t*)hIpcServer;

#if _WIN32
    while (handleInternal->is_running) {
        DWORD result = WaitForSingleObject((HANDLE)handleInternal->hNativeEvent, 100);

        if (result == WAIT_OBJECT_0) {
            WaitForSingleObject((HANDLE)handleInternal->hNativeMutex, INFINITE);
            IpcCommandType_t* cmdType = (IpcCommandType_t*)handleInternal->pSharedMemory;
            void* pArgs = (void*)((char*)handleInternal->pSharedMemory + sizeof(IpcCommandType_t));

            for (const auto& func : handleInternal->registed_functions) {
                if (func.unCmdType == *cmdType) {
                    if (func.pfnCallback) {
                        func.pfnCallback(*cmdType, hIpcServer, pArgs, handleInternal->userData);
                    }
                    break;
                }
            }

            ResetEvent((HANDLE)handleInternal->hNativeEvent);
            ReleaseMutex((HANDLE)handleInternal->hNativeMutex);
        }
    }
#else
#error "Unsupported platform"
#endif
}

IpcHandle_t ipc_server_init(IpcCreateArguments_t args) {
    if (!args.szSharedMemoryName) {
        return k_hInvalidIpcHandle;
    }

    __internal__IpcHandle_t* handleInternal = (__internal__IpcHandle_t*)calloc(1, sizeof(__internal__IpcHandle_t));

    if (!handleInternal) {
        return k_hInvalidIpcHandle;
    }

    handleInternal->userData = args.userData;
    handleInternal->szSharedMemoryName = args.szSharedMemoryName;
    size_t totalSharedMemSize = IPC_FUNCTION_ARGS_TOTAL_SIZE + args.dwSharedBufferSizeBytes;

#if _WIN32
    handleInternal->hSharedMemory = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, totalSharedMemSize, args.szSharedMemoryName);
    if (handleInternal->hSharedMemory == NULL) {
        free(handleInternal);
        return k_hInvalidIpcHandle;
    }

    handleInternal->pSharedMemory = MapViewOfFile(handleInternal->hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, totalSharedMemSize);
    if (handleInternal->pSharedMemory == NULL) {
        CloseHandle(handleInternal->hSharedMemory);
        free(handleInternal);
        return k_hInvalidIpcHandle;
    }

    std::string mutexName = std::string(args.szSharedMemoryName) + "_FUNC_MTX";
    std::string eventName = std::string(args.szSharedMemoryName) + "_FUNC_EVT";
    handleInternal->hNativeMutex = CreateMutexA(NULL, FALSE, mutexName.c_str());
    handleInternal->hNativeEvent = CreateEventA(NULL, TRUE, FALSE, eventName.c_str());
    if (handleInternal->hNativeMutex == NULL || handleInternal->hNativeEvent == NULL) {
        IpcHandle_t hIpcHandle = ((IpcHandle_t)handleInternal);
        ipc_server_shutdown(&hIpcHandle);
        return k_hInvalidIpcHandle;
    }

    for (size_t i = 0; i < args.dwFunctionCount; ++i) {
        const IpcFunction_t& reg = args.aFunctions[i];
        __internal__IpcFunction_t func = {
            .unCmdType = reg.commandType,
            .pfnCallback = reg.callback
        };
        handleInternal->registed_functions.push_back(func);
    }

    handleInternal->is_running = true;
    handleInternal->poll_thread = std::thread(__internal_ipc_server_poll_requests, (IpcHandle_t)handleInternal);
#else
#error "Unsupported platform"
#endif

    return (IpcHandle_t)handleInternal;
}

IpcHandle_t ipc_client_init(IpcCreateArguments_t args) {
    if (!args.szSharedMemoryName) {
        return k_hInvalidIpcHandle;
    }

    __internal__IpcHandle_t* handleInternal = (__internal__IpcHandle_t*)calloc(1, sizeof(__internal__IpcHandle_t));

    if (!handleInternal) {
        return k_hInvalidIpcHandle;
    }

    handleInternal->szSharedMemoryName = args.szSharedMemoryName;

#if _WIN32
    handleInternal->hSharedMemory = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, args.szSharedMemoryName);
    if (handleInternal->hSharedMemory == NULL) {
        free(handleInternal);
        return k_hInvalidIpcHandle;
    }

    handleInternal->pSharedMemory = MapViewOfFile(handleInternal->hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (handleInternal->pSharedMemory == NULL) {
        CloseHandle(handleInternal->hSharedMemory);
        free(handleInternal);
        return k_hInvalidIpcHandle;
    }

    std::string mutexName = std::string(args.szSharedMemoryName) + "_FUNC_MTX";
    std::string eventName = std::string(args.szSharedMemoryName) + "_FUNC_EVT";
    handleInternal->hNativeMutex = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, mutexName.c_str());
    handleInternal->hNativeEvent = OpenEventA(EVENT_ALL_ACCESS, FALSE, eventName.c_str());
    if (handleInternal->hNativeMutex == NULL || handleInternal->hNativeEvent == NULL) {
        IpcHandle_t handleFinal = ((IpcHandle_t)handleInternal);
        ipc_client_shutdown(&handleFinal);
        return k_hInvalidIpcHandle;
    }

    for (size_t i = 0; i < args.dwFunctionCount; ++i) {
        const IpcFunction_t& reg = args.aFunctions[i];
        __internal__IpcFunction_t func = {
            .unCmdType = reg.commandType,
        };
        handleInternal->registed_functions.push_back(func);
    }
#else
#error "Unsupported platform"
#endif

    return (IpcHandle_t)handleInternal;
}

bool ipc_server_shutdown(IpcHandle_t* hIpcServer) {
    if (hIpcServer && *hIpcServer != k_hInvalidIpcHandle) {
        __internal__IpcHandle_t* handleInternal = (__internal__IpcHandle_t*)*hIpcServer;

#if _WIN32
        handleInternal->is_running = false;
        if (handleInternal->poll_thread.joinable()) {
            handleInternal->poll_thread.join();
        }

        for (size_t i = 0; i < handleInternal->registed_operations.size(); ++i) {
            if (handleInternal->registed_operations[i].hNativeEvent)
                CloseHandle((HANDLE)handleInternal->registed_operations[i].hNativeEvent);
            if (handleInternal->registed_operations[i].hNativeMutex)
                CloseHandle((HANDLE)handleInternal->registed_operations[i].hNativeMutex);
        }

        if (handleInternal->hNativeMutex)
            CloseHandle((HANDLE)handleInternal->hNativeMutex);
        if (handleInternal->hNativeEvent)
            CloseHandle((HANDLE)handleInternal->hNativeEvent);

        if (handleInternal->pSharedMemory)
            UnmapViewOfFile(handleInternal->pSharedMemory);
        if (handleInternal->hSharedMemory)
            CloseHandle(handleInternal->hSharedMemory);
#else
#error "Unsupported platform"
#endif

        free(*hIpcServer);
        *hIpcServer = k_hInvalidIpcHandle;
        return true;
    }
    return false;
}
bool ipc_client_shutdown(IpcHandle_t* hIpcClient) {
    if (hIpcClient && *hIpcClient != k_hInvalidIpcHandle) {
        __internal__IpcHandle_t* handleInternal = (__internal__IpcHandle_t*)*hIpcClient;

#if _WIN32
        for (size_t i = 0; i < handleInternal->registed_operations.size(); ++i) {
            if (handleInternal->registed_operations[i].hNativeEvent)
                CloseHandle((HANDLE)handleInternal->registed_operations[i].hNativeEvent);
            if (handleInternal->registed_operations[i].hNativeMutex)
                CloseHandle((HANDLE)handleInternal->registed_operations[i].hNativeMutex);
        }

        if (handleInternal->hNativeMutex)
            CloseHandle((HANDLE)handleInternal->hNativeMutex);
        if (handleInternal->hNativeEvent)
            CloseHandle((HANDLE)handleInternal->hNativeEvent);

        if (handleInternal->pSharedMemory)
            UnmapViewOfFile(handleInternal->pSharedMemory);
        if (handleInternal->hSharedMemory)
            CloseHandle(handleInternal->hSharedMemory);
#else
#error "Unsupported platform"
#endif

        free(*hIpcClient);
        *hIpcClient = k_hInvalidIpcHandle;
        return true;
    }
    return false;
}

bool ipc_client_dispatch_function(IpcHandle_t hIpcClient, IpcCommandType_t unCmdType, void* pArgs, size_t dwArgsSize) {
    if (hIpcClient == k_hInvalidIpcHandle || dwArgsSize > IPC_FUNCTION_ARGS_TOTAL_SIZE - sizeof(IpcCommandType_t)) {
        return false;
    }
    __internal__IpcHandle_t* handleInternal = (__internal__IpcHandle_t*)hIpcClient;

    // unCmdType must be registered
    bool bCmdIsValid = false;
    for (size_t i = 0; i < handleInternal->registed_functions.size(); i++) {
        if (handleInternal->registed_functions[i].unCmdType == unCmdType) {
            bCmdIsValid = true;
            break;
        }
    }
    if (!bCmdIsValid) {
        return false;
    }

#if _WIN32
    WaitForSingleObject((HANDLE)handleInternal->hNativeMutex, INFINITE);
    void* pSharedMem = handleInternal->pSharedMemory;
    *((IpcCommandType_t*)pSharedMem) = unCmdType;
    memcpy((char*)pSharedMem + sizeof(IpcCommandType_t), pArgs, dwArgsSize);
    SetEvent((HANDLE)handleInternal->hNativeEvent);
    WaitForSingleObject((HANDLE)handleInternal->hNativeEvent, INFINITE);
    ReleaseMutex((HANDLE)handleInternal->hNativeMutex);
#else
#error "Unsupported platform"
#endif

    return true;
}

bool ipc_server_register_operation(IpcHandle_t hIpcServer, IpcOperation_t* ipcOperation) {
    if (hIpcServer == k_hInvalidIpcHandle || !ipcOperation->szIdentifier) {
        return false;
    }
    __internal__IpcHandle_t* handleInternal = (__internal__IpcHandle_t*)hIpcServer;

#if _WIN32
    std::string mutexName = std::string(handleInternal->szSharedMemoryName) + "_" + std::string(ipcOperation->szIdentifier) + "_MTX";
    std::string eventName = std::string(handleInternal->szSharedMemoryName) + "_" + std::string(ipcOperation->szIdentifier) + "_EVT";

    ipcOperation->hNativeMutex = CreateMutexA(NULL, FALSE, mutexName.c_str());
    ipcOperation->hNativeEvent = CreateEventA(NULL, FALSE, FALSE, eventName.c_str()); // Auto-reset event

    if (ipcOperation->hNativeMutex == NULL || ipcOperation->hNativeEvent == NULL) {
        if (ipcOperation->hNativeMutex)
            CloseHandle((HANDLE)ipcOperation->hNativeMutex);
        if (ipcOperation->hNativeEvent)
            CloseHandle((HANDLE)ipcOperation->hNativeEvent);
        return false;
    }

    handleInternal->registed_operations.push_back(*ipcOperation);
#else
#error "Unsupported platform"
#endif

    return true;
}

bool ipc_server_write_shared_memory(IpcHandle_t hIpcServer, IpcOperation_t ipcOperation, void* pSrc, size_t destSize) {
    if (hIpcServer == k_hInvalidIpcHandle || !pSrc || destSize == 0) {
        return false;
    }
    if (!ipcOperation.hNativeMutex || !ipcOperation.hNativeEvent)
        return false;
    __internal__IpcHandle_t* handleInternal = (__internal__IpcHandle_t*)hIpcServer;

#if _WIN32
    DWORD ret = WaitForSingleObject((HANDLE)ipcOperation.hNativeMutex, 10);
    if (ret == WAIT_TIMEOUT) {
        ReleaseMutex((HANDLE)ipcOperation.hNativeMutex);
        return false;
    }
    void* pDest = (char*)handleInternal->pSharedMemory + IPC_FUNCTION_ARGS_TOTAL_SIZE + ipcOperation.dwSharedMemoryOffset;
    memcpy(pDest, pSrc, destSize);
    SetEvent((HANDLE)ipcOperation.hNativeEvent);
    ReleaseMutex((HANDLE)ipcOperation.hNativeMutex);
#else
#error "Unsupported platform"
#endif

    return true;
}

bool ipc_client_read_shared_memory(IpcHandle_t hIpcClient, IpcOperation_t ipcOperation, void* pDest, size_t destSize) {
    if (hIpcClient == k_hInvalidIpcHandle || !pDest || destSize == 0) {
        return false;
    }
    if (!ipcOperation.hNativeMutex || !ipcOperation.hNativeEvent)
        return false;
    __internal__IpcHandle_t* handleInternal = (__internal__IpcHandle_t*)hIpcClient;

#if _WIN32
    DWORD ret = WaitForSingleObject((HANDLE)ipcOperation.hNativeEvent, 10);
    if (ret == WAIT_TIMEOUT) {
        ReleaseMutex((HANDLE)ipcOperation.hNativeMutex);
        return false;
    }
    ret = WaitForSingleObject((HANDLE)ipcOperation.hNativeMutex, 10);
    if (ret == WAIT_TIMEOUT) {
        ReleaseMutex((HANDLE)ipcOperation.hNativeMutex);
        return false;
    }
    void* pSrc = (char*)handleInternal->pSharedMemory + IPC_FUNCTION_ARGS_TOTAL_SIZE + ipcOperation.dwSharedMemoryOffset;
    memcpy(pDest, pSrc, destSize);
    ReleaseMutex((HANDLE)ipcOperation.hNativeMutex);
#else
#error "Unsupported platform"
#endif

    return true;
}

#endif // IPC_IMPLEMENTATION
#endif // HEKKY_IPC_INC
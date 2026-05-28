#pragma once

#include <string>
#include <map>
#include "log.h"
#include "platform.h"
#if OS_WINDOWS
#include <MinHook.h>
#elif OS_LINUX
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define ALIGN_TO_PAGE(addr, pageSize) \
    ((void*)((uintptr_t)(addr) & ~(uintptr_t)((pageSize) - 1)))
#endif

namespace hooking {
    class IHook
    {
    public:
        const std::string name;

        IHook(const std::string& name) : name(name) { }
        virtual ~IHook() { }

        virtual void Destroy() = 0;

        static bool Exists(const std::string& name);
        static void Register(IHook* hook);
        static void Unregister(IHook* hook);
        static void DestroyAll();

    private:
        static std::map<std::string, IHook*> hooks;
    };

    template<class FuncType> class Hook : public IHook
    {
    public:
        FuncType originalFunc = nullptr;
        Hook(const std::string& name) : IHook(name) { }

        bool CreateHookInObjectVTable(void* object, int vtableOffset, void* detourFunction)
        {
            // For virtual objects, VC++ adds a pointer to the vtable as the first member.
            // To access the vtable, we simply dereference the object.
            void** vtable = *((void***)object);

#if OS_WINDOWS
            // The vtable itself is an array of pointers to member functions,
            // in the order they were declared in.
            targetFunc = vtable[vtableOffset];

            auto err = MH_CreateHook(targetFunc, detourFunction, (LPVOID*)&originalFunc);
            if (err != MH_OK)
            {
                LOG_HOOKING_ERROR("Failed to create hook for {}, error: {}", name, MH_StatusToString(err));
                return false;
            }

            err = MH_EnableHook(targetFunc);
            if (err != MH_OK)
            {
                LOG_HOOKING_ERROR("Failed to enable hook for {}, error: {}", name, MH_StatusToString(err));
                MH_RemoveHook(targetFunc);
                return false;
            }
#elif OS_LINUX
            targetFunc = (void**)&vtable[vtableOffset];
            originalFunc = (FuncType)vtable[vtableOffset];

            long pageSize = sysconf(_SC_PAGESIZE);
            void* pageStart = ALIGN_TO_PAGE(targetFunc, pageSize);
            void* pageEnd = ALIGN_TO_PAGE((uintptr_t)targetFunc + sizeof(void*) - 1, pageSize);
            size_t length = (uintptr_t)pageEnd - (uintptr_t)pageStart + pageSize;

            if (mprotect(pageStart, length, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
                LOG_HOOKING_ERROR("Failed to make vtable writable for {}: {}", name, strerror(errno));
                return false;
            }

            *targetFunc = detourFunction;

#if COMPILER_GCC || COMPILER_CLANG
            // https://blog.pathogenstudios.com/rolling-your-own-breakpoints-without-0xcc/
            // https://github.com/ridpath/gamehacking-cheatsheet#apple-silicon-rosetta-2-game-reversing
            // flush cache -> required for aarch64
            // emits no-op on amd64 (done automatically by hardware on amd64 so no need)
            __builtin___clear_cache(
                reinterpret_cast<char*>(targetFunc),
                reinterpret_cast<char*>(targetFunc) + sizeof(void*)
            );
#endif // COMPILER_GCC || COMPILER_CLANG

            if (mprotect(pageStart, length, PROT_READ | PROT_EXEC) != 0) {
                LOG_HOOKING_ERROR("Failed to restore vtable protection for {}: {}", name, strerror(errno));
            }
#endif // OS_LINUX

            LOG_HOOKING_INFO("Enabled hook for {}", name);
            enabled = true;
            return true;
        }

        void Destroy()
        {
#if OS_WINDOWS
            if (enabled)
            {
                MH_RemoveHook(targetFunc);
                enabled = false;
            }
#elif OS_LINUX
            if (enabled && targetFunc)
            {
                long pageSize = sysconf(_SC_PAGESIZE);
                void* pageStart = ALIGN_TO_PAGE(targetFunc, pageSize);
                void* pageEnd = ALIGN_TO_PAGE((uintptr_t)targetFunc + sizeof(void*) - 1, pageSize);
                size_t length = (uintptr_t)pageEnd - (uintptr_t)pageStart + pageSize;

                if (mprotect(pageStart, length, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
                    *targetFunc = (void*)originalFunc;
                    mprotect(pageStart, length, PROT_READ | PROT_EXEC);
                }
                enabled = false;
            }
#endif // OS_LINUX
        }

    private:
        bool enabled = false;
#if OS_WINDOWS
        void* targetFunc = nullptr;
#elif OS_LINUX
        void** targetFunc = nullptr;
#undef ALIGN_TO_PAGE
#endif
    };
}
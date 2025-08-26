#include "log_formatters.h"

auto fmtquill::formatter<ipc::protocol::Version_t>::format(ipc::protocol::Version_t version, format_context& ctx) const -> format_context::iterator {
    string_view versionStr{};
    switch (version) {
    case ipc::protocol::IPC_PROTOCOL_VER_INVALID: versionStr = "IPC_PROTOCOL_VER_INVALID"; break;
    case ipc::protocol::IPC_PROTOCOL_VER_1: versionStr = "IPC_PROTOCOL_VER_1"; break;
    case ipc::protocol::IPC_PROTOCOL_VER_2: versionStr = "IPC_PROTOCOL_VER_2"; break;
    case ipc::protocol::IPC_PROTOCOL_VER_3: versionStr = "IPC_PROTOCOL_VER_3"; break;
    case ipc::protocol::IPC_PROTOCOL_VER_4: versionStr = "IPC_PROTOCOL_VER_4"; break;
    case ipc::protocol::IPC_PROTOCOL_VER_5: versionStr = "IPC_PROTOCOL_VER_5"; break;
    default: versionStr = fmtquill::format("IPC_PROTOCOL_VER_UNK_", (uint32_t)version); break;
    }
    return formatter<string_view>::format(versionStr, ctx);
}
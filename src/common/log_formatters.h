#pragma once

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/Logger.h"
#include "quill/LogMacros.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/FileSink.h"

#include "protocol.h"
#include <quill/bundled/fmt/format.h>
#include <quill/bundled/fmt/chrono.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"

template <> struct fmtquill::formatter<ipc::protocol::Version_t> : formatter<string_view> {
    auto format(ipc::protocol::Version_t version, format_context& ctx) const ->format_context::iterator;
};

#pragma clang diagnostic pop
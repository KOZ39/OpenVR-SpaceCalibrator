#pragma once

#include "protocol.h"

namespace hmd {

    // Virtual Desktop functionality is only on Windows, on Linux these are no-ops as the functionality is stubbed.
    bool initVD();
    ipc::protocol::VD_HmdModel VD_getHmdModel();
    bool VD_isStageTrackingEnabled();
}
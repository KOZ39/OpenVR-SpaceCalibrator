#pragma once

#include <inttypes.h>
#include <string>

namespace spacecal {
namespace bluetooth {
    constexpr uint8_t k_INVALID_BASE_STATION_CHANNEL = 0xFF;

// range is 1, 2, 3, ..., 14, 15, 16
#define IS_BASE_STATION_20_CHANNEL_VALID(x) ((x) != ::spacecal::bluetooth::k_INVALID_BASE_STATION_CHANNEL && (x) > 0 && (x) < 17)

    enum EPowerState_t : uint8_t {
        PowerState_Unknown = 0xFF,
        PowerState_Sleep = 0x00,
        PowerState_Standby = 0x02,
        PowerState_Awake_TooOldFirmware = 0x03,
        PowerState_Awake_From_Sleep = 0x09, // cmd to put bs 2.0 to sleep
        PowerState_Awake_From_Standby = 0x0B,
    };

    enum EBaseStationType_t : uint8_t {
        BaseStationType_10,
        BaseStationType_20,
        BaseStationType_Unknown,
    };

    enum EBaseStation20_StandbySupport_t : uint8_t {
        StandbySupport_Unknown,
        StandbySupport_Supported,
        StandbySupport_Unavailable,
    };

    struct BaseStation_t {
        std::string szSerialNumber; // LHB-XXXXXXXX
        std::string szMacAddress;

        // On 1.0s : char denoting A, B, or C (actually invalid since its not enumerated at all over BLE)
        // On 2.0s : 1-16
        uint8_t channel = k_INVALID_BASE_STATION_CHANNEL;

        EBaseStationType_t eType = EBaseStationType_t::BaseStationType_Unknown;
        EPowerState_t powerState = EPowerState_t::PowerState_Unknown;
        EBaseStation20_StandbySupport_t firmwareSupportsStandby = EBaseStation20_StandbySupport_t::StandbySupport_Unknown;

        bool isConnected = false;
        bool isFirmwareCooked = false; // if true the base station is not gonna be usable
    };

    bool is_bluetooth_available();
    bool init_base_station_management();
    bool shutdown_base_station_management();
    size_t get_base_station_count();
    BaseStation_t get_base_station(size_t index);
    bool set_base_station_channel(size_t index, uint8_t channel);
    bool set_base_station_power_state(size_t index, EPowerState_t state);

    bool set_all_base_station_power_state(EPowerState_t state);
    bool auto_assign_base_station_channels();
    bool do_base_station_channels_collide();
} // bluetooth
} // spacecal
#include "base_station_management.h"
#include "platform.h"
#include "log.h"
#include "util.h"
#include <simplecble/simplecble.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <string.h>
#include <mutex>

// use to dump hex data to log
// @NOTE: makes log files significantly larger!
// #define BLUETOOTH_LOGGING_VERBOSE

#if OS_WINDOWS

// increase BLE poll rate for speed on windows
// https://stackoverflow.com/a/37328965
#include <windows.h>
#include <Bthsdpdef.h>
#include <BluetoothAPIs.h>

const DWORD IOCTL_BLE_SCAN_FREQUENCY = 0x41118C;

typedef struct ScanRequest_t {
    uint32_t unknown1;
    uint32_t scan_type; // 0 = passive, 1 = active
    uint32_t unknown2;
    uint16_t scan_interval;
    uint16_t scan_window;
    uint32_t unknown3[2];
} ScanRequest_t;

#endif // OS_WINDOWS

namespace spacecal {
    namespace bluetooth {

        struct BaseStationInternal_t {
            BaseStation_t base_station;

            simpleble_peripheral_t hBtDevice = nullptr;
            bool isOldFirmware = false;

            uint8_t targetChannel = 0xFF;
            bool isChannelDirty = false;

            EPowerState_t targetPowerState = PowerState_Unknown;
            bool isPowerDirty = false;
        };

        struct BaseStationManagementState_t {
            std::vector<simpleble_adapter_t> aAdapters;
            std::vector<BaseStationInternal_t> aTrackedBaseStations;
            std::mutex mutexBaseStationList;
            size_t _itersElapsed = 0;
            bool bStationsChannelCollision = false;
        };

        BaseStationManagementState_t g_base_station_state;

        // Base station 1.0 GATT characteristics
        constexpr simpleble_uuid_t BASE_STATION_1_POWER_SERVICE_UUID = { "0000cb00-0000-1000-8000-00805f9b34fb" };
        constexpr simpleble_uuid_t BASE_STATION_1_POWER_CHARACTERISTIC_UUID = { "0000cb01-0000-1000-8000-00805f9b34fb" };

        // Base station 2.0 GATT characteristics
        constexpr simpleble_uuid_t BASE_STATION_2_SERVICE_UUID = { "00001523-1212-efde-1523-785feabcd124" };
        constexpr simpleble_uuid_t BASE_STATION_2_MODE_CHARACTERISTIC_UUID = { "00001524-1212-efde-1523-785feabcd124" };
        constexpr simpleble_uuid_t BASE_STATION_2_IDENTIFY_CHARACTERISTIC_UUID = { "00008421-1212-efde-1523-785feabcd124" };
        constexpr simpleble_uuid_t BASE_STATION_2_POWER_CHARACTERISTIC_UUID = { "00001525-1212-efde-1523-785feabcd124" };

        constexpr uint8_t k_LIGHTHOUSE_10_PING_MAGIC = 0x12;
        const uint32_t k_LIGHTHOUSE_10_GENERIC_ID = 0xFFFFFFFF;

        // we need a delay before we can send commands until bluetooth fully initialises
        constexpr size_t k_MIN_ITERS_BEFORE_COMMS = 60;
        constexpr size_t k_ESTIMATED_MAX_BASE_STATION_COUNT = 32;

        // Should do something similar for BS 1.0s
        struct BaseStation20_InfoState {
            char header[2];
            uint8_t channel;
            uint8_t unk1;
            uint8_t power_state;
            uint8_t unk3;
            uint8_t unk4;
            uint8_t footer;
        };

        // to turn on from standby -> 
        enum Lighthouse10Command_t : uint8_t {
            LH10Cmd_PowerOn = 0x00,
            LH10Cmd_Standby = 0x01,
            LH10Cmd_Wakeup = 0x02,
        };

        enum Lighthouse10Channel_t : uint8_t {
            LH10Chn_A = 0x00,
            LH10Chn_B = 0x01,
            LH10Chn_C = 0x02,
        };

        // 1.0 power packet
        struct Lighthouse10_PowerPacket_t {
            uint8_t magic = k_LIGHTHOUSE_10_PING_MAGIC;
            uint8_t command;

            uint16_t timeout; // big endian, seconds
            uint32_t id = k_LIGHTHOUSE_10_GENERIC_ID;

            uint8_t _tail[12];
        };

        bool is_base_station_10(char* identifier) {
            if (!identifier) { return false; }
            size_t len = strlen(identifier);
            if (len >= 13) {
                if (strncmp(identifier, "HTC BS ", 7) != 0) {
                    return false;
                }
                for (int i = 7; i < 13; ++i) {
                    if (!isxdigit(identifier[i])) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        bool is_base_station_20(char* identifier) {
            if (!identifier) { return false; }
            size_t len = strlen(identifier);
            if (len >= 12) {
                if (strncmp(identifier, "LHB-", 4) != 0) {
                    return false;
                }
                for (int i = 4; i < 12; ++i) {
                    if (!isxdigit(identifier[i])) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

// range is 1, 2, 3, ..., 14, 15, 16
#define IS_BASE_STATION_20_CHANNEL_VALID(x) ((x) != k_INVALID_BASE_STATION_CHANNEL && (x) > 0 && (x) < 17)

        // appends or updates an entry in g_base_station_state.aTrackedBaseStations
        void add_base_station_to_collection(const BaseStationInternal_t& baseStation) {

            bool foundStation = false;
            for (size_t i = 0; i < g_base_station_state.aTrackedBaseStations.size() && !foundStation; i++) {
                if (g_base_station_state.aTrackedBaseStations[i].base_station.szSerialNumber == baseStation.base_station.szSerialNumber) {
                    // already present, update this and exit
                    g_base_station_state.aTrackedBaseStations[i] = baseStation;
                    foundStation = true;
                }
            }

            if (!foundStation) {
                g_base_station_state.aTrackedBaseStations.push_back(baseStation);
            }

            // channels are unknown with 1.0 base stations, so this only matters for 2.0s
            bool bCollisionDetected = false;
            uint16_t activeChannelsMask = 0; // bitwise mask; we have 16 channels on 2.0s so only up to 16 unique bits (slots) to use

            // build a bitwise mask of the occupied channels, if one is set more than once 
            for (const auto& tracked : g_base_station_state.aTrackedBaseStations) {
                if (tracked.base_station.eType == BaseStationType_20) {
                    auto channel = tracked.base_station.channel;
                    if (IS_BASE_STATION_20_CHANNEL_VALID(channel)) {
                        uint16_t channelBit = (1U << (channel - 1));
                        if (activeChannelsMask & channelBit) {
                            bCollisionDetected = true;
                            break;
                        }
                        activeChannelsMask |= channelBit;
                    }
                }
            }
            g_base_station_state.bStationsChannelCollision = bCollisionDetected;
        }

        // returns an entry from g_base_station_state.aTrackedBaseStations or nullptr
        BaseStationInternal_t* find_base_station_from_collection(char* szSerialNumber) {
            bool foundStation = false;

            for (size_t i = 0; i < g_base_station_state.aTrackedBaseStations.size() && !foundStation; i++) {
                if (strcmp(g_base_station_state.aTrackedBaseStations[i].base_station.szSerialNumber.data(), szSerialNumber) == 0) {
                    // already present, update this and exit
                    return &g_base_station_state.aTrackedBaseStations[i];
                }
            }

            return nullptr;
        }

        void _ble_callbackStart(simpleble_adapter_t adapter, void* userdata) {
            platform::setThreadName("BluetoothPollThread");
        }
        void _ble_callbackStop(simpleble_adapter_t adapter, void* userdata) {}

        void _ble_callbackDevUpdate(simpleble_adapter_t adapter, simpleble_peripheral_t peripheral, void* userdata) {
            // @NOTE: this runs on bt thread
            BaseStationManagementState_t* base_station_state = (BaseStationManagementState_t*)userdata;

            // see if present in list
            char* devDisplayName = simpleble_peripheral_identifier(peripheral); // SN: LHB-XXXXXXXX : HTC BS XXXXXX
            char* devAddr = simpleble_peripheral_address(peripheral); // mac address
            size_t manDataCount = simpleble_peripheral_manufacturer_data_count(peripheral);

            // connection state
            bool isConnected = false;
            bool canConnect = false;
            simpleble_err_t err = simpleble_peripheral_is_connected(peripheral, &isConnected);
            err = simpleble_peripheral_is_connectable(peripheral, &canConnect);

            BaseStationInternal_t* pBaseStation = find_base_station_from_collection(devDisplayName);

            if (is_base_station_20(devDisplayName)) {
                // update state
                uint8_t baseStationChannel = k_INVALID_BASE_STATION_CHANNEL;
                EPowerState_t baseStationPowerState = PowerState_Unknown;
                bool isOnOldFirmware = false;

                for (size_t i = 0; i < manDataCount; i++) {
                    simpleble_manufacturer_data_t manufacturerData = { 0 };
                    simpleble_err_t err = simpleble_peripheral_manufacturer_data_get(peripheral, i, &manufacturerData);
                    if (err == SIMPLEBLE_SUCCESS) {
                        // The data stored is base station state
                        BaseStation20_InfoState* infoState = (BaseStation20_InfoState*)manufacturerData.data;

                        baseStationChannel = infoState->channel;
                        baseStationPowerState = (EPowerState_t)infoState->power_state;
                        isOnOldFirmware = infoState->power_state == PowerState_Awake_TooOldFirmware;

#if defined(BLUETOOTH_LOGGING_VERBOSE)
                        std::string szVerboseBaseStationState = fmt::format("MN: Got {} bytes for {} ({}) :: {:02X}",
                            manufacturerData.data_length,
                            devDisplayName,
                            manufacturerData.manufacturer_id,
                            fmt::join(manufacturerData.data, manufacturerData.data + manufacturerData.data_length, " "));

                        LOG_BLUETOOTH_INFO("{}", szVerboseBaseStationState);
#endif // BLUETOOTH_LOGGING_VERBOSE


                        // push to collection
                        add_base_station_to_collection({
                            .base_station = {
                                .szSerialNumber = devDisplayName,
                                .szMacAddress = devAddr,

                                .channel = baseStationChannel,
                                .eType = EBaseStationType_t::BaseStationType_20,
                                .powerState = baseStationPowerState,

                                .isConnected = isConnected,
                            },

                            .hBtDevice = peripheral,
                            .isOldFirmware = isOnOldFirmware,
                            });
                    }
                }

                if (base_station_state->_itersElapsed < k_MIN_ITERS_BEFORE_COMMS) {
                    base_station_state->_itersElapsed++;
                    goto dev_find_cleanup;
                }

                // write cmds
                if (pBaseStation) {
                    if (pBaseStation->isChannelDirty) {
                        if (!isConnected && canConnect) {
                            err = simpleble_peripheral_connect(peripheral);
                            err = simpleble_peripheral_is_connected(peripheral, &isConnected);
                            if (err == SIMPLEBLE_SUCCESS && isConnected) {
                                LOG_BLUETOOTH_INFO("Connected to base station {}", devDisplayName);
                            }
                        }

                        if (isConnected) {
                            size_t serviceCount = simpleble_peripheral_services_count(peripheral);
                            std::vector<simpleble_service_t> bufServices(serviceCount);
                            for (size_t i = 0; i < serviceCount; i++) {
                                err = simpleble_peripheral_services_get(peripheral, i, &bufServices[i]);
                            }

                            uint8_t writeData[] = { pBaseStation->targetChannel };
                            size_t writeDataSize = sizeof(writeData) / sizeof(writeData[0]);
                            err = simpleble_peripheral_write_command(peripheral, BASE_STATION_2_SERVICE_UUID, BASE_STATION_2_MODE_CHARACTERISTIC_UUID, writeData, writeDataSize);
                            if (err == SIMPLEBLE_SUCCESS) {
                                pBaseStation->isChannelDirty = false;
                            }

                            err = simpleble_peripheral_disconnect(peripheral);
                            if (err != SIMPLEBLE_SUCCESS) {
                                LOG_BLUETOOTH_WARN("Failed to disconnect from base station {}...", devDisplayName);
                            }
                            else {
                                LOG_BLUETOOTH_INFO("Disconnected from base station {}!", devDisplayName);
                            }
                        }
                    }

                    if (pBaseStation->isPowerDirty) {
                        uint8_t writeData[] = { pBaseStation->targetPowerState };
                        size_t writeDataSize = sizeof(writeData) / sizeof(writeData[0]);

                        if (!isConnected && canConnect) {
                            err = simpleble_peripheral_connect(peripheral);
                            err = simpleble_peripheral_is_connected(peripheral, &isConnected);
                            if (err == SIMPLEBLE_SUCCESS && isConnected) {
                                LOG_BLUETOOTH_INFO("Connected to base station {}", devDisplayName);
                            }
                        }

                        if (isConnected) {
                            size_t serviceCount = simpleble_peripheral_services_count(peripheral);
                            std::vector<simpleble_service_t> bufServices(serviceCount);
                            for (size_t i = 0; i < serviceCount; i++) {
                                err = simpleble_peripheral_services_get(peripheral, i, &bufServices[i]);
                            }

                            bool isOldFirmware = pBaseStation->isOldFirmware;
                            bool foundPowerCharacteristic = false;
                            for (size_t i = 0; i < serviceCount && !foundPowerCharacteristic; i++) {
                                if (strcmp(bufServices[i].uuid.value, BASE_STATION_2_SERVICE_UUID.value) == 0) {
                                    // found base station 2.0 service
                                    for (size_t j = 0; j < bufServices[i].characteristic_count; j++) {
                                        if (strcmp(bufServices[i].characteristics[j].uuid.value, BASE_STATION_2_POWER_CHARACTERISTIC_UUID.value) == 0) {
                                            // found base station 2.0 power characteristic
#if defined(BLUETOOTH_LOGGING_VERBOSE)
                                            LOG_BLUETOOTH_INFO("PWR CHR: read: {}, writeReq {}, writeCmd: {}, notify: {}, indicate: {}",
                                                bufServices[i].characteristics[j].can_read,
                                                bufServices[i].characteristics[j].can_write_request,
                                                bufServices[i].characteristics[j].can_write_command,
                                                bufServices[i].characteristics[j].can_notify,
                                                bufServices[i].characteristics[j].can_indicate
                                            );
#endif // BLUETOOTH_LOGGING_VERBOSE

                                            isOldFirmware = !bufServices[i].characteristics[j].can_read;

                                            foundPowerCharacteristic = true;
                                            break;
                                        }
                                    }
                                }
                            }

                            bool isValidCommand = true;

                            // 2.0s have 2 firmwares which are relavant, old and new firmware
                            // 
                            // new firmware commands and data:
                            // Awake:
                            //   0x01
                            // Standby:
                            //   0x02
                            // Sleep:
                            //   0x01 ; followed by a separate packet with 0x00
                            // 
                            // old firmware commands and data:
                            // Awake:
                            //   0x09
                            // Standby:
                            //   0x02
                            // Sleep:
                            //   0x00 ; followed by a separate packet with 0x00

                            // @TODO: Somehow figure out if this is a new FW 2.0
                            // We can query services and depending on the characteristics
                            // props of BASE_STATION_2_POWER_CHARACTERISTIC_UUID
                            // we can tell if we're on new or old FW
                            // 
                            if (!pBaseStation->isOldFirmware) {
                                switch (pBaseStation->targetPowerState) {
                                case PowerState_Awake_From_Sleep:
                                    writeData[0] = 0x01;
                                    break;
                                case PowerState_Standby:
                                    writeData[0] = 0x02;
                                    break;
                                case PowerState_Sleep:
                                    writeData[0] = 0x01;
                                    break;
                                default:
                                    isValidCommand = false;
                                    LOG_BLUETOOTH_WARN("Got invalid command {:02X} for base station {}, ignoring...", (uint8_t)pBaseStation->targetPowerState, devDisplayName);
                                    break;
                                }
                            }

                            if (isValidCommand) {
                                err = simpleble_peripheral_write_command(peripheral, BASE_STATION_2_SERVICE_UUID, BASE_STATION_2_POWER_CHARACTERISTIC_UUID, writeData, writeDataSize);
                                if (err == SIMPLEBLE_SUCCESS) {
                                    pBaseStation->isPowerDirty = false;
                                }

                                if (pBaseStation->targetPowerState == PowerState_Sleep) {
                                    // Sleep cmd requires to send 0x01 followed by 0x00
                                    writeData[0] = 0x00;
                                    err = simpleble_peripheral_write_command(peripheral, BASE_STATION_2_SERVICE_UUID, BASE_STATION_2_POWER_CHARACTERISTIC_UUID, writeData, writeDataSize);
                                    if (err == SIMPLEBLE_SUCCESS) {
                                        pBaseStation->isPowerDirty = false;
                                    }
                                }
                            }

                            err = simpleble_peripheral_disconnect(peripheral);
                            if (err != SIMPLEBLE_SUCCESS) {
                                LOG_BLUETOOTH_WARN("Failed to disconnect from base station {}...", devDisplayName);
                            }
                            else {
                                LOG_BLUETOOTH_INFO("Disconnected from base station {}!", devDisplayName);
                            }
                        }
                    }
                }
            }
            
            if (is_base_station_10(devDisplayName)) {
                // 1.0s do not leak state so we can't know what they're doing ; keep track of them and nothing else :/
                // push to collection
                add_base_station_to_collection({
                    .base_station = {
                        .szSerialNumber = devDisplayName,
                        .szMacAddress = devAddr,

                        .channel = k_INVALID_BASE_STATION_CHANNEL,
                        .eType = EBaseStationType_t::BaseStationType_10,
                        .powerState = PowerState_Unknown,

                        .isConnected = isConnected,
                    },
                    .hBtDevice = peripheral,
                    .isOldFirmware = false,
                });
            }

        dev_find_cleanup:
            simpleble_free(devDisplayName);
            simpleble_free(devAddr);
        }

        void async_set_base_station_10_power_state(size_t index, EPowerState_t target_state) {
            g_base_station_state.mutexBaseStationList.lock();

            if (index >= g_base_station_state.aTrackedBaseStations.size()) {
                g_base_station_state.mutexBaseStationList.unlock();
                return;
            }

            simpleble_peripheral_t hBtDevice = g_base_station_state.aTrackedBaseStations[index].hBtDevice;
            std::string szSerialNumber = g_base_station_state.aTrackedBaseStations[index].base_station.szSerialNumber;
            g_base_station_state.mutexBaseStationList.unlock();

            bool success = false;
            bool bConnectable = false;
            if (simpleble_peripheral_is_connectable(hBtDevice, &bConnectable) == SIMPLEBLE_FAILURE) {
                return;
            }
            if (simpleble_peripheral_connect(hBtDevice) == SIMPLEBLE_SUCCESS) {
                Lighthouse10Command_t lh10Cmd = LH10Cmd_PowerOn;
                uint16_t dwTimeout = 0;
                switch (target_state) {
                case PowerState_Awake_TooOldFirmware:
                case PowerState_Awake_From_Sleep:
                case PowerState_Awake_From_Standby:
                    lh10Cmd = LH10Cmd_PowerOn;
                    dwTimeout = 0;
                    break;
                case PowerState_Standby:
                case PowerState_Sleep:
                    lh10Cmd = LH10Cmd_Standby;
                    dwTimeout = 4;
                    break;
                }
                Lighthouse10_PowerPacket_t lighthouse_10_packet = {
                    .magic = k_LIGHTHOUSE_10_PING_MAGIC,
                    .command = lh10Cmd,
                    .timeout = make_be16(dwTimeout),
                    .id = k_LIGHTHOUSE_10_GENERIC_ID,
                };

                if (simpleble_peripheral_write_command(hBtDevice, BASE_STATION_1_POWER_SERVICE_UUID, BASE_STATION_1_POWER_CHARACTERISTIC_UUID, (const uint8_t*)&lighthouse_10_packet, sizeof(lighthouse_10_packet)) != SIMPLEBLE_SUCCESS) {
                    LOG_BLUETOOTH_WARN("Failed to set {} power state, got BLE failure", szSerialNumber);
                } else {
                    success = true;
                }

                simpleble_peripheral_disconnect(hBtDevice);
            }

            g_base_station_state.mutexBaseStationList.lock();
            if (index < g_base_station_state.aTrackedBaseStations.size()) {
                auto& final_station = g_base_station_state.aTrackedBaseStations[index];
                if (success) {
                    final_station.base_station.powerState = target_state;
                }
            }
            g_base_station_state.mutexBaseStationList.unlock();
        }

        bool is_bluetooth_available() {
            return simpleble_adapter_is_bluetooth_enabled();
        }

        bool init_base_station_management() {
            bool bSuccess = true;
            if (g_base_station_state.aAdapters.empty()) {
                g_base_station_state.aAdapters.resize(simpleble_adapter_get_count());
                g_base_station_state.aTrackedBaseStations.reserve(k_ESTIMATED_MAX_BASE_STATION_COUNT);
            }

            // Scan bluetooth adapters
            for (size_t i = 0; i < g_base_station_state.aAdapters.size(); i++) {
                g_base_station_state.aAdapters[i] = simpleble_adapter_get_handle(i);
            }

#if OS_WINDOWS
            // Tell the windows API to increase the scan frequency because I like speed
            BLUETOOTH_FIND_RADIO_PARAMS btFindRadioParam = { 0 };
            HANDLE hBtRadio = INVALID_HANDLE_VALUE;
            HBLUETOOTH_RADIO_FIND findResult = BluetoothFindFirstRadio(&btFindRadioParam, &hBtRadio);
            if (findResult != INVALID_HANDLE_VALUE) {
                ScanRequest_t scanReq = {
                    .scan_type = 0,
                    .scan_interval = 29,
                    .scan_window = 29,
                };
                DWORD dwBytesWritten = 0;
                if (!DeviceIoControl(hBtRadio, IOCTL_BLE_SCAN_FREQUENCY, &scanReq, sizeof(scanReq), NULL, 0, &dwBytesWritten, NULL)) {
                    LOG_BLUETOOTH_WARN("Failed to increase BLE scan frequency. Bluetooth management may be slower.");
                }
            }
#endif

            // Scan for stuff
            for (size_t i = 0; i < g_base_station_state.aAdapters.size(); i++) {
                simpleble_adapter_t adapter = g_base_station_state.aAdapters[i];
                bSuccess = bSuccess && (simpleble_adapter_set_callback_on_scan_start(adapter, _ble_callbackStart, (void*)&g_base_station_state) == simpleble_err_t::SIMPLEBLE_SUCCESS);
                bSuccess = bSuccess && (simpleble_adapter_set_callback_on_scan_stop(adapter, _ble_callbackStop, (void*)&g_base_station_state) == simpleble_err_t::SIMPLEBLE_SUCCESS);
                bSuccess = bSuccess && (simpleble_adapter_set_callback_on_scan_found(adapter, _ble_callbackDevUpdate, (void*)&g_base_station_state) == simpleble_err_t::SIMPLEBLE_SUCCESS);
                bSuccess = bSuccess && (simpleble_adapter_set_callback_on_scan_updated(adapter, _ble_callbackDevUpdate, (void*)&g_base_station_state) == simpleble_err_t::SIMPLEBLE_SUCCESS);

                // Scan begin scanning
                bSuccess = bSuccess && (simpleble_adapter_scan_start(adapter) == simpleble_err_t::SIMPLEBLE_SUCCESS);
            }
            return bSuccess;
        }

        bool shutdown_base_station_management() {
            bool bSuccess = true;
            for (size_t i = 0; i < g_base_station_state.aAdapters.size(); i++) {
                simpleble_adapter_t adapter = g_base_station_state.aAdapters[i];
                bSuccess = bSuccess && (simpleble_adapter_scan_stop(adapter) == simpleble_err_t::SIMPLEBLE_SUCCESS);
            }

            if (g_base_station_state.aTrackedBaseStations.size() > 0) {
                for (size_t i = 0; i < g_base_station_state.aTrackedBaseStations.size(); i++) {
                    if (g_base_station_state.aTrackedBaseStations[i].base_station.isConnected) {
                        LOG_BLUETOOTH_INFO("Disconnecting from base station {}", g_base_station_state.aTrackedBaseStations[i].base_station.szSerialNumber);
                        bSuccess = bSuccess && (simpleble_peripheral_disconnect(g_base_station_state.aTrackedBaseStations[i].hBtDevice) == simpleble_err_t::SIMPLEBLE_SUCCESS);
                    }
                }
            }

            return bSuccess;
        }

        size_t get_base_station_count() {
            std::lock_guard<std::mutex> lock(g_base_station_state.mutexBaseStationList);
            return g_base_station_state.aTrackedBaseStations.size();
        }

        BaseStation_t get_base_station(size_t index) {
            BaseStation_t station = {};
            std::lock_guard<std::mutex> lock(g_base_station_state.mutexBaseStationList);
            if (g_base_station_state.aTrackedBaseStations.size() > 0 && index < g_base_station_state.aTrackedBaseStations.size()) {
                return g_base_station_state.aTrackedBaseStations[index].base_station;
            }
            return station;
        }

        bool set_base_station_channel(size_t index, uint8_t channel) {
            std::lock_guard<std::mutex> lock(g_base_station_state.mutexBaseStationList);
            if (g_base_station_state.aTrackedBaseStations.size() > 0 && index < g_base_station_state.aTrackedBaseStations.size()) {
                if (g_base_station_state.aTrackedBaseStations[index].base_station.eType == BaseStationType_10) {
                    LOG_BLUETOOTH_WARN("Attempted to assign channel {0} on Base Station 1.0 {1}, but the operation is unsupported as it's a 1.0 Base Station! Ignoring...",
                        channel, g_base_station_state.aTrackedBaseStations[index].base_station.szSerialNumber);
                } else if (g_base_station_state.aTrackedBaseStations[index].base_station.eType == BaseStationType_20) {
                    g_base_station_state.aTrackedBaseStations[index].targetChannel = channel;
                    g_base_station_state.aTrackedBaseStations[index].isChannelDirty = true;
                }
            }
            return true;
        }

        bool set_base_station_power_state(size_t index, EPowerState_t state) {
            std::lock_guard<std::mutex> lock(g_base_station_state.mutexBaseStationList);
            if (g_base_station_state.aTrackedBaseStations.size() > 0 && index < g_base_station_state.aTrackedBaseStations.size()) {
                if (g_base_station_state.aTrackedBaseStations[index].base_station.eType == BaseStationType_10) {
                    std::thread(async_set_base_station_10_power_state, index, state).detach();
                } else if (g_base_station_state.aTrackedBaseStations[index].base_station.eType == BaseStationType_20) {
                    g_base_station_state.aTrackedBaseStations[index].targetPowerState = state;
                    g_base_station_state.aTrackedBaseStations[index].isPowerDirty = true;
                }
            }
            return true;
        }

        bool set_all_base_station_power_state(EPowerState_t state) {
            bool bResult = true;
            std::lock_guard<std::mutex> lock(g_base_station_state.mutexBaseStationList);
            for (size_t i = 0; i < g_base_station_state.aTrackedBaseStations.size(); i++) {
                bResult = bResult && set_base_station_power_state(i, state);
            }

            return bResult;
        }

        bool auto_assign_base_station_channels() {
            bool bResult = true;
            std::lock_guard<std::mutex> lock(g_base_station_state.mutexBaseStationList);

            if (!do_base_station_channels_collide()) {
                return true;
            }

            // we can only assign channels to 2.0s, so ignore all 1.0s
            int base_station_20_count = 0;
            for (size_t i = 0; i < g_base_station_state.aTrackedBaseStations.size(); i++) {
                if (g_base_station_state.aTrackedBaseStations[i].base_station.eType == BaseStationType_20) {
                    base_station_20_count++;
                }
            }

            // we shouldn't touch the channel if there's only a single 2.0 or no 2.0s
            if (base_station_20_count < 2) {
                return true;
            }

            int v2_index = 0;
            double step = (base_station_20_count > 1) ? (15.0 / (base_station_20_count - 1)) : 0.0;

            // distribute channels across channels 1 through 16
            for (size_t i = 0; i < g_base_station_state.aTrackedBaseStations.size(); i++) {
                auto& station = g_base_station_state.aTrackedBaseStations[i];
                if (station.base_station.eType != BaseStationType_20) {
                    continue;
                }

                int ideal_channel = 1;
                if (base_station_20_count > 1) {
                    ideal_channel = (int) (std::round(1.0 + (v2_index * step)));
                }

                if (ideal_channel < 1) ideal_channel = 1;
                if (ideal_channel > 16) ideal_channel = 16;

                if (station.base_station.channel != ideal_channel) {
                    station.isChannelDirty = true;
                    station.targetChannel = ideal_channel;
                }

                v2_index++;
            }

            return bResult;
        }

        bool do_base_station_channels_collide() {
            return g_base_station_state.bStationsChannelCollision;
        }
    } // bluetooth
} // spacecal
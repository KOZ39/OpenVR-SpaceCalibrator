#pragma once

#include <cstdint>
#include <inttypes.h>

#if !defined(_OPENVR_API)
#if defined(IS_DRIVER)
#include <openvr_driver.h>
#elif defined(IS_OVERLAY)
#include <openvr.h>
#endif
#endif

#if defined(_OPENVR_API) && defined(IS_OVERLAY)

namespace vr
{
    // We can't include openvr_driver.h as it will result in multiple definition of some structures.
    // However, we need to share driver-specific structures with the client application, so duplicate them
    // here.

    struct DriverPose_t
    {
        /* Time offset of this pose, in seconds from the actual time of the pose,
         * relative to the time of the PoseUpdated() call made by the driver.
         */
        double poseTimeOffset;

        /* Generally, the pose maintained by a driver
         * is in an inertial coordinate system different
         * from the world system of x+ right, y+ up, z+ back.
         * Also, the driver is not usually tracking the "head" position,
         * but instead an internal IMU or another reference point in the HMD.
         * The following two transforms transform positions and orientations
         * to app world space from driver world space,
         * and to HMD head space from driver local body space.
         *
         * We maintain the driver pose state in its internal coordinate system,
         * so we can do the pose prediction math without having to
         * use angular acceleration.  A driver's angular acceleration is generally not measured,
         * and is instead calculated from successive samples of angular velocity.
         * This leads to a noisy angular acceleration values, which are also
         * lagged due to the filtering required to reduce noise to an acceptable level.
         */
        vr::HmdQuaternion_t qWorldFromDriverRotation;
        double vecWorldFromDriverTranslation[3];

        vr::HmdQuaternion_t qDriverFromHeadRotation;
        double vecDriverFromHeadTranslation[3];

        /* State of driver pose, in meters and radians. */
        /* Position of the driver tracking reference in driver world space
         * +[0] (x) is right
         * +[1] (y) is up
         * -[2] (z) is forward
         */
        double vecPosition[3];

        /* Velocity of the pose in meters/second */
        double vecVelocity[3];

        /* Acceleration of the pose in meters/second */
        double vecAcceleration[3];

        /* Orientation of the tracker, represented as a quaternion */
        vr::HmdQuaternion_t qRotation;

        /* Angular velocity of the pose in axis-angle
         * representation. The direction is the angle of
         * rotation and the magnitude is the angle around
         * that axis in radians/second. */
        double vecAngularVelocity[3];

        /* Angular acceleration of the pose in axis-angle
         * representation. The direction is the angle of
         * rotation and the magnitude is the angle around
         * that axis in radians/second^2. */
        double vecAngularAcceleration[3];

        ETrackingResult result;

        bool poseIsValid;
        bool willDriftInYaw;
        bool shouldApplyHeadModel;
        bool deviceIsConnected;
    };

}

#endif

#define ENUM_FLAG_OPERATORS(T)                                                                                                                                                  \
    inline T operator~ (T a) { return static_cast<T>( ~static_cast<std::underlying_type<T>::type>(a) ); }                                                                       \
    inline T operator| (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) | static_cast<std::underlying_type<T>::type>(b) ); }                   \
    inline T operator& (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) & static_cast<std::underlying_type<T>::type>(b) ); }                   \
    inline T operator^ (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) ^ static_cast<std::underlying_type<T>::type>(b) ); }                   \
    inline T& operator|= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) |= static_cast<std::underlying_type<T>::type>(b) ); }   \
    inline T& operator&= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) &= static_cast<std::underlying_type<T>::type>(b) ); }   \
    inline T& operator^= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) ^= static_cast<std::underlying_type<T>::type>(b) ); }
#define ENUM_HAS_FLAGS(val, flags) ((val & flags) == flags)

namespace ipc::protocol
{
    enum Version_t {
        IPC_PROTOCOL_VER_INVALID,
        IPC_PROTOCOL_VER_1 = 1,
        IPC_PROTOCOL_VER_2,
        IPC_PROTOCOL_VER_3,
        IPC_PROTOCOL_VER_4,
        IPC_PROTOCOL_VER_5,

        IPC_PROTOCOL_CURRENT = IPC_PROTOCOL_VER_5,
    };

    enum CommandType_t : uint8_t {
        IPC_COMMAND_UNKNOWN,
        IPC_COMMAND_HANDSHAKE,
        IPC_COMMAND_SET_ALIGNMENT_SPEED_PARAMS,
        IPC_COMMAND_RESET_CALIBRATION,
        IPC_COMMAND_REQUEST_VIRTUAL_DESKTOP_PROPS,
        IPC_COMMAND_COUNT,
    };

    // these quirks enable unique behaviour depending on what device is detected, to correct for poor vendor implementations
    enum DeviceQuirks_t : uint16_t {
        QUIRK_NONE = 0,
        QUIRK_SCALE = 1 << 0, // 1 unit in this playspace is not 1 meter
        QUIRK_RECOMPUTE_VELOCITY = 1 << 1, // The velocity of this device is not considered to be reliable. We shall thus recompute it entirely from first principles.
        QUIRK_RECOMPUTE_ANGULAR_VELOCITY = 1 << 2, // The angular velocity of this device is not considered to be reliable. We shall thus recompute it entirely from first principles.
        QUIRK_HAS_WORLDSPACE_ANGULAR_VELOCITY = 1 << 3, // The angular velocity of this device is given in world space rather than object space by the vendor. We shall transform it to local space to improve SteamVR prediction.
    };
    ENUM_FLAG_OPERATORS(DeviceQuirks_t);

    enum VirtualDesktop_HmdModel
    {
        VD_HmdModel_None,
        VD_HmdModel_GearVR,
        VD_HmdModel_OculusGo,
        VD_HmdModel_OculusQuest,
        VD_HmdModel_OculusQuest2,
        VD_HmdModel_MetaQuestPro,
        VD_HmdModel_Pico4,
        VD_HmdModel_MetaQuest3,
        VD_HmdModel_MetaQuest3S,
        VD_HmdModel_Pico4Ultra,
        VD_HmdModel_SamsungMoohan,
        VD_HmdModel_PlayForDreamMR,
        VD_HmdModel_Count,
    };

    struct Command_Handshake_t {
        Version_t version = Version_t::IPC_PROTOCOL_CURRENT;
    };

    struct Command_SetDeviceTransform_t {
        // @TODO: does this make sense to perform per device or per tracking system? per system probably makes a lot more sense tbh also bc it lends itself to multiple calibrations for effectively free
        // (need to think about ux for it though lol)
    private:
        uint8_t _boolFlags = 0;
    public:
        DeviceQuirks_t quirks;
        vr::TrackedDeviceIndex_t unTargetOpenVrDeviceId = vr::k_unTrackedDeviceIndexInvalid;
        vr::TrackedDeviceIndex_t unRelativeReferenceOpenvrDeviceId = vr::k_unTrackedDeviceIndexInvalid;
        vr::HmdVector3d_t translation;
        vr::HmdQuaternion_t rotation;
        double scale = 1.0;

#define FLAG(name, bitfield) \
        inline bool name() const { \
            return (_boolFlags & bitfield) == bitfield; \
        } \
        inline void name(bool newBool) { \
            _boolFlags = (_boolFlags & ~bitfield) | (static_cast<std::uint8_t>(newBool) * bitfield); \
        }

        FLAG(enabled,               0b00000001)
        FLAG(updateTranslation,     0b00000010)
        FLAG(updateRotation,        0b00000100)
        FLAG(updateScale,           0b00001000)
        FLAG(lerpCalibrations,      0b00010000)
        FLAG(hideContinuousTracker, 0b00100000)
        FLAG(relativeCoordSystem,   0b01000000) // the calibration values are interpreted as relative to the hmd if this is set
#undef FLAG
    };

    struct Command_SetAlignmentSpeedParams_t {
        /**
         * The threshold at which we adjust the alignment speed based on the position offset
         * between current and target calibrations. Generally, we increase the speed if we go
         * above small/large, and decrease it only once it's under tiny.
         *
         * These values are expressed as distance squared
         */
        double thr_trans_tiny, thr_trans_small, thr_trans_large;

        /**
         * Similar thresholds for rotation offsets, in radians
         */
        double thr_rot_tiny, thr_rot_small, thr_rot_large;

        /**
         * The speed of alignment, expressed as a lerp/slerp factor. 1 will blend most of the way in <1 second.
         * (We actually do a lerp(s * delta_t) where s is the speed factor here)
         */
        double align_speed_tiny, align_speed_small, align_speed_large;
    };

    struct SharedData_HmdMetadata {
        bool isVirtualDesktopAvailable = false;
        bool VD_stageTrackingEnabled = false;
        VirtualDesktop_HmdModel VD_hmdModel = VD_HmdModel_None;
    };

    constexpr uint32_t k_unSharedMemoryPoseSize = vr::k_unMaxTrackedDeviceCount * sizeof(vr::DriverPose_t);
    constexpr uint32_t k_unSharedMemoryDeviceTransformSize = vr::k_unMaxTrackedDeviceCount * sizeof(protocol::Command_SetDeviceTransform_t);
    constexpr uint32_t k_unSharedMemoryElementSize = k_unSharedMemoryPoseSize + k_unSharedMemoryDeviceTransformSize + sizeof(SharedData_HmdMetadata);
    constexpr const char* k_szIpcIdentifier = "SPACE_CALIBRATOR_NOVA";
}
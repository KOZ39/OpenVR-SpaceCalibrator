#pragma once

#include "one_euro_filter.h"
#include <Eigen/Dense>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <openvr_driver.h>

namespace spacecal {
constexpr size_t k_MIN_PAIRED_SAMPLES = 30;
constexpr size_t k_MAX_DEVICE_SAMPLES = 90;
constexpr double k_MAX_LATENCY_SECONDS = 0.075;
constexpr int k_NUM_COARSE_SEARCH_STEPS = 64;
constexpr double k_CONTINUITY_BIAS_WEIGHT = 3000.0; //
constexpr double k_MIN_USEFUL_WEIGHT = 0.05; // minimum weight threshold for velocity data to be considered useful
constexpr double k_MAX_ACCEPTABLE_DIFFERENCE = 1.4; // if difference from coarse search is higher than this it will be rejected and 0 latency will be assumed
constexpr double k_LATENCY_SMOOTHING_TIME_SECONDS = 0.3;
constexpr double k_MIN_IMPROVEMENT_TO_SWITCH = 0.03;

class DeviceVelocityHistory {
public:
    struct LatencySample_t {
        double time = 0.0;
        double speed = 0.0;
    };
    void push_sample(double time, const Eigen::Vector3d& position);
    inline size_t count() const { return m_count; }
    inline uint64_t frame_counter() const { return m_frameCounter; }
    const LatencySample_t& at(size_t i) const;
    bool speed_at(double time, double& outSpeed) const;

private:
    size_t m_head = 0;
    size_t m_count = 0;
    uint64_t m_frameCounter = 0;

    double m_prevTime = 0.0;
    Eigen::Vector3d m_prevPos = Eigen::Vector3d::Zero();
    LatencySample_t m_samples[k_MAX_DEVICE_SAMPLES] = {};
};

class LatencyEstimator {
public:
    void push_pose(vr::TrackedDeviceIndex_t deviceIndex, const vr::DriverPose_t& driverPose);
    double get_latency(vr::TrackedDeviceIndex_t refIndex, vr::TrackedDeviceIndex_t targetIndex);
    inline void set_hmd_refresh_rate(double refreshRate) { m_hmdRefreshRate = refreshRate; }

private:
    struct CacheEntry_t {
        std::atomic<uint64_t> referenceFrameCount = 0;
        std::atomic<uint64_t> targetFrameCount = 0;
        std::atomic<double> latency = 0.0;
        std::atomic<bool> hasEstimate = false;
        std::atomic<bool> claimed = false;
        double lastTimeSeconds = 0.0;
        OneEuro1D smoother;
    };

    double compute_latency(const DeviceVelocityHistory& ref, const DeviceVelocityHistory& target, double priorLatency) const;
    double weighted_mean_of(const std::vector<double>& values, const std::vector<double>& weights, double totalWeight) const;
    double weighted_standard_deviation_of(const std::vector<double>& values, const std::vector<double>& weights, double totalWeight, double mean) const;
    double curve_difference_at(const DeviceVelocityHistory& ref, const DeviceVelocityHistory& target, double deltaTime) const;
    double regularized_difference_at(const DeviceVelocityHistory& ref, const DeviceVelocityHistory& target, double deltaTime, double priorLatency) const;

    double m_hmdRefreshRate = 1.0 / 90.0;
    std::array<DeviceVelocityHistory, vr::k_unMaxTrackedDeviceCount> m_histories {};
    CacheEntry_t m_cache[vr::k_unMaxTrackedDeviceCount][vr::k_unMaxTrackedDeviceCount] = {};
};
}
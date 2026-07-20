#include "audio.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define NOMINMAX
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace {

constexpr float kSampleRate = 48000.0f;
constexpr float kPi = 3.14159265358979323846f;

constexpr int kEngineCylinders = 8;
constexpr float kEngineMasterGain = 0.50f;

constexpr float kIdleRpm = 900.0f;
constexpr float kRedlineRpm = 6000.0f;
constexpr float kAudioRpmScale = 0.62f;
constexpr float kIdleAudioScale = 0.65f;
constexpr float kIdleWobbleLpHz = 1.0f;
constexpr float kIdleWobbleDepth = 0.12f;
constexpr float kThrottleSmoothRate = 3.5f;

constexpr float kPulseWidthFrac = 0.60f;
constexpr float kPulseWidthMinS = 0.002f;
constexpr float kPulseWidthMaxS = 0.016f;
constexpr float kPulseAttackFrac = 0.35f;
constexpr float kPulseAmpJitterIdle = 0.28f;
constexpr float kPulseAmpJitterHigh = 0.10f;
constexpr float kFireTimingJitterIdle = 0.07f;
constexpr float kFireTimingJitterHigh = 0.02f;
constexpr float kCylinderAmps[kEngineCylinders] = {
    1.00f, 0.88f, 1.10f, 0.94f, 1.06f, 0.90f, 1.08f, 0.96f};
constexpr float kCylinderIntervals[kEngineCylinders] = {
    0.86f, 1.16f, 0.94f, 1.06f, 1.12f, 0.88f, 1.04f, 0.94f};
constexpr float kCombustionNoiseGain = 1.0f;
constexpr float kCombustionNoiseLpHz = 1800.0f;
constexpr float kEngineToneLpIdleHz = 900.0f;
constexpr float kEngineToneLpHighHz = 6500.0f;

constexpr int kNumResonators = 3;
constexpr float kResonatorFreqsHz[kNumResonators] = {95.0f, 320.0f, 900.0f};
constexpr float kResonatorQs[kNumResonators] = {1.5f, 2.5f, 3.5f};
constexpr float kResonatorGains[kNumResonators] = {1.2f, 0.5f, 0.25f};
constexpr float kDirectLpHz = 300.0f;
constexpr float kDirectGain = 1.4f;
constexpr float kDcBlockHz = 20.0f;

constexpr float kRoughnessLpHz = 12.0f;
constexpr float kRoughnessIdleDepth = 0.40f;
constexpr float kRoughnessHighDepth = 0.10f;

constexpr float kSlipThreshold = 1.0f;
constexpr float kSlipRange = 6.0f;
constexpr float kSlipGainAttackRate = 4.0f;
constexpr float kSlipGainReleaseRate = 14.0f;
constexpr float kSkidBaseFreqHz = 950.0f;
constexpr float kSkidFreqPerLateral = 30.0f;
constexpr float kSkidQ1 = 10.0f;
constexpr float kSkidQ2 = 8.0f;
constexpr float kSkidHarmonicRatio = 2.07f;
constexpr float kSkidHarmonicGain = 0.35f;
constexpr float kSkidFreqWobbleLpHz = 8.0f;
constexpr float kSkidFreqWobbleDepth = 0.08f;
constexpr float kSkidFlutterLpHz = 25.0f;
constexpr float kSkidFlutterDepth = 0.45f;
constexpr float kSkidLateralSmoothRate = 12.0f;
constexpr float kSkidMasterGain = 0.05f;

constexpr float kShiftDuration = 0.18f;
constexpr float kShiftThumpFreqHz = 90.0f;
constexpr float kShiftThumpDecay = 18.0f;
constexpr float kShiftClickDecay = 60.0f;
constexpr float kShiftDuckFloor = 0.12f;

constexpr float kMasterGain = 0.6f;

float nextNoise(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state) / static_cast<float>(0xFFFFFFFFu) * 2.0f - 1.0f;
}

float mixf(float a, float b, float t) {
    return a + (b - a) * t;
}

}

struct AudioEngine::Impl {
    ma_device device{};
    bool deviceInitialized = false;

    std::atomic<float> rpm{kIdleRpm};
    std::atomic<float> lateralSpeed{0.0f};
    std::atomic<bool> grounded{true};
    std::atomic<float> throttle{0.0f};
    std::atomic<uint32_t> shiftEventCounter{0};

    struct Pulse {
        float t = 0.0f;
        float attack = 0.001f;
        float life = 0.0f;
        float amp = 0.0f;
        bool active = false;
    };
    static constexpr int kMaxPulses = 4;
    Pulse pulses[kMaxPulses];
    float fireTimer = 0.0f;
    int cylinderIndex = 0;
    uint32_t pulseNoiseState = 0x243F6A88u;
    uint32_t combustionNoiseState = 0x85EBCA6Bu;

    float resonatorLow[kNumResonators] = {};
    float resonatorBand[kNumResonators] = {};
    float directLpState = 0.0f;
    float dcBlockState = 0.0f;
    float combustionLpState = 0.0f;
    float engineToneLpState = 0.0f;

    uint32_t roughnessNoiseState = 0xC2B2AE35u;
    float roughnessLpState = 0.0f;

    uint32_t idleWobbleNoiseState = 0x7F4A7C15u;
    float idleWobbleLpState = 0.0f;
    float throttleSmooth = 0.0f;

    uint32_t skidNoiseState = 0x2545F491u;
    uint32_t skidWobbleNoiseState = 0x94D049BBu;
    float skidLow1 = 0.0f;
    float skidBand1 = 0.0f;
    float skidLow2 = 0.0f;
    float skidBand2 = 0.0f;
    float skidFreqWobbleLp = 0.0f;
    float skidFlutterLp = 0.0f;
    float slipGainSmooth = 0.0f;
    float lateralAudioSmooth = 0.0f;

    uint32_t lastHandledShiftEvent = 0;
    bool shiftActive = false;
    float shiftEnvTime = 0.0f;
    uint32_t shiftNoiseState = 0x1234ABCDu;

    void renderSample(float& outSample) {
        constexpr float dt = 1.0f / kSampleRate;

        float rpmNow = rpm.load(std::memory_order_relaxed);
        float throttleNow = std::abs(throttle.load(std::memory_order_relaxed));
        float lateralNow = std::abs(lateralSpeed.load(std::memory_order_relaxed));
        bool groundedNow = grounded.load(std::memory_order_relaxed);

        uint32_t currentShiftEvent = shiftEventCounter.load(std::memory_order_relaxed);
        if (currentShiftEvent != lastHandledShiftEvent) {
            lastHandledShiftEvent = currentShiftEvent;
            shiftActive = true;
            shiftEnvTime = 0.0f;
        }

        float rpmNorm = std::clamp((rpmNow - kIdleRpm) / (kRedlineRpm - kIdleRpm), 0.0f, 1.0f);

        float idleWobbleRaw = nextNoise(idleWobbleNoiseState);
        float idleWobbleAlpha = dt / (1.0f / (2.0f * kPi * kIdleWobbleLpHz) + dt);
        idleWobbleLpState += idleWobbleAlpha * (idleWobbleRaw - idleWobbleLpState);
        float idleFade = (1.0f - rpmNorm) * (1.0f - rpmNorm);
        float idleWobble = 1.0f + idleWobbleLpState * kIdleWobbleDepth * idleFade;
        float idleSlow = mixf(kIdleAudioScale, 1.0f, rpmNorm);

        float rpmAudio = rpmNow * kAudioRpmScale * idleSlow * idleWobble;
        float firingFreq = (rpmAudio / 60.0f) * (kEngineCylinders * 0.5f);

        fireTimer -= dt;
        if (fireTimer <= 0.0f) {
            float period = 1.0f / firingFreq;
            float dur = std::clamp(period * kPulseWidthFrac, kPulseWidthMinS, kPulseWidthMaxS);
            for (Pulse& p : pulses) {
                if (!p.active) {
                    p.t = 0.0f;
                    p.attack = dur * kPulseAttackFrac;
                    p.life = dur * 2.0f;
                    float ampJitter = mixf(kPulseAmpJitterIdle, kPulseAmpJitterHigh, rpmNorm);
                    p.amp = kCylinderAmps[cylinderIndex] *
                            (1.0f + ampJitter * nextNoise(pulseNoiseState));
                    p.active = true;
                    break;
                }
            }
            float timingJitter = mixf(kFireTimingJitterIdle, kFireTimingJitterHigh, rpmNorm);
            float interval = period * kCylinderIntervals[cylinderIndex] *
                             (1.0f + timingJitter * nextNoise(pulseNoiseState));
            fireTimer += interval;
            cylinderIndex = (cylinderIndex + 1) % kEngineCylinders;
        }

        float pulseSum = 0.0f;
        for (Pulse& p : pulses) {
            if (!p.active) continue;
            float x = p.t / p.attack;
            pulseSum += p.amp * x * std::exp(1.0f - x);
            p.t += dt;
            if (p.t > p.life) p.active = false;
        }

        float combustionRaw = nextNoise(combustionNoiseState);
        float combustionAlpha = dt / (1.0f / (2.0f * kPi * kCombustionNoiseLpHz) + dt);
        combustionLpState += combustionAlpha * (combustionRaw - combustionLpState);
        float exciter = pulseSum * (1.0f + kCombustionNoiseGain * combustionLpState);

        float directAlpha = dt / (1.0f / (2.0f * kPi * kDirectLpHz) + dt);
        directLpState += directAlpha * (exciter - directLpState);
        float engineRaw = directLpState * kDirectGain;

        for (int i = 0; i < kNumResonators; ++i) {
            float f = 2.0f * std::sin(kPi * kResonatorFreqsHz[i] / kSampleRate);
            resonatorLow[i] += f * resonatorBand[i];
            float high = exciter - resonatorLow[i] - (1.0f / kResonatorQs[i]) * resonatorBand[i];
            resonatorBand[i] += f * high;
            engineRaw += resonatorBand[i] * kResonatorGains[i];
        }

        float toneHz = mixf(kEngineToneLpIdleHz, kEngineToneLpHighHz, rpmNorm);
        float toneAlpha = dt / (1.0f / (2.0f * kPi * toneHz) + dt);
        engineToneLpState += toneAlpha * (engineRaw - engineToneLpState);
        engineRaw = engineToneLpState;

        float dcAlpha = dt / (1.0f / (2.0f * kPi * kDcBlockHz) + dt);
        dcBlockState += dcAlpha * (engineRaw - dcBlockState);
        engineRaw -= dcBlockState;

        float roughnessRaw = nextNoise(roughnessNoiseState);
        float roughnessAlpha = dt / (1.0f / (2.0f * kPi * kRoughnessLpHz) + dt);
        roughnessLpState += roughnessAlpha * (roughnessRaw - roughnessLpState);
        float roughnessDepth = mixf(kRoughnessIdleDepth, kRoughnessHighDepth, rpmNorm);
        engineRaw *= 1.0f + roughnessLpState * roughnessDepth;

        throttleSmooth += (throttleNow - throttleSmooth) * std::min(1.0f, kThrottleSmoothRate * dt);
        float engineLevel = (0.42f + 0.30f * throttleSmooth) * (0.8f + 0.4f * rpmNorm);
        float engineDuck = 1.0f;
        float shiftSample = 0.0f;
        if (shiftActive) {
            if (shiftEnvTime < kShiftDuration) {
                float thump = std::sin(2.0f * kPi * kShiftThumpFreqHz * shiftEnvTime) *
                              std::exp(-shiftEnvTime * kShiftThumpDecay);
                float clickEnv = std::exp(-shiftEnvTime * kShiftClickDecay);
                float clickNoise = nextNoise(shiftNoiseState) * clickEnv;
                shiftSample = thump * 0.5f + clickNoise * 0.4f;
                engineDuck = kShiftDuckFloor + (1.0f - kShiftDuckFloor) *
                             std::min(1.0f, shiftEnvTime / kShiftDuration);
                shiftEnvTime += dt;
            } else {
                shiftActive = false;
            }
        }

        float engineSample = engineRaw * engineLevel * engineDuck * kEngineMasterGain;

        float slipTargetGain = groundedNow
            ? std::clamp((lateralNow - kSlipThreshold) / kSlipRange, 0.0f, 1.0f)
            : 0.0f;
        float slipRate = slipTargetGain > slipGainSmooth ? kSlipGainAttackRate : kSlipGainReleaseRate;
        slipGainSmooth += (slipTargetGain - slipGainSmooth) * std::min(1.0f, slipRate * dt);

        float skidWobbleRaw = nextNoise(skidWobbleNoiseState);
        float freqWobbleAlpha = dt / (1.0f / (2.0f * kPi * kSkidFreqWobbleLpHz) + dt);
        skidFreqWobbleLp += freqWobbleAlpha * (skidWobbleRaw - skidFreqWobbleLp);
        float flutterAlpha = dt / (1.0f / (2.0f * kPi * kSkidFlutterLpHz) + dt);
        skidFlutterLp += flutterAlpha * (nextNoise(skidWobbleNoiseState) - skidFlutterLp);

        lateralAudioSmooth += (lateralNow - lateralAudioSmooth) *
                              std::min(1.0f, kSkidLateralSmoothRate * dt);
        float skidCenterFreq = (kSkidBaseFreqHz + lateralAudioSmooth * kSkidFreqPerLateral) *
                               (1.0f + skidFreqWobbleLp * kSkidFreqWobbleDepth);
        float noiseIn = nextNoise(skidNoiseState);

        float f1 = 2.0f * std::sin(kPi * skidCenterFreq / kSampleRate);
        skidLow1 += f1 * skidBand1;
        float skidHigh1 = noiseIn - skidLow1 - (1.0f / kSkidQ1) * skidBand1;
        skidBand1 += f1 * skidHigh1;

        float f2 = 2.0f * std::sin(kPi * std::min(skidCenterFreq * kSkidHarmonicRatio,
                                                  kSampleRate * 0.24f) / kSampleRate);
        skidLow2 += f2 * skidBand2;
        float skidHigh2 = noiseIn - skidLow2 - (1.0f / kSkidQ2) * skidBand2;
        skidBand2 += f2 * skidHigh2;

        float flutter = 1.0f + skidFlutterLp * kSkidFlutterDepth;
        float skidSample = (skidBand1 + skidBand2 * kSkidHarmonicGain) *
                           flutter * slipGainSmooth * kSkidMasterGain;

        float mixed = (engineSample + shiftSample * 0.5f + skidSample) * kMasterGain;
        outSample = std::tanh(mixed);
    }

    static void dataCallback(ma_device* device, void* output, const void* /*input*/, ma_uint32 frameCount) {
        auto* impl = static_cast<Impl*>(device->pUserData);
        auto* out = static_cast<float*>(output);
        ma_uint32 channels = device->playback.channels;
        for (ma_uint32 i = 0; i < frameCount; ++i) {
            float sample = 0.0f;
            impl->renderSample(sample);
            for (ma_uint32 c = 0; c < channels; ++c) {
                out[i * channels + c] = sample;
            }
        }
    }
};

AudioEngine::AudioEngine() : impl_(std::make_unique<Impl>()) {}
AudioEngine::~AudioEngine() { cleanup(); }

void AudioEngine::init() {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = static_cast<ma_uint32>(kSampleRate);
    config.dataCallback = Impl::dataCallback;
    config.pUserData = impl_.get();

    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) {
        fprintf(stderr, "failed to initialize audio device\n");
        return;
    }
    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        fprintf(stderr, "failed to start audio device\n");
        ma_device_uninit(&impl_->device);
        return;
    }
    impl_->deviceInitialized = true;
}

void AudioEngine::cleanup() {
    if (impl_ && impl_->deviceInitialized) {
        ma_device_uninit(&impl_->device);
        impl_->deviceInitialized = false;
    }
}

void AudioEngine::update(float engineRpm, float lateralSpeed, bool grounded, float throttle) {
    impl_->rpm.store(engineRpm, std::memory_order_relaxed);
    impl_->lateralSpeed.store(lateralSpeed, std::memory_order_relaxed);
    impl_->grounded.store(grounded, std::memory_order_relaxed);
    impl_->throttle.store(throttle, std::memory_order_relaxed);
}

void AudioEngine::triggerShift() {
    impl_->shiftEventCounter.fetch_add(1, std::memory_order_relaxed);
}

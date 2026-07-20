#include "noise.h"

#include <cmath>

namespace noise {

namespace {

uint32_t hash2i(int x, int z, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(z) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

} // namespace

float hashFloat(int x, int z, uint32_t seed) {
    return static_cast<float>(hash2i(x, z, seed) & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

float valueNoise(float x, float z, uint32_t seed) {
    int x0 = static_cast<int>(std::floor(x));
    int z0 = static_cast<int>(std::floor(z));
    int x1 = x0 + 1, z1 = z0 + 1;
    float tx = smoothstep(x - static_cast<float>(x0));
    float tz = smoothstep(z - static_cast<float>(z0));

    float v00 = hashFloat(x0, z0, seed);
    float v10 = hashFloat(x1, z0, seed);
    float v01 = hashFloat(x0, z1, seed);
    float v11 = hashFloat(x1, z1, seed);

    float a = v00 + (v10 - v00) * tx;
    float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * tz;
}

float fbm(float x, float z, int octaves, uint32_t seedBase) {
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float sum = 0.0f;
    float maxAmp = 0.0f;
    for (int octave = 0; octave < octaves; ++octave) {
        uint32_t seed = (static_cast<uint32_t>(octave) * 101u + 7u) ^ seedBase;
        sum += valueNoise(x * frequency, z * frequency, seed) * amplitude;
        maxAmp += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.03f;
    }
    return maxAmp > 0.0f ? sum / maxAmp : 0.0f;
}

float ridged(float x, float z, uint32_t seed) {
    return 1.0f - std::fabs(2.0f * valueNoise(x, z, seed) - 1.0f);
}

float fbmRidged(float x, float z, int octaves, uint32_t seedBase) {
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float sum = 0.0f;
    float maxAmp = 0.0f;
    for (int octave = 0; octave < octaves; ++octave) {
        uint32_t seed = (static_cast<uint32_t>(octave) * 131u + 17u) ^ seedBase;
        sum += ridged(x * frequency, z * frequency, seed) * amplitude;
        maxAmp += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.11f;
    }
    return maxAmp > 0.0f ? sum / maxAmp : 0.0f;
}

} // namespace noise

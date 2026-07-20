#pragma once

#include <cstdint>

namespace noise {

float hashFloat(int x, int z, uint32_t seed);
float valueNoise(float x, float z, uint32_t seed);

float fbm(float x, float z, int octaves, uint32_t seedBase = 0u);

float ridged(float x, float z, uint32_t seed);
float fbmRidged(float x, float z, int octaves, uint32_t seedBase = 0u);

}

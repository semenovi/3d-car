#pragma once

#include <cstdint>

// Small collection of deterministic, hash-based noise primitives (no external
// noise library needed). Shared by landscape.cpp (terrain shape) and
// roads.cpp (road wander).
namespace noise {

float hashFloat(int x, int z, uint32_t seed);
float valueNoise(float x, float z, uint32_t seed);

// Fractal sum of valueNoise octaves, normalized to roughly [0, 1].
// seedBase differentiates independent fbm "layers" (hills vs. mountains vs.
// ravines) so they don't all line up with each other.
float fbm(float x, float z, int octaves, uint32_t seedBase = 0u);

// "Ridged" noise: sharp ridges near 1.0, smooth valleys near 0.0 - good for
// mountain ridgelines and carved ravines.
float ridged(float x, float z, uint32_t seed);
float fbmRidged(float x, float z, int octaves, uint32_t seedBase = 0u);

} // namespace noise

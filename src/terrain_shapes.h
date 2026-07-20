#pragma once

// Raw mountain/ravine shaping, shared by landscape.cpp (which sculpts the
// actual terrain height) and roads.cpp (which steers roads away from tall
// terrain). Kept in its own module - rather than living inside landscape.cpp,
// which already depends on roads.h for the road/terrain height blend - so
// roads.cpp can read "how mountainous is it over there" without landscape.h
// and roads.h ending up including each other.
namespace terrain_shapes {

// Broad, slow-varying 0..1 "how much of a mountain/ravine region is this"
// signal - smooth over hundreds of meters, meant for steering roads away well
// before they'd actually reach the rough ridge detail below.
float mountainRegionMask(float worldX, float worldZ);
float ravineRegionMask(float worldX, float worldZ);

// Full raw height contribution, ridge detail included, *before* any road
// fade-out is applied: mountains as a positive addend, ravines as a positive
// depth the caller subtracts.
float mountainHeight(float worldX, float worldZ);
float ravineDepth(float worldX, float worldZ);

} // namespace terrain_shapes

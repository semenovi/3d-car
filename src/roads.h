#pragma once

// A network of gently wandering roads laid out on an infinite grid (lines of
// constant world X, and lines of constant world Z), spaced kSpacing apart.
//
// Why a grid: driving in *any* direction from *any* point, you cross a line
// of one family or the other within at most kSpacing * sqrt(2) (the diagonal
// worst case) - and at most kSpacing if you happen to be driving parallel to
// one axis. At the truck's top speed (~22 m/s) that's roughly 30-40s in the
// straight-line worst case, comfortably inside the requested 30-80s window
// once normal driving (accelerating, turning, slightly imperfect headings)
// is accounted for. The wander amplitude is kept small relative to kSpacing
// so it doesn't meaningfully change that bound - it just keeps the roads from
// looking like a rigid, obviously artificial grid.
namespace roads {

inline constexpr float kSpacing = 650.0f;
inline constexpr float kHalfWidth = 4.0f;      // ~8m wide - proportionate to the truck, not a highway
inline constexpr float kEdgeSoftness = 3.0f;   // blend width of the road surface itself
inline constexpr float kGradingRadius = 35.0f; // mountains/ravines fade out over this distance from a road

// Distance (world units) from (worldX, worldZ) to the nearest road centerline.
float distanceToNearestRoad(float worldX, float worldZ);

// 1.0 exactly on the road surface, fading to 0.0 over kEdgeSoftness - used to
// flatten the road itself to a smooth height.
float surfaceMask(float distance);

// 1.0 on the road, fading to 0.0 over the much wider kGradingRadius - used to
// fade out mountains/ravines on the approach to a road so surfaceMask's sharp
// blend never has to bridge a cliff.
float gradingMask(float distance);

} // namespace roads

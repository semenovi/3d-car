#pragma once

#include <glm/glm.hpp>

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
inline constexpr float kHalfWidth = 4.0f;    // ~8m wide - proportionate to the truck, not a highway
// Width of the shoulder blending the road into wild terrain. Kept close to
// the terrain grid's own cell size (see Terrain::kCellSize) on purpose: the
// road is "read" off this same coarse grid (see terrain.cpp's roadMask-based
// brightness), so a blend much wider than a couple of grid cells spreads the
// transition so thin across many edges that no single one of them is bright
// enough to actually read as a road boundary.
inline constexpr float kBlendRadius = 10.0f;

// Distance (world units) from (worldX, worldZ) to the nearest road centerline.
float distanceToNearestRoad(float worldX, float worldZ);

// Distance to the nearest road of just one family (vertical = constant-X
// lines, horizontal = constant-Z lines). Used by road_mesh.cpp to detect when
// a road it's tracing is approaching a *different* (perpendicular) road, so
// it can stop drawing its own edge lines there - two roads' edges crossing
// through each other at a junction looks like a broken spiderweb, not an
// intersection.
float distanceToNearestVerticalRoad(float worldX, float worldZ);
float distanceToNearestHorizontalRoad(float worldX, float worldZ);

// Wandering center-X of the vertical road line with grid index i, at a given
// world Z (and the mirror for horizontal lines). Exposed so callers can trace
// an actual road (for spawning on one, or drawing it) instead of only asking
// "how far is the nearest road" via distanceToNearestRoad.
float verticalLineX(int i, float worldZ);
float horizontalLineZ(int j, float worldX);

// 1.0 exactly on the road surface, smoothly fading to 0.0 over kBlendRadius.
// landscape.cpp uses a *single* mask for both flattening the road surface and
// suppressing mountains/ravines on the shoulder leading up to it - using two
// differently-sized falloffs for those (an earlier version of this) leaves a
// gap where the surface is already fully flattened but the terrain height
// hasn't finished fading yet, i.e. a cliff right next to the road.
float mask(float distance);

// If (worldX, worldZ) is close enough to a road's actual edge (within
// kSnapBand of it), returns the point pulled exactly onto that edge - moved
// only sideways (perpendicular to the road), never along it. Used by
// terrain.cpp to move the *terrain mesh's own grid vertices* onto the road
// boundary while generating a chunk, so the road's outline is traced by real
// mesh edges instead of separate overlay geometry that has to be biased to
// avoid depth-fighting against the terrain. `snapped` is false (x/z
// unchanged) when the point isn't near any edge.
struct EdgeSnap {
    float x, z;
    bool snapped;
    bool vertical; // true if snapped to a vertical (constant-X) road, false if horizontal
    float side;    // +1 or -1: which of the road's two edges it snapped to
};
EdgeSnap snapToRoadEdge(float worldX, float worldZ);

} // namespace roads

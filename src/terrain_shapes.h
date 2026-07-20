#pragma once

namespace terrain_shapes {

float mountainRegionMask(float worldX, float worldZ);
float ravineRegionMask(float worldX, float worldZ);

float mountainHeight(float worldX, float worldZ);
float ravineDepth(float worldX, float worldZ);

}

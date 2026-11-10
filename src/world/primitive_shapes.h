#pragma once
#include "scene_objects.h"
namespace kami::world {
// Generates only basic, uncut, untwisted SL volume primitives when their
// actual path/profile wire parameters are known. All other shapes remain
// clearly approximate bounding-box proxies (not downloaded mesh assets).
TerrainMesh make_primitive_proxy(const SceneObject& ob);
bool basic_primitive_shape_supported(const PrimitiveShape& shape);
}

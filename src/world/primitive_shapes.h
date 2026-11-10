#pragma once
#include "scene_objects.h"
#include "texture_atlas.h"
namespace kami::world {
// Generates only basic, uncut, untwisted SL volume primitives when their
// actual path/profile wire parameters are known. All other shapes remain
// clearly approximate bounding-box proxies (not downloaded mesh assets).
TerrainMesh make_primitive_proxy(const SceneObject& ob);
bool basic_primitive_shape_supported(const PrimitiveShape& shape);
// Apply received TE face tints to vertex colors. Alpha and images are NOT
// rendered here: those require a separate material/texture pipeline.
bool tint_primitive_proxy(const SceneObject& object,TerrainMesh& mesh,const TextureAtlas* atlas=nullptr);
}

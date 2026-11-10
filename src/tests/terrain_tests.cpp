#include "world/terrain.h"
#include "world/region_protocol.h"
#include <cassert>
#include <cmath>
#include <vector>
int main() {
  using namespace kami::world;
  TerrainAssembler terrain(4,4);
  assert(!terrain.complete());
  assert(terrain.received_samples()==0);
  assert(terrain.missing_samples()==16);
  assert(!terrain.any_renderable_cells());
  assert(terrain.renderable_cells()==0);
  assert(terrain.missing_cells()==9);
  assert(terrain.renderable_coverage()==0.0f);
  float elevation=0;
  assert(!terrain.sample_height(0,0,elevation));
  assert(!terrain.sample_height_bilinear(0.5f,0.5f,elevation));
  TerrainMesh mesh;
  assert(!terrain.build_partial_render_mesh(mesh));
  assert(!terrain.build_render_mesh(mesh));
  TerrainPatch p;
  p.x=0;p.y=0;p.width=2;p.height=2;p.heights={1,2,3,4};
  assert(terrain.apply_decoded_patch(p));
  assert(terrain.received_samples()==4);
  assert(terrain.renderable_cells()==1);
  assert(terrain.missing_cells()==8);
  assert(std::fabs(terrain.renderable_coverage()-1.0f/9.0f)<.001f);
  assert(terrain.any_renderable_cells());
  assert(terrain.build_partial_render_mesh(mesh));
  assert(validate_mesh(mesh));
  assert(mesh.indices.size()==6);
  assert(terrain.sample_height_bilinear(.5f,.5f,elevation));
  assert(std::fabs(elevation-2.5f)<.001f);
  assert(terrain.sample_world_height(.5f,.5f,elevation));
  assert(std::fabs(elevation-2.5f)<.001f);
  float low=0,high=0;
  assert(terrain.min_max_received_heights(low,high));
  assert(low==1 && high==4);
  assert(terrain.patch_received(0,0,2,2));
  assert(!terrain.patch_received(0,0,3,3));
  assert(!terrain.patch_received(3,3,2,2));
  p.heights={5,6,7,8};
  assert(terrain.apply_patch_if_new(p));
  assert(terrain.sample_height(0,0,elevation) && elevation==5);
  terrain.reset();
  assert(!terrain.any_renderable_cells());
  assert(!terrain.min_max_received_heights(low,high));
  return 0;
}

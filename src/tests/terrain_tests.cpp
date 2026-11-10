#include "world/terrain.h"
#include "world/lludp.h"
#include "world/region_protocol.h"
#include <cassert>
#include <cmath>
#include <vector>
int main() {
  using namespace kami::world;
  TerrainAssembler terrain(4,4);
  const uint8_t empty_land_layer[]={0,0,16,0x4c,97};
  assert(!terrain.apply_land_layer(empty_land_layer,sizeof(empty_land_layer)));
  kami::sl::Packet unrelated;
  unrelated.payload={0x12,0x34};
  assert(!terrain.apply_land_packet(unrelated));
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
  assert(mesh.vertices.empty() && mesh.indices.empty());
  TerrainPatch p;
  p.x=0;p.y=0;p.width=2;p.height=2;p.heights={1,2,3,4};
  assert(terrain.apply_decoded_patch(p));
  assert(terrain.received_samples()==4);
  const auto initial_steps=terrain.measure_continuity(2);
  assert(initial_steps.adjacent_pairs==4);
  assert(initial_steps.largest_step_m==2);
  assert(initial_steps.patch_boundary_pairs==0);
  assert(initial_steps.largest_patch_boundary_step_m==0);
  TerrainAssembler seams(4,4);
  TerrainPatch left;
  left.x=0;left.y=0;left.width=2;left.height=2;
  left.heights={0,0,0,0};
  TerrainPatch right=left;right.x=2;
  right.heights={50,50,50,50};
  assert(seams.apply_decoded_patch(left));
  assert(seams.apply_decoded_patch(right));
  const auto discontinuity=seams.measure_continuity(2);
  assert(discontinuity.largest_patch_boundary_step_m==50);
  assert(discontinuity.largest_step_m==50);
  assert(discontinuity.patch_boundary_pairs==2);
  assert(std::fabs(terrain.completion_ratio()-.25f)<.001f);
  assert(terrain.renderable_cells()==1);
  assert(terrain.missing_cells()==8);
  assert(std::fabs(terrain.renderable_coverage()-1.0f/9.0f)<.001f);
  assert(terrain.any_renderable_cells());
  assert(terrain.build_partial_render_mesh(mesh));
  assert(validate_mesh(mesh));
  assert(mesh.indices.size()==6);
  assert(mesh.vertices.size()==4);
  assert(terrain.sample_height_bilinear(.5f,.5f,elevation));
  assert(std::fabs(elevation-2.5f)<.001f);
  assert(!terrain.sample_height_bilinear(-1.0f,.5f,elevation));
  assert(!terrain.sample_height_bilinear(2.5f,2.5f,elevation));
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
  assert(terrain.received_samples()==4);
  TerrainPatch invalid=p;
  invalid.heights={9};
  assert(!terrain.apply_decoded_patch(invalid));
  assert(!terrain.apply_patch_batch({p,invalid}));
  assert(terrain.sample_height(0,0,elevation) && elevation==5);
  terrain.reset();
  assert(!terrain.any_renderable_cells());
  assert(!terrain.min_max_received_heights(low,high));
  assert(terrain.missing_samples()==16);
  assert(terrain.completion_ratio()==0.0f);
  return 0;
}

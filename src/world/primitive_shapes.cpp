#include "primitive_shapes.h"
#include <cmath>
#include <stdexcept>
namespace kami::world {
namespace {
constexpr float pi=3.14159265358979323846f;
TerrainMesh make_round_prism(const SceneObject& ob,unsigned segments) {
  TerrainMesh out;
  if(segments<3 || segments>64) return out;
  const float rx=ob.sx*0.5f,rz=ob.sy*0.5f,hy=ob.sz*0.5f;
  // Mesh coordinate convention is X, up (SL Z), SL Y.
  // Triangle cross-sections are equilateral; circular cross-sections
  // approximate cylindrical volumes using 24 sides.
  for (unsigned i=0;i<segments;++i) {
    const float angle=2.0f*pi*float(i)/float(segments);
    const float x=std::cos(angle)*rx,z=std::sin(angle)*rz;
    out.vertices.push_back({x,-hy,z,.57f,.60f,.69f});
    out.vertices.push_back({x, hy,z,.57f,.60f,.69f});
  }
  out.vertices.push_back({0,-hy,0,.57f,.60f,.69f});
  out.vertices.push_back({0, hy,0,.57f,.60f,.69f});
  const uint32_t bottom=segments*2,top=bottom+1;
  for(unsigned i=0;i<segments;++i) {
    const uint32_t a=2*i,b=2*((i+1)%segments);
    out.indices.insert(out.indices.end(),
      {a,b,a+1,a+1,b,b+1,bottom,a,b,top,b+1,a+1});
  }
  return out;
}
TerrainMesh make_sphere_proxy(const SceneObject& ob) {
  constexpr unsigned latitude=12,longitude=24;
  TerrainMesh out;
  out.vertices.reserve((latitude+1)*(longitude+1));
  out.indices.reserve(latitude*longitude*6);
  for(unsigned v=0;v<=latitude;++v) {
    const float polar=pi*float(v)/float(latitude);
    const float h=std::cos(polar)*ob.sz*0.5f;
    const float ring=std::sin(polar);
    for(unsigned u=0;u<=longitude;++u) {
      const float theta=2.f*pi*float(u)/float(longitude);
      out.vertices.push_back({ring*std::cos(theta)*ob.sx*.5f,
                              h,ring*std::sin(theta)*ob.sy*.5f,
                              .63f,.65f,.71f});
    }
  }
  for(unsigned v=0;v<latitude;++v)
    for(unsigned u=0;u<longitude;++u) {
      const uint32_t a=v*(longitude+1)+u,b=a+longitude+1;
      out.indices.insert(out.indices.end(),{a,a+1,b,b,a+1,b+1});
    }
  return out;
}
}
bool basic_primitive_shape_supported(const PrimitiveShape& p) {
  if(!p.valid || p.path_begin || p.path_end || p.profile_begin ||
     p.profile_end || p.profile_hollow ||
     p.path_shear_x || p.path_shear_y ||
     p.path_twist || p.path_twist_begin ||
     p.radius_offset || p.taper_x || p.taper_y ||
     p.skew || p.revolutions) return false;
  const uint8_t profile=p.profile_curve & 0x0fu;
  if(p.profile_curve!=profile) return false; // hollow profile variants
  if (p.path_curve==0x10u)
    return p.path_scale_x==100&&p.path_scale_y==100&&
           (profile==0 || profile==1 || profile==2 ||
            profile==3 || profile==4);
  if(p.path_curve==0x20u)
    return profile==5; // full, uncut sphere approximation
  return false;
}
TerrainMesh make_primitive_proxy(const SceneObject& ob) {
  if(!basic_primitive_shape_supported(ob.shape))
    return make_box_mesh(0,0,0,ob.sx,ob.sz,ob.sy);
  if(ob.shape.path_curve==0x20u) return make_sphere_proxy(ob);
  const auto profile=ob.shape.profile_curve&0x0fu;
  if(profile==1) return make_box_mesh(0,0,0,ob.sx,ob.sz,ob.sy);
  if(profile==0) return make_round_prism(ob,24);
  if(profile>=2&&profile<=4) return make_round_prism(ob,3);
  return make_box_mesh(0,0,0,ob.sx,ob.sz,ob.sy);
}
}

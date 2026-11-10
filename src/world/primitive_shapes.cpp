#include "primitive_shapes.h"
#include <cmath>
#include <stdexcept>
#include <utility>
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
bool tint_primitive_proxy(const SceneObject& ob,TerrainMesh& mesh,const TextureAtlas* atlas) {
  if(!ob.textures.valid || mesh.vertices.empty())return false;
  // For a validated, uncut square/linear volume the generated box index
  // stream has six contiguous groups (one per face). Duplicating vertices
  // prevents unrelated faces from sharing colors. Face-to-side orientation
  // remains an approximation until full LLVolume face generation is ported.
  const bool box=basic_primitive_shape_supported(ob.shape) &&
      ob.shape.path_curve==0x10u && ob.shape.profile_curve==0x01u &&
      mesh.vertices.size()==8 && mesh.indices.size()==36;
  if(box) {
    TerrainMesh colored;
    colored.vertices.reserve(36);
    colored.indices.reserve(36);
    for(size_t face=0;face<6;++face) {
      const auto& rgba=ob.textures.faces[face].rgba;
      const float r=float(rgba[0])/255.f,g=float(rgba[1])/255.f,
                  b=float(rgba[2])/255.f;
      for(size_t i=0;i<6;++i) {
        const uint32_t index=mesh.indices[6*face+i];
        if(index>=mesh.vertices.size())return false;
        auto v=mesh.vertices[index];
        v.r=r;v.g=g;v.b=b;
        if(atlas) {
          // Cube face pairs are stored in fixed order: +/-Z, +/-X, +/-Y.
          // The face-local coordinates are normalized before atlas mapping.
          const float sx=ob.sx>0?ob.sx:1.f;
          const float sy=ob.sy>0?ob.sy:1.f;
          const float sz=ob.sz>0?ob.sz:1.f;
          const float local_u=(face<2?(v.x/sx+0.5f):
                                face<4?(v.z/sy+0.5f):(v.x/sx+0.5f));
          const float local_v=(face<4?(v.y/sz+0.5f):(v.z/sy+0.5f));
          float slot=0,transformed_u=0,transformed_v=0;
          const auto& material=ob.textures.faces[face];
          if(transform_texture_uv(material,local_u,local_v,
                                  transformed_u,transformed_v) &&
             atlas->shader_slot(material.texture_id,slot)) {
            v.u=transformed_u;v.v=transformed_v;
            v.texture_mix=slot;
          }
        }
        colored.indices.push_back(static_cast<uint32_t>(colored.vertices.size()));
        colored.vertices.push_back(v);
      }
    }
    mesh=std::move(colored);
    return true;
  }
  // Until the exact face topology of each prim is decoded, use the default
  // TE tint rather than painting its faces with fabricated assignments.
  const auto& rgba=ob.textures.faces[0].rgba;
  for(auto& v:mesh.vertices) {
    v.r=float(rgba[0])/255.f;
    v.g=float(rgba[1])/255.f;
    v.b=float(rgba[2])/255.f;
  }
  return true;
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

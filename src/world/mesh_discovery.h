#pragma once
#include "texture_asset_queue.h"
#include <cstddef>
#include <cstdint>
namespace kami::world {
struct MeshExtra {
  AssetId mesh_id{};
  uint8_t sculpt_type=0;
  bool valid=false;
};
// Second Life LLDataPackerBinaryBuffer ExtraParams: U8 count, then
// U16 type, U32 byte length and that parameter's binary payload.
// A mesh uses type 0x60 (or sculpt 0x30) and a 17-byte payload:
// UUID followed by sculpt type 5, optionally with high flag bits.
bool find_mesh_extra(const uint8_t* bytes,size_t count,MeshExtra& result);
}

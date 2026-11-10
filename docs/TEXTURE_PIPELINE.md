# KAMI DX12 texture pipeline — implementation and constraints

1. The authenticated login result gives an opaque seed capability URL.
   KAMI requests `ViewerAsset` from that simulator endpoint using
   certificate-verified HTTPS. URLs are bearer secrets: never logged.
2. ObjectUpdate TextureEntry fields supply image asset UUIDs and per-face
   tint. The manifest deduplicates up to 2,048 IDs. Downloads are serialized
   off the render thread, bounded to 4 MiB each/32 MiB encoded in total.
3. The OpenJPEG decoder (BSD 2-clause) converts supported raw J2K and JP2
   imagery to RGBA8, with 2,048×2,048 maximum dimensions. Only full-resolution
   grayscale, RGB, or RGBA components are currently supported. Chroma
   subsampling and SYCC intentionally fail closed rather than using broken
   colors. WIC is an optional fallback, not a JPEG2000 dependency.
4. The CPU atlas holds 64 64×64-pixel tiles. Validated images are
   downsampled, placed at deterministic coordinates, and uploaded to a
   DX12 shader resource view. Basic uncut boxes can use the received
   TextureEntry face IDs to sample this atlas. Other primitives still
   show received face tint/neutral proxies.
5. The material shader uses true atlas RGB times primitive tint. It does
   not currently implement alpha blending, UV repeats, per-face texture
   transforms, sculpted or mesh UVs, mipmaps, or PBR.

OpenJPEG is compiled statically from an upstream commit *after* the
2026-02-10 `opj_j2k_read_sod` bounds fix. Its original LICENSE is copied
into the Windows artifact. This reduces one known codec risk, but decoding
untrusted media still has security and memory implications and requires
ongoing dependency updates.

Automated tests exercise real lossless JPEG2000 pixels, WIC conversion,
texture atlas allocation, primitive UV coordinates, and HLSL compilation.
**No CI test proves actual textures were visible in Second Life on a GPU.**
Test a non-sensitive region in KAMI and inspect the `caps`, `J2K`, and
`RGBA decoded` counters before claiming live image rendering success.

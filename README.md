# KAMI — Beyond Reality

Windows-only **development preview** for a clean-start Second Life viewer using DirectX 12.

The executable is not yet a complete Second Life viewer. It has a local demo scene,
a packet-to-terrain decoder and renderer, and an offline diagnostic replay option.
**Live Second Life authentication, inventory, objects and avatars remain unimplemented.**

Build on Windows with Visual Studio 2022 and CMake 3.24+:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build/src --build-config Release --output-on-failure
```

See [terrain replay instructions](docs/TERRAIN_REPLAY.md) for the offline
packet container. No password or session token should be put into a capture.

The owner decides when to release KAMI; passing CI is not a claim of
production readiness.

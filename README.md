# KAMI — Beyond Reality

KAMI is a Windows-only **experimental Second Life DX12 viewer**, not yet
a full-featured viewer. It now has an authenticating HTTPS login path, simulator
circuit and presence messages, LLUDP receive/acknowledge, and a partial terrain
mesh streaming path. It also retains an offline terrain replay and local demo.

## Building and testing

Use Visual Studio 2022 with Windows SDK and CMake 3.24+:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build/src --build-config Release --output-on-failure
```

GitHub Actions builds the development executable and runs regression tests.
A successful build only proves compilation and the simulated packet fixtures,
not successful production login or world rendering. **No user account or
live simulator was tested by CI.**

## Live connection

Enter an avatar username (single-name accounts use Resident as last name)
and password in the development preview, then click Connect.
The request uses WinHTTP over verified HTTPS to the official Second Life Agni
login endpoint, with no redirect following. The password is not saved to disk
or output to logs. On successful XML-RPC authentication, KAMI sends
UseCircuitCode, CompleteAgentMovement, RegionHandshakeReply and repeated
AgentUpdate messages. Any decodable LandLayer terrain packets received from
that authenticated simulator connection are uploaded to DX12.

**Limitations:** MFA, policy acknowledgement flows, complete CAPS/EventQueueGet,
avatar/object updates, mesh assets, textures, inventory, and full agent
simulation are not finished. The login may be rejected or the world may stay
blank until these are implemented and validated. Do not use this development
executable as your sole Second Life viewer.

See [live connection notes](docs/LIVE_LOGIN.md) and
[terrain replay](docs/TERRAIN_REPLAY.md).
The project owner controls release decisions.

## First live simulator object geometry

KAMI now receives ObjectUpdate and ObjectUpdateCompressed LLUDP messages
and recognizes ObjectUpdateCached cache probes and KillObject removal messages.
The viewer can ask the authenticated simulator for uncached object updates.
Recognized **root volume prims** appear as **neutral, untextured proxy boxes**
at received simulator coordinates and sizes. This is a useful intermediate
validation of object placement, **not** the correct visual representation
of existing SL objects. Object meshes, PBR materials, textures, child-link
transforms, avatar bodies and vegetation remain unsupported. The number of
received and displayed prim proxies appears in the DX12 title bar.

Cache-miss requests now have bounded, acknowledged UDP retries. Complete
reliability for the rest of the simulator protocol, re-requesting missing
assets and asset-fault recovery have not been implemented yet. Do not mistake
a successful CI build for a successful live-object or inventory test.

# KAMI offline terrain replay

This is a **developer diagnostic** for rendering decoded simulator terrain
with the existing DX12 vertex/index drawing path. It is **not** a Second Life
login or live connection. The app never requests your Second Life password.

## Capture format

Extension: `.kudp` (suggested). Binary format:

- First 8 ASCII bytes: `KAMIUDP1`
- For each record: a 4-byte *little-endian* unsigned length followed by that
  many bytes of one **complete raw LLUDP datagram**, including the six-byte
  packet header. Do not put a compressed LandLayer body alone in a record.
- Recorded datagrams must contain official LandLayer (message ID 11) terrain
  data. KAMI rejects captures with no renderable terrain.
- Each record: 6–65,535 bytes. Up to 2,048 records and 16 MiB of packet bytes.
  A malformed or truncated capture is rejected without replacing the scene.

**Privacy:** Capturing all simulator traffic could expose information about
your avatar or session. Restrict captures to LandLayer messages, never publish
full-session packet dumps or credentials, and only open captures you trust.

## Viewing terrain

Open `KAMIViewer.exe`, select **Load recorded simulator terrain (.kudp)**,
choose your diagnostic capture, then use WASD or the arrow keys to navigate.
The original demo remains separately available. A failed load leaves the
existing scene unchanged.

KAMI renders only samples actually decoded from received LandLayer packets;
it does not fill missing terrain with fabricated geometry. A quarter-parcel is
part of a region — capture uses the region's 256x256 default, not parcel size.

## Not implemented

Acquiring packets from an authenticated simulator, secure login/MFA,
CAPS/event queue, live objects, avatars, and inventory are outstanding. Offline
replay proves only the packet-to-mesh integration path, not world readiness.

## Capture live land packets for decoder diagnostics

In the experimental live-login viewer, wait for terrain to load and press
**F8**. Choose a local `.kudp` destination. The capture records only validly
framed `LayerData` land-heightfield messages. It strips LLUDP flags, ACKs
and sequence metadata; it never records login credentials, MFA codes, chat,
avatar/object messages, or inventory. Nothing is uploaded automatically.

The file **does contain your region heightmap**, including potentially
recognizable terraforming. Only share it if that is acceptable. A bounded
capture stores at most 2,048 land datagrams or 16 MiB. Files can be loaded
with the login screen's **Load recorded terrain (.kudp)** button, and can
be inspected offline to reproduce height-map discontinuities.

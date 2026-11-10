# Experimental Second Life login and simulator integration

The Win32 development preview can attempt a **real Agni authentication** over
TLS using Windows WinHTTP at
`https://login.agni.lindenlab.com/cgi-bin/login.cgi`.
It uses an XML-RPC `login_to_simulator` request, a legacy 16-byte-limited MD5
password hash as required by this protocol, and checks the returned agent ID,
session ID, UDP endpoint, and circuit code. TLS certificate errors are not
ignored and HTTPS redirects are disabled. The password input is cleared after
starting a request; no credentials are persisted.

On server acceptance, KAMI starts an UDP simulator circuit with authenticated
credentials, sends the legacy circuit and complete agent movement messages,
acknowledges reliable incoming messages, responds to a RegionHandshake, and
sends bounded regular AgentUpdate interest messages. LandLayer packets go to
the existing terrain assembler, and only received sample cells become DX12
mesh geometry.

## Unverified / currently unsupported

- No live authentication test with a resident account has been completed.
- MFA challenge/response, new account security requirements, Terms of Service
  acceptance, and login redirects may require additional implementation.
- CAPS and EventQueueGet, viewer inventory, avatars, objects and PBR textures
  remain outstanding; full land rendering is not guaranteed.
- Region dimensions default to 256×256, based on standard SL regions.
  Quarter-parcel dimensions should never be confused with region dimensions.
- This is a protocol-development executable, not a release-ready viewer.

No real credentials are used by the automated tests. CI tests synthetic XML-RPC
and UDP protocol fixtures only. Keep the official Second Life viewer installed
until KAMI's live login, terrain, and presence are tested successfully.

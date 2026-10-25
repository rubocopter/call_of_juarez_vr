# CoJ world-fire definition pointers under LAA

This boundary belongs to original Steam 2006 `code.pak` SHA-256
`f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12`
and ChromeEngine3 SHA-256
`db69bc35919fe57187766771a2452aca11090474f6d63df1a85a80eded131ec8`.

## Native return value and Java branch

Native registration at engine RVA `0x10C950` binds `FXLoadEmiterDef` to
`0x10C850`, forwarding to loader `0x11A9B0`. `FXLoadParticleDef` uses `0x10C870`
and loader `0x11A760`. These loaders return actual definition heap pointers,
including reused named definitions; allocation failure returns null. Definition
objects are 0x2B0/0x2D0 bytes. They are not small positive indices.

Exact `FXEmitter.class` SHA-256
`920777e1953681d22a19b78e1d354f4b50c0e508c14bf8372722d3473ad31bdf`
initializes both definition fields to zero and stores those native returns as
Java ints. `Start()` rejects either value with `ifle` at offsets 11 and 18,
then nevertheless sets `m_bIsStarted=true`. A valid heap pointer with bit 31 set
therefore skips flame creation under LAA. `FireEmitterCore.Start()` separately
starts its burning timer; burning depends on started/fire-force state rather
than successful flame creation. This is a concrete conditional invisible-damage
defect, not proof that every invisible fire has this cause.

`DestroyFXDefs()` similarly skips releasing high pointers at offsets 4 and 23.
The exact derivative in `tools/coj_fx_definitions.py` changes these four `ifle`
instructions to same-length `ifeq` null tests. All branch destinations and
other method instructions remain intact.

The same verified native-load contract applies to smoke/sparks/steam definitions
in exact `FireEmitterCore.class` SHA-256
`4a18a1051e1c9326d8ff19655ea1dba763ac93a54fbe6ff9374a3e12afc21e56`.
Its six creation checks (`Start`/`StartSteam`) and six corresponding release
checks also become null tests. Sound time comparisons and emitter-handle checks
remain signed/native. Both exact classes reject unknown/already patched input.
Null creation/release still rejects;
creation and release both preserve all 32 bits. Native emitter runtime handles,
their positive tests, generation, timers, damage and authored effects are not
changed. A loader-produced definition pointer and an emitter handle are distinct
contracts; native handle construction uses a slot plus randomized high bits.

Unknown or already transformed class/archive identities reject mutation. This
derivative is included with the existing transactional native code-archive patch;
original archives restore byte-for-byte.

## Evidence and limits

Offline interpretation of original Start reproduces zero creation calls with
high definition pointers while the burning timer still starts. The corrected
creation/release replay covers low/high/null pairs, signed argument preservation
and repeated destruction without duplicate releases. Exact instruction-difference
checks restrict changes to the sixteen intended branch bytes across both classes.
The shipped Java verifier
accepts the derivative and rejects deliberately invalid Start/release bodies.
These are host checks, not physical fire validation.

The subsequent operator report confirms visible flames on one campfire seen
from a distance. This is scoped visual recovery; close-range flame/damage
agreement, other emitters and attribution to high-pointer definitions remain
unqualified. Do not require a repeat solely to reconfirm that distant view.

The long headset session reports invisible campfires while other fires render.
Global FX flags and sampled camera identity were valid; sampled weapon-emitter
clocks/counters did not advance between the two eyes. World-fire definition
pointers, actual campfire emitter bounds and in-cull classifier outcomes were not
recorded. Attribution of the report to the high-pointer defect remains unknown.
Creation with the derivative and visible flame/damage agreement require physical
evidence. If it persists, observe the nearby exact world emitter's definitions,
runtime handle and native culling/queue/draw, rather than disable depth blending
or change global FX/camera constants. Full wrapper `0x30FB0` remains authoritative.

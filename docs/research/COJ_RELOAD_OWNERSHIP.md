# Exact CoJ reload ownership

These contracts apply to the original Steam Call of Juarez (2006) archive
`code.pak`, SHA-256
`F9DB47C166E03F23E37CBCDFD5344E4AD4C5C9134F35E8F6DCDF66DB7E71CE12`.
Game hand indices are 0/right and 1/left.

## Native request and completion

`PlayerController.ExecuteInput(IFLjava/lang/Object;)V` action 31 calls
`ApplyReloadWeapon()` on a positive input edge. Its controller eligibility and
public `ArmedPlayerBeing.ReloadWeapon()` remain authoritative. The public method
rejects existing reload, computes reloadable ammunition for both desired weapons
and selects one hand. A genuinely empty support hand makes that choice
unambiguous. The protected `ReloadWeapon(I)V` only sets a desired reload state;
calling it directly would bypass public checks.

The pistol state machine has ordinary move/idle node 1, empty node 0 and reload
begin/body/end nodes 20/21/22. Alternate states carry bit 128 and remain outside
the initial gesture contract. `IsWeaponReloading()` observes desired states,
so it cannot alone establish current/destination readiness.

Reload begin/body/end callbacks own weapon skin, sounds and animation. Actual
round transfer occurs through `OnHandStateFinished_Reload` and
`PawnArmed.WeaponReload`: it resolves reserve, caps the transfer, writes both
weapon ammunition and reserve, and participates in reload-loop completion and
the weapon-reloaded game event. Frontier/Peacemaker logic defaults to single-round
loops; Schofield defaults to whole-clip transfer. Parameters remain authoritative.
`ForceReloadWeapon`, direct ammunition/reserve writes or direct completion calls
are not a demonstrated gesture-paced reload boundary.

## Initial hybrid gesture

The initial implementation requests the ordinary native reload after a deliberate
free-hand ammunition-zone-to-gun journey. It leaves native ammunition, automatic
reload, animation, sounds and events intact, and retains Square. The baseline route delegates the whole reload to native animation. The
experimental paced route below adds a visual token, not physical cylinder/ejection
ownership.

The passive reader admits only the exact concrete classes
`WeaponPistolFrontier1878_Regular`, `WeaponPistolSchofield_A`,
`WeaponPistolSchofield_B` and `WeaponPistolPeacemaker`. The shipped Home01 opening `ColtPeacemaker` is the Peacemaker class;
omitting it would reject the tutorial pistol before gesture policy. Current, destination, desired and actual states must
all be 1 for the armed hand and 0 for the empty support hand. Actual, active and
desired weapon objects must match by JNI identity; the support hand has none.
Both hands reject attack and actual/desired/state-machine two-hand flags. Death,
carrying, reload, alternate states, replacement and unavailable observation fail
closed. A second bounded observation rechecks coherence before publishing the
positive `GameObject.GetThisID()` identity. The caller supplies current local
player generation, native gameplay epoch and input context ownership.

These getters read shipped state. No ammunition-capacity, reserve-eligibility or
weapon-parameter helper is called: `GetWeaponMaxAmmoToReload` and parameter
getters can create native parameter objects on a cache miss. No native update,
trace, state change or reload is invoked by the reader.

Gesture policy is game-neutral, in XR metres, with physical trigger claims
retained until their own available release even after weapon-hand changes.
Native epoch loss/reacquisition, menu/wheel/equipment/interaction conflicts,
recenter, unavailable input/pose and stale samples cancel the journey. Completion
is a one-sample intent without a retry queue. Native reload continues to own
hand/weapon animation and the existing restoration path.

Policy, passive JNI and adapter checks are host-tested for the four exact classes;
the corrected hybrid gesture has operator headset acceptance for the exercised
Peacemaker context. Other contexts and interruption/recovery gates remain open.

## Cartridge-paced native completion

The experimental extension is limited to exact Frontier 1878 Regular and
Peacemaker classes. A fresh pickup/insertion arms a bridge-owned pending weapon
reference immediately before public action 31. The protected native request,
after public eligibility, validates the selected weapon, ordinary reload state,
single-round parameter, network ownership and non-multiplayer context before
any state request. Rejection clears pending and returns without starting reload.
Accepted state delivery transfers pending into a native-owned active reference
and hand. The bridge clears pending immediately after dispatch; renderer/context
cancellation cannot remove the accepted native limiter.

The ordinary reload completion still calls `WeaponReload` and retains native
round/reserve accounting and `_WEAPON_RELOADED`. The scoped limiter selects the
existing move/end route after one natural completion. Native end/replacement
reconciles active ownership. There are no direct ammo writes, forced completion
calls or parameter changes. Square, automatic reload, non-opted players and
Schofield retain native paths. Unknown class/archive hashes fail closed.

The pickup publishes a bounded cartridge visual token in the same captured
head-space geometry as both eye images. Fourteen bounded surfaces form a
controller-oriented brass/copper solid with per-eye back-face rejection. Its
tip pose matches the rendered apex and exposes local +Z inward. It is compositor
geometry rather than an individual native cartridge mesh and has no scene depth
occlusion.
An optional explicit insertion pose is supported by neutral policy; the active
adapter retains the validated grip-proximity gesture until a model loading-port
anchor is demonstrated. Native reload continues to own arm/weapon animation.

After a cartridge-paced request is dispatched, the adapter does not mint the
next token immediately. It first requires observation of the native reload
interval and then the same player, weapon, armed hand, input generation and
native gameplay context returning to ordinary eligible state. Only then does a
new presentation-only cartridge appear in the empty support hand. It does not
claim the trigger until a fresh press; a release near the gun requests exactly
one further native round, while a missed release leaves the token in hand for a
fresh retry. Context loss, owner/weapon replacement, native faults or lack of an
observed reload interval cancel the continuation. Tracking or support-trigger
availability loss during that interval also cancels it, as do non-finite poses,
regressing sequence/time, a gap over 250 ms or failed native observation.
Duplicate render polls cannot establish native progress or consume replenishment.
Native recovery can retain reload states and temporary two-handed flags after
`IsWeaponReloading` becomes false. A separate passive reader admits only the same
exact single-round pistol, actual/active/desired weapon object identity, empty
support hand and idle/reload-only native states. It retains an already accepted
continuation; it cannot start a new journey or replenish before ordinary admission
returns. Attack, replacement, observation failure and foreign states cancel it.
`HasSomethingInHand` conflates a carried object with the armed state machine's
two-hand animation ownership. Recovery therefore separately reads the pure
`HumanBeing.IsCarrying` predicate (`m_cCarriedObject != null`) twice. Positive
support occupancy is tolerated only when the armed current state and its own
state-machine two-hand flag explain that fallback, with no carried object,
zero support states and absent actual/active/desired support weapons. Initial
gesture admission still uses the original occupancy rejection. This distinction
is not permission to load while holding a world object.
During that verified interval, a held support trigger is consumed until a fresh
available release, including across subsequent context loss. This prevents native
cross-hand attack fallback while tracked muzzle ownership yields to reload animation.
This keeps ammunition,
capacity, reserve checks and completion timing native-owned while avoiding a
new waist pickup for every chamber.

The installed paced continuation was physically rejected: it loaded one round
without replenishing the token. Recovery first failed on temporary native two-hand
ownership and later on `support_occupied`. The corrected passive reader distinguishes
carried objects from animation occupancy and preserves the support-trigger release
barrier. These corrections are host-tested, pending physical acceptance; they do
not establish a completed manual reload or comfortable cartridge presentation.

Full manual cylinder reload remains planned pending measured loading-port and
ejection geometry, tracked pose ownership during reload, save/load recovery and
physical validation of the cartridge-paced extension. Constructor single-round
defaults are insufficient; the request checks live native parameters.

## Offline cylinder and loading-gate geometry

The shipped D3D9 meshes have separate animated loading parts and six modeled
rear chamber-mouth rings, but no named insertion sockets or individual cartridge
nodes. Mesh identity remains mandatory before interpreting these model frames:

| Model | Mesh SHA-256 | Cylinder / gate elements |
| --- | --- | --- |
| ColtPeacemaker | `0df76de44edefcba8ea640b85f119d5dbd20a31c1d2df7d1ab1e1c909aff63f0` | `ColtDrum` / `ColtLock` |
| ColtFrontier | `6d4577416a2c775783bed8294c3d72b3cf966381a5c2e6779a23761f3ded6044` | `GunDrum` / `GunLoader` |

Cylinder-local centimetre candidates measured from rear-face geometry are
Peacemaker `(-1.234718, -0.712462, -2.383278)` and Frontier
`(-1.185826, -0.010422, -2.010485)`. Their mouth-edge radii are about 0.593 cm
and 0.519 cm. These are geometric observations, not interaction tolerances or
proof of gate clearance. The insertion approaches along cylinder-local +Z from
the rear. A future target must use the actual displayed cylinder frame, including
native accumulated phase, and a cartridge-tip trajectory rather than grip distance.

Ordinary reload scripts select begin frames 0–8, loop 8–16 and end 16–25 at
30 fps. Isolated native ANM sampling uses frame-domain time: frame 8 opens
`ColtLock` approximately -50 degrees about local Z and `GunLoader` approximately
-40 degrees. The Peacemaker drum also rotates in its ANM, while Frontier's drum
ANM stays identity; native `ReloadRotate` and `RotateElementWithAnim` add a
separate accumulated rotation. Peacemaker's animated gate translation differs
from its mesh bind pivot, so a bind translation combined with the opening
quaternion is not a valid demonstrated pose.

Gate accessibility, final phase composition, reload-skin/attachment coherence
and both-eye restoration require native observation before enabling physical
gate manipulation. Offline ring and animation measurements do not accept those
boundaries or establish occupied-chamber/ejection semantics.

The displayed-frame observer resolves the drum and gate by native element name,
checks readback against the current controller-mapped child transaction and
rechecks JNI weapon identity after the reads. Six local mouth observations are
transformed through that actual drum frame in centimetres. This is read-only
diagnostic geometry; matching a weapon class/name does not verify mesh content
or permit a new physical loading mode.

## Persistent-session boundary

Original reload continuation traverses 21 -> 22 -> 20 -> 21. Pair-state reversal
can shorten visible closing/reopening, but still invokes end/begin/body callbacks.
An early zero-credit return from `OnHandStateFinished_Reload` prevents transfer
but still allows sounds, important-state bookkeeping and native cylinder rotation;
it also skips capacity/reserve termination. The completion callback ignores its
third boolean argument for transfer, so cancellation needs a no-transfer guard.

The original `StateMashine.Update()Z` transition boundary at bytecode offsets
54–63 precedes `SetCurrentState` clearing the old state, after transition
eligibility. It is a concrete investigation seam, not an enabled pause API.
An owner-scoped hold there requires proof of cadence, cancellation, replacement,
save/load and restoration before integration. The existing one-round native route
remains authoritative while that session boundary is unresolved.

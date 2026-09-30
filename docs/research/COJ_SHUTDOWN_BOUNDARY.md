# Exact CoJ pre-exit shutdown boundary

## Inspected binary contract

**Verified, static inspection:** the Steam D3D9 executable with SHA-256
`5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`
imports `ChromeEngine3.dll!DestroyGame` at executable RVA `0x9034`.
Its normal message loop handles `WM_QUIT`, reaches RVA `0x20FA`, and calls that
import at RVA `0x2105` when its game instance is non-null. Remaining engine and
filesystem cleanup follows the call before WinMain returns.

```text
CoJ.exe + 0x20FA: test esi, esi
CoJ.exe + 0x2103: je   +0x210B
CoJ.exe + 0x2105: call dword ptr [image_base + 0x9034]
```

The inspected engine SHA-256 is
`DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8`.
Its `DestroyGame` export is RVA `0x2D00`; the wrapper loads its own game/context
globals, calls its internal destroy routine and returns without stack arguments.
The adapter therefore preserves a `void __cdecl()` import call. These addresses
and calling conventions apply only to those exact x86 files.

## Owner lifetime

**Host-tested:** a DLL atexit callback runs too late to rely on a presenter
worker's normal cleanup during process exit. A real x86 DLL/host test reproduces
an owner being terminated without executing its cleanup; the same fixture using
the production pre-exit hook cleans and joins the owner before forwarding the
original destroy function and before DLL atexit runs.

Windows documents [ExitProcess thread/detach ordering](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-exitprocess)
and [DLL cleanup restrictions](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices).

## Bounded production adapter

**Implemented / host-tested:** `game_shutdown_hook` checks the executable and
engine hashes, x86 PE headers, the loaded indirect-call operand and the current
import target against the inspected engine export. It replaces only that import,
using the existing protected pointer-patch ownership primitive. Unknown or
conflicting targets fail closed; restoration preserves a foreign replacement.
The presenter is started only after this shutdown boundary is installed.

The callback stops producers, restores camera/device hooks, joins the OpenVR
owner, drains pending D3D11 copies and releases capture resources, restores its
own import, and emits final summaries. It forwards the original destroy call
once per game call. Finalization itself runs once, including cached/reentrant
hook calls. Runtime state has explicit process lifetime so static DLL teardown
does not repeat thread or GPU cleanup. The retained factory/device roots remain
OS-reclaimed at process exit, as before.

If the boundary is bypassed or incomplete, a minimal direct file write reports
failure without taking the shared log mutex or performing GPU/COM/thread cleanup
from atexit. It emits no replacement success summary. The verifier requires
pre-exit installation/completion/restoration as well as actual owner-shutdown
and transport-drain evidence.

Final GPU and proxy summaries must each be unique, contain every required
counter, agree on copied-frame totals and report copied = completed, zero
pending copies and zero abandoned leases. Missing or inconsistent counters
fail acceptance even when OpenVR shutdown itself completed.

Tests cover hash/bitness/call-site rejection, import conflicts, protected-slot
restoration, owner cleanup before original forwarding, repeat callbacks,
ordinary process/DLL exit ordering, missing-boundary failure and producer
resource reclamation after pending GPU copies drain. Verifier negatives cover
abandoned/pending copies, incomplete fences, inconsistent summary totals and
missing required drain counters.

**Live-tested normal quit:** the exact-build physical native-stereo run reaches
this boundary, restores the import, reports actual owner `shutdown_complete=true`
and completes capture cleanup and `run_end`. Unique final GPU/proxy summaries
agree on copied/completed totals with zero pending copies or abandoned leases;
every published producer lease is reclaimed. This closes the observed normal
shutdown failure on this host, not abnormal-exit recovery or device-loss behavior.

**Host-tested telemetry correction:** quit may cancel a final fenced producer
frame that was never published. Resource release resets that slot and increments
its invalidation counter, but previously left the recorded ring depth stale.
A real GPU regression exercises this final unpublished frame alongside drained
consumer copies and verifies zero final depth, one cancellation and retained
peak depth. Refreshing depth after slot reset fixes the statistic. A subsequent
physical binary includes that correction and reports final ring depth zero,
every published producer lease reclaimed and all consumer copies complete.
The corrected final statistic is **live-tested** on this build/host.

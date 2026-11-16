# Local replication and JIP check

## Verified data run

`E:\CodexWork\MRAD-QA-profile\network-20261116T151544271791Z` passed on installed 1.9.0.37. Server PID 27988, live client PID 24644, and JIP client PID 8812 all exited normally with code 0. The first client observed both snapshots; the separate JIP process received the already changed second snapshot. The server logged both final acknowledgements. All three required result markers below reported zero failures. Source manifest SHA-256: `5c5a3394f51766fdecd0e692893f9a69034758acd33048b6a5920f33364caa98`.

This completed run used the default data mode. It verifies actual row/page/tick replication, live update and JIP; it does not establish a pass for the optional ADS checks or impacts.

## Runner and fixture

`run_network.py` starts only its own server and game processes with separate profiles. It does not control an already open Workbench or game. Backend HTTP is disabled and both clients connect to `127.0.0.1`. The installed engine's local `-server` mode listens on `0.0.0.0:2001`; its config-hosting `-bindIP`/`-bindPort` arguments and `-port` do not override that listener. The runner checks that UDP 2001 is free before launch and verifies the actual listener's owner PID before starting a client. It makes no firewall changes and closes only processes it started.

```text
python tools/qa/run_network.py
  --server "<installed ArmaReforgerServerDiag.exe>"
  --client "<installed ArmaReforgerSteamDiag.exe>"
  --addons-dir "<vanilla addons>,<checkout>,<checkout>/tools/qa"
  --profile "<isolated QA profile>"
  --port 2001
```

The exact executable paths depend on the installation. `-server`, `-client`, `-addons`, `-addonsDir`, `-noBackend`, and window/focus flags are described in the [official startup parameter reference](https://community.bistudio.com/wiki/Arma_Reforger:Startup_Parameters). The direct local-server mode ignores hosting configuration. A started process alone is not connection evidence. The runner is intended for the installed Windows executables and uses read-only `netstat` to confirm ownership of its listener.

The dedicated world is `{CBA77A6A1C5E4B22}Worlds/MRAD_NetworkQA/MRAD_NetworkQA.ent`. The CLI receives its path `Worlds/MRAD_NetworkQA/MRAD_NetworkQA.ent` without the GUID prefix: in 1.9.0.37 the GUID-bearing string loads on the host but fails the joining client's world-file lookup. It inherits the vanilla MP test scene and contains a replicated QA probe. The short single-process runtime probe is not included.

The server spawns a real AUTO notebook, native first-slot weapon/character fixture, and MRAD PIP scope. It publishes the notebook component and scope-state replication IDs along with an independent expected snapshot. The client resolves those actual components and compares every serialized row and state value. No second calculation or local stand-in is used as the observed replica.

| Phase | Notebook | Scope ticks | Evidence |
|---|---|---|---|
| 1 | 10 rows, 5 per page, page index 1 | Elevation 10, windage 30 | First client must acknowledge matching actual replicas before phase 2 is published. |
| 2 | 12 rows, 6 per page, page index 0 | Elevation 17, windage 19 | First client observes the live update and exits. A separate process connects afterward and must receive phase 2 through JIP. |

Acknowledgements use an owned `SCR_PlayerController` RPC defined **only in the QA addon**. The test does not try to send owner-only RPC through the server-owned probe. That controller also requests QA-only `EnableStreamingForConnection`: the native MP test lobby can keep runtime streaming disabled until a player respawns, while this data fixture deliberately needs no possessed character. The server closes after both final acknowledgements or fails after 90 seconds. A client logs stream/state diagnostics every five seconds and fails after 55 seconds, before the launcher's 65-second client limit. Each client closes its own process after comparison.

Required completion markers:

```text
MRAD_NETWORK_CLIENT_RESULT snapshots=2 firstPhase=1 lastPhase=2 failures=0
MRAD_NETWORK_CLIENT_RESULT snapshots=1 firstPhase=2 lastPhase=2 failures=0
MRAD_NETWORK_SERVER_RESULT phase=2 finalAcks=2 failures=0
```

The three markers must come from the three separate process logs with normal exits and no script errors. The launcher creates `result.json` with process IDs, commands, evidence locations and the combined verdict. Missing streams, unready controllers, connection refusal, or mismatched state cannot yield a pass.

Keep the PC checkout frozen for the whole multi-process run. The runner writes a source manifest and checks it before starting each peer. A source change between the live client and JIP client is a harness error, and the engine correctly rejects mismatched script checksums. The runner never disables that validation.

This check covers serialization, active replication updates, owner-routed acknowledgement transport, and join-in-progress. A QA-only `InsertMPObserver` places each test connection's observer at the fixture's location; the test does not require a possessed character or camera to establish relevance. Only those QA observers are removed during cleanup. It does not prove real in-hand RT display, ADS controls, inventory transfer, moving-camera relevance changes, or AC-11 impacts. Those acceptance checks remain separate.

## Optional owned-client ADS and product-RPC check

**Verification status:** this optional mode has no completed passing run. Attempts `network-20261116T151645407367Z` and `network-20261116T152116174829Z` passed the two data snapshots and the first two ADS-helper stages, then timed out while entering native ADS at stage 3. The latter confirmed actual client ownership and raised/input flags. The subsequent scripted `SetWeaponADS` fixture revision is prepared but has no completed confirming run. None of these attempts establishes an ADS-action, camera, or impact pass.

Add `--ads` to run a separate, bounded helper after the first client's two data snapshots match. It spawns a separate character with the MRAD M21 PIP example, assigns it through native `SCR_PlayerController.SetInitialMainEntity`, and drives its `CharacterInputContext`. The native possession method transfers replication ownership and disables AI; a bare `SetControlledEntity` does not perform that transfer. The helper waits for both controlled-entity equality and the character's actual client-side `RplComponent.IsOwner`. It does not possess the notebook data fixture or alter the expected row snapshot.

ADS entry itself is scripted fixture setup through the public `CharacterControllerComponent.SetWeaponADS(bool)` setter. The setter is called once per desired ADS transition. A pass still requires the native `IsSightADSActive` camera condition on the client and `IsWeaponADS` independently observed on the server. This does not test physical ADS-key sampling. The MRAD fine-action pulse later goes through the actual `ActionManager` and its listeners.

The 14 stages verify initial possession, refusal outside ADS, a real native ADS transition on both peers, refusal for a different scope, valid per-axis production RPC changes, soft-limit clamping, rejection of malformed deltas/limits, reset, one native `ActionManager` fine-action pulse, ADS exit, refusal after exit, state retention on ADS reentry, and final reset. Every successful stage requires an independent server inspection followed by matching replicated item state on the client. The action pulse has a separate DOWN listener; it never calls a protected product callback.

The helper has a 35-second limit inside the normal client's 55-second world limit. The optional runner additionally requires:

```text
MRAD_ADS_CLIENT_RESULT stages=14 actionDispatches=1 failures=0
MRAD_ADS_AUTHORITY_RESULT stages=14 failures=0
```

These are test requirements, not a claim that a run has passed. Physical keyboard/Shift mapping, aim alignment, actual projectile impact, the 2D example's rendered camera path, and inventory transfer remain distinct checks.

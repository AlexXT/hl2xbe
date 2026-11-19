# Two recompilers, one retail Xbox Source build

A comparison of bringing up the same title — **Half-Life 2 (Xbox, retail,
2005-10-14, XDK 1.0.5849)** — with two independent static-recompilation
toolkits, plus the Halo 2 beta work on one of them.

## The two toolkits

| | `sp00nznet/xboxrecomp` | `TGP482/XboxXBERecomp` (this project uses the `hk21x` fork) |
|---|---|---|
| Pipeline language | Python + Capstone | C++ + Zydis |
| Output | C, compiled with MSVC | C++, compiled with Clang |
| Runtime | XDK APIs rebuilt as host shims; NV2A pushbuffer executor; D3D11/GL | D3D8 API-level HLE; native D3D12 renderer with vertex/pixel shader translation; SDL3 input, XAudio2 |
| Game project | `regen.sh` + generated C | Template repo with `sdk/`, `game.toml`, `src/game` hooks |
| Symbol recovery | RTTI + datamaps + callback seeds; very rich for HL2 | `xbe_symbols` from signature hashes/XTLID of known XDK builds, plus hooks/overrides |
| Failure style | **silently** skips unresolved indirect calls; a bad lift can pass byte-length checks | Trapping stubs for missing symbols; fails the build or traps visibly |

## What the other project achieved

From the `hl2-recomp` README and `docs/` (their measurements, not ours):

- 49,498 functions, 13.4 M lines of C, 91.3% instruction reachability.
- RTTI: 2,336 classes, 2,932 vtables, 12,288 unique virtual methods, exact
  inheritance graph; 84% of 1,435 class names resolve to a file in public
  Source SDK 2013; feeding RTTI back into the disassembler found 7,992 missed
  functions.
- Boots to its own main menu with no command line; level loads 22.3 MB of
  content; **stops at a plateau at ~99%** — not playable.
- Its main research finding is a class of *silent miscompiles* found only by
  running the game: forward `rep movs` lowered to `memcpy`, `bt`/`bts` memory
  bit-offset masking, dead carry flag after arithmetic, tail calls not ending
  functions, `__SEH_prolog` frames, `comiss` operands clobbered before the
  branch, `swizzle_offset` collisions. All were fixed upstream with negative
  controls.

## What this project achieved

Measured in this repository (`docs/FINDINGS.md` has the addresses and evidence):

- 52,629 functions, 2.02 M instructions lifted from `hl2_xbox.xbe`.
- Symbol recovery from `xbe_symbols`: 92/278 functions, 5/13 data — low because
  `D3D8LTCG` reshapes library bodies. The gap was closed by *reading the
  guest's own code*, not by more signature matching.
- Recovered from HL2's XBE: five D3D8I data globals, the default FVF shader
  object, the index-data base, a missed allocator thunk, and five LTCG ABI maps.
- Content: `default.xbe` was recompiled and run under the same runtime; its own
  installer produced byte-correct `.xzp` archives (390 + 57 duplicated files
  compared, all identical) — the console's LZX doing the console's job.
- Level load: the guest spun in miniport status-poll loops. Hooking candidate
  loop heads and reading the tested words gave the exact exit condition; two
  host hooks now emulate those MMIO-like side effects. `NEW GAME` completes.
- Result: native D3D12 rendering, working F1 console routed into the engine's
  `Cbuf_AddText`, `sv_cheats 1` / `impulse 101` confirmed working — playable.
- Halo 2 beta on the same toolkit (separate repo) is "almost working" per its
  author, with the black-world/pixel-stage problem being the remaining issue.

## What the comparison shows

1. **The symbol story dominates early bring-up.** The RTTI-rich HL2 build gave
   `xboxrecomp` a huge static head start; the signature-driven toolkit needed
   manual recovery of LTCG globals and ABIs. Both converged: the game's own code
   is the ultimate symbol table when the tool's recovery misses.

2. **Fail-loud beats fail-silent for this class of work.** Skipped indirect
   calls in `xboxrecomp` surfaced three functions later as garbage pointers;
   trapping stubs in `XboxXBERecomp` made each missing piece explicit. The
   miniport stall here was invisible until a stall detector reported thread
   stacks, then became trivial once hooked.

3. **API-level HLE + a native renderer reaches "playable" sooner.** D3D8 HLE with
   a D3D12 backend gives modern rendering and working menus early; the NV2A
   pushbuffer-executor route gives deeper hardware fidelity but requires
   implementing texture sampling, swizzles and fences before the same point.

4. **Hardware-status emulation is the last frontier.** Both projects end at a
   hardware-shaped wait: one at a level-load plateau, the other in GPU status
   loops. It is not a code-generation problem anymore — it needs a host hook
   that behaves like the device. Here that took dynamic tracing, not guessing.

5. **Frame cadence is another missing hardware fact.** Querying `fps_max` in the
   F1 console returns `300` — the engine imposes no 30 FPS cap. The retail
   console's cadence came from the display present interval, which the host
   cannot read: `xdk/5849/signatures.toml` has `D3D_PresentInterval = {}`, so
   `PresentVblanks()` falls back to 1 vblank = 60 FPS. Frame-time-dependent
   Source code then runs at twice the intended cadence and shows artefacts;
   setting host `fps_cap = 30` restores them to the console's timing and the
   artefacts disappear. Recovering the present-interval global would let the
   runtime derive 30 FPS by itself.

5. **Preservation payoff.** The deliverable is a self-contained native binary
   plus readable generated C and a documented set of recovered facts. No game
   data is committed; the ISO is supplied by the owner. The recompiled game also
   gains ultrawide/settings/console features that the original never had.

## Honest limitations

- We did not run `hl2-recomp` ourselves; its figures come from its own docs.
- "Playable" here is a user report ("works beautifully, occasional artefacts"),
  not a formal conformance measurement.
- Halo 2 on this toolkit is a different game and a different author's work; it
  is included as evidence that the toolkit generalizes, not as a controlled
  comparison.

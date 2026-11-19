# Findings from bringing up `hl2_xbox.xbe`

Everything below was measured from the retail XBE or from the XDK sources, not
guessed. Addresses are guest VAs.

## The image

- Title `45410091`, "Half Life 2 - Retail Build", built 2005-10-14.
- Entry `0x0059C612`, kernel thunk `0x00626160`, 10 sections, 124 kernel imports.
- All eight libraries are XDK **1.0.5849**, including `D3D8LTCG`.
- `xbe_symbols` recovers 92/278 functions and 5/13 data symbols. The low count
  is expected for LTCG: the library bodies are inlined/reshaped, so signature
  byte matches miss. The important D3D entry points are still found by constants.

## Hand-recovered configuration

`config/game_hand.toml` includes the generated `config/game.toml` and adds:

### ABI maps (LTCG calling conventions)

| Function | Map | Why |
|---|---|---|
| `CDevice_KickOff` | `ecx, ebx` | host declares a 2-arg fastcall (`ecx, edx`); this image passes the second arg in `ebx` |
| `D3DDevice_SelectVertexShader` | `eax, ebx` | 2-arg stdcall in the XDK, registers here |
| `D3DDevice_SetTransform` | `eax, edx` | same |
| `D3D_BlockOnTime` | `eax, stack` | first arg in `eax`, second on the stack (4 bytes popped) |
| `D3D_InternalRelease` | `esi` | 1-arg stdcall, resource passed in `esi` |

The recompiler refuses to guess these: it writes `["?"]` into the scan config
and fails the build with "not proven". That is a good failure mode.

### A missed function entry

`0x005BA637` is a nine-byte allocator thunk:

```
E8 92 15 00 00   call 0x005BBBCE
83 C0 08         add eax, 8
C3               ret
```

It sits immediately after a pointer table with no padding, so the linear sweep
decoded past it. It is called from dozens of places; the first guest trap on
boot was exactly `005BA637: function was not decoded`. Added as an explicit
`[[function]]` with `end = 0x005BA640`.

### D3D8I data globals

`xbe_symbols` could not locate these because its 5849 anchors were written from
a non-LTCG title. Each was recovered from HL2's own code:

| Global | Address | Evidence in the image |
|---|---|---|
| `D3D_DirtyFlags` | `0x0061EDF0` | `or dword ptr [0x61EDF0], 0x4800` — the XDK's `D3DDIRTYFLAG_TEXTURE` |
| `D3D_RenderState` | `0x0061EDF8` | `[state*4 + 0x61EDF8]` read/written by every SetRenderState path |
| `D3D_DeferredTextureState` | `0x0061C730` | `[stage*32 + type]*4 + 0x61C730`, matching the XDK layout's stage stride 32 |
| `D3D_Streams` | `0x0061FDB8` | stride-12 records; `+8` is the vertex-buffer field (XDK `Stream_Buffer`) |
| `D3D_SimpleRenderStateEncode` | `0x00772FB8` | `[state*4]` method passed to the pushbuffer writer at `0x60D250` |

## Toolchain fixes made on the way

Two general bugs were found and fixed in the SDK fork
([AlexXT/XboxXBERecomp](https://github.com/AlexXT/XboxXBERecomp)):

- `d91d5cb` — an undecoded function seed emitted `goto loc_<entry>` with no
  label, so the generated C did not compile. Now the empty-body trap is the
  whole function.
- `a8d50d6` — Halo 2's debug-console/assert hooks referenced guest symbols in
  the shared runtime unconditionally, so any other title failed to link. They
  are now guarded by the `HAS_<symbol>` macros codegen already generates.

Also: the local `C:\Program Files\LLVM` install (clang 20.1.5) fails to link
`xbe_recomp.exe` with undefined `_mm_*` intrinsics; `C:\Program Files\LLVM20`
links correctly. `build.bat` now prefers LLVM20.

## Build metrics

- 52,629 functions, ~2.02M instructions, 6.2 MB of code bytes lifted.
- Generated C: 128 split files; 17 unimplemented instruction kinds (mostly
  rare x87/3DNow!/protection forms), all counted by the recompiler.
- `build.bat` builds `out\build\release\Game.exe` (~34 MB).

## Content staging: the loader installs its own archives

HL2's game XBE only opens `.xzp` archives; the disc ships `.xz_` (Microsoft
XCompress/LZX). Rather than reimplement LZX, `default.xbe` was temporarily made
the build's XBE and run under the same runtime. Its installer wrote:

```
hdd/Z/hl2/hl2x/zip0_xbox.xzp              434,653,523 bytes
hdd/Z/hl2/hl2x/zip0_xbox_english.xzp      186,195,539 bytes
hdd/Z/LoaderMedia/title_load.xmv
hdd/Z/version_235.txt                     (the install marker)
```

The main archive's length matches the xCmp header's declared uncompressed size
exactly. `xzp_verify.py` compared 390 duplicated files in `zip0_xbox.xzp` and 57
in the English one: **all identical**, so the extraction is byte-correct. This
is the console's own decompressor running as recompiled C.

`GameInfo::images` maps `hl2_xbox.xbe` to the CMake target's exe name (`Game`);
otherwise the loader's image switch looks for a nonexistent `hl2xbe.exe` and
the handoff fails after the intro.

A pure-Python LZX decoder from `sp00nznet/360tools` was also tried, but its
XEX2 block framing does not match the xCmp payload; the recompiled loader is
both more faithful and less work.

## Level-load frontier

With content staged, the game reaches the menu and `NEW GAME` loads the intro
map to ~97–99%. The remaining failure is not a missing symbol: the XDK's own
`CreateDevice` initialises a *miniport* object at `device+0x1C28` that HLE
replaced. Mirroring a zeroed block fixed the first null dereference in the
material-setup helper (`sub_006179C7`); presenting its status bit as ready
moved the stall deeper.

The guest now spins in miniport status-polling loops (`0x618250`, `0x618290`)
waiting for NV2A-style bits that no hardware here changes. That is the honest
frontier: it needs real GPU-status emulation or a host hook that advances those
fields, not another config address. Everything up to this point is committed
and reproducible.

### Solved: emulate the miniport's MMIO side effects

xemu models NV2A as MMIO register blocks (`PGRAPH` at `0x400000`, `PCRTC` at
`0x600000`, etc.) with interrupt/pending bits, but this guest polls *XDK-side
shadow words* in the miniport — there is no direct offset mapping. Guessing
status bits from xemu register names did not work; four such attempts were
reverted.

What worked was dynamic: one-shot `[[hook]]` probes on candidate loop heads
showed the guest reaching only `0x6179CA`, then `0x617A10`, then stalling in
`sub_00617E30`. Reading the tested words from the hook gave the exact
conditions:

| Hook | Emulated side effect |
|---|---|
| `0x6179CA` | set bits `0x10` at `miniport+0x3214` and `+0x2400`, clear it at `+0x3220` — the loop's exit condition |
| `0x617EE0` | the guest writes `miniport+0x600100` (an NV2A-style clear register) and waits for bit `0x1000000` at `+0x100` to drop; mirror that write by clearing the bit |

The first probe accidentally logged every loop iteration and produced a 3.5 GB
log; it was replaced with one-shot logging and the logs removed.

With those two hooks the guest leaves the polling loop, `NEW GAME` completes
its load and the game runs. `docs/SOURCES.md` still describes the reference
toolkits; this file is the record of what was actually recovered.

## Xbox console leftovers in the retail HL2 build

Unlike Halo 2's *in-game* developer console (which the shared SDK bridges),
HL2's leftover is an **external debugger console, VXConsole**. It is visible in
both the binary and the public SDK headers:

- `hl2_xbox.xbe` strings: `-novxconsole`, `Waiting For VXConsole Connection...`,
  `XBX_InitDebug: failed to register command processor`,
  `XBX_SendRemoteCommand: failed on %s`, plus the command protocol tokens
  `__connect__`, `XCMD`, `XPRT`, `XCLR[%8.8x]`, `%s!%s%s`, and
  `Resuming debug session.` — all next to the `-dev` / `-retail` launch strings.
- Source SDK 2013 still declares `ConVar_PublishToVXConsole()` in
  `src/public/tier1/convar.h`, `ICVar::PublishToVXConsole()` in `icvar.h`,
  `VXConsoleReportMode` in `tier0/vprof.h`, and VXConsole-specific stack
  translation in `tier0/stacktools.h`.

It is dormant on the normal retail boot (no `-dev`, and the title takes the
empty command line), so it needs no hook here. The Halo 2 console code in the
shared SDK is already compiled out for HL2 by the `HAS_console_*` guards and
can be removed from that SDK later without touching this project.

### Routing the F1 Console page into HL2

The SDK's F1 overlay Console page queues commands through `app::TakeConsoleCommand`.
HL2's command buffer was found by following the XBE string
`"Cbuf_AddText: buffer overflow"` to `sub_0035DC90` (`0x0035DC90`): it takes the
command string in **ECX**, has no stack arguments, and copies into the buffer at
`[0x008CC87C]`; `Cbuf_Execute` later runs the queued text each frame.

Hook `0x0040ED10` (top of `CModAppSystemGroup::Main`'s frame loop) drains the
overlay queue, allocates a guest string, sets `ECX` and calls `Cbuf_AddText`
through `kernel::CallGuest` (which restores registers). `XBE_CONSOLE_CMD` seeds
one command at startup for headless verification: with `XBE_CONSOLE_CMD="map intro"`
the log shows `intro.bsp` opening at 1.5 s. HL2's own console UI is not opened —
commands simply execute — but the F1 page is now a working command launcher.

Earlier candidates (`0x001E226F`, `0x001ABBFD`) were rejected: the former takes
an object pointer, the latter is a different helper. The string-reference trace
was what found the real entry point.

### HUD texture corruption

Screenshots showed garbled HUD digits/subtitles and stretched particle sprites
while geometry stayed correct — textured quads sampling stale bytes.
`SurfaceSig` (`gpu/d3d_common.cpp`) hashes surfaces above `kSigWholeMax`
(64 KB) from only 64 evenly spaced 64-byte samples. A font page is a few
hundred KB, so a single glyph write changes none of the samples and the stale
GPU texture is reused. Raising the threshold to 256 KB makes font pages hash
whole. 30 FPS makes the glitches rarer but does not remove them, confirming a
change-detection bug rather than a frame-rate problem.

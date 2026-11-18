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

xemu was checked for guidance: its NV2A model is a set of MMIO register blocks
(`PGRAPH` at `0x400000`, `PCRTC` at `0x600000`, etc.) with interrupt/pending
bits, whereas this guest polls XDK-side shadow words inside the miniport.
There is no direct offset mapping, so guessing bits from the register names
does not work — four speculative status writes (one-shot, per-swap and a
host thread) were tried and reverted. The correct next step is dynamic:
find the exact guest address being polled (a watchpoint or a sampled EIP),
then mirror that field with a host hook.

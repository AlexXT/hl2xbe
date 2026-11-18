# Reference sources

Three sets of source sit beside this project. They answer different questions,
and mixing them up is an easy way to reach a confident wrong answer.

## 1. The XDK tree (authority for the libraries and hardware)

A full Xbox XDK source tree is available locally at:

```
D:\mygame\xbox trunk\xbox
```

This is the code the recompiler replaces — the kernel, D3D8I, XAPI, the NV2A
register definitions. When the guest behaves in a way that looks like a
rendering bug, check here first: the Halo 2 work found the Xbox's render-state
numbering, `D3DPixelShaderDef`, stencil op encodings and `nv_ref_2a.h` here.
`public\xdk\inc\d3d8types.h` and
`private\windows\directx\dxg\d3d8\se\` are the usual starting points.

The exact XDK build matters. Halo 2's beta is 5455; Half-Life 2 is 5849, whose
signatures ship in the SDK fork's `xdk/5849`. The tree may be a different
build, so treat its offsets as a guide and confirm against the binary.

## 2. Source SDK 2013 (guide to intent, not an oracle)

```
ref/source-sdk-2013/   https://github.com/ValveSoftware/source-sdk-2013
```

Valve's public release. Recovered RTTI/datamap names can often be resolved to
an exact file here — `hl2-recomp` measured 84% of 1,435 class names. That is
the main use: `address → class name → likely .cpp`.

Treat it as a description of *intent*, never as the exact code being lifted.
The Xbox build is Source 2004 plus the September 2005 CPU/memory changes;
SDK 2013 contains later Episodes/Portal drift. Reliability by area:

| Area | Use |
|---|---|
| `tier1`, `mathlib`, `tier0` | near-identical, safe to read as-is |
| public headers, datamaps, net tables | structurally identical |
| `game/server`, `game/client` | same design, drifted bodies |
| `engine`, `studiorender`, physics | absent from the public SDK |

The classes the SDK cannot name are the engine internals — usually exactly
where a bring-up gets hard.

No unlicensed/leaked Source trees are used or stored in this project.

## 3. Analysis tools

`ref/hl2-recomp` and `ref/xboxrecomp` are described in `tools/README.md`. They
recover facts from the XBE; neither is part of the build.

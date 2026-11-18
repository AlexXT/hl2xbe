# HL2 analysis tools

The build itself needs nothing here — this folder is for recovering facts from
the XBE before and during bring-up.

The recovery code comes from two public MIT-licensed projects. Both are cloned
into the gitignored `ref/` folder, so nothing derived from them is vendored
here:

```
ref/hl2-recomp/     https://github.com/sp00nznet/hl2-recomp
ref/xboxrecomp/     https://github.com/sp00nznet/xboxrecomp
```

To recreate them:

```powershell
git clone --depth 1 https://github.com/sp00nznet/hl2-recomp.git  ref/hl2-recomp
git clone --depth 1 https://github.com/sp00nznet/xboxrecomp.git  ref/xboxrecomp
```

What each is for:

| Tool | Source | Use |
|---|---|---|
| `tools/rtti` (run as `python -m tools.rtti` from `ref/xboxrecomp`) | xboxrecomp | Recovers RTTI classes, vtables, inheritance and virtual-method addresses — independent of which recompiler lifts the code. |
| `tools/datamaps.py` | hl2-recomp | Recovers `typedescription_t` entries: member name, type and byte offset for named classes. |
| `tools/xcmp.py`, `tools/xzp_verify.py` | hl2-recomp | Walks the disc's `xCmp` container and verifies the extracted XZP against its duplicated files. |
| `config/seed_functions.json` | hl2-recomp | Function addresses that only a *run* discovered. Same XBE, same addresses — transferable as analysis overrides. |

Run the self-checks once the XBE is available:

```powershell
py -3 ref/hl2-recomp/tools/datamaps.py --self-check
py -3 ref/hl2-recomp/tools/xcmp.py --self-check
py -3 ref/hl2-recomp/tools/xzp_verify.py --self-check
```

These scripts are the work of their respective authors and keep their own
licences; they are used here as analysis tools only.

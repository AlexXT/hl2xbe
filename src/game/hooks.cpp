// HL2-specific emulation of the XDK D3D miniport's MMIO-like side effects.
//
// The retail D3D8I library polls shadow status words in the miniport object
// while setting up shaders/materials. On a real console, hardware writes change
// those words; here the object is plain guest RAM, so the two loops below need
// their side effects mirrored by the host. Both addresses were found by
// hooking candidate loop heads and measuring which ones the guest reached.
#include "cpu/x86_ops.h"

// At 0x6179CA sub_006179C7 leaves its loop only when +3214 and +2400 have bit
// 0x10 set and +3220 has it clear. Present that state to the guest.
void EmulateMiniportReady(X86Context& ctx, uint8_t* base) {
    (void)ctx;
    auto r32 = [&](uint32_t a) { return *reinterpret_cast<const uint32_t*>(base + a); };
    auto w32 = [&](uint32_t a, uint32_t v) { *reinterpret_cast<uint32_t*>(base + a) = v; };
    uint32_t mp = r32(0x61C940u + 0x1C28u);
    if (!mp) return;
    w32(mp + 0x3214, r32(mp + 0x3214) | 0x10u);
    w32(mp + 0x2400, r32(mp + 0x2400) | 0x10u);
    w32(mp + 0x3220, r32(mp + 0x3220) & ~0x10u);
}

// At 0x617EE0 the guest writes miniport+0x600100 (the NV2A-style "clear"
// register) and waits for bit 0x1000000 at miniport+0x100 to drop. In plain
// guest RAM the write has no side effect, so mirror it here.
void ClearMiniportStatus(X86Context& ctx, uint8_t* base) {
    (void)ctx;
    auto r32 = [&](uint32_t a) { return *reinterpret_cast<const uint32_t*>(base + a); };
    auto w32 = [&](uint32_t a, uint32_t v) { *reinterpret_cast<uint32_t*>(base + a) = v; };
    uint32_t mp = r32(0x61C940u + 0x1C28u);
    if (mp) w32(mp + 0x100, r32(mp + 0x100) & ~0x01000000u);
}

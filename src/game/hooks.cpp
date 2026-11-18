// HL2-specific emulation and console routing.
//
// The miniport MMIO side effects were found by hooking candidate loop heads and
// measuring which ones the guest reached. Cbuf_AddText (sub_0035DC90) was found
// by following the XBE's "Cbuf_AddText: buffer overflow" string: it takes the
// command string in ECX and no stack args.
#include "app/app.h"
#include "core/log.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

#include <cstdlib>
#include <cstring>
#include <string>

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

static void RunCommand(X86Context& ctx, uint8_t* base, const std::string& cmd) {
    if (cmd.empty()) return;
    uint32_t p = mem::AllocSystem(static_cast<uint32_t>(cmd.size() + 1), 16);
    std::memcpy(base + p, cmd.c_str(), cmd.size() + 1);
    // CallGuest saves/restores registers, so set ECX before it and restore after.
    uint32_t old_ecx = ctx.ecx;
    ctx.ecx = p;
    kernel::CallGuest(ctx, 0x0035DC90u, nullptr, 0);  // Cbuf_AddText(text in ECX)
    ctx.ecx = old_ecx;
}

// F1 overlay Console page -> HL2's command buffer, driven from the top of
// CModAppSystemGroup::Main's frame loop (0x40ED10).
void HL2ConsoleTick(X86Context& ctx, uint8_t* base) {
    static bool from_env = false;
    if (!from_env) {
        from_env = true;
        if (const char* e = std::getenv("XBE_CONSOLE_CMD")) RunCommand(ctx, base, e);
    }
    std::string cmd;
    while (app::TakeConsoleCommand(cmd)) RunCommand(ctx, base, cmd);
    app::TakeConsoleOpen();  // HL2's console UI is engine-owned; command runs without it
}

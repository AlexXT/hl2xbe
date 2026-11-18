#include "app/app.h"

static void Init() {
    app::SetConsoleQuickCommands({
        {"cheats on", "sv_cheats 1"},
        {"impulse 101", "impulse 101"},
        {"noclip", "noclip"},
        {"god", "god"},
        {"give health", "give health"},
        {"main menu", "main_menu"},
        {"version", "version"},
    });
}

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Half-Life 2 (Xbox)",  // window and splash
    .short_title = L"HL2",           // message boxes
    .name = L"hl2",                  // window classes, crash dumps, handover event
    .log = "hl2",                    // log file prefix
    .settings = "settings.toml",     // settings file beside the exe
    .splash = L"splash",             // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the Xbox disc image in the assets folder, or hl2_xbox.xbe with the game's folders beside it.",
    .images = {{"hl2_xbox.xbe", "Game"}},  // the CMake target's exe name, used when the loader relaunches into the game
    .options = {},                   // game settings kept in the settings file
    .load = nullptr,                 // reads more from the settings file's toml::table
    .save = nullptr,                 // writes more into it
    .init = Init,                    // HL2 quick commands for the F1 Console page
    .loaded = nullptr,               // image in memory, before its entry point runs
    .window = nullptr,               // the game window is up
    .message = nullptr,              // window messages first; true takes one
    .ui = nullptr,                   // ImGui drawing each frame
    .pad = nullptr,                  // changes a polled pad
    .rumble = nullptr,               // changes a rumble before it is set
    // optional, after these: .achievements (conditions on guest memory), .same_code (other releases' xbe SHA-1s with
    // the same code), .wheel (what a steering wheel's force feedback follows)
};

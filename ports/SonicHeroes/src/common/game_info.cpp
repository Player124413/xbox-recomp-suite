#include "app/app.h"

#include "game/performance.h"

// What the runtime asks of the game; strings must be set, hooks may stay null.
const app::GameInfo app::kGame = {
    .title = L"Sonic Heroes",             // window and splash
    .short_title = L"Sonic Heroes",       // message boxes
    .name = L"SonicHeroes",               // window classes, crash dumps, handover event
    .log = "sonicheroes",                 // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"sonicheroes",             // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it. On Android, choose the ISO or extracted folder in the launcher.",
    .images = {{"default.xbe", "Sonic Heroes"}},  // each recompiled xbe and its exe, for title launches
    .options = {},                        // game settings kept in the settings file
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = &sonic::InitPerformance,      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = nullptr,                   // window messages first; true takes one
    .ui = nullptr,                        // the shared settings panel draws each frame
    .pad = nullptr,                       // the touch overlay feeds controller 0 directly
    .rumble = nullptr,                    // changes a rumble before it is set
    .render_thread = &sonic::RenderThread,  // pin the render thread to the phone's big cores
    .present_thread = &sonic::PresentThread,  // periodic frame-time report
    .keyboard_pad = true,                 // the keyboard plays as controller 1 on desktop
};

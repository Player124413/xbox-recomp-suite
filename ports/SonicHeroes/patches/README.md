# Sonic Heroes fixes: OGlue → this suite

Reference: [GenryTheFox0/OGlue](https://github.com/GenryTheFox0/OGlue), the
Sonic Heroes bring-up notes and fixes in `src/kernel/nv2a_pb_exec.c`,
`src/kernel/nv2a_genry_gpu.c`, `src/kernel/nv2a_pb_scan.c`, `src/apu/`,
`src/kernel/kernel_*.c` and `src/video/gpu_present.c` (snapshot `5d41b20`).

OGlue is an emulator with its own NV2A/D3D11/DirectSound implementation. This
suite is a static Xbox recompiler with a Vulkan D3D8 runtime. The fixes
therefore do not apply as literal patches; the table below records what each
OGlue change protects against, where the equivalent lives here, and what still
has to be done for a specific executable.

**No OGlue source is copied.** OGlue is MIT licensed; its APU/NV2A parts are
LGPL-2.1 from xemu. This port is an independent implementation informed by the
same hardware behaviour. Keep the attribution if you redistribute OGlue code.

## Implemented in the shared runtime

| OGlue fix | What it prevents | Here | Status |
| --- | --- | --- | --- |
| `flip_pace()` in `nv2a_pb_exec.c` | A title that paces itself on the flip runs as fast as it can draw: Sonic Heroes menus at 90 FPS, game at 1.5× speed. | `app::Pace()` in `app/window_sdl.cpp` uses an absolute `clock_nanosleep` deadline and a 2 ms spin; `vsync=false` selects `VK_PRESENT_MODE_IMMEDIATE`; `fps_cap=60`. | Implemented |
| `gpu_present_enabled()` in `video/gpu_present.c` | GDI's HALFTONE stretch of 640×480 to 1080p took ~45 ms; 60 flips produced 24 visible frames. | The Vulkan swapchain presents the guest's front surface through `DrawFrame`; no GDI stretch. `ApplySettings()` rebuilds targets when the render scale changes. | Implemented |
| `note_main()` area rule in `nv2a_pb_exec.c` | A 252×252 shadow target drawn first stood for the frame and flashed on screen. | The suite presents the guest's `D3DDevice_Swap` front surface, not a window-picked target. If a custom present path is added, use the largest-area rule. | Not applicable |
| Vertex stride for `SHORT`/`NORMSHORT` in `nv2a_pb_exec.c` | A 20-byte vertex read as 24 bytes made every vertex after the first read the previous one; menu quads drew a slab. | `gpu/vertex.cpp` `HostFormat()` uses `2 * n` for type 1/5 and the matching DXGI format. | Already correct |
| Near-plane clipping in `nv2a_genry_gpu.c` | Geometry between the eye and the near plane projected into slabs across the screen. | The Vulkan renderer sets the viewport from the guest and the shader clips against the near plane; `c.surf[2] = 1/zscale`. | Already correct |
| `GENRY_HONEST_FENCE` / fence mirror in `nv2a_pb_exec.c` and `xbox_memory_layout.c` | The mirror said "submitted is done" before the executor read the memory; vertex/texture reuse tore models and flickered save strings. | The suite's `Threaded` backend has real fences and `WaitFence`; the guest's `D3DDevice_BlockOnFence` waits on the backend. | Already correct |
| `xbox_DpcDrainHere()` / inline DPC in `apu_core.c` | DirectSound's idle-voice DPC ran on another host thread while the title held DirectSound at DISPATCH_LEVEL; the title read a stale "voice done" and freed the song's voice. | `Settings::inline_dpc` (on for Sonic Heroes) makes `QueueDpc` run the DPC on the guest thread that is inside `IrqlGate`; `IrqlGate::Held()` exposes the state. | Implemented |
| `RECOMP_DISPATCH_GATE` in `kernel_hal.c` | A sound DPC ran beside the thread that holds DirectSound at DISPATCH and freed a voice. | `kernel::IrqlGate` serializes DISPATCH regions; `inline_dpc` preserves the console's DPC ordering. | Implemented |
| `GENRY_GUEST_CORE` in `kernel_thread.c` | Sonic Heroes animates models on one thread while another submits them; on a PC both run at once and a half-written vertex stretches a triangle. | The suite runs guest threads concurrently by design. `RenderThread()` pins only the render thread. If tearing appears, add a guest-thread affinity setting to the port. | Candidate |
| Lost wake-up between `NtSuspendThread` and `NtResumeThread` in `kernel_thread.c` | The sound thread parked after a resume that landed while it was still running; music stopped a minute in. | `kernel/threads.cpp` has suspend/resume; check a long session for the same lost wake-up. | Candidate |
| `GENRY_PB_FOLLOW` in `nv2a_pb_scan.c` | A ring wrap or a kick ending inside a method's parameters fed garbage vertices; giant triangles about once a second. | `xdk/d3d8.cpp` `Walk()` reads the pushbuffer. If giant triangles appear, add a port hook that dumps the batch and the inline-array state. | Candidate |
| Window clip registers in `nv2a_pb_exec.c` | Team-select models spilled over their cards. | The suite's `SetScissor`/`SetViewport` and the shader's viewport handle the window clip. If models spill, check the scissor state in the port's `hooks.cpp`. | Candidate |
| Texture-target sampling in `nv2a_genry_gpu.c` | Reading back and decoding a 256×256 shadow/reflection target every frame cost a third of the stage frame time. | `VulkanRenderer` already keeps targets as `ImageRef` and can sample them; check that a target used as a texture is not read back. | Candidate |

## What to do on the first boot

1. Build the APK with the workflow and install it.
2. Select your ISO in the launcher. Capture `adb logcat -v time SonicHeroes:V SDL:V AndroidRuntime:E libc:F '*:S'`.
3. If the game hangs on a loading screen with 0 FPS, toggle **Inline DPC at
   DISPATCH_LEVEL** in the settings panel. If it boots with the setting off,
   the DPC is not reentrant for this executable; record the DPC routine in the
   port notes.
4. If the music stops after a minute, check the same setting and the
   `NtSuspendThread`/`NtResumeThread` path. The OGlue note points at a lost
   wake-up between the sound thread and the main thread.
5. If a giant triangle appears about once a second, capture a frame and look
   at the draw that produced it. Add a `D3DDevice_DrawVertices` hook in
   `src/game/hooks.cpp` that logs the vertex format, the first vertex and the
   push pointer; compare with the OGlue `GENRY_PB_FOLLOW` note.
6. If the frame rate is below 60, lower the render scale to 66% or 50%, set
   the resolution floor to 50%, and try power mode 1 (big cores). If the phone
   is hot, use power mode 0 or 2.
7. If the screen flashes a shadow map, the suite's present path is not the
   OGlue window path; check the guest's `D3DDevice_Swap` front surface and the
   `ApplySettings()` target rebuild instead.

## Port hooks

`src/game/hooks.cpp` is empty by design. The recompiled function names are
`sub_<address>`; add a hook only when the log identifies a function and the
workaround is needed. Keep every workaround behind an environment variable so
the default build stays faithful to the original. The OGlue notes above are
the checklist for what to look for.

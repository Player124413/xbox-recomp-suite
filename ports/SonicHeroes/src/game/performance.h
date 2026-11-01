#pragma once
// Sonic Heroes' mobile performance profile: a 60 FPS target, a conservative
// internal resolution, dynamic resolution and big-core placement for the
// render thread. Everything is configurable from the in-game settings panel.
namespace sonic {

void InitPerformance();
void RenderThread();
void PresentThread();

}  // namespace sonic

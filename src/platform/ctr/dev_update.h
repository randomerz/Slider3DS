#pragma once

namespace plat {

// Downloads the latest build from the dev PC and overwrites the installed .3dsx.
// Owns gfx init/exit itself; the app should quit after calling it.
void runDevUpdate();

} // namespace plat

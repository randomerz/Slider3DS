#pragma once

namespace plat {

// ndsp up / down (platform_ctr's init / shutdown); without the DSP firmware audio stays off.
void initAudio();
void exitAudio();

} // namespace plat

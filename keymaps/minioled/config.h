#pragma once

// Smaller 0.91" OLED: same width (128), slightly lower height (32)
#ifdef OLED_DISPLAY_128X64
#    undef OLED_DISPLAY_128X64
#endif

#define OLED_DISPLAY_128X32

#pragma once

// Ensure TinyUSB host only advertises the VID/PID we actually support.
#ifdef CFG_TUH_CDC_CH34X_VID_PID_LIST
#undef CFG_TUH_CDC_CH34X_VID_PID_LIST
#endif

#define CFG_TUH_CDC_CH34X_VID_PID_LIST \
    {0x1A86, 0x7523}


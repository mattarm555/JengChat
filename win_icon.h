#pragma once

#ifdef _WIN32
void SetJengTaskbarIcon(void* windowHandle);
#else
inline void SetJengTaskbarIcon(void*) {}
#endif

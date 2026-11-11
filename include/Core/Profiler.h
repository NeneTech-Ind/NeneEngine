#pragma once

#ifdef NENEENGINE_ENABLE_TRACY
#include <tracy/Tracy.hpp>

#define NENE_PROFILE_SCOPE(name) ZoneScopedN(name)
#define NENE_PROFILE_ZONE_NAME(name, size) ZoneName(name, size)
#define NENE_PROFILE_ZONE_TEXT(text, size) ZoneText(text, size)
#define NENE_PROFILE_THREAD_NAME(name) tracy::SetThreadName(name)
#define NENE_PROFILE_FRAME() FrameMark
#define NENE_PROFILE_PLOT(name, value) TracyPlot(name, static_cast<int64_t>(value))
#define NENE_PROFILE_IS_CONNECTED() TracyIsConnected
#else
#define NENE_PROFILE_SCOPE(name) ((void)0)
#define NENE_PROFILE_ZONE_NAME(name, size) ((void)0)
#define NENE_PROFILE_ZONE_TEXT(text, size) ((void)0)
#define NENE_PROFILE_THREAD_NAME(name) ((void)(name))
#define NENE_PROFILE_FRAME() ((void)0)
#define NENE_PROFILE_PLOT(name, value) ((void)(value))
#define NENE_PROFILE_IS_CONNECTED() false
#endif

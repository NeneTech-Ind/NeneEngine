#pragma once

#ifdef NENEENGINE_ENABLE_TRACY
#include <tracy/Tracy.hpp>

#define NENE_PROFILE_SCOPE(name) ZoneScopedN(name)
#define NENE_PROFILE_ZONE_NAME(name, size) ZoneName(name, size)
#define NENE_PROFILE_THREAD_NAME(name) tracy::SetThreadName(name)
#define NENE_PROFILE_FRAME() FrameMark
#else
#define NENE_PROFILE_SCOPE(name) ((void)0)
#define NENE_PROFILE_ZONE_NAME(name, size) ((void)0)
#define NENE_PROFILE_THREAD_NAME(name) ((void)(name))
#define NENE_PROFILE_FRAME() ((void)0)
#endif

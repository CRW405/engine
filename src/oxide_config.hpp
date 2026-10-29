#pragma once

// Build-time configuration for Oxide.
//
// Both features default to enabled for development builds. Projects that want
// a lean release build can disable them through CMake:
//
//   -DOXIDE_ENABLE_VALIDATION=OFF
//   -DOXIDE_ENABLE_LOGGING=OFF
//
// The CMake options define these macros as 0 or 1 for every translation unit
// that links against the oxide target.

#ifndef OXIDE_ENABLE_VALIDATION
#define OXIDE_ENABLE_VALIDATION 1
#endif

#ifndef OXIDE_ENABLE_LOGGING
#define OXIDE_ENABLE_LOGGING 1
#endif

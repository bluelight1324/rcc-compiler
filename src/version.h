#ifndef RCC_VERSION_H
#define RCC_VERSION_H

// RCC version information — single source of truth
// Major.Minor.Patch — follows semantic versioning
#define RCC_VERSION_MAJOR 6
#define RCC_VERSION_MINOR 0
#define RCC_VERSION_PATCH 0

// Build number — auto-incremented by CMake on each build
// Defined via CMake: -DRCC_BUILD_NUMBER=N
#ifndef RCC_BUILD_NUMBER
#define RCC_BUILD_NUMBER 0
#endif

// Stringify helpers
#define RCC_STR2(x) #x
#define RCC_STR(x) RCC_STR2(x)

// Full version string: "6.0.0" or "6.0.0.42" (with build number)
#if RCC_BUILD_NUMBER > 0
#define RCC_VERSION RCC_STR(RCC_VERSION_MAJOR) "." RCC_STR(RCC_VERSION_MINOR) "." RCC_STR(RCC_VERSION_PATCH) "." RCC_STR(RCC_BUILD_NUMBER)
#else
#define RCC_VERSION RCC_STR(RCC_VERSION_MAJOR) "." RCC_STR(RCC_VERSION_MINOR) "." RCC_STR(RCC_VERSION_PATCH)
#endif

// Numeric version for __RCC_VERSION__ macro (major * 100 + minor * 10 + patch)
#define RCC_VERSION_NUM (RCC_VERSION_MAJOR * 100 + RCC_VERSION_MINOR * 10 + RCC_VERSION_PATCH)

#endif // RCC_VERSION_H

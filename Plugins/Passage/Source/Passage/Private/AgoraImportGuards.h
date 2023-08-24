
#pragma once

/* The purpose of this file is to isolate the Agora includes inside blocks
 * that isolate Unreal from Windows API leakage.
 */

#if PLATFORM_WINDOWS
 // These API guards are provided to prevent leakage from the Windows API 
 // overwriting macros like TEXT.
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"

// Before we modify the warnings that the compiler will emit, we save the state
#pragma warning(push)
// Now suppress the spurious compiler warnings that Visual Studio is throwing 
// at us.
#pragma warning(disable: 4582 4583)


#endif // PLATFORM_WINDOWS

#include "AgoraBase.h"
#include "AgoraMediaBase.h"
#include "IAgoraRtcEngine.h"

#if PLATFORM_WINDOWS
// restore the error reporting state to the previous value
#pragma warning(pop)
// close out the UE4 provided Windows API guards
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif // PLATFORM_WINDOWS
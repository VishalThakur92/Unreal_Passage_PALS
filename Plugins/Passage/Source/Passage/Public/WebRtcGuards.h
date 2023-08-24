// Copyright Enva Division

#pragma once

/* The purpose of this file is to isolate the WebRTC includes inside blocks 
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

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/audio_device/include/audio_device.h"
#include "pc/peer_connection_factory.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/time_utils.h"


#if PLATFORM_WINDOWS
// restore the error reporting state to the previous value
#pragma warning(pop)
// close out the UE4 provided Windows API guards
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif // PLATFORM_WINDOWS
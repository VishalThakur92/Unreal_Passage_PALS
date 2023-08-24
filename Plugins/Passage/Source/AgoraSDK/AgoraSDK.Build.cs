using System.IO;
using UnrealBuildTool;

// For information on third party libraries and the Unreal build system, see:
// https://docs.unrealengine.com/5.0/en-US/integrating-third-party-libraries-into-unreal-engine/

// For a specific example of Unreal/Agora integration, see the
// AgoraIO-Extensions repo for their semi-official Unreal plugin:
// https://github.com/AgoraIO-Extensions/Agora-Unreal-RTC-SDK/blob/main/Agora-Unreal-SDK-CPP/AgoraPlugin/Source/ThirdParty/AgoraPluginLibrary/AgoraPluginLibrary.Build.cs

public class AgoraSDK: ModuleRules
{
    public AgoraSDK(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            var AdditionalLibraries = new[]
            {
                "agora_rtc_sdk.dll.lib",
                "libagora-fdkaac.dll.lib",
                "libagora-ffmpeg.dll.lib",
                "libagora-soundtouch.dll.lib",
                "libagora_ai_echo_cancellation_extension.dll.lib",
                "libagora_ai_noise_suppression_extension.dll.lib",
                "libagora_audio_beauty_extension.dll.lib",
                "libagora_clear_vision_extension.dll.lib",
                "libagora_content_inspect_extension.dll.lib",
                "libagora_drm_loader_extension.dll.lib",
                "libagora_face_detection_extension.dll.lib",
                "libagora_screen_capture_extension.dll.lib",
                "libagora_segmentation_extension.dll.lib",
                "libagora_spatial_audio_extension.dll.lib",
                "libagora_udrm3_extension.lib",
                "libagora_video_decoder_extension.dll.lib",
                "libagora_video_encoder_extension.dll.lib",
                "libagora_video_quality_analyzer_extension.dll.lib",
                "video_dec.lib",
                "video_enc.lib",
            };
            foreach (var Item in AdditionalLibraries)
            {
                var AbsPath = Path.Combine(ModuleDirectory, "Win64", Item);
                PublicAdditionalLibraries.Add(AbsPath);
                RuntimeDependencies.Add(
			        Path.Combine(Path.Combine("$(ProjectDir)/Binaries/", Target.Platform+"", Item)),
			        AbsPath);
            }

            var DynamicLinkLibraries = new[]
            {
                "agora_rtc_sdk.dll",
                "glfw3.dll",
                "libagora-core.dll",
                "libagora-fdkaac.dll",
                "libagora-ffmpeg.dll",
                "libagora-soundtouch.dll",
                "libagora-wgc.dll",
                "libagora_ai_echo_cancellation_extension.dll",
                "libagora_ai_noise_suppression_extension.dll",
                "libagora_audio_beauty_extension.dll",
                "libagora_clear_vision_extension.dll",
                "libagora_content_inspect_extension.dll",
                "libagora_drm_loader_extension.dll",
                "libagora_face_detection_extension.dll",
                "libagora_screen_capture_extension.dll",
                "libagora_segmentation_extension.dll",
                "libagora_spatial_audio_extension.dll",
                "libagora_udrm3_extension.dll",
                "libagora_video_decoder_extension.dll",
                "libagora_video_encoder_extension.dll",
                "libagora_video_quality_analyzer_extension.dll",
                "video_dec.dll",
                "video_enc.dll",
            };
            foreach (var Item in DynamicLinkLibraries)
            {
                var AbsPath = Path.Combine(ModuleDirectory, "Win64", Item);
                PublicDelayLoadDLLs.Add(AbsPath);
                RuntimeDependencies.Add(
			        Path.Combine(Path.Combine("$(ProjectDir)/Binaries/", Target.Platform+"", Item)),
			        AbsPath);
            }

        }
        else if(Target.Platform == UnrealTargetPlatform.Mac)
        {
        }
    }
}

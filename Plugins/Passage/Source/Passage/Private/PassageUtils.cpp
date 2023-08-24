// Fill out your copyright notice in the Description page of Project Settings.


#include "PassageUtils.h"

#include "Commandlets/Commandlet.h"

DEFINE_LOG_CATEGORY(LogPassageUtils);

void UPassageUtils::GetConfigValue(const FString& Name, const FString& Default, FString& Value)
{
	// Highest precedence, return a command line parameter if present
	FString CmdLine = FCommandLine::Get();
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;
	UCommandlet::ParseCommandLine(*CmdLine, Tokens, Switches, Params);

	if (Params.Contains(Name)) {
		Value = Params[Name];
		return;
	}

	// If we have not returned a command line parameter, then we check if
	// there's an environment variable.
	FString EnvVar = FPlatformMisc::GetEnvironmentVariable(*Name);
	if (!EnvVar.IsEmpty()) {
		Value = EnvVar;
		return;
	}

	// If neither the command line nor the environment variable are present,
	// then we return the provided default value.
	Value = Default;
}

bool UPassageUtils::HasConfigFlag(const FString& Name)
{
	FString CmdLine = FCommandLine::Get();
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;
	UCommandlet::ParseCommandLine(*CmdLine, Tokens, Switches, Params);

	// if there is a bare switch like -MySwitch (even without an argument)
	if(Switches.Contains(Name))
	{
		return true;
	}
	// if there is a command line param like -MySpiffyParam=somevalue
	if(Params.Contains(Name))
	{
		return true;
	}
	// if the environment variable has a value, i.e. is non-empty
	if (const FString EnvVar = FPlatformMisc::GetEnvironmentVariable(*Name); !EnvVar.IsEmpty()) {
		return true;
	}
	// if we have no switch, no param, and no environment variable, then we
	// don't have the flag, so we return false
	return false;
}

FString UPassageUtils::GetBackendRoot()
{
	FString DirectoryUrl;
	UPassageUtils::GetConfigValue(
		TEXT("DirectoryUrl"),
		TEXT("wss://mm.passage3d.com/api/v1/directory/game"),
		DirectoryUrl);
	FString Prefix;
	FString Trash;
	DirectoryUrl.Split(TEXT("/directory"), &Prefix, &Trash);

	const auto Result = FString::Printf(TEXT("http%s"), &Prefix[2]);
	return Result;
}

bool UPassageUtils::ParseUInt32(const FString& Input, uint32& Result)
{
	const auto End = Input.Len() - 1;
	uint32 Total = 0;
	uint32 PlaceValue = 1;

	if(End < 0)
	{
		UE_LOG(LogPassageUtils, Error,
			TEXT("UPassageUtils::ParseUInt32() Cannot parse empty string"));
		return false;
	}

	for(int32 i=End; i >= 0; i--)
	{
		switch (Input[i])
		{
		case '0':
			break;
		case '1':
			Total += PlaceValue;
			break;
		case '2':
			Total += 2 * PlaceValue;
			break;
		case '3':
			Total += 3 * PlaceValue;
			break;
		case '4':
			Total += 4 * PlaceValue;
			break;
		case '5':
			Total += 5 * PlaceValue;
			break;
		case '6':
			Total += 6 * PlaceValue;
			break;
		case '7':
			Total += 7 * PlaceValue;
			break;
		case '8':
			Total += 8 * PlaceValue;
			break;
		case '9':
			Total += 9 * PlaceValue;
			break;
		default:
			UE_LOG(LogPassageUtils, Error,
				TEXT("UPassageUtils::ParseUInt32() Unrecognized character '%c'"), Input[i]);
			return false;
		}
		PlaceValue *= 10;
	}

	Result = Total;
	return true;
}

UTexture2D* UPassageUtils::CreateVideoTexture(const uint32 Width, const uint32 Height)
{
	const uint32_t ImgDataLength = Width * Height * 4;

	// See the call to webrtc::ConvertFromI420(...) for the generator that
	// matches this format. The byte ordering ends up being reversed, for
	// some reason. Conveniently, the format is the same for Agora and
	// WebRTC.
	const auto Texture2D = UTexture2D::CreateTransient(
		Width, Height, PF_B8G8R8A8);
	Texture2D->UpdateResource();

	uint8* ImgData = new uint8[ImgDataLength];
	for (uint32 i = 0; i < ImgDataLength; i += 4)
	{
		ImgData[i + 0] = 0;
		ImgData[i + 1] = 0;
		ImgData[i + 2] = 0;
		ImgData[i + 3] = 255;
	}

	// Note that this call will asynchronously free ImgData (on the render
	// thread) once the update is complete.
	UpdateVideoTexture(Texture2D, ImgData, Width, Height);
	
	return Texture2D;
}

void UPassageUtils::UpdateVideoTexture(UTexture2D* VideoTexture, uint8* ImgData,
	const uint32 Width,	const uint32 Height)
{
	const auto Regions = 
		new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);

	VideoTexture->UpdateTextureRegions(
		0, // update MipIndex 0, i.e. the most detailed level
		1, // only update the 0th level, we don't need mipmaps for video
		Regions,
		Width * 4u, // "pitch" of data", i.e. bytes/pixel * width
		4u,			// the size of each pixel in bytes
		ImgData,
		[](const uint8* ImgData, const FUpdateTextureRegion2D* Regions)
		{
			delete Regions;
			delete[] ImgData;
		}
	);
}

bool UPassageUtils::WriteArrayToFile(const TArray<uint8>& Bytes, const FString& FilePath)
{
	return FFileHelper::SaveArrayToFile(Bytes, *FilePath);
}

FColor UPassageUtils::HexToColor(FString HexString)
{
	return FColor::FromHex(HexString);
}

FString UPassageUtils::ColorToHex(FColor Color)
{
	return Color.ToHex();
}
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "PassageUtils.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPassageUtils, Log, All);

/**
 * 
 */
UCLASS(Blueprintable)
class PASSAGE_API UPassageUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	/**
	 * @brief Checks the environment and command line parameters for a given
	 * name and returns the first matching value. The order of checking is
	 * commandline, environment, and the given default. Other config sources
	 * may be added as they become available.
	 * @param Name 
	 * @param Default 
	 * @param Value 
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Passage")
	static void GetConfigValue(
		UPARAM() const FString& Name,
		UPARAM() const FString& Default,
		UPARAM() FString& Value
		);

	/**
	 * Checks if a command line flag is present or if the given name has a
	 * command line parameter, or environment variable. Note that if a
	 * parameter is given as -MySpiffyParam=false, this will still return true.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Passage")
	static bool HasConfigFlag(UPARAM()const FString& Name);

	/**
	 * @brief Derives the HTTP(s) root of the backend from the DirectoryUrl.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Passage")
	static FString GetBackendRoot();

	/**
	 * @brief Parse a decimal string into a 32 bit integer. This was written
	 * for reading the Agora UID after it has been transmitted as a string.
	 * @param Input This should be an FString consisting of characters 0-9 with
	 * no other characters like decimal point, whitespace, etc.
	 * @param Result The result will be written into this int32 value, and the
	 * status is indicated in the return value
	 * @returns true if the parse succeeded, false if it encountered a
	 * non-numeric character in the input.
	 */
	static bool ParseUInt32(const FString& Input, uint32& Result);

	/**
	 * @brief Convenience to create a texture in the format we use for video.
	 * @param Width The width of the texture expressed in pixels
	 * @param Height The height of the texture expressed in pixels
	 * @return A "transient" texture, which won't be saved when the game is
	 * serialized, i.e. during a save game. The texture is initialized to
	 * opaque black.
	 */
	static UTexture2D* CreateVideoTexture(const uint32 Width, const uint32 Height);

	/**
	 * @brief Convenience to update a texture in the format we use for video.
	 * @param VideoTexture The existing texture to update. This was probably
	 * created by the CreateVideoTexture static method of this class.
	 * @param ImgData A uint8 array with length Width * Height * 4, where the
	 * channels are arranged according to the Unreal format PF_B8G8R8A8 (a
	 * global constant in Unreal C++). IMPORTANTLY, this array will be freed
	 * once the texture is updated.
	 * @param Width The width of the image in pixels. I believe this needs to
	 * match the source (ImgData), but maybe not necessarily the size of the
	 * texture itself. I think it squishes if they don't match.
	 * @param Height The height of the image in pixels. (see Width above)
	 */
	static void UpdateVideoTexture(UTexture2D* VideoTexture, uint8* ImgData,
	                               const uint32 Width, const uint32 Height);

	//**Write an array to a filePath -Vishal**//
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Passage")
	static bool WriteArrayToFile(const TArray<uint8>& Bytes, const FString& FilePath);


	/** Converts hex string to color. Supports formats RGB, RRGGBB, RRGGBBAA, RGB, #RRGGBB, #RRGGBBAA */
	UFUNCTION(BlueprintCallable, Category = "Passage")
	static FColor HexToColor(FString HexString);

	/** Converts color to hex string */
	UFUNCTION(BlueprintCallable, Category = "Passage")
	static FString ColorToHex(FColor Color);
};

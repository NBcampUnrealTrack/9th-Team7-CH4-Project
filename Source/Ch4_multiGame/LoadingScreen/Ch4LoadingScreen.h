// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UCh4LoadingScreenDataAsset;
class UTexture2D;
struct FLoadingScreenAttributes;

namespace Ch4LoadingScreen
{
	/** Compares full package paths, allowing URL options, object paths and PIE prefixes. */
	FString GetMapPackageName(const FString& MapPath);

	/** Pure policy check. The caller also verifies the destination package exists on disk. */
	bool ShouldShowLoadingScreen(const UCh4LoadingScreenDataAsset* Data,
		const FString& SourceMap, const FString& DestinationMap, bool bAlreadyPrepared = false);

	/** Only accepts resolved textures; empty/invalid candidates fall back to the legacy image. */
	UTexture2D* SelectRandomImage(TConstArrayView<TObjectPtr<UTexture2D>> Images, UTexture2D* FallbackImage);

	/** Builds Slate content on the game thread; no asset loading or MoviePlayer playback here. */
	FLoadingScreenAttributes BuildAttributes(const UCh4LoadingScreenDataAsset* Data, UTexture2D* LoadedImage);
}

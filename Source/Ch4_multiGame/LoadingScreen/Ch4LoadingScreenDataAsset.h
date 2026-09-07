// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Ch4LoadingScreenDataAsset.generated.h"

class UTexture2D;
class UWorld;

/** Local lobby-to-gameplay loading rules and editor-replaceable images. */
UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4LoadingScreenDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Required source world. An unset map disables the loading screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading Screen|Travel")
	TSoftObjectPtr<UWorld> LobbyMap;

	/** Allowed destinations. Keep these in sync with the lobby's Gameplay Maps pool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading Screen|Travel")
	TArray<TSoftObjectPtr<UWorld>> AllowedGameplayMaps;

	/** One valid image is chosen locally once per eligible travel, independently of the map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading Screen|Images")
	TArray<TSoftObjectPtr<UTexture2D>> BackgroundImages;

	/** Legacy serialized property. Used when the array has no successfully loaded images. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading Screen|Images", meta=(DisplayName="Background Image (Fallback)"))
	TSoftObjectPtr<UTexture2D> BackgroundImage;

	/** Seconds, measured from playback start. Loading must also finish before the screen closes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading Screen", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float MinimumDisplayTime = 0.0f;
};

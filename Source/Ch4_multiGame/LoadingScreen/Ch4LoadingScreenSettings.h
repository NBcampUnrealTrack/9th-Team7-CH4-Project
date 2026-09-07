// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Ch4LoadingScreenSettings.generated.h"

class UCh4LoadingScreenDataAsset;

/** Project Settings > Game > Loading Screen. Keeps the existing native GameInstance class. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Loading Screen"))
class CH4_MULTIGAME_API UCh4LoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("LoadingScreen"); }

	/** Shared settings for every map. Include the asset/image directory in packaged builds. */
	UPROPERTY(Config, EditAnywhere, Category="Loading Screen")
	TSoftObjectPtr<UCh4LoadingScreenDataAsset> LoadingScreenData;
};

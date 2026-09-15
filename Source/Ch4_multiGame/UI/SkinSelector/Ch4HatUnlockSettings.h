// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Ch4HatUnlockSettings.generated.h"

class UCh4HatUnlockConfigDataAsset;

/** Project Settings > Game > Hat Unlocks. Keeps the existing native GameInstance class. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Hat Unlocks"))
class CH4_MULTIGAME_API UCh4HatUnlockSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("HatUnlocks"); }

	/** Shared requirement asset used by progress, selection validation, and Skin Selector UI. */
	UPROPERTY(Config, EditAnywhere, Category="Hat Unlocks")
	TSoftObjectPtr<UCh4HatUnlockConfigDataAsset> HatUnlockConfig;
};

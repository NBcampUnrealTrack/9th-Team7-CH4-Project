// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Ch4HatUnlockConfigDataAsset.generated.h"

namespace Ch4Headwear
{
	CH4_MULTIGAME_API extern const FName DrinkingHat;
	CH4_MULTIGAME_API extern const FName IronHelmet;
	CH4_MULTIGAME_API extern const FName Snapback;
	CH4_MULTIGAME_API extern const FName TrooperHat;
}

/** Designer-owned thresholds for the four achievement-locked hats. */
UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4HatUnlockConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** IronHelmet row shown as Knight in WBP_SkinSelector. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hat Unlocks|Score", meta=(ClampMin="0", UIMin="0"))
	int32 KnightScoreRequirement = 1000;

	/** DrinkingHat row shown as Drink Helmet in WBP_SkinSelector. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hat Unlocks|Score", meta=(ClampMin="0", UIMin="0"))
	int32 DrinkHelmetScoreRequirement = 2000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hat Unlocks|Delivered Cargo", meta=(ClampMin="0", UIMin="0"))
	int32 SnapbackCargoRequirement = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hat Unlocks|Delivered Cargo", meta=(ClampMin="0", UIMin="0"))
	int32 TrooperCargoRequirement = 20;

	/** None and hats outside this achievement set remain available. */
	UFUNCTION(BlueprintPure, Category="Hat Unlocks")
	bool IsHeadwearUnlocked(FName HeadwearID, int32 BestSingleGameScore,
		int32 BestSingleGameDeliveredCargo) const;

	/** Localized text suitable for an optional lock tooltip or label. */
	UFUNCTION(BlueprintPure, Category="Hat Unlocks")
	FText GetRequirementText(FName HeadwearID) const;
};

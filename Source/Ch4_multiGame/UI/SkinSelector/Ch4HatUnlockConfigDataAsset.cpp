// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SkinSelector/Ch4HatUnlockConfigDataAsset.h"

#define LOCTEXT_NAMESPACE "Ch4HatUnlockConfig"

namespace Ch4Headwear
{
	const FName DrinkingHat(TEXT("DrinkingHat"));
	const FName IronHelmet(TEXT("IronHelmet"));
	const FName Snapback(TEXT("Snapback"));
	const FName TrooperHat(TEXT("TrooperHat"));
}

bool UCh4HatUnlockConfigDataAsset::IsHeadwearUnlocked(
	const FName HeadwearID,
	const int32 BestSingleGameScore,
	const int32 BestSingleGameDeliveredCargo) const
{
	const int32 SafeBestScore = FMath::Max(BestSingleGameScore, 0);
	const int32 SafeBestCargo = FMath::Max(BestSingleGameDeliveredCargo, 0);

	if (HeadwearID == Ch4Headwear::IronHelmet)
	{
		return SafeBestScore >= FMath::Max(KnightScoreRequirement, 0);
	}
	if (HeadwearID == Ch4Headwear::DrinkingHat)
	{
		return SafeBestScore >= FMath::Max(DrinkHelmetScoreRequirement, 0);
	}
	if (HeadwearID == Ch4Headwear::Snapback)
	{
		return SafeBestCargo >= FMath::Max(SnapbackCargoRequirement, 0);
	}
	if (HeadwearID == Ch4Headwear::TrooperHat)
	{
		return SafeBestCargo >= FMath::Max(TrooperCargoRequirement, 0);
	}

	return true;
}

FText UCh4HatUnlockConfigDataAsset::GetRequirementText(const FName HeadwearID) const
{
	if (HeadwearID == Ch4Headwear::IronHelmet)
	{
		return FText::Format(LOCTEXT("KnightRequirement", "Best score {0}+"),
			FText::AsNumber(FMath::Max(KnightScoreRequirement, 0)));
	}
	if (HeadwearID == Ch4Headwear::DrinkingHat)
	{
		return FText::Format(LOCTEXT("DrinkHelmetRequirement", "Best score {0}+"),
			FText::AsNumber(FMath::Max(DrinkHelmetScoreRequirement, 0)));
	}
	if (HeadwearID == Ch4Headwear::Snapback)
	{
		return FText::Format(LOCTEXT("SnapbackRequirement", "Deliver {0} cargo in one game"),
			FText::AsNumber(FMath::Max(SnapbackCargoRequirement, 0)));
	}
	if (HeadwearID == Ch4Headwear::TrooperHat)
	{
		return FText::Format(LOCTEXT("TrooperRequirement", "Deliver {0} cargo in one game"),
			FText::AsNumber(FMath::Max(TrooperCargoRequirement, 0)));
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingScreen/Ch4LoadingScreen.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "LoadingScreen/Ch4LoadingScreenDataAsset.h"
#include "Misc/PackageName.h"
#include "MoviePlayer.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Owns both the brush and its texture until MoviePlayer releases the last widget reference. */
	class SCh4LoadingScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCh4LoadingScreen) {}
			SLATE_ARGUMENT(UTexture2D*, BackgroundImage)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			UTexture2D* Image = IsValid(InArgs._BackgroundImage) ? InArgs._BackgroundImage : nullptr;
			BackgroundImage.Reset(Image);
			BackgroundBrush.DrawAs = Image ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
			if (Image)
			{
				BackgroundBrush.SetResourceObject(Image);
				BackgroundBrush.ImageSize = FVector2D(
					FMath::Max(Image->GetSizeX(), 1), FMath::Max(Image->GetSizeY(), 1));
			}

			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor::Black)
				.Padding(0.0f)
				[
					SNew(SOverlay)
					.Clipping(EWidgetClipping::ClipToBoundsAlways)
					+ SOverlay::Slot()
					[
						SNew(SScaleBox)
						.Stretch(EStretch::ScaleToFill)
						.StretchDirection(EStretchDirection::Both)
						[
							SNew(SImage).Image(&BackgroundBrush)
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(32.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Ch4LoadingScreen", "Loading", "Loading..."))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 24))
						.ColorAndOpacity(FLinearColor::White)
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.ShadowColorAndOpacity(FLinearColor::Black)
					]
				]
			];
		}

	private:
		TStrongObjectPtr<UTexture2D> BackgroundImage;
		FSlateBrush BackgroundBrush;
	};
}

FString Ch4LoadingScreen::GetMapPackageName(const FString& MapPath)
{
	FString PackageName = MapPath;
	int32 OptionsIndex = INDEX_NONE;
	if (PackageName.FindChar(TEXT('?'), OptionsIndex))
	{
		PackageName.LeftInline(OptionsIndex);
	}
	PackageName = UWorld::RemovePIEPrefix(FPackageName::ObjectPathToPackageName(PackageName));
	return FPackageName::IsValidLongPackageName(PackageName) ? PackageName : FString();
}

bool Ch4LoadingScreen::ShouldShowLoadingScreen(const UCh4LoadingScreenDataAsset* Data,
	const FString& SourceMap, const FString& DestinationMap, bool bAlreadyPrepared)
{
	if (!IsValid(Data) || bAlreadyPrepared)
	{
		return false;
	}
	const FString Source = GetMapPackageName(SourceMap);
	const FString Destination = GetMapPackageName(DestinationMap);
	const FString Lobby = GetMapPackageName(Data->LobbyMap.ToSoftObjectPath().GetLongPackageName());
	if (Source.IsEmpty() || Destination.IsEmpty() || Lobby.IsEmpty()
		|| Source != Lobby || Destination == Lobby)
	{
		return false;
	}
	return Data->AllowedGameplayMaps.ContainsByPredicate([&Destination](const TSoftObjectPtr<UWorld>& Map)
	{
		return GetMapPackageName(Map.ToSoftObjectPath().GetLongPackageName()) == Destination;
	});
}

UTexture2D* Ch4LoadingScreen::SelectRandomImage(
	TConstArrayView<TObjectPtr<UTexture2D>> Images, UTexture2D* FallbackImage)
{
	TArray<UTexture2D*, TInlineAllocator<8>> ValidImages;
	for (UTexture2D* Image : Images)
	{
		if (IsValid(Image))
		{
			ValidImages.AddUnique(Image);
		}
	}
	return ValidImages.IsEmpty()
		? (IsValid(FallbackImage) ? FallbackImage : nullptr)
		: ValidImages[FMath::RandHelper(ValidImages.Num())];
}

FLoadingScreenAttributes Ch4LoadingScreen::BuildAttributes(
	const UCh4LoadingScreenDataAsset* Data, UTexture2D* LoadedImage)
{
	FLoadingScreenAttributes Attributes;
	Attributes.MinimumLoadingScreenDisplayTime = IsValid(Data) && FMath::IsFinite(Data->MinimumDisplayTime)
		? FMath::Max(Data->MinimumDisplayTime, 0.0f)
		: 0.0f;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bWaitForManualStop = false;
	Attributes.bMoviesAreSkippable = false;
	Attributes.bAllowEngineTick = false;
	Attributes.bAllowInEarlyStartup = false;
	Attributes.WidgetLoadingScreen = SNew(SCh4LoadingScreen).BackgroundImage(LoadedImage);
	return Attributes;
}

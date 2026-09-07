// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "LoadingScreen/Ch4LoadingScreen.h"
#include "LoadingScreen/Ch4LoadingScreenDataAsset.h"
#include "MoviePlayer.h"
#include "UObject/StrongObjectPtr.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4LoadingScreenSettingsTest,
	"Ch4_multiGame.LoadingScreen.Settings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4LoadingScreenSettingsTest::RunTest(const FString& Parameters)
{
	// Attribute/widget construction only: never start MoviePlayer or claim to test actual rendering.
	FLoadingScreenAttributes Attributes = Ch4LoadingScreen::BuildAttributes(nullptr, nullptr);
	TestTrue(TEXT("Missing DataAsset still provides a valid fallback screen"), Attributes.IsValid());
	TestTrue(TEXT("Fallback has a Slate widget"), Attributes.WidgetLoadingScreen.IsValid());
	TestEqual(TEXT("Default screen adds no artificial minimum time"), Attributes.MinimumLoadingScreenDisplayTime, 0.0f);
	TestTrue(TEXT("Screen automatically completes after loading"), Attributes.bAutoCompleteWhenLoadingCompletes);
	TestFalse(TEXT("Screen never requires a manual close"), Attributes.bWaitForManualStop);
	TestFalse(TEXT("Loading cannot be dismissed by input"), Attributes.bMoviesAreSkippable);
	TestFalse(TEXT("No gameplay ticks while waiting for the screen"), Attributes.bAllowEngineTick);
	TestFalse(TEXT("UObject images are not used during early engine startup"), Attributes.bAllowInEarlyStartup);
	TestTrue(TEXT("No movie or map paths are required"), Attributes.MoviePaths.IsEmpty());

	TStrongObjectPtr<UCh4LoadingScreenDataAsset> Data(NewObject<UCh4LoadingScreenDataAsset>());
	TestTrue(TEXT("Background image is optional by default"), Data->BackgroundImage.IsNull());
	Data->MinimumDisplayTime = 2.0f;
	Attributes = Ch4LoadingScreen::BuildAttributes(Data.Get(), nullptr);
	TestTrue(TEXT("Configured data without an image remains safe"), Attributes.IsValid());
	TestEqual(TEXT("Configured minimum time reaches MoviePlayer"), Attributes.MinimumLoadingScreenDisplayTime, 2.0f);
	TestTrue(TEXT("Minimum time does not disable auto-completion"), Attributes.bAutoCompleteWhenLoadingCompletes);

	TStrongObjectPtr<UTexture2D> Image(NewObject<UTexture2D>());
	Data->BackgroundImage = Image.Get();
	Attributes = Ch4LoadingScreen::BuildAttributes(Data.Get(), Image.Get());
	TestTrue(TEXT("A resolved texture creates a valid screen without rendering"), Attributes.IsValid());
	Attributes = Ch4LoadingScreen::BuildAttributes(Data.Get(), nullptr);
	TestTrue(TEXT("A reference whose texture failed to resolve uses the fallback"), Attributes.IsValid());

	Data->MinimumDisplayTime = -5.0f;
	Attributes = Ch4LoadingScreen::BuildAttributes(Data.Get(), nullptr);
	TestEqual(TEXT("Negative minimum time clamps to zero"), Attributes.MinimumLoadingScreenDisplayTime, 0.0f);
	Data->MinimumDisplayTime = std::numeric_limits<float>::infinity();
	Attributes = Ch4LoadingScreen::BuildAttributes(Data.Get(), nullptr);
	TestEqual(TEXT("Non-finite minimum cannot hold loading open forever"), Attributes.MinimumLoadingScreenDisplayTime, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4LoadingScreenTravelRulesTest,
	"Ch4_multiGame.LoadingScreen.TravelRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4LoadingScreenTravelRulesTest::RunTest(const FString& Parameters)
{
	// Synthetic soft paths test policy without loading worlds or requiring future maps to exist.
	const FString Lobby = TEXT("/Game/Lobby/L_Lobby");
	const FString Forest = TEXT("/Game/Map/Level/ForestLevel");
	const FString Space = TEXT("/Game/Map/Level/SpaceLevel");
	TStrongObjectPtr<UCh4LoadingScreenDataAsset> Data(NewObject<UCh4LoadingScreenDataAsset>());
	Data->LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(Lobby + TEXT(".L_Lobby")));
	Data->AllowedGameplayMaps = {
		TSoftObjectPtr<UWorld>(FSoftObjectPath(Forest + TEXT(".ForestLevel"))),
		TSoftObjectPtr<UWorld>(FSoftObjectPath(Space + TEXT(".SpaceLevel"))),
		TSoftObjectPtr<UWorld>()
	};
	const auto ShouldShow = [&Data](const FString& Source, const FString& Destination, bool bActive = false)
	{
		return Ch4LoadingScreen::ShouldShowLoadingScreen(Data.Get(), Source, Destination, bActive);
	};
	TestTrue(TEXT("Lobby to configured Forest shows loading"), ShouldShow(Lobby, Forest));
	TestTrue(TEXT("Future Space candidate uses the same rule"), ShouldShow(Lobby, Space));
	TestFalse(TEXT("Gameplay reload does not show loading"), ShouldShow(Forest, Forest));
	TestFalse(TEXT("Gameplay to another allowed map does not show loading"), ShouldShow(Forest, Space));
	TestFalse(TEXT("Gameplay to Lobby does not show loading"), ShouldShow(Forest, Lobby));
	TestFalse(TEXT("Startup to Lobby does not show loading"), ShouldShow(TEXT(""), Lobby));
	TestFalse(TEXT("Startup directly into gameplay does not show loading"), ShouldShow(TEXT(""), Forest));
	TestFalse(TEXT("Unconfigured destination does not show loading"), ShouldShow(Lobby, TEXT("/Game/Unknown/InvalidMap")));
	TestFalse(TEXT("Empty destination does not show loading"), ShouldShow(Lobby, TEXT("")));
	TestFalse(TEXT("Short map name is not an exact package reference"), ShouldShow(Lobby, TEXT("ForestLevel")));
	TestFalse(TEXT("Same short name in another folder is not the allowed map"), ShouldShow(Lobby, TEXT("/Game/Other/ForestLevel")));
	TestFalse(TEXT("Duplicate callback cannot prepare or choose another image"), ShouldShow(Lobby, Forest, true));
	TestTrue(TEXT("A completed screen allows the next eligible travel"), ShouldShow(Lobby, Forest, false));
	TestTrue(TEXT("Listen URL options do not affect map identity"), ShouldShow(Lobby, Forest + TEXT("?listen?game=Example")));
	TestTrue(TEXT("PIE source and destination prefixes normalize for policy tests"),
		ShouldShow(TEXT("/Game/Lobby/UEDPIE_1_L_Lobby"), TEXT("/Game/Map/Level/UEDPIE_1_ForestLevel")));
	TestTrue(TEXT("Object reference paths normalize to the same package"),
		ShouldShow(Lobby + TEXT(".L_Lobby"), Forest + TEXT(".ForestLevel")));
	TestFalse(TEXT("Missing data disables the screen"),
		Ch4LoadingScreen::ShouldShowLoadingScreen(nullptr, Lobby, Forest));
	Data->AllowedGameplayMaps.Add(Data->LobbyMap);
	TestFalse(TEXT("Lobby reload stays excluded even if mistakenly allowed"), ShouldShow(Lobby, Lobby));
	Data->AllowedGameplayMaps.Reset();
	TestFalse(TEXT("An empty allowed list disables the screen"), ShouldShow(Lobby, Forest));
	Data->AllowedGameplayMaps.Add(TSoftObjectPtr<UWorld>(FSoftObjectPath(Forest + TEXT(".ForestLevel"))));
	Data->LobbyMap.Reset();
	TestFalse(TEXT("Missing source map configuration disables the screen"), ShouldShow(Lobby, Forest));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4LoadingScreenRandomImageTest,
	"Ch4_multiGame.LoadingScreen.RandomImage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4LoadingScreenRandomImageTest::RunTest(const FString& Parameters)
{
	TArray<TObjectPtr<UTexture2D>> Images;
	TStrongObjectPtr<UTexture2D> LegacyImage(NewObject<UTexture2D>());
	TStrongObjectPtr<UTexture2D> FirstImage(NewObject<UTexture2D>());
	TStrongObjectPtr<UTexture2D> SecondImage(NewObject<UTexture2D>());
	TStrongObjectPtr<UTexture2D> ThirdImage(NewObject<UTexture2D>());
	TestNull(TEXT("No images safely selects the black background"), Ch4LoadingScreen::SelectRandomImage(Images, nullptr));
	TestTrue(TEXT("Empty array preserves the legacy image"),
		Ch4LoadingScreen::SelectRandomImage(Images, LegacyImage.Get()) == LegacyImage.Get());
	Images.Add(nullptr);
	TestTrue(TEXT("All unresolved images safely fall back"),
		Ch4LoadingScreen::SelectRandomImage(Images, LegacyImage.Get()) == LegacyImage.Get());
	TestNull(TEXT("Unresolved images with no fallback select black"), Ch4LoadingScreen::SelectRandomImage(Images, nullptr));
	Images.Add(FirstImage.Get());
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		TestTrue(TEXT("One valid candidate always wins over the fallback"),
			Ch4LoadingScreen::SelectRandomImage(Images, LegacyImage.Get()) == FirstImage.Get());
	}
	Images.Add(SecondImage.Get());
	Images.Add(ThirdImage.Get());
	for (int32 Attempt = 0; Attempt < 32; ++Attempt)
	{
		UTexture2D* Selected = Ch4LoadingScreen::SelectRandomImage(Images, LegacyImage.Get());
		TestTrue(TEXT("Every random choice belongs to the valid array, never null or fallback"),
			Selected == FirstImage.Get() || Selected == SecondImage.Get() || Selected == ThirdImage.Get());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

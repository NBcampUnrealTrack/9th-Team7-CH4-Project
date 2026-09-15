#if WITH_DEV_AUTOMATION_TESTS

#include "UI/SkinSelector/Ch4HatUnlockConfigDataAsset.h"

#include "Ch4_multiGamePlayerController.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Player/Ch4PlayerProgressSaveGame.h"
#include "Player/Ch4_multiGameGameInstance.h"
#include "UI/SkinSelector/Ch4SkinSelectorViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4HatUnlockProgressTest,
	"Ch4_multiGame.Player.HatUnlockProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4HatUnlockProgressTest::RunTest(const FString& Parameters)
{
	UCh4HatUnlockConfigDataAsset* Config = NewObject<UCh4HatUnlockConfigDataAsset>();
	TestNotNull(TEXT("Hat unlock config can be constructed"), Config);
	if (!Config)
	{
		return false;
	}

	TestEqual(TEXT("Knight default score requirement"), Config->KnightScoreRequirement, 1000);
	TestEqual(TEXT("Drink Helmet default score requirement"), Config->DrinkHelmetScoreRequirement, 2000);
	TestEqual(TEXT("Snapback default cargo requirement"), Config->SnapbackCargoRequirement, 10);
	TestEqual(TEXT("Trooper default cargo requirement"), Config->TrooperCargoRequirement, 20);
	TestFalse(TEXT("Knight is locked at 999"),
		Config->IsHeadwearUnlocked(Ch4Headwear::IronHelmet, 999, 0));
	TestTrue(TEXT("Knight unlocks at 1000"),
		Config->IsHeadwearUnlocked(Ch4Headwear::IronHelmet, 1000, 0));
	TestFalse(TEXT("Drink Helmet is locked at 1999"),
		Config->IsHeadwearUnlocked(Ch4Headwear::DrinkingHat, 1999, 0));
	TestTrue(TEXT("Drink Helmet unlocks at 2000"),
		Config->IsHeadwearUnlocked(Ch4Headwear::DrinkingHat, 2000, 0));
	TestFalse(TEXT("Snapback is locked at 9 delivered cargo"),
		Config->IsHeadwearUnlocked(Ch4Headwear::Snapback, 0, 9));
	TestTrue(TEXT("Snapback unlocks at 10 delivered cargo"),
		Config->IsHeadwearUnlocked(Ch4Headwear::Snapback, 0, 10));
	TestFalse(TEXT("Trooper is locked at 19 delivered cargo"),
		Config->IsHeadwearUnlocked(Ch4Headwear::TrooperHat, 0, 19));
	TestTrue(TEXT("Trooper unlocks at 20 delivered cargo"),
		Config->IsHeadwearUnlocked(Ch4Headwear::TrooperHat, 0, 20));
	TestTrue(TEXT("No Hat is always available"),
		Config->IsHeadwearUnlocked(NAME_None, 0, 0));
	TestTrue(TEXT("Unrelated future hats are not locked by this four-hat policy"),
		Config->IsHeadwearUnlocked(TEXT("FutureFreeHat"), 0, 0));

	Config->KnightScoreRequirement = 500;
	Config->SnapbackCargoRequirement = 5;
	TestFalse(TEXT("Customized Knight threshold is read from the config at 499"),
		Config->IsHeadwearUnlocked(Ch4Headwear::IronHelmet, 499, 0));
	TestTrue(TEXT("Customized Knight threshold unlocks exactly at 500"),
		Config->IsHeadwearUnlocked(Ch4Headwear::IronHelmet, 500, 0));
	TestFalse(TEXT("Customized Snapback threshold is read from the config at 4"),
		Config->IsHeadwearUnlocked(Ch4Headwear::Snapback, 0, 4));
	TestTrue(TEXT("Customized Snapback threshold unlocks exactly at 5"),
		Config->IsHeadwearUnlocked(Ch4Headwear::Snapback, 0, 5));

	UCh4PlayerProgressSaveGame* Progress = NewObject<UCh4PlayerProgressSaveGame>();
	TestNotNull(TEXT("Progress SaveGame can be constructed"), Progress);
	if (!Progress)
	{
		return false;
	}

	FCh4GameResult NoResult;
	NoResult.FinalCargoScore = 9999;
	NoResult.DeliveredCargoCount = 99;
	TestFalse(TEXT("A preparation/no-result state cannot update progress"),
		Progress->ApplyGameResult(NoResult));
	TestEqual(TEXT("No-result score remains zero"), Progress->BestSingleGameScore, 0);
	TestEqual(TEXT("No-result cargo remains zero"), Progress->BestSingleGameDeliveredCargo, 0);

	FCh4GameResult FirstResult;
	FirstResult.bResultAvailable = true;
	FirstResult.FinalCargoScore = 600;
	FirstResult.DeliveredCargoCount = 6;
	TestTrue(TEXT("First authoritative result improves both records"),
		Progress->ApplyGameResult(FirstResult));

	FCh4GameResult SecondResult = FirstResult;
	SecondResult.FinalCargoScore = 700;
	SecondResult.DeliveredCargoCount = 7;
	TestTrue(TEXT("Second authoritative result improves both records"),
		Progress->ApplyGameResult(SecondResult));
	TestEqual(TEXT("Scores are not accumulated across games"), Progress->BestSingleGameScore, 700);
	TestEqual(TEXT("Delivered cargo is not accumulated across games"),
		Progress->BestSingleGameDeliveredCargo, 7);

	FCh4GameResult LowerResult = FirstResult;
	LowerResult.FinalCargoScore = 500;
	LowerResult.DeliveredCargoCount = 5;
	TestFalse(TEXT("A lower result does not request another save"),
		Progress->ApplyGameResult(LowerResult));
	TestEqual(TEXT("Lower score cannot reduce the record"), Progress->BestSingleGameScore, 700);
	TestEqual(TEXT("Lower cargo cannot reduce the record"),
		Progress->BestSingleGameDeliveredCargo, 7);

	FCh4GameResult MixedResult = FirstResult;
	MixedResult.FinalCargoScore = 1800;
	MixedResult.DeliveredCargoCount = 4;
	TestTrue(TEXT("One improved field updates without changing the other record"),
		Progress->ApplyGameResult(MixedResult));
	TestEqual(TEXT("Score advances by Max"), Progress->BestSingleGameScore, 1800);
	TestEqual(TEXT("Cargo record is preserved by Max"),
		Progress->BestSingleGameDeliveredCargo, 7);

	Progress->BestSingleGameScore = 2100;
	Progress->BestSingleGameDeliveredCargo = 21;
	TArray<uint8> SaveBytes;
	TestTrue(TEXT("Progress serializes through UE SaveGame"),
		UGameplayStatics::SaveGameToMemory(Progress, SaveBytes));
	UCh4PlayerProgressSaveGame* LoadedProgress = Cast<UCh4PlayerProgressSaveGame>(
		UGameplayStatics::LoadGameFromMemory(SaveBytes));
	TestNotNull(TEXT("Progress deserializes through UE SaveGame"), LoadedProgress);
	if (LoadedProgress)
	{
		TestEqual(TEXT("Loaded best score persists"), LoadedProgress->BestSingleGameScore, 2100);
		TestEqual(TEXT("Loaded best cargo persists"),
			LoadedProgress->BestSingleGameDeliveredCargo, 21);
		TestTrue(TEXT("Loaded record unlocks Knight"),
			Config->IsHeadwearUnlocked(Ch4Headwear::IronHelmet,
				LoadedProgress->BestSingleGameScore,
				LoadedProgress->BestSingleGameDeliveredCargo));
		TestTrue(TEXT("Loaded record unlocks Drink Helmet"),
			Config->IsHeadwearUnlocked(Ch4Headwear::DrinkingHat,
				LoadedProgress->BestSingleGameScore,
				LoadedProgress->BestSingleGameDeliveredCargo));
		TestTrue(TEXT("Loaded record unlocks Snapback"),
			Config->IsHeadwearUnlocked(Ch4Headwear::Snapback,
				LoadedProgress->BestSingleGameScore,
				LoadedProgress->BestSingleGameDeliveredCargo));
		TestTrue(TEXT("Loaded record unlocks Trooper"),
			Config->IsHeadwearUnlocked(Ch4Headwear::TrooperHat,
				LoadedProgress->BestSingleGameScore,
				LoadedProgress->BestSingleGameDeliveredCargo));
	}

	const FString TestSlot = FString::Printf(TEXT("Ch4HatUnlockAutomation_%s"), *FGuid::NewGuid().ToString());
	constexpr int32 TestUserIndex = 0;
	TestTrue(TEXT("Progress writes through the platform SaveGame system"),
		UGameplayStatics::SaveGameToSlot(Progress, TestSlot, TestUserIndex));
	UCh4PlayerProgressSaveGame* DiskLoadedProgress = Cast<UCh4PlayerProgressSaveGame>(
		UGameplayStatics::LoadGameFromSlot(TestSlot, TestUserIndex));
	TestNotNull(TEXT("Progress reloads from the platform SaveGame system"), DiskLoadedProgress);
	if (DiskLoadedProgress)
	{
		TestEqual(TEXT("Disk-loaded best score persists"), DiskLoadedProgress->BestSingleGameScore, 2100);
		TestEqual(TEXT("Disk-loaded best cargo persists"), DiskLoadedProgress->BestSingleGameDeliveredCargo, 21);
	}
	TestTrue(TEXT("Automation progress slot is removed after verification"),
		UGameplayStatics::DeleteGameInSlot(TestSlot, TestUserIndex));

	TestEqual(TEXT("Locked UI state shows a hit-test-invisible lock"),
		UCh4SkinSelectorViewModel::GetLockVisibilityForUnlockedState(false),
		ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("Unlocked UI state collapses the lock"),
		UCh4SkinSelectorViewModel::GetLockVisibilityForUnlockedState(true),
		ESlateVisibility::Collapsed);

	const UFunction* ControllerDumpFunction =
		ACh4_multiGamePlayerController::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(ACh4_multiGamePlayerController, DumpHatUnlockState));
	TestNotNull(TEXT("PlayerController exposes the Hat debug dump"), ControllerDumpFunction);
	if (ControllerDumpFunction)
	{
		TestTrue(TEXT("Hat debug dump is callable from the PIE console"),
			ControllerDumpFunction->HasAnyFunctionFlags(FUNC_Exec));
	}
	const UFunction* GameInstanceDumpFunction =
		UCh4_multiGameGameInstance::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UCh4_multiGameGameInstance, DumpHatUnlockState));
	TestNotNull(TEXT("GameInstance exposes the complete Hat state dump"), GameInstanceDumpFunction);
	if (GameInstanceDumpFunction)
	{
		TestTrue(TEXT("GameInstance Hat dump is available to Blueprint debug flows"),
			GameInstanceDumpFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	}

	return true;
}

#endif

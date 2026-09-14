#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Ch4_multiGameGameMode.h"
#include "Ch4_multiGamePlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "UI/GameResult/Ch4GameResultWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4GameResultPresentationTest,
	"Ch4_multiGame.UI.GameResultPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4GameResultPresentationTest::RunTest(const FString&)
{
	TestEqual(TEXT("Success title"),
		UCh4GameResultWidget::GetResultTitle(true).ToString(), FString(TEXT("DELIVERY COMPLETE")));
	TestEqual(TEXT("Failure title"),
		UCh4GameResultWidget::GetResultTitle(false).ToString(), FString(TEXT("DELIVERY FAILED")));
	TestEqual(TEXT("Sub-minute clear time format"),
		UCh4GameResultWidget::FormatClearTime(34.25f).ToString(), FString(TEXT("00:34.25")));
	TestEqual(TEXT("Multi-minute clear time format"),
		UCh4GameResultWidget::FormatClearTime(154.72f).ToString(), FString(TEXT("02:34.72")));
	TestEqual(TEXT("Ten-minute clear time format"),
		UCh4GameResultWidget::FormatClearTime(605.42f).ToString(), FString(TEXT("10:05.42")));
	TestEqual(TEXT("Score uses localized grouping"),
		UCh4GameResultWidget::FormatScore(1850).ToString(), FString(TEXT("1,850")));
	TestEqual(TEXT("Countdown uses the replicated server deadline"),
		UCh4GameResultWidget::CalculateCountdownSeconds(110.0f, 100.0f), 10);
	TestEqual(TEXT("Countdown never displays a negative value"),
		UCh4GameResultWidget::CalculateCountdownSeconds(100.0f, 110.0f), 0);

	const FClassProperty* WidgetClassProperty = FindFProperty<FClassProperty>(
		ACh4_multiGamePlayerController::StaticClass(), TEXT("GameResultWidgetClass"));
	TestTrue(TEXT("Controller accepts only Ch4 Game Result Widget subclasses"),
		WidgetClassProperty
			&& WidgetClassProperty->HasAnyPropertyFlags(CPF_Edit)
			&& WidgetClassProperty->MetaClass == UCh4GameResultWidget::StaticClass());

#if WITH_METADATA
	const auto HasWidgetMetadata = [this](const TCHAR* PropertyName, const TCHAR* MetadataName)
	{
		const FProperty* Property = FindFProperty<FProperty>(
			UCh4GameResultWidget::StaticClass(), FName(PropertyName));
		TestNotNull(FString::Printf(TEXT("%s exists"), PropertyName), Property);
		return Property && Property->HasMetaData(FName(MetadataName));
	};
	TestTrue(TEXT("Result title is required"), HasWidgetMetadata(TEXT("Text_ResultTitle"), TEXT("BindWidget")));
	TestTrue(TEXT("Clear time is required"), HasWidgetMetadata(TEXT("Text_ClearTime"), TEXT("BindWidget")));
	TestTrue(TEXT("Cargo count is required"), HasWidgetMetadata(TEXT("Text_CargoCount"), TEXT("BindWidget")));
	TestTrue(TEXT("Total score is required"), HasWidgetMetadata(TEXT("Text_TotalScore"), TEXT("BindWidget")));
	TestTrue(TEXT("Countdown is optional"), HasWidgetMetadata(TEXT("Text_Countdown"), TEXT("BindWidgetOptional")));
	TestTrue(TEXT("Confirm button is optional"), HasWidgetMetadata(TEXT("Btn_Confirm"), TEXT("BindWidgetOptional")));
	TestTrue(TEXT("FadeInAnim is optional"), HasWidgetMetadata(TEXT("FadeInAnim"), TEXT("BindWidgetAnimOptional")));
#endif

	if (!GEngine)
	{
		return false;
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World)
	{
		return false;
	}
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ACh4_multiGameGameState* State = World->SpawnActor<ACh4_multiGameGameState>();
	ACh4_multiGameGameMode* Rule = World->SpawnActor<ACh4_multiGameGameMode>();
	World->SetGameState(State);
	bool bSuccess = State && Rule;
	if (bSuccess)
	{
		FCh4DeliveryScoreSummary Summary;
		Summary.bHasScoreData = true;
		Summary.DeliveredCargoCount = 3;
		Summary.DeliveredCargoScore = 700;
		AActor* GoalTarget = World->SpawnActor<AActor>();
		Rule->SetDeliveryScoreSummaryForTesting(GoalTarget, Summary);
		bSuccess = Rule->RequestCargoInitialization(3)
			&& Rule->RequestGameStart();
		Rule->SetGameplayTimesForTesting(100.0f, 254.72f);
		bSuccess = bSuccess && Rule->NotifyGoalReached(GoalTarget);
		const FCh4GameResult Result = State->GetGameResult();
		TestEqual(TEXT("Snapshot title is ready for the Widget"),
			UCh4GameResultWidget::GetResultTitle(Result.bSucceeded).ToString(),
			FString(TEXT("DELIVERY COMPLETE")));
		TestEqual(TEXT("Snapshot time is ready for the Widget"),
			UCh4GameResultWidget::FormatClearTime(Result.ClearTimeSeconds).ToString(),
			FString(TEXT("02:34.72")));
		TestEqual(TEXT("Snapshot Cargo count"), Result.DeliveredCargoCount, 3);
		TestEqual(TEXT("Snapshot score"), Result.FinalCargoScore, 700);
	}

	World->EndPlay(EEndPlayReason::Quit);
	World->SetGameState(nullptr);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return bSuccess;
}

#endif

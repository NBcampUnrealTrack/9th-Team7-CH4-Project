#include "UI/SkinSelector/Ch4SkinSelectorViewModel.h"
#include "UI/SkinSelector/Ch4_CharacterPreviewStudio.h"
#include "Ch4_multiGame.h"
#include "Blueprint/UserWidget.h"
#include "Ch4_multiGamePlayerController.h"
#include "Player/Ch4_multiGamePlayerState.h"
#include "Player/Ch4_multiGameGameInstance.h"
#include "Player/Ch4_PlayerCharacter.h"
#include "UI/SkinSelector/Ch4HatUnlockConfigDataAsset.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "View/MVVMView.h"

UWorld* UCh4SkinSelectorViewModel::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed | RF_FinishDestroyed))
	{
		return nullptr;
	}

	for (UObject* CurrOuter = GetOuter(); CurrOuter; CurrOuter = CurrOuter->GetOuter())
	{
		if (UWorld* World = CurrOuter->GetWorld())
		{
			return World;
		}
	}

	if (GEngine && GEngine->GetWorldContexts().Num() > 0)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				return Context.World();
			}
		}
	}

	return Super::GetWorld();
}

void UCh4SkinSelectorViewModel::InitializeFromPlayerState()
{
	RefreshHatUnlockState();

	bool bFoundType = false;
	if (ACh4_multiGamePlayerState* PS = GetOwningCh4PlayerState())
	{
		ECh4CharacterType CurrentType = PS->GetCharacterType();
		if (Ch4Character::IsValidType(CurrentType))
		{
			SetPendingCharacterType(CurrentType);
			bFoundType = true;
		}
	}

	// PlayerState에 유효한 타입이 없으면 현재 조종 중인 캐릭터 폰의 클래스에서 확인
	if (!bFoundType)
	{
		if (const ACh4_PlayerCharacter* Character = GetOwningCh4Character())
		{
			const FString ClassName = Character->GetClass()->GetName();
			if (ClassName.Contains(TEXT("Dog")))
			{
				SetPendingCharacterType(ECh4CharacterType::Dog);
			}
			else if (ClassName.Contains(TEXT("Gorilla")))
			{
				SetPendingCharacterType(ECh4CharacterType::Gorilla);
			}
			else if (ClassName.Contains(TEXT("Otter")))
			{
				SetPendingCharacterType(ECh4CharacterType::Otter);
			}
			else
			{
				SetPendingCharacterType(ECh4CharacterType::Cat);
			}
		}
		else
		{
			SetPendingCharacterType(ECh4CharacterType::Cat);
		}
	}

	// 현재 인게임 캐릭터가 착용 중인 모자 읽어오기
	FName CurrentHat = GetEquippedHeadwearIDFromCharacter();
	if (!IsHeadwearUnlocked(CurrentHat))
	{
		CurrentHat = NAME_None;
		if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
		{
			PC->RequestHeadwear(NAME_None);
		}
	}
	SetPendingHeadwearID(CurrentHat);

	UpdateAnimalSelectionBooleans();
	UpdateHatSelectionBooleans();

	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->SetPreviewCharacterType(PendingCharacterType);
		ActivePreviewStudio->SetPreviewHeadwear(PendingHeadwearID);
	}
}

ESlateVisibility UCh4SkinSelectorViewModel::GetLockVisibilityForUnlockedState(const bool bUnlocked)
{
	return bUnlocked ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
}

bool UCh4SkinSelectorViewModel::IsHeadwearUnlocked(const FName HeadwearID) const
{
	if (const ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		return PC->CanSelectHeadwear(HeadwearID);
	}

	// A detached ViewModel is a safe locked state for achievement hats while keeping
	// None and unrelated/free hats available according to the same policy object.
	return GetDefault<UCh4HatUnlockConfigDataAsset>()->IsHeadwearUnlocked(HeadwearID, 0, 0);
}

void UCh4SkinSelectorViewModel::RefreshHatUnlockState()
{
	UUserWidget* OwningWidget = GetOwningUserWidget();
	UMVVMView* WidgetView = OwningWidget ? OwningWidget->GetExtension<UMVVMView>() : nullptr;
	UObject* BoundViewModel = WidgetView
		? WidgetView->GetViewModel(TEXT("Ch4SkinSelectorViewModel")).GetObject()
		: nullptr;
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[SkinSelector] MVVM source=%s RefreshTarget=%s SameInstance=%s SourcesInitialized=%s BindingsInitialized=%s"),
		*GetNameSafe(BoundViewModel),
		*GetNameSafe(this),
		BoundViewModel == this ? TEXT("true") : TEXT("false"),
		WidgetView && WidgetView->AreSourcesInitialized() ? TEXT("true") : TEXT("false"),
		WidgetView && WidgetView->AreBindingsInitialized() ? TEXT("true") : TEXT("false"));
	if (BoundViewModel && BoundViewModel != this)
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[SkinSelector] ERROR: The refreshed ViewModel is not the View Binding source; use one ViewModel instance in WBP_SkinSelector"));
	}

	const ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController();
	const UCh4_multiGameGameInstance* GameInstance = PC
		? PC->GetGameInstance<UCh4_multiGameGameInstance>() : nullptr;
	const UCh4HatUnlockConfigDataAsset* Config = GameInstance
		? GameInstance->GetHatUnlockConfig()
		: GetDefault<UCh4HatUnlockConfigDataAsset>();
	const int32 BestScore = GameInstance ? GameInstance->GetBestSingleGameScore() : 0;
	const int32 BestCargo = GameInstance ? GameInstance->GetBestSingleGameDeliveredCargo() : 0;
	if (!PC || !GameInstance)
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[SkinSelector] ERROR: ViewModel=%s Widget=%s Controller=%s GameInstance=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OwningWidget),
			*GetNameSafe(PC),
			*GetNameSafe(GameInstance));
	}
	if (!Config)
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[SkinSelector] ERROR: No unlock config is available; refresh aborted"));
		return;
	}

	const bool bInitialRefresh = !bHasRefreshedHatUnlockState;
	const bool bOldDrinkingUnlocked = bIsHatDrinkingUnlocked;
	const bool bOldHelmetUnlocked = bIsHatHelmetUnlocked;
	const bool bOldTrooperUnlocked = bIsHatTrooperUnlocked;
	const bool bOldSnapbackUnlocked = bIsHatSnapbackUnlocked;
	const ESlateVisibility OldDrinkingLockVisibility = HatDrinkingLockVisibility;
	const ESlateVisibility OldHelmetLockVisibility = HatHelmetLockVisibility;
	const ESlateVisibility OldTrooperLockVisibility = HatTrooperLockVisibility;
	const ESlateVisibility OldSnapbackLockVisibility = HatSnapbackLockVisibility;
	const FText OldDrinkingRequirementText = HatDrinkingRequirementText;
	const FText OldHelmetRequirementText = HatHelmetRequirementText;
	const FText OldTrooperRequirementText = HatTrooperRequirementText;
	const FText OldSnapbackRequirementText = HatSnapbackRequirementText;

	bIsHatDrinkingUnlocked = Config->IsHeadwearUnlocked(Ch4Headwear::DrinkingHat, BestScore, BestCargo);
	bIsHatHelmetUnlocked = Config->IsHeadwearUnlocked(Ch4Headwear::IronHelmet, BestScore, BestCargo);
	bIsHatTrooperUnlocked = Config->IsHeadwearUnlocked(Ch4Headwear::TrooperHat, BestScore, BestCargo);
	bIsHatSnapbackUnlocked = Config->IsHeadwearUnlocked(Ch4Headwear::Snapback, BestScore, BestCargo);
	HatDrinkingLockVisibility = GetLockVisibilityForUnlockedState(bIsHatDrinkingUnlocked);
	HatHelmetLockVisibility = GetLockVisibilityForUnlockedState(bIsHatHelmetUnlocked);
	HatTrooperLockVisibility = GetLockVisibilityForUnlockedState(bIsHatTrooperUnlocked);
	HatSnapbackLockVisibility = GetLockVisibilityForUnlockedState(bIsHatSnapbackUnlocked);
	HatDrinkingRequirementText = Config->GetRequirementText(Ch4Headwear::DrinkingHat);
	HatHelmetRequirementText = Config->GetRequirementText(Ch4Headwear::IronHelmet);
	HatTrooperRequirementText = Config->GetRequirementText(Ch4Headwear::TrooperHat);
	HatSnapbackRequirementText = Config->GetRequirementText(Ch4Headwear::Snapback);

	const bool bNotifyDrinkingUnlock = bInitialRefresh || bOldDrinkingUnlocked != bIsHatDrinkingUnlocked;
	const bool bNotifyHelmetUnlock = bInitialRefresh || bOldHelmetUnlocked != bIsHatHelmetUnlocked;
	const bool bNotifyTrooperUnlock = bInitialRefresh || bOldTrooperUnlocked != bIsHatTrooperUnlocked;
	const bool bNotifySnapbackUnlock = bInitialRefresh || bOldSnapbackUnlocked != bIsHatSnapbackUnlocked;
	const bool bNotifyDrinkingLock = bInitialRefresh || OldDrinkingLockVisibility != HatDrinkingLockVisibility;
	const bool bNotifyHelmetLock = bInitialRefresh || OldHelmetLockVisibility != HatHelmetLockVisibility;
	const bool bNotifyTrooperLock = bInitialRefresh || OldTrooperLockVisibility != HatTrooperLockVisibility;
	const bool bNotifySnapbackLock = bInitialRefresh || OldSnapbackLockVisibility != HatSnapbackLockVisibility;
	const bool bNotifyDrinkingRequirement = bInitialRefresh || !OldDrinkingRequirementText.EqualTo(HatDrinkingRequirementText);
	const bool bNotifyHelmetRequirement = bInitialRefresh || !OldHelmetRequirementText.EqualTo(HatHelmetRequirementText);
	const bool bNotifyTrooperRequirement = bInitialRefresh || !OldTrooperRequirementText.EqualTo(HatTrooperRequirementText);
	const bool bNotifySnapbackRequirement = bInitialRefresh || !OldSnapbackRequirementText.EqualTo(HatSnapbackRequirementText);

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[SkinSelector] Refresh ViewModel=%s Widget=%s Controller=%s GameInstance=%s Config=%s BestScore=%d BestCargo=%d Requirements={Knight:%d Drink:%d Snapback:%d Trooper:%d}"),
		*GetNameSafe(this),
		*GetNameSafe(OwningWidget),
		*GetNameSafe(PC),
		*GetNameSafe(GameInstance),
		*GetPathNameSafe(Config),
		BestScore,
		BestCargo,
		Config->KnightScoreRequirement,
		Config->DrinkHelmetScoreRequirement,
		Config->SnapbackCargoRequirement,
		Config->TrooperCargoRequirement);

	if (bNotifyDrinkingUnlock) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsHatDrinkingUnlocked);
	if (bNotifyHelmetUnlock) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsHatHelmetUnlocked);
	if (bNotifyTrooperUnlock) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsHatTrooperUnlocked);
	if (bNotifySnapbackUnlock) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsHatSnapbackUnlocked);
	if (bNotifyDrinkingLock) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HatDrinkingLockVisibility);
	if (bNotifyHelmetLock) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HatHelmetLockVisibility);
	if (bNotifyTrooperLock) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HatTrooperLockVisibility);
	if (bNotifySnapbackLock) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HatSnapbackLockVisibility);
	if (bNotifyDrinkingRequirement) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HatDrinkingRequirementText);
	if (bNotifyHelmetRequirement) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HatHelmetRequirementText);
	if (bNotifyTrooperRequirement) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HatTrooperRequirementText);
	if (bNotifySnapbackRequirement) UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HatSnapbackRequirementText);

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[SkinSelector] HelmetUnlocked %s -> %s UnlockFieldNotify=%s LockVisibility=%s LockFieldNotify=%s"),
		bOldHelmetUnlocked ? TEXT("true") : TEXT("false"),
		bIsHatHelmetUnlocked ? TEXT("true") : TEXT("false"),
		bNotifyHelmetUnlock ? TEXT("emitted") : TEXT("skipped"),
		*UEnum::GetValueAsString(HatHelmetLockVisibility),
		bNotifyHelmetLock ? TEXT("emitted") : TEXT("skipped"));
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[SkinSelector] DrinkingUnlocked %s -> %s UnlockFieldNotify=%s LockVisibility=%s LockFieldNotify=%s"),
		bOldDrinkingUnlocked ? TEXT("true") : TEXT("false"),
		bIsHatDrinkingUnlocked ? TEXT("true") : TEXT("false"),
		bNotifyDrinkingUnlock ? TEXT("emitted") : TEXT("skipped"),
		*UEnum::GetValueAsString(HatDrinkingLockVisibility),
		bNotifyDrinkingLock ? TEXT("emitted") : TEXT("skipped"));
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[SkinSelector] SnapbackUnlocked %s -> %s UnlockFieldNotify=%s LockVisibility=%s LockFieldNotify=%s"),
		bOldSnapbackUnlocked ? TEXT("true") : TEXT("false"),
		bIsHatSnapbackUnlocked ? TEXT("true") : TEXT("false"),
		bNotifySnapbackUnlock ? TEXT("emitted") : TEXT("skipped"),
		*UEnum::GetValueAsString(HatSnapbackLockVisibility),
		bNotifySnapbackLock ? TEXT("emitted") : TEXT("skipped"));
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[SkinSelector] TrooperUnlocked %s -> %s UnlockFieldNotify=%s LockVisibility=%s LockFieldNotify=%s"),
		bOldTrooperUnlocked ? TEXT("true") : TEXT("false"),
		bIsHatTrooperUnlocked ? TEXT("true") : TEXT("false"),
		bNotifyTrooperUnlock ? TEXT("emitted") : TEXT("skipped"),
		*UEnum::GetValueAsString(HatTrooperLockVisibility),
		bNotifyTrooperLock ? TEXT("emitted") : TEXT("skipped"));
	bHasRefreshedHatUnlockState = true;
}

void UCh4SkinSelectorViewModel::SetPendingCharacterType(ECh4CharacterType NewType)
{
	if (PendingCharacterType != NewType)
	{
		PendingCharacterType = NewType;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PendingCharacterType);
		UpdateAnimalSelectionBooleans();
	}
}

void UCh4SkinSelectorViewModel::SetPendingHeadwearID(FName NewHeadwearID)
{
	if (PendingHeadwearID != NewHeadwearID)
	{
		PendingHeadwearID = NewHeadwearID;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PendingHeadwearID);
		UpdateHatSelectionBooleans();
	}
}

void UCh4SkinSelectorViewModel::UpdateAnimalSelectionBooleans()
{
	bIsCatSelected = (PendingCharacterType == ECh4CharacterType::Cat);
	bIsDogSelected = (PendingCharacterType == ECh4CharacterType::Dog);
	bIsGorillaSelected = (PendingCharacterType == ECh4CharacterType::Gorilla);
	bIsOtterSelected = (PendingCharacterType == ECh4CharacterType::Otter);

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsCatSelected);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsDogSelected);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsGorillaSelected);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsOtterSelected);
}

void UCh4SkinSelectorViewModel::UpdateHatSelectionBooleans()
{
	bIsHatNoneSelected = PendingHeadwearID.IsNone();
	bIsHatDrinkingSelected = (PendingHeadwearID == FName(TEXT("DrinkingHat")));
	bIsHatHelmetSelected = (PendingHeadwearID == FName(TEXT("IronHelmet")));
	bIsHatTrooperSelected = (PendingHeadwearID == FName(TEXT("TrooperHat")));
	bIsHatSnapbackSelected = (PendingHeadwearID == FName(TEXT("Snapback")));

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsHatNoneSelected);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsHatDrinkingSelected);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsHatHelmetSelected);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsHatTrooperSelected);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsHatSnapbackSelected);
}

void UCh4SkinSelectorViewModel::StartPreviewStudio()
{
	if (ActivePreviewStudio)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 스폰 전에 플레이어의 실제 캐릭터와 모자를 먼저 동기화하여 불필요한 캐릭터 교체 딜레이 방지
	InitializeFromPlayerState();

	// 다른 플레이어 및 인게임 월드와 100% 격리된 지하 고립 위치에 스폰 (12m 지하)
	const FVector StudioLocation(0.0f, 0.0f, -1200.0f);
	const FRotator StudioRotation = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bNoFail = true;

	TSubclassOf<ACh4_CharacterPreviewStudio> ClassToSpawn = PreviewStudioClass
		? PreviewStudioClass
		: TSubclassOf<ACh4_CharacterPreviewStudio>(ACh4_CharacterPreviewStudio::StaticClass());

	ActivePreviewStudio = World->SpawnActor<ACh4_CharacterPreviewStudio>(ClassToSpawn, StudioLocation, StudioRotation, SpawnParams);
	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->SetPreviewCharacterType(PendingCharacterType);
		ActivePreviewStudio->SetPreviewHeadwear(PendingHeadwearID);
	}
}

void UCh4SkinSelectorViewModel::StopPreviewStudio()
{
	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->Destroy();
		ActivePreviewStudio = nullptr;
	}
	RestoreGameInputMode();
}

void UCh4SkinSelectorViewModel::BeginDestroy()
{
	if (ActivePreviewStudio && IsValid(ActivePreviewStudio))
	{
		ActivePreviewStudio->Destroy();
		ActivePreviewStudio = nullptr;
	}
	Super::BeginDestroy();
}

void UCh4SkinSelectorViewModel::AddPreviewYaw(float DeltaYaw)
{
	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->AddPreviewYaw(DeltaYaw);
	}
}

void UCh4SkinSelectorViewModel::ResetPreviewRotation()
{
	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->ResetPreviewRotation();
	}
}

void UCh4SkinSelectorViewModel::SetPreviewBackdropColor(const FLinearColor& NewColor)
{
	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->SetBackdropColor(NewColor);
	}
}

void UCh4SkinSelectorViewModel::SelectCharacterType(ECh4CharacterType NewType)
{
	SetPendingCharacterType(NewType);

	// 인게임 캐릭터는 건드리지 않고 3D 프리뷰 스튜디오의 마네킹만 즉시 변경
	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->SetPreviewCharacterType(NewType);
	}
}

void UCh4SkinSelectorViewModel::SelectHeadwear(FName HeadwearID)
{
	if (!IsHeadwearUnlocked(HeadwearID))
	{
		return;
	}
	SetPendingHeadwearID(HeadwearID);

	// 인게임 캐릭터는 건드리지 않고 3D 프리뷰 스튜디오의 마네킹만 즉시 변경
	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->SetPreviewHeadwear(HeadwearID);
	}
}

void UCh4SkinSelectorViewModel::SaveSelection()
{
	if (!IsHeadwearUnlocked(PendingHeadwearID))
	{
		SetPendingHeadwearID(NAME_None);
	}

	// 1. 서버 RPC 호출로 캐릭터 스킨 및 모자 영구 저장
	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		PC->RequestCharacterType(PendingCharacterType);
		PC->RequestHeadwear(PendingHeadwearID);
	}

	// 2. 실제 인게임 캐릭터에 최종 모자 장착
	ApplyHeadwearToInGameCharacter(PendingHeadwearID);

	// 3. UI 닫기 및 인풋 모드 복원
	CloseFittingRoom();
}

void UCh4SkinSelectorViewModel::ResetSelection()
{
	if (ACh4_multiGamePlayerState* PS = GetOwningCh4PlayerState())
	{
		ECh4CharacterType OriginalType = PS->GetCharacterType();
		if (Ch4Character::IsValidType(OriginalType))
		{
			SetPendingCharacterType(OriginalType);
		}
	}

	FName OriginalHat = GetEquippedHeadwearIDFromCharacter();
	if (!IsHeadwearUnlocked(OriginalHat))
	{
		OriginalHat = NAME_None;
	}
	SetPendingHeadwearID(OriginalHat);

	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->SetPreviewCharacterType(PendingCharacterType);
		ActivePreviewStudio->SetPreviewHeadwear(PendingHeadwearID);
		ActivePreviewStudio->ResetPreviewRotation();
	}
}

void UCh4SkinSelectorViewModel::CloseFittingRoom()
{
	StopPreviewStudio();
	RestoreGameInputMode();

	if (UUserWidget* Widget = GetOwningUserWidget())
	{
		Widget->RemoveFromParent();
	}
}

// ----------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------

UUserWidget* UCh4SkinSelectorViewModel::GetOwningUserWidget() const
{
	for (UObject* CurrOuter = const_cast<UCh4SkinSelectorViewModel*>(this)->GetOuter(); CurrOuter; CurrOuter = CurrOuter->GetOuter())
	{
		if (UUserWidget* Widget = Cast<UUserWidget>(CurrOuter))
		{
			return Widget;
		}
	}
	return nullptr;
}

void UCh4SkinSelectorViewModel::RestoreGameInputMode()
{
	if (HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
	{
		return;
	}

	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}
}

ACh4_multiGamePlayerController* UCh4SkinSelectorViewModel::GetOwningCh4PlayerController() const
{
	if (HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
	{
		return nullptr;
	}

	if (const UUserWidget* Widget = GetOwningUserWidget())
	{
		if (ACh4_multiGamePlayerController* PC = Cast<ACh4_multiGamePlayerController>(Widget->GetOwningPlayer()))
		{
			return PC;
		}
	}
	if (const UWorld* World = GetWorld())
	{
		if (GEngine)
		{
			if (APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(World))
			{
				return Cast<ACh4_multiGamePlayerController>(LocalPC);
			}
		}
		return Cast<ACh4_multiGamePlayerController>(World->GetFirstPlayerController());
	}
	return nullptr;
}

ACh4_multiGamePlayerState* UCh4SkinSelectorViewModel::GetOwningCh4PlayerState() const
{
	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		return PC->GetPlayerState<ACh4_multiGamePlayerState>();
	}
	return nullptr;
}

ACh4_PlayerCharacter* UCh4SkinSelectorViewModel::GetOwningCh4Character() const
{
	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		return Cast<ACh4_PlayerCharacter>(PC->GetPawn());
	}
	return nullptr;
}

FName UCh4SkinSelectorViewModel::GetEquippedHeadwearIDFromCharacter() const
{
	if (const ACh4_PlayerCharacter* Character = GetOwningCh4Character())
	{
		for (const UActorComponent* Comp : Character->GetComponents())
		{
			if (Comp && Comp->GetClass()->GetName().Contains(TEXT("BPC_AccessoryEquipment")))
			{
				if (const FProperty* Prop = Comp->GetClass()->FindPropertyByName(TEXT("EquippedHeadwearID")))
				{
					if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
					{
						return NameProp->GetPropertyValue_InContainer(Comp);
					}
				}
				break;
			}
		}
	}
	return NAME_None;
}

void UCh4SkinSelectorViewModel::ApplyHeadwearToInGameCharacter(FName HeadwearID)
{
	if (ACh4_PlayerCharacter* Character = GetOwningCh4Character())
	{
		Character->ApplyHeadwear(HeadwearID);
	}
}

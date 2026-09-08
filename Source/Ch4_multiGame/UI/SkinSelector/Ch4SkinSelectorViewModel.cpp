#include "UI/SkinSelector/Ch4SkinSelectorViewModel.h"
#include "UI/SkinSelector/Ch4_CharacterPreviewStudio.h"
#include "Blueprint/UserWidget.h"
#include "Ch4_multiGamePlayerController.h"
#include "Player/Ch4_multiGamePlayerState.h"
#include "Player/Ch4_PlayerCharacter.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

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
	SetPendingHeadwearID(CurrentHat);

	UpdateAnimalSelectionBooleans();
	UpdateHatSelectionBooleans();

	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->SetPreviewCharacterType(PendingCharacterType);
		ActivePreviewStudio->SetPreviewHeadwear(PendingHeadwearID);
	}
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
	SetPendingHeadwearID(HeadwearID);

	// 인게임 캐릭터는 건드리지 않고 3D 프리뷰 스튜디오의 마네킹만 즉시 변경
	if (ActivePreviewStudio)
	{
		ActivePreviewStudio->SetPreviewHeadwear(HeadwearID);
	}
}

void UCh4SkinSelectorViewModel::SaveSelection()
{
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

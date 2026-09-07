#include "UI/SkinSelector/Ch4SkinSelectorViewModel.h"
#include "Blueprint/UserWidget.h"
#include "Ch4_multiGamePlayerController.h"
#include "Player/Ch4_multiGamePlayerState.h"
#include "Player/Ch4_PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"

void UCh4SkinSelectorViewModel::InitializeFromPlayerState()
{
	if (ACh4_multiGamePlayerState* PS = GetOwningCh4PlayerState())
	{
		ECh4CharacterType CurrentType = PS->GetCharacterType();
		if (Ch4Character::IsValidType(CurrentType))
		{
			SetPendingCharacterType(CurrentType);
			return;
		}
	}
	// 기본값 초기화
	SetPendingCharacterType(ECh4CharacterType::Cat);
}

void UCh4SkinSelectorViewModel::SetPendingCharacterType(ECh4CharacterType NewType)
{
	if (PendingCharacterType != NewType)
	{
		PendingCharacterType = NewType;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PendingCharacterType);

		// 선택된 카드 하이라이트 불리언들 자동 갱신
		UpdateSelectionBooleans();
	}
}

void UCh4SkinSelectorViewModel::UpdateSelectionBooleans()
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

void UCh4SkinSelectorViewModel::SelectCharacterType(ECh4CharacterType NewType)
{
	SetPendingCharacterType(NewType);
	
	// 로컬 화면 속 캐릭터 즉시 미리보기
	ApplyPreview(NewType);
}

void UCh4SkinSelectorViewModel::ApplyPreview(ECh4CharacterType TypeToPreview)
{
	if (ACh4_PlayerCharacter* Character = GetOwningCh4Character())
	{
		Character->ApplyCharacterType(TypeToPreview);
	}
}

void UCh4SkinSelectorViewModel::SaveSelection()
{
	// Controller를 통해 Server RPC 호출 (멀티플레이 & 맵 이동 저장)
	if (ACh4_multiGamePlayerController* PC = GetOwningCh4PlayerController())
	{
		PC->RequestCharacterType(PendingCharacterType);
	}
}

void UCh4SkinSelectorViewModel::ResetSelection()
{
	// 원래 PlayerState의 값으로 되돌리기
	if (ACh4_multiGamePlayerState* PS = GetOwningCh4PlayerState())
	{
		ECh4CharacterType OriginalType = PS->GetCharacterType();
		if (Ch4Character::IsValidType(OriginalType))
		{
			SetPendingCharacterType(OriginalType);
			ApplyPreview(OriginalType);
			return;
		}
	}
	SetPendingCharacterType(ECh4CharacterType::Cat);
	ApplyPreview(ECh4CharacterType::Cat);
}

void UCh4SkinSelectorViewModel::CloseFittingRoom()
{
	ResetSelection();
}

// ----------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------

ACh4_multiGamePlayerController* UCh4SkinSelectorViewModel::GetOwningCh4PlayerController() const
{
	if (const UUserWidget* Widget = Cast<UUserWidget>(GetOuter()))
	{
		if (ACh4_multiGamePlayerController* PC = Cast<ACh4_multiGamePlayerController>(Widget->GetOwningPlayer()))
		{
			return PC;
		}
	}
	if (UWorld* World = GetWorld())
	{
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

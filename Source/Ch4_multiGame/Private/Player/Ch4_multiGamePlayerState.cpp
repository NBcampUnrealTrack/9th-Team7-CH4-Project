#include "Player/Ch4_multiGamePlayerState.h"

#include "Ch4_multiGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Player/Ch4_multiGameGameInstance.h"
#include "Player/Ch4_PlayerCharacter.h"

void ACh4_multiGamePlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);
	if (auto* NewState = Cast<ACh4_multiGamePlayerState>(NewPlayerState))
	{
		// UE swaps the Lobby/Gameplay controller and PlayerState during seamless travel.
		// Transfer the server's existing selection; Ready is deliberately not copied.
		NewState->CharacterType = CharacterType;
	}
}

void ACh4_multiGamePlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACh4_multiGamePlayerState, CharacterType);
}

bool ACh4_multiGamePlayerState::SetCharacterTypeFromServer(
	const ECh4CharacterType NewCharacterType)
{
	if (!HasAuthority() || !Ch4Character::IsValidType(NewCharacterType))
	{
		return false;
	}

	if (CharacterType == NewCharacterType)
	{
		CacheCharacterTypeForOwningLocalPlayer();
		ApplyCharacterTypeToPawn();
		return true;
	}

	CharacterType = NewCharacterType;
	CacheCharacterTypeForOwningLocalPlayer();
	ApplyCharacterTypeToPawn();
	OnCharacterTypeChanged.Broadcast(CharacterType);
	ForceNetUpdate();

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[CharacterSelection] Server accepted %s for %s"),
		*UEnum::GetValueAsString(CharacterType),
		*GetPlayerName());
	return true;
}

void ACh4_multiGamePlayerState::OnRep_CharacterType()
{
	ApplyCharacterTypeToPawn();
	CacheCharacterTypeForOwningLocalPlayer();
	OnCharacterTypeChanged.Broadcast(CharacterType);

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[CharacterSelection] Replicated %s for %s"),
		*UEnum::GetValueAsString(CharacterType),
		*GetPlayerName());
}

void ACh4_multiGamePlayerState::ApplyCharacterTypeToPawn() const
{
	if (!Ch4Character::IsValidType(CharacterType))
	{
		return;
	}

	if (ACh4_PlayerCharacter* PlayerCharacter = Cast<ACh4_PlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->ApplyCharacterType(CharacterType);
	}
}

void ACh4_multiGamePlayerState::CacheCharacterTypeForOwningLocalPlayer() const
{
	if (!Ch4Character::IsValidType(CharacterType) || !GetWorld())
	{
		return;
	}

	for (FConstPlayerControllerIterator ControllerIt = GetWorld()->GetPlayerControllerIterator();
		ControllerIt;
		++ControllerIt)
	{
		APlayerController* PlayerController = ControllerIt->Get();
		if (IsValid(PlayerController)
			&& PlayerController->IsLocalController()
			&& PlayerController->PlayerState == this)
		{
			if (UCh4_multiGameGameInstance* GameInstance =
				GetWorld()->GetGameInstance<UCh4_multiGameGameInstance>())
			{
				if (GameInstance->CacheAuthoritativeCharacterType(CharacterType))
				{
					UE_LOG(LogCh4_multiGame, Log,
						TEXT("[CharacterSelection] Cached server-confirmed local selection for travel: %s"),
						*UEnum::GetValueAsString(CharacterType));
				}
				else
				{
					UE_LOG(LogCh4_multiGame, Verbose,
						TEXT("[CharacterSelection] Ignored stale server selection while a newer local request is pending: %s"),
						*UEnum::GetValueAsString(CharacterType));
				}
			}
			return;
		}
	}
}

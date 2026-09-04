#if WITH_DEV_AUTOMATION_TESTS

#include "Player/Ch4_multiGameGameInstance.h"
#include "Player/Ch4_multiGamePlayerState.h"

#include "Camera/CameraComponent.h"
#include "Ch4_multiGamePlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Lobby/Ch4_multiGameLobbyGameMode.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Player/Ch4_PlayerCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCh4CharacterSelectionContractTest,
	"Ch4_multiGame.Player.CharacterSelectionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCh4CharacterSelectionContractTest::RunTest(const FString& Parameters)
{
	FString ConfiguredGameInstanceClass;
	GConfig->GetString(
		TEXT("/Script/EngineSettings.GameMapsSettings"),
		TEXT("GameInstanceClass"),
		ConfiguredGameInstanceClass,
		GEngineIni);
	TestEqual(TEXT("Project config uses the non-seamless travel character cache GameInstance"),
		ConfiguredGameInstanceClass,
		FString(TEXT("/Script/Ch4_multiGame.Ch4_multiGameGameInstance")));

	UClass* GameplayGameModeClass = StaticLoadClass(
		AGameModeBase::StaticClass(),
		nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
	TestNotNull(TEXT("Gameplay GameMode Blueprint resolves"), GameplayGameModeClass);
	if (GameplayGameModeClass)
	{
		TestFalse(TEXT("Gameplay GameMode cannot run the lobby-only initial allocator"),
			GameplayGameModeClass->IsChildOf(
				ACh4_multiGameLobbyGameMode::StaticClass()));
		const AGameModeBase* GameplayGameModeDefaults =
			GameplayGameModeClass->GetDefaultObject<AGameModeBase>();
		TestTrue(TEXT("Gameplay uses the shared replicated PlayerState"),
			GameplayGameModeDefaults->PlayerStateClass
				&& GameplayGameModeDefaults->PlayerStateClass->IsChildOf(
					ACh4_multiGamePlayerState::StaticClass()));
		TestTrue(TEXT("Gameplay controller inherits the character request path"),
			GameplayGameModeDefaults->PlayerControllerClass
				&& GameplayGameModeDefaults->PlayerControllerClass->IsChildOf(
					ACh4_multiGamePlayerController::StaticClass()));
		TestTrue(TEXT("Gameplay pawn inherits the common player controls"),
			GameplayGameModeDefaults->DefaultPawnClass
				&& GameplayGameModeDefaults->DefaultPawnClass->IsChildOf(
					ACh4_PlayerCharacter::StaticClass()));
	}
	TestFalse(TEXT("Invalid is rejected as a selectable character"),
		Ch4Character::IsValidType(ECh4CharacterType::Invalid));
	TestEqual(TEXT("Out-of-range indexes map to Invalid"),
		Ch4Character::FromIndex(4), ECh4CharacterType::Invalid);

	UCh4_multiGameGameInstance* GameInstance = NewObject<UCh4_multiGameGameInstance>();
	TestNotNull(TEXT("Character selection GameInstance can be constructed"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}

	TestEqual(TEXT("The appearance catalog contains four entries"),
		GameInstance->GetCharacterClassCount(), 4);
	for (int32 CharacterIndex = 0; CharacterIndex < 4; ++CharacterIndex)
	{
		const ECh4CharacterType CharacterType = Ch4Character::FromIndex(CharacterIndex);
		const TSubclassOf<ACh4_PlayerCharacter> CharacterClass =
			GameInstance->LoadCharacterClass(CharacterType);
		TestNotNull(
			FString::Printf(TEXT("Character class %d resolves"), CharacterIndex),
			CharacterClass.Get());
		if (!CharacterClass)
		{
			continue;
		}

		ACh4_PlayerCharacter* CharacterDefaults =
			CharacterClass->GetDefaultObject<ACh4_PlayerCharacter>();
		TestNotNull(TEXT("Character defaults are available"), CharacterDefaults);
		if (!CharacterDefaults)
		{
			continue;
		}

		TestNotNull(TEXT("Appearance has a skeletal mesh"),
			CharacterDefaults->GetMesh()->GetSkeletalMeshAsset());
		TestNotNull(TEXT("Appearance has an animation Blueprint"),
			CharacterDefaults->GetMesh()->GetAnimClass());
		TestNotNull(TEXT("Common spring arm exists"), CharacterDefaults->SpringArmComponent);
		TestNotNull(TEXT("Common camera exists"), CharacterDefaults->CameraComponent);
		TestNotNull(TEXT("Common player mapping context exists"), CharacterDefaults->InputMappingContext);
		TestNotNull(TEXT("Common move action exists"), CharacterDefaults->MoveAction);
		TestNotNull(TEXT("Common look action exists"), CharacterDefaults->LookAction);
		TestNotNull(TEXT("Common jump action exists"), CharacterDefaults->JumpAction);
		TestFalse(TEXT("Appearance does not rotate directly with controller yaw"),
			CharacterDefaults->bUseControllerRotationYaw);
		TestTrue(TEXT("Appearance uses movement-oriented rotation"),
			CharacterDefaults->GetCharacterMovement()->bOrientRotationToMovement);
	}

	TestTrue(TEXT("An initial server assignment is cached"),
		GameInstance->CacheAuthoritativeCharacterType(ECh4CharacterType::Cat));
	TestTrue(TEXT("A newer local final choice is accepted"),
		GameInstance->StoreLocalCharacterRequest(ECh4CharacterType::Otter));
	TestTrue(TEXT("The local final choice remains pending until server confirmation"),
		GameInstance->HasPendingCharacterRequest());
	TestFalse(TEXT("A stale initial replication cannot overwrite the newer local choice"),
		GameInstance->CacheAuthoritativeCharacterType(ECh4CharacterType::Cat));
	ECh4CharacterType RestoredCharacterType = ECh4CharacterType::Invalid;
	TestTrue(TEXT("The local final choice survives a read-back"),
		GameInstance->TryGetLocalCharacterType(RestoredCharacterType));
	TestEqual(TEXT("The restored final choice is unchanged"),
		RestoredCharacterType, ECh4CharacterType::Otter);
	TestTrue(TEXT("The matching server confirmation is accepted"),
		GameInstance->CacheAuthoritativeCharacterType(ECh4CharacterType::Otter));
	TestFalse(TEXT("The matching server confirmation clears the pending request"),
		GameInstance->HasPendingCharacterRequest());
	TestFalse(TEXT("Invalid cannot overwrite the local final choice"),
		GameInstance->StoreLocalCharacterRequest(ECh4CharacterType::Invalid));

	const ECh4CharacterType DuplicateSelections[] = {
		ECh4CharacterType::Dog,
		ECh4CharacterType::Dog,
		ECh4CharacterType::Otter,
		ECh4CharacterType::Gorilla};
	for (int32 PlayerIndex = 0; PlayerIndex < UE_ARRAY_COUNT(DuplicateSelections); ++PlayerIndex)
	{
		UCh4_multiGameGameInstance* PlayerGameInstance =
			NewObject<UCh4_multiGameGameInstance>();
		const ECh4CharacterType SelectedType = DuplicateSelections[PlayerIndex];
		TestTrue(
			FString::Printf(TEXT("Player %d can request a duplicate-capable selection"), PlayerIndex + 1),
			PlayerGameInstance->StoreLocalCharacterRequest(SelectedType));
		TestTrue(
			FString::Printf(TEXT("Player %d selection receives server confirmation"), PlayerIndex + 1),
			PlayerGameInstance->CacheAuthoritativeCharacterType(SelectedType));

		ECh4CharacterType PersistedType = ECh4CharacterType::Invalid;
		TestTrue(
			FString::Printf(TEXT("Player %d selection has a persistent representation"), PlayerIndex + 1),
			PlayerGameInstance->TryGetLocalCharacterType(PersistedType));
		TestEqual(
			FString::Printf(TEXT("Player %d preserves the exact final selection"), PlayerIndex + 1),
			PersistedType,
			SelectedType);
	}

	return true;
}

#endif

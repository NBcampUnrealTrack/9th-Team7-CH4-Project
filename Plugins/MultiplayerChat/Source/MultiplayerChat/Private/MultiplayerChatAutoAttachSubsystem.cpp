#include "MultiplayerChatAutoAttachSubsystem.h"

#include "MultiplayerChatComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

bool UMultiplayerChatAutoAttachSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);

	if (World == nullptr || !World->IsGameWorld())
	{
		return false;
	}

	// 네트워크 모드는 월드 시작 시점에 최종적으로 검사합니다.
	return true;
}

void UMultiplayerChatAutoAttachSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 메인 메뉴 같은 독립 실행 월드에는 채팅을 자동 장착하지 않습니다.
	if (!InWorld.IsGameWorld() || InWorld.GetNetMode() == NM_Standalone)
	{
		return;
	}

	// 이후 생성되는 PlayerController를 감지하도록 Actor 생성 이벤트를 등록합니다.
	if (!ActorSpawnedDelegateHandle.IsValid())
	{
		ActorSpawnedDelegateHandle = InWorld.AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(
				this,
				&UMultiplayerChatAutoAttachSubsystem::HandleActorSpawned
			)
		);
	}

	// Subsystem이 시작되기 전에 생성된 PlayerController도 처리합니다.
	for (
		FConstPlayerControllerIterator Iterator =
			InWorld.GetPlayerControllerIterator();
		Iterator;
		++Iterator
	)
	{
		AttachChatComponent(Iterator->Get());
	}
}

void UMultiplayerChatAutoAttachSubsystem::HandleActorSpawned(AActor* SpawnedActor)
{
	APlayerController* PlayerController = Cast<APlayerController>(SpawnedActor);

	if (PlayerController == nullptr)
	{
		return;
	}

	AttachChatComponent(PlayerController);
}

void UMultiplayerChatAutoAttachSubsystem::AttachChatComponent(APlayerController* PlayerController)
{
	if (PlayerController == nullptr)
	{
		return;
	}

	UMultiplayerChatComponent* ChatComponent = PlayerController->FindComponentByClass<UMultiplayerChatComponent>();

	// 기존 컴포넌트가 있으면 중복 생성하지 않고 로컬 UI만 복구합니다.
	if (ChatComponent != nullptr)
	{
		if (PlayerController->IsLocalController())
		{
			ChatComponent->EnsureLocalChatInitialized();
		}

		return;
	}

	// 클라이언트는 컴포넌트를 생성하지 않고 서버의 복제를 기다립니다.
	if (!PlayerController->HasAuthority())
	{
		return;
	}

	// 서버가 PlayerController에 복제 가능한 채팅 컴포넌트를 생성합니다.
	ChatComponent = NewObject<UMultiplayerChatComponent>(
		PlayerController,
		UMultiplayerChatComponent::StaticClass(),
		TEXT("MultiplayerChatComponent")
	);

	if (ChatComponent == nullptr)
	{
		return;
	}

	PlayerController->AddInstanceComponent(ChatComponent);
	ChatComponent->SetIsReplicated(true);
	ChatComponent->RegisterComponent();

	// Listen Server의 로컬 플레이어 UI도 즉시 준비합니다.
	if (PlayerController->IsLocalController())
	{
		ChatComponent->EnsureLocalChatInitialized();
	}

	// 새 컴포넌트가 소유 클라이언트에 빠르게 복제되도록 갱신합니다.
	PlayerController->ForceNetUpdate();
}

// 월드 종료 시 Actor 생성 이벤트를 안전하게 해제
void UMultiplayerChatAutoAttachSubsystem::Deinitialize()
{
	if (ActorSpawnedDelegateHandle.IsValid())
	{
		UWorld* World = GetWorld();

		if (World != nullptr)
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
		}

		ActorSpawnedDelegateHandle.Reset();
	}

	Super::Deinitialize();
}
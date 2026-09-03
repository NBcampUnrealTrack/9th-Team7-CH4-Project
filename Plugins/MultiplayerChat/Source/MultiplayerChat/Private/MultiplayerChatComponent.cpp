#include "MultiplayerChatComponent.h"

#include "MultiplayerChatIdentityProvider.h"
#include "Components/InputComponent.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogMultiplayerChat, Log, All);

UMultiplayerChatComponent::UMultiplayerChatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	// 플러그인에 포함된 기본 채팅 위젯 클래스를 설정
	static ConstructorHelpers::FClassFinder<UUserWidget>DefaultChatWidgetClass(TEXT("/MultiplayerChat/UI/WBP_MultiplayerChat"));

	if (DefaultChatWidgetClass.Succeeded())
	{
		ChatWidgetClass = DefaultChatWidgetClass.Class;
	}

	ReservedNicknames =
	{
		TEXT("System"),
		TEXT("Server"),
		TEXT("Admin")
	};

	// Server RPC와 Client RPC를 사용할 수 있도록 컴포넌트 복제를 활성화
	SetIsReplicatedByDefault(true);
}

void UMultiplayerChatComponent::BeginPlay()
{
	Super::BeginPlay();

	EnsureLocalChatInitialized();
}

void UMultiplayerChatComponent::EnsureLocalChatInitialized()
{
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	// 로컬 PlayerController에서만 UI와 입력을 준비합니다.
	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	if (bAutoCreateChatWidget && ChatWidgetClass != nullptr)
	{
		// 레벨 전환으로 기존 위젯이 제거됐다면 다시 생성합니다.
		if (!IsValid(ChatWidget))
		{
			ChatWidget = CreateWidget<UUserWidget>(OwningPlayerController,ChatWidgetClass);
		}

		// 위젯이 존재하지만 화면에서 제거된 경우 다시 표시합니다.
		if (IsValid(ChatWidget) && !ChatWidget->IsInViewport())
		{
			ChatWidget->AddToPlayerScreen();
		}
	}

	SetupLocalChatInput();
}

void UMultiplayerChatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsChatInputActive)
	{
		DeactivateChatInput();
	}

	TeardownLocalChatInput();

	// 생성된 채팅 UI가 있으면 화면에서 제거
	if (ChatWidget != nullptr)
	{
		ChatWidget->RemoveFromParent();
		ChatWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UMultiplayerChatComponent::SubmitChatInput(const FString& InputText)
{
	// 입력의 앞뒤 공백을 한 번만 정리합니다.
	FString TrimmedInput = InputText;
	TrimmedInput.TrimStartAndEndInline();

	if (TrimmedInput.IsEmpty())
	{
		return;
	}

	// 명령어로 처리된 입력은 전체 채팅으로 전송하지 않습니다.
	if (TryHandleChatCommand(TrimmedInput))
	{
		return;
	}

	if (ActiveChatChannel == EMultiplayerChatChannel::Party)
	{
		SendPartyMessage(TrimmedInput);
		return;
	}

	SendGlobalMessage(TrimmedInput);
}

EMultiplayerChatChannel UMultiplayerChatComponent::GetActiveChatChannel() const
{
	return ActiveChatChannel;
}

void UMultiplayerChatComponent::SetActiveChatChannel(EMultiplayerChatChannel NewChannel)
{
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	// 소유 로컬 플레이어의 발신 채널만 변경할 수 있습니다.
	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	// 현재 입력 모드에서는 전체와 파티 채널만 기본 발신 채널로 사용합니다.
	if (
		NewChannel != EMultiplayerChatChannel::Global &&
		NewChannel != EMultiplayerChatChannel::Party
	)
	{
		return;
	}

	if (ActiveChatChannel == NewChannel)
	{
		return;
	}

	ActiveChatChannel = NewChannel;
	OnActiveChannelChanged.Broadcast(ActiveChatChannel);
}

bool UMultiplayerChatComponent::TryHandleChatCommand(const FString& InputText)
{
	// 슬래시로 시작하지 않으면 일반 채팅입니다.
	if (!InputText.StartsWith(TEXT("/")))
	{
		return false;
	}

	// 첫 슬래시를 제거하고 명령어 이름과 나머지 인수를 분리합니다.
	FString CommandLine = InputText.Mid(1);
	CommandLine.TrimStartAndEndInline();

	FString CommandName;
	FString Arguments;

	if (!CommandLine.Split(TEXT(" "), &CommandName, &Arguments))
	{
		CommandName = CommandLine;
	}

	CommandName.TrimStartAndEndInline();
	Arguments.TrimStartAndEndInline();

	if (CommandName.Equals(TEXT("help"), ESearchCase::IgnoreCase))
	{
		DisplayLocalSystemMessage(
			TEXT(
				"Chat commands:\n"
				"/g [message] - Switch to global chat or send a global message.\n"
				"/p [message] - Switch to party chat or send a party message.\n"
				"/w <nickname> <message> - Send a whisper.\n"
				"/party create - Create a party.\n"
				"/party join <nickname> - Join a player's party.\n"
				"/party leave - Leave the current party.\n"
				"/nick <nickname> - Change your nickname.\n"
				"/help - Show this help."
			)
		);

		return true;
	}

	if (CommandName.Equals(TEXT("nick"), ESearchCase::IgnoreCase))
	{
		RequestNicknameChange(Arguments);
		return true;
	}

	if (CommandName.Equals(TEXT("w"), ESearchCase::IgnoreCase))
	{
		FString TargetNickname;
		FString WhisperMessage;

		if (!Arguments.Split(TEXT(" "), &TargetNickname, &WhisperMessage))
		{
			TargetNickname = Arguments;
		}

		TargetNickname.TrimStartAndEndInline();
		WhisperMessage.TrimStartAndEndInline();

		SendWhisperMessage(TargetNickname, WhisperMessage);
		return true;
	}

	if (CommandName.Equals(TEXT("p"), ESearchCase::IgnoreCase))
	{
		SetActiveChatChannel(EMultiplayerChatChannel::Party);

		if (!Arguments.IsEmpty())
		{
			SendPartyMessage(Arguments);
		}

		return true;
	}

	if (CommandName.Equals(TEXT("g"), ESearchCase::IgnoreCase))
	{
		SetActiveChatChannel(EMultiplayerChatChannel::Global);

		if (!Arguments.IsEmpty())
		{
			SendGlobalMessage(Arguments);
		}

		return true;
	}

	if (CommandName.Equals(TEXT("party"), ESearchCase::IgnoreCase))
	{
		FString PartyCommand;
		FString PartyArguments;

		if (!Arguments.Split(TEXT(" "), &PartyCommand, &PartyArguments))
		{
			PartyCommand = Arguments;
		}

		PartyCommand.TrimStartAndEndInline();
		PartyArguments.TrimStartAndEndInline();

		if (
			PartyCommand.Equals(TEXT("create"), ESearchCase::IgnoreCase) &&
			PartyArguments.IsEmpty()
		)
		{
			CreateParty();
			return true;
		}

		if (PartyCommand.Equals(TEXT("join"), ESearchCase::IgnoreCase))
		{
			JoinParty(PartyArguments);
			return true;
		}

		if (
			PartyCommand.Equals(TEXT("leave"), ESearchCase::IgnoreCase) &&
			PartyArguments.IsEmpty()
		)
		{
			LeaveParty();
			return true;
		}

		// 비어 있거나 잘못된 파티 명령은 서버에서 사용법 메시지로 처리합니다.
		JoinParty(TEXT(""));
		return true;
	}

	// 알 수 없는 슬래시 명령어도 전체 채팅에는 노출하지 않습니다.
	UE_LOG(
		LogMultiplayerChat,
		Warning,
		TEXT("Unknown chat command: /%s"),
		*CommandName
	);

	DisplayLocalSystemMessage(TEXT("Unknown command. Type /help to see available commands."));
	return true;
}

void UMultiplayerChatComponent::DisplayLocalSystemMessage(const FString& MessageText)
{
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	FString SanitizedMessage = MessageText;
	SanitizedMessage.TrimStartAndEndInline();

	if (SanitizedMessage.IsEmpty())
	{
		return;
	}

	FMultiplayerChatMessage SystemMessage;
	SystemMessage.MessageId = FGuid::NewGuid();
	SystemMessage.SenderId = TEXT("");
	SystemMessage.SenderName = TEXT("System");
	SystemMessage.Channel = EMultiplayerChatChannel::System;
	SystemMessage.ChannelId = TEXT("");
	SystemMessage.ChannelDisplayName = TEXT("System");
	SystemMessage.MessageText = SanitizedMessage;
	SystemMessage.ServerTimestamp = FDateTime::UtcNow();

	OnMessageReceived.Broadcast(SystemMessage);
}

void UMultiplayerChatComponent::SendGlobalMessage(const FString& MessageText)
{
	// 사용자 입력의 앞뒤 공백을 제거
	FString TrimmedMessage = MessageText;
	TrimmedMessage.TrimStartAndEndInline();

	// 비어 있는 메시지는 서버로 전송하지 않습니다.
	if (TrimmedMessage.IsEmpty())
	{
		return;
	}

	// 소유 클라이언트에서 서버 RPC를 호출
	ServerSendGlobalMessage(TrimmedMessage);
}

void UMultiplayerChatComponent::SendWhisperMessage(const FString& TargetNickname, const FString& MessageText)
{
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	// 소유 로컬 플레이어만 귓속말을 요청할 수 있습니다.
	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	FString SanitizedTargetNickname = TargetNickname;
	SanitizedTargetNickname.TrimStartAndEndInline();

	FString SanitizedMessage = MessageText;
	SanitizedMessage.TrimStartAndEndInline();

	ServerSendWhisperMessage(SanitizedTargetNickname, SanitizedMessage);
}

void UMultiplayerChatComponent::CreateParty()
{
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	// 소유 로컬 플레이어만 파티 생성을 요청할 수 있습니다.
	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	ServerCreateParty();
}

void UMultiplayerChatComponent::JoinParty(const FString& TargetNickname)
{
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	// 소유 로컬 플레이어만 파티 참가를 요청할 수 있습니다.
	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	FString SanitizedTargetNickname = TargetNickname;
	SanitizedTargetNickname.TrimStartAndEndInline();

	ServerJoinParty(SanitizedTargetNickname);
}

void UMultiplayerChatComponent::LeaveParty()
{
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	// 소유 로컬 플레이어만 파티 탈퇴를 요청할 수 있습니다.
	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	ServerLeaveParty();

	SetActiveChatChannel(EMultiplayerChatChannel::Global);
}

void UMultiplayerChatComponent::SendPartyMessage(const FString& MessageText)
{
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	// 소유 로컬 플레이어만 파티 메시지를 요청할 수 있습니다.
	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	FString SanitizedMessage = MessageText;
	SanitizedMessage.TrimStartAndEndInline();

	ServerSendPartyMessage(SanitizedMessage);
}

void UMultiplayerChatComponent::ActivateChatInput()
{
	if (bIsChatInputActive)
	{
		return;
	}

	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	bIsChatInputActive = true;

	if (ChatInputComponent != nullptr && ChatInputComponent->KeyBindings.IsValidIndex(CancelInputBindingIndex))
	{
		// 채팅 중에는 Escape가 게임의 일시정지 등에 전달되지 않게 합니다.
		ChatInputComponent->KeyBindings[CancelInputBindingIndex].bConsumeInput = true;
	}

	FInputModeGameAndUI InputMode;
	OwningPlayerController->SetInputMode(InputMode);

	// Blueprint UI가 실제 입력 위젯에 포커스를 줄 수 있도록 알립니다.
	OnChatInputStateChanged.Broadcast(true);
}

void UMultiplayerChatComponent::DeactivateChatInput()
{
	if (!bIsChatInputActive)
	{
		return;
	}

	// 포커스 변경 과정에서 재호출되어도 안전하도록 먼저 상태를 변경합니다.
	bIsChatInputActive = false;

	if (ChatInputComponent != nullptr && ChatInputComponent->KeyBindings.IsValidIndex(CancelInputBindingIndex))
	{
		// 채팅이 끝나면 Escape를 다시 다른 게임 입력에 전달합니다.
		ChatInputComponent->KeyBindings[CancelInputBindingIndex].bConsumeInput = false;
	}

	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	if (OwningPlayerController != nullptr && OwningPlayerController->IsLocalController())
	{
		FInputModeGameOnly InputMode;
		OwningPlayerController->SetInputMode(InputMode);
	}

	OnChatInputStateChanged.Broadcast(false);
}

void UMultiplayerChatComponent::RequestNicknameChange(const FString& NewNickname)
{
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	// 소유 로컬 플레이어만 닉네임 변경을 요청할 수 있습니다.
	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	FString SanitizedNickname = NewNickname;
	SanitizedNickname.TrimStartAndEndInline();

	ServerRequestNicknameChange(SanitizedNickname);
}

void UMultiplayerChatComponent::ServerSendGlobalMessage_Implementation(const FString& MessageText)
{
	constexpr int32 MaximumMessageLength = 256;
	constexpr double MinimumMessageInterval = 0.5;

	UWorld* World = GetWorld();
	APlayerController* SenderController = Cast<APlayerController>(GetOwner());

	// 유효한 월드와 PlayerController가 없으면 메시지를 처리하지 않습니다.
	if (World == nullptr || SenderController == nullptr)
	{
		return;
	}

	// 서버에서도 메시지의 앞뒤 공백을 다시 제거
	FString SanitizedMessage = MessageText;
	SanitizedMessage.TrimStartAndEndInline();

	// 비어 있거나 최대 길이를 초과한 메시지는 거부
	if (SanitizedMessage.IsEmpty() || SanitizedMessage.Len() > MaximumMessageLength)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();

	// 플레이어가 너무 빠르게 연속 전송하면 메시지를 거부
	if (LastAcceptedMessageTime >= 0.0 && CurrentTime - LastAcceptedMessageTime < MinimumMessageInterval)
	{
		return;
	}

	APlayerState* SenderPlayerState = SenderController->PlayerState;

	// 서버에서 발신자 정보를 확인할 수 없으면 전송하지 않습니다.
	if (SenderPlayerState == nullptr)
	{
		return;
	}

	LastAcceptedMessageTime = CurrentTime;

	// 서버가 신뢰할 수 있는 정보로 최종 메시지를 생성
	FMultiplayerChatMessage ChatMessage;
	ChatMessage.MessageId = FGuid::NewGuid();
	ChatMessage.SenderId = ResolvePlayerId(SenderPlayerState);
	ChatMessage.SenderName = ResolveDisplayName(SenderPlayerState);
	ChatMessage.Channel = EMultiplayerChatChannel::Global;
	ChatMessage.ChannelId = TEXT("");
	ChatMessage.MessageText = SanitizedMessage;
	ChatMessage.ServerTimestamp = FDateTime::UtcNow();

	// 서버에 접속한 모든 플레이어의 채팅 컴포넌트를 찾습니다.
	for (FConstPlayerControllerIterator Iterator = 
		World->GetPlayerControllerIterator();
		 Iterator;
		 ++Iterator)
	{
		APlayerController* TargetController = Iterator->Get();

		if (TargetController == nullptr)
		{
			continue;
		}

		UMultiplayerChatComponent* TargetChatComponent = TargetController->FindComponentByClass<UMultiplayerChatComponent>();

		if (TargetChatComponent == nullptr)
		{
			continue;
		}

		// 각 PlayerController의 소유 클라이언트로 메시지를 전송
		TargetChatComponent->ClientReceiveMessage(ChatMessage);
	}
}

void UMultiplayerChatComponent::SendSystemMessageToOwner(const FString& MessageText)
{
	AActor* OwnerActor = GetOwner();

	// 시스템 메시지는 서버에서만 생성합니다.
	if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
	{
		return;
	}

	FString SanitizedMessage = MessageText;
	SanitizedMessage.TrimStartAndEndInline();

	if (SanitizedMessage.IsEmpty())
	{
		return;
	}

	FMultiplayerChatMessage SystemMessage;
	SystemMessage.MessageId = FGuid::NewGuid();
	SystemMessage.SenderId = TEXT("");
	SystemMessage.SenderName = TEXT("System");
	SystemMessage.Channel = EMultiplayerChatChannel::System;
	SystemMessage.ChannelId = TEXT("");
	SystemMessage.ChannelDisplayName = TEXT("System");
	SystemMessage.MessageText = SanitizedMessage;
	SystemMessage.ServerTimestamp = FDateTime::UtcNow();

	ClientReceiveMessage(SystemMessage);
}

void UMultiplayerChatComponent::ServerSendWhisperMessage_Implementation(
	const FString& TargetNickname,
	const FString& MessageText
)
{
	constexpr int32 MaximumMessageLength = 256;
	constexpr double MinimumMessageInterval = 0.5;

	UWorld* World = GetWorld();
	APlayerController* SenderController = Cast<APlayerController>(GetOwner());
	APlayerState* SenderPlayerState = SenderController != nullptr ? SenderController->PlayerState : nullptr;

	if (World == nullptr || SenderController == nullptr || SenderPlayerState == nullptr)
	{
		return;
	}

	FString SanitizedTargetNickname = TargetNickname;
	SanitizedTargetNickname.TrimStartAndEndInline();

	FString SanitizedMessage = MessageText;
	SanitizedMessage.TrimStartAndEndInline();

	const double CurrentTime = World->GetTimeSeconds();

	// 전체 채팅과 동일한 전송 간격 제한을 공유
	if (
		LastAcceptedMessageTime >= 0.0 && CurrentTime - LastAcceptedMessageTime < MinimumMessageInterval)
	{
		return;
	}

	// 잘못된 대상 검색 요청도 반복할 수 없도록 요청 시각을 기록
	LastAcceptedMessageTime = CurrentTime;

	if (SanitizedTargetNickname.IsEmpty() || SanitizedMessage.IsEmpty())
	{

		SendSystemMessageToOwner(TEXT("Usage: /w <nickname> <message>"));
		return;
	}

	if (SanitizedMessage.Len() > MaximumMessageLength)
	{

		SendSystemMessageToOwner(TEXT("Whisper messages must be 256 characters or fewer."));
		return;
	}

	if (SanitizedTargetNickname.Len() > MaximumNicknameLength)
	{
		SendSystemMessageToOwner(TEXT("Target player not found."));
		return;
	}

	APlayerController* RecipientController = nullptr;
	APlayerState* RecipientPlayerState = nullptr;
	FString RecipientDisplayName;
	int32 MatchingPlayerCount = 0;

	// 서버가 현재 접속한 플레이어의 표시 이름으로 수신자를 검색합니다.
	for (
		FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator();
		Iterator;
		++Iterator
	)
	{
		APlayerController* CandidateController = Iterator->Get();

		if (CandidateController == nullptr || CandidateController->PlayerState == nullptr)
		{
			continue;
		}

		const FString CandidateDisplayName = ResolveDisplayName(CandidateController->PlayerState);

		if (!CandidateDisplayName.Equals(
			SanitizedTargetNickname,
			ESearchCase::IgnoreCase
		))
		{
			continue;
		}

		++MatchingPlayerCount;
		RecipientController = CandidateController;
		RecipientPlayerState = CandidateController->PlayerState;
		RecipientDisplayName = CandidateDisplayName;
	}

	if (MatchingPlayerCount == 0)
	{
		SendSystemMessageToOwner(TEXT("Target player not found."));
		return;
	}

	// 외부 Identity Provider가 중복 이름을 제공하면 잘못된 대상에게 보내지 않습니다.
	if (MatchingPlayerCount > 1)
	{
		SendSystemMessageToOwner(
			TEXT("Multiple players have that nickname. Unable to send the whisper.")
		);
		return;
	}

	if (RecipientController == SenderController)
	{
		SendSystemMessageToOwner(TEXT("You cannot whisper to yourself."));
		return;
	}

	UMultiplayerChatComponent* RecipientChatComponent = RecipientController->FindComponentByClass<UMultiplayerChatComponent>();

	if (RecipientChatComponent == nullptr || RecipientPlayerState == nullptr)
	{
		SendSystemMessageToOwner(TEXT("The target player cannot use chat."));
		return;
	}

	// 서버가 검증한 발신자와 수신자 정보로 귓속말을 생성합니다.
	FMultiplayerChatMessage WhisperMessage;
	WhisperMessage.MessageId = FGuid::NewGuid();
	WhisperMessage.SenderId = ResolvePlayerId(SenderPlayerState);
	WhisperMessage.SenderName = ResolveDisplayName(SenderPlayerState);
	WhisperMessage.Channel = EMultiplayerChatChannel::Whisper;
	WhisperMessage.ChannelId = ResolvePlayerId(RecipientPlayerState);
	WhisperMessage.ChannelDisplayName = RecipientDisplayName;
	WhisperMessage.MessageText = SanitizedMessage;
	WhisperMessage.ServerTimestamp = FDateTime::UtcNow();

	// 귓속말은 발신자와 수신자에게만 전달합니다.
	ClientReceiveMessage(WhisperMessage);
	RecipientChatComponent->ClientReceiveMessage(WhisperMessage);
}

void UMultiplayerChatComponent::ServerSendPartyMessage_Implementation(const FString& MessageText)
{
	constexpr int32 MaximumMessageLength = 256;
	constexpr double MinimumMessageInterval = 0.5;

	UWorld* World = GetWorld();
	APlayerController* SenderController = Cast<APlayerController>(GetOwner());
	APlayerState* SenderPlayerState = SenderController != nullptr ? SenderController->PlayerState : nullptr;

	if (
		World == nullptr ||
		SenderController == nullptr ||
		SenderPlayerState == nullptr
	)
	{
		return;
	}

	FString SanitizedMessage = MessageText;
	SanitizedMessage.TrimStartAndEndInline();

	const double CurrentTime = World->GetTimeSeconds();

	// 전체 채팅과 귓속말의 전송 간격 제한을 공유합니다.
	if (LastAcceptedMessageTime >= 0.0 && CurrentTime - LastAcceptedMessageTime < MinimumMessageInterval)
	{
		return;
	}

	// 잘못된 파티 메시지 반복도 제한하도록 요청 시각을 기록합니다.
	LastAcceptedMessageTime = CurrentTime;

	if (SanitizedMessage.IsEmpty())
	{
		SendSystemMessageToOwner(TEXT("Usage: /p <message>"));
		return;
	}

	if (SanitizedMessage.Len() > MaximumMessageLength)
	{
		SendSystemMessageToOwner(TEXT("Party messages must be 256 characters or fewer."));
		return;
	}

	if (CurrentPartyId.IsEmpty())
	{
		SendSystemMessageToOwner(
			TEXT("You must join a party before using party chat.")
		);
		return;
	}

	// 서버가 검증한 발신자와 파티 정보로 메시지를 생성합니다.
	FMultiplayerChatMessage PartyMessage;
	PartyMessage.MessageId = FGuid::NewGuid();
	PartyMessage.SenderId = ResolvePlayerId(SenderPlayerState);
	PartyMessage.SenderName = ResolveDisplayName(SenderPlayerState);
	PartyMessage.Channel = EMultiplayerChatChannel::Party;
	PartyMessage.ChannelId = CurrentPartyId;
	PartyMessage.ChannelDisplayName = TEXT("Party");
	PartyMessage.MessageText = SanitizedMessage;
	PartyMessage.ServerTimestamp = FDateTime::UtcNow();

	// 같은 서버 파티 식별자를 가진 플레이어에게만 메시지를 전달합니다.
	for (
		FConstPlayerControllerIterator Iterator =
			World->GetPlayerControllerIterator();
		Iterator;
		++Iterator
	)
	{
		APlayerController* TargetController = Iterator->Get();

		if (TargetController == nullptr)
		{
			continue;
		}

		UMultiplayerChatComponent* TargetChatComponent = TargetController->FindComponentByClass<UMultiplayerChatComponent>();

		if (TargetChatComponent == nullptr || TargetChatComponent->CurrentPartyId != CurrentPartyId)
		{
			continue;
		}

		TargetChatComponent->ClientReceiveMessage(PartyMessage);
	}
}

void UMultiplayerChatComponent::ServerCreateParty_Implementation()
{
	constexpr double MinimumPartyCommandInterval = 0.5;

	UWorld* World = GetWorld();
	APlayerController* RequestingController = Cast<APlayerController>(GetOwner());

	if (
		World == nullptr ||
		RequestingController == nullptr ||
		RequestingController->PlayerState == nullptr
	)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();

	// 파티 명령을 지나치게 빠르게 반복할 수 없도록 제한
	if (
		LastPartyCommandTime >= 0.0 &&
		CurrentTime - LastPartyCommandTime < MinimumPartyCommandInterval
	)
	{
		SendSystemMessageToOwner(TEXT("Please try the party command again shortly."));
		return;
	}

	LastPartyCommandTime = CurrentTime;

	// 이미 파티에 속한 플레이어는 새 파티를 생성할 수 없습니다.
	if (!CurrentPartyId.IsEmpty())
	{
		SendSystemMessageToOwner(TEXT("You are already in a party."));
		return;
	}

	// 서버가 예측하기 어려운 고유 파티 식별자를 생성
	CurrentPartyId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

	SendSystemMessageToOwner(TEXT("Party created."));
}

void UMultiplayerChatComponent::ServerJoinParty_Implementation(const FString& TargetNickname)
{
	constexpr double MinimumPartyCommandInterval = 0.5;

	UWorld* World = GetWorld();
	APlayerController* RequestingController = Cast<APlayerController>(GetOwner());

	if (
		World == nullptr ||
		RequestingController == nullptr ||
		RequestingController->PlayerState == nullptr
	)
	{
		return;
	}

	FString SanitizedTargetNickname = TargetNickname;
	SanitizedTargetNickname.TrimStartAndEndInline();

	const double CurrentTime = World->GetTimeSeconds();

	// 파티 명령을 지나치게 빠르게 반복할 수 없도록 제한합니다.
	if (
		LastPartyCommandTime >= 0.0 && CurrentTime - LastPartyCommandTime < MinimumPartyCommandInterval)
	{
		SendSystemMessageToOwner(TEXT("Please try the party command again shortly."));
		return;
	}

	LastPartyCommandTime = CurrentTime;

	if (SanitizedTargetNickname.IsEmpty())
	{
		SendSystemMessageToOwner(
			TEXT("Usage: /party create | /party join <nickname> | /party leave")
		);
		return;
	}

	if (SanitizedTargetNickname.Len() > MaximumNicknameLength)
	{
		SendSystemMessageToOwner(TEXT("Target player not found."));
		return;
	}

	// 이미 파티에 속한 플레이어는 다른 파티에 참가할 수 없습니다.
	if (!CurrentPartyId.IsEmpty())
	{
		SendSystemMessageToOwner(TEXT("You are already in a party."));
		return;
	}

	APlayerController* TargetController = nullptr;
	FString TargetDisplayName;
	int32 MatchingPlayerCount = 0;

	// 서버가 현재 접속한 플레이어의 표시 이름으로 참가 대상을 검색합니다.
	for (
		FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator();
		Iterator;
		++Iterator
	)
	{
		APlayerController* CandidateController = Iterator->Get();

		if (CandidateController == nullptr || CandidateController->PlayerState == nullptr)
		{
			continue;
		}

		const FString CandidateDisplayName = ResolveDisplayName(
			CandidateController->PlayerState
		);

		if (!CandidateDisplayName.Equals(SanitizedTargetNickname, ESearchCase::IgnoreCase))
		{
			continue;
		}

		++MatchingPlayerCount;
		TargetController = CandidateController;
		TargetDisplayName = CandidateDisplayName;
	}

	if (MatchingPlayerCount == 0)
	{
		SendSystemMessageToOwner(TEXT("Target player not found."));
		return;
	}

	if (MatchingPlayerCount > 1)
	{
		SendSystemMessageToOwner(
			TEXT("Multiple players have that nickname. Unable to join the party.")
		);
		return;
	}

	if (TargetController == RequestingController)
	{
		SendSystemMessageToOwner(TEXT("You cannot target yourself when joining a party."));
		return;
	}

	UMultiplayerChatComponent* TargetChatComponent = TargetController->FindComponentByClass<UMultiplayerChatComponent>();

	if (TargetChatComponent == nullptr)
	{
		SendSystemMessageToOwner(TEXT("The target player cannot use party features."));
		return;
	}

	if (TargetChatComponent->CurrentPartyId.IsEmpty())
	{
		SendSystemMessageToOwner(TEXT("The target player is not in a party."));
		return;
	}

	// 서버가 대상 플레이어의 검증된 파티 식별자를 참가자에게 복사합니다.
	CurrentPartyId = TargetChatComponent->CurrentPartyId;

	SendSystemMessageToOwner(
		FString::Printf(
			TEXT("You joined %s's party."),
			*TargetDisplayName
		)
	);
}

void UMultiplayerChatComponent::ServerLeaveParty_Implementation()
{
	constexpr double MinimumPartyCommandInterval = 0.5;

	UWorld* World = GetWorld();
	APlayerController* RequestingController = Cast<APlayerController>(GetOwner());

	if (
		World == nullptr ||
		RequestingController == nullptr ||
		RequestingController->PlayerState == nullptr
	)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();

	// 파티 명령을 지나치게 빠르게 반복할 수 없도록 제한합니다.
	if (LastPartyCommandTime >= 0.0 && CurrentTime - LastPartyCommandTime < MinimumPartyCommandInterval)
	{
		SendSystemMessageToOwner(TEXT("Please try the party command again shortly."));
		return;
	}

	LastPartyCommandTime = CurrentTime;

	// 파티에 참가하지 않은 플레이어는 탈퇴할 수 없습니다.
	if (CurrentPartyId.IsEmpty())
	{
		SendSystemMessageToOwner(TEXT("You are not currently in a party."));
		return;
	}

	// 서버가 관리하는 파티 소속 정보를 제거합니다.
	CurrentPartyId.Empty();

	SendSystemMessageToOwner(TEXT("You left the party."));
}

void UMultiplayerChatComponent::ClientReceiveMessage_Implementation(const FMultiplayerChatMessage& Message)
{
	// UI가 없는 상태에서도 수신 결과를 확인할 수 있도록 로그를 출력
	UE_LOG(
		LogMultiplayerChat,
		Log,
		TEXT("[%s] %s: %s"),
		*UEnum::GetValueAsString(Message.Channel),
		*Message.SenderName,
		*Message.MessageText
	);

	// Blueprint와 UMG가 메시지를 표시할 수 있도록 이벤트를 발생
	OnMessageReceived.Broadcast(Message);
}

void UMultiplayerChatComponent::SetupLocalChatInput()
{
	if (!bAutoBindChatInput || ChatInputComponent != nullptr)
	{
		return;
	}

	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	if (OwningPlayerController == nullptr || !OwningPlayerController->IsLocalController())
	{
		return;
	}

	ChatInputComponent = NewObject<UInputComponent>(OwningPlayerController,TEXT("MultiplayerChatInputComponent"));

	if (ChatInputComponent == nullptr)
	{
		return;
	}

	ChatInputComponent->RegisterComponent();
	ChatInputComponent->Priority = ChatInputPriority;
	ChatInputComponent->bBlockInput = false;

	FInputKeyBinding& ActivateBinding =
		ChatInputComponent->BindKey(
			ActivateChatKey,
			IE_Pressed,
			this,
			&UMultiplayerChatComponent::HandleActivateChatInput
		);

	// 활성화 키는 다른 게임 입력으로 전달하지 않습니다.
	ActivateBinding.bConsumeInput = true;

	FInputKeyBinding& CancelBinding =
		ChatInputComponent->BindKey(
			CancelChatKey,
			IE_Pressed,
			this,
			&UMultiplayerChatComponent::HandleCancelChatInput
		);

	// 채팅 중이 아닐 때는 Escape를 다른 게임 입력으로 전달
	CancelBinding.bConsumeInput = false;
	CancelInputBindingIndex = ChatInputComponent->KeyBindings.Num() - 1;

	OwningPlayerController->PushInputComponent(ChatInputComponent);
}

void UMultiplayerChatComponent::TeardownLocalChatInput()
{
	if (ChatInputComponent == nullptr)
	{
		return;
	}

	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());

	if (OwningPlayerController != nullptr)
	{
		OwningPlayerController->PopInputComponent(ChatInputComponent);
	}

	ChatInputComponent->DestroyComponent();
	ChatInputComponent = nullptr;
	CancelInputBindingIndex = INDEX_NONE;
}

void UMultiplayerChatComponent::HandleActivateChatInput()
{
	ActivateChatInput();
}

void UMultiplayerChatComponent::HandleCancelChatInput()
{
	DeactivateChatInput();
}

// 플레이어 ID 결정 함수
FString UMultiplayerChatComponent::ResolvePlayerId(APlayerState* PlayerState) const
{
	if (PlayerState == nullptr)
	{
		return FString();
	}

	const bool bHasIdentityProvider = PlayerState->GetClass()->ImplementsInterface(UMultiplayerChatIdentityProvider::StaticClass());

	if (bHasIdentityProvider)
	{
		FString ProviderPlayerId = IMultiplayerChatIdentityProvider::Execute_GetMultiplayerChatPlayerId(PlayerState);

		ProviderPlayerId.TrimStartAndEndInline();

		if (!ProviderPlayerId.IsEmpty())
		{
			return ProviderPlayerId;
		}
	}

	const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId();

	if (UniqueId.IsValid())
	{
		return UniqueId.ToString();
	}

	// Online Subsystem이 없는 PIE와 LAN 환경에서는 세션 PlayerId를 사용합니다.
	return FString::FromInt(PlayerState->GetPlayerId());
}

// 표시 이름 결정 함수
FString UMultiplayerChatComponent::ResolveDisplayName(APlayerState* PlayerState) const
{
	if (PlayerState == nullptr)
	{
		return FString();
	}
	const bool bHasIdentityProvider = PlayerState->GetClass()->ImplementsInterface(UMultiplayerChatIdentityProvider::StaticClass());

	if (bHasIdentityProvider)
	{
		FString ProviderDisplayName = IMultiplayerChatIdentityProvider::Execute_GetMultiplayerChatDisplayName(PlayerState);

		ProviderDisplayName.TrimStartAndEndInline();

		if (!ProviderDisplayName.IsEmpty())
		{
			return ProviderDisplayName;
		}
	}

	// Identity Provider가 없거나 빈 이름을 반환하면 기본 PlayerState 이름을 사용합니다.
	return PlayerState->GetPlayerName();
}

void UMultiplayerChatComponent::ServerRequestNicknameChange_Implementation(const FString& NewNickname)
{
	FString SanitizedNickname = NewNickname;
	SanitizedNickname.TrimStartAndEndInline();

	APlayerController* OwningPlayerController =Cast<APlayerController>(GetOwner());

	APlayerState* SenderPlayerState = OwningPlayerController != nullptr? OwningPlayerController->PlayerState: nullptr;

	UWorld* World = GetWorld();

	if (OwningPlayerController == nullptr || SenderPlayerState == nullptr || World == nullptr)
	{
		ClientReceiveNicknameChangeResult(EMultiplayerChatNicknameResult::Unavailable,SanitizedNickname);
		return;
	}

	if (!bAllowClientNicknameChanges)
	{
		ClientReceiveNicknameChangeResult(EMultiplayerChatNicknameResult::ChangesDisabled,SanitizedNickname);
		return;
	}

	// 외부 Identity Provider가 이름을 제공하면 플러그인이 변경하지 않습니다.
	const bool bHasIdentityProvider =SenderPlayerState->GetClass()->ImplementsInterface(UMultiplayerChatIdentityProvider::StaticClass());

	if (bHasIdentityProvider)
	{
		FString ProviderDisplayName =IMultiplayerChatIdentityProvider::Execute_GetMultiplayerChatDisplayName(SenderPlayerState);

		ProviderDisplayName.TrimStartAndEndInline();

		if (!ProviderDisplayName.IsEmpty())
		{
			ClientReceiveNicknameChangeResult(EMultiplayerChatNicknameResult::ManagedExternally,SanitizedNickname);
			return;
		}
	}

	// 0보다 큰 제한값을 사용하고 성공 횟수가 한도에 도달했다면 요청을 거부합니다.
	if (
		MaxNicknameChangesPerSession > 0
		&& SuccessfulNicknameChangesThisSession >= MaxNicknameChangesPerSession
	)
	{
		ClientReceiveNicknameChangeResult(
			EMultiplayerChatNicknameResult::ChangeLimitReached,
			SanitizedNickname
		);
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();

	// 너무 빠른 닉네임 변경 요청을 거부합니다.
	if (LastNicknameChangeRequestTime >= 0.0 && CurrentTime - LastNicknameChangeRequestTime< MinimumNicknameChangeInterval)
	{
		ClientReceiveNicknameChangeResult(EMultiplayerChatNicknameResult::RateLimited,SanitizedNickname);
		return;
	}

	LastNicknameChangeRequestTime = CurrentTime;

	if (SanitizedNickname.Len() < MinimumNicknameLength || SanitizedNickname.Len() > MaximumNicknameLength)
	{
		ClientReceiveNicknameChangeResult(EMultiplayerChatNicknameResult::InvalidLength,SanitizedNickname);
		return;
	}

	// 한글, 영문, 숫자와 밑줄만 허용합니다.
	for (const TCHAR Character : SanitizedNickname)
	{
		const bool bIsHangul =
			(Character >= 0x1100 && Character <= 0x11FF)
			|| (Character >= 0x3130 && Character <= 0x318F)
			|| (Character >= 0xAC00 && Character <= 0xD7A3);

		const bool bIsAllowed =
			FChar::IsAlnum(Character) || bIsHangul || Character == TEXT('_');

		if (!bIsAllowed)
		{
			ClientReceiveNicknameChangeResult(EMultiplayerChatNicknameResult::InvalidCharacters,SanitizedNickname);
			return;
		}
	}

	for (const FString& ReservedNickname : ReservedNicknames)
	{
		if (ReservedNickname.Equals(SanitizedNickname,ESearchCase::IgnoreCase))
		{
			ClientReceiveNicknameChangeResult(EMultiplayerChatNicknameResult::ReservedName,SanitizedNickname);
			return;
		}
	}

	// 다른 접속 플레이어가 동일한 닉네임을 사용하는지 확인합니다.
	for (FConstPlayerControllerIterator Iterator =
			World->GetPlayerControllerIterator();
		 Iterator;
		 ++Iterator)
	{
		APlayerController* TargetController = Iterator->Get();

		if (TargetController == nullptr || TargetController == OwningPlayerController || TargetController->PlayerState == nullptr)
		{
			continue;
		}

		const FString ExistingDisplayName =ResolveDisplayName(TargetController->PlayerState);

		if (ExistingDisplayName.Equals(SanitizedNickname,ESearchCase::IgnoreCase))
		{
			ClientReceiveNicknameChangeResult(EMultiplayerChatNicknameResult::AlreadyInUse,SanitizedNickname);
			return;
		}
	}
	// 검증에 통과한 이름을 서버의 PlayerState에 적용합니다.
	SenderPlayerState->SetPlayerName(SanitizedNickname);
	SenderPlayerState->ForceNetUpdate();

	// PlayerState에 이름을 적용한 뒤에만 성공 횟수를 증가시킵니다.
	++SuccessfulNicknameChangesThisSession;

	ClientReceiveNicknameChangeResult(EMultiplayerChatNicknameResult::Success,SanitizedNickname);
}

void UMultiplayerChatComponent::ClientReceiveNicknameChangeResult_Implementation(EMultiplayerChatNicknameResult Result,const FString& Nickname)
{
	OnNicknameChangeResult.Broadcast(Result, Nickname);
}

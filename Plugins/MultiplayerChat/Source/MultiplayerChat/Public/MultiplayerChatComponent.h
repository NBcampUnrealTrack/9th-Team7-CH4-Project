#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Components/ActorComponent.h"
#include "MultiplayerChatTypes.h"
#include "MultiplayerChatComponent.generated.h"

class UUserWidget;
class UInputComponent;
class APlayerState;

// 클라이언트가 채팅 메시지를 수신했을 때 발생하는 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnMultiplayerChatMessageReceived,
	const FMultiplayerChatMessage&,
	Message
);

// 채팅 입력 활성 상태가 변경될 때 발생하는 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnMultiplayerChatInputStateChanged,
	bool,
	bIsActive
);

// 로컬 플레이어의 기본 발신 채널이 변경될 때 발생하는 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnMultiplayerChatActiveChannelChanged,
	EMultiplayerChatChannel,
	Channel
);

// 서버가 닉네임 변경 요청을 처리한 뒤 로컬 UI에 결과를 전달
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnMultiplayerChatNicknameChangeResult,
	EMultiplayerChatNicknameResult,
	Result,
	const FString&,
	Nickname
);

// 플레이어의 채팅 송수신을 담당하는 네트워크 컴포넌트
// 이 컴포넌트는 PlayerController에 장착할 예정
// 클라이언트가 보낸 메시지를 서버에 전달하고,
// 서버가 승인한 메시지를 다시 클라이언트에 전달
UCLASS(ClassGroup = (MultiplayerChat), meta = (BlueprintSpawnableComponent))
class MULTIPLAYERCHAT_API UMultiplayerChatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMultiplayerChatComponent();

	// 레벨 전환 후 로컬 채팅 UI와 입력을 다시 준비합니다.
	void EnsureLocalChatInitialized();

	// 채팅 입력을 명령어 또는 일반 메시지로 구분하여 처리
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Chat")
	void SubmitChatInput(const FString& InputText);

	// 전체 채팅 메시지를 서버로 전송
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Chat")
	void SendGlobalMessage(const FString& MessageText);

	// 지정한 닉네임의 플레이어에게 귓속말을 전송
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Chat")
	void SendWhisperMessage(const FString& TargetNickname, const FString& MessageText);

	// 새 파티 생성을 서버에 요청
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Chat|Party")
	void CreateParty();

	// 지정한 플레이어가 속한 파티 참가를 서버에 요청
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Chat|Party")
	void JoinParty(const FString& TargetNickname);

	// 현재 파티 탈퇴를 서버에 요청
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Chat|Party")
	void LeaveParty();

	// 현재 파티에 채팅 메시지를 전송
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Chat|Party")
	void SendPartyMessage(const FString& MessageText);

	// 현재 기본 발신 채널을 반환
	UFUNCTION(BlueprintPure, Category = "Multiplayer Chat|Channel")
	EMultiplayerChatChannel GetActiveChatChannel() const;

	// 기본 발신 채널이 변경될 때 UI에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Chat|Channel")
	FOnMultiplayerChatActiveChannelChanged OnActiveChannelChanged;

	// 서버에서 승인된 메시지를 수신할 때 호출되는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Chat")
	FOnMultiplayerChatMessageReceived OnMessageReceived;

	// 채팅 입력을 활성화하고 Game And UI 입력 모드로 변경
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Chat|Input")
	void ActivateChatInput();

	// 채팅 입력을 종료하고 Game Only 입력 모드로 복귀
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Chat|Input")
	void DeactivateChatInput();

	// 채팅 입력 상태가 변경될 때 UI에 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Chat|Input")
	FOnMultiplayerChatInputStateChanged OnChatInputStateChanged;

	// 로컬 플레이어가 서버에 세션 닉네임 변경을 요청
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Chat|Identity")
	void RequestNicknameChange(const FString& NewNickname);

	// 닉네임 변경 요청 결과를 Blueprint와 UI에 전달
	UPROPERTY(BlueprintAssignable, Category = "Multiplayer Chat|Identity")
	FOnMultiplayerChatNicknameChangeResult OnNicknameChangeResult;

protected:
	// 컴포넌트가 게임에 진입하면 로컬 플레이어의 채팅 UI를 생성
	virtual void BeginPlay() override;

	// 게임이 종료되거나 컴포넌트가 제거될 때 채팅 UI를 정리
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:

	// 슬래시 명령어를 분석하고 처리
	// 입력을 명령어로 소비했다면 true를 반환
	bool TryHandleChatCommand(const FString& InputText);
	
	// 로컬 플레이어의 채팅창에 시스템 메시지를 표시
	void DisplayLocalSystemMessage(const FString& MessageText);

	// 로컬 플레이어의 기본 발신 채널을 변경
	void SetActiveChatChannel(EMultiplayerChatChannel NewChannel);

	// 클라이언트가 서버로 메시지를 보낼 때 사용하는 RPC
	UFUNCTION(Server, Reliable)
	void ServerSendGlobalMessage(const FString& MessageText);

	// 클라이언트가 서버에 귓속말 전송을 요청할 때 사용하는 RPC
	UFUNCTION(Server, Reliable)
	void ServerSendWhisperMessage(const FString& TargetNickname, const FString& MessageText);

	// 클라이언트가 서버에 파티 메시지 전송을 요청할 때 사용하는 RPC
	UFUNCTION(Server, Reliable)
	void ServerSendPartyMessage(const FString& MessageText);

	// 클라이언트의 파티 생성 요청을 서버에서 처리
	UFUNCTION(Server, Reliable)
	void ServerCreateParty();

	// 클라이언트의 파티 참가 요청을 서버에서 처리
	UFUNCTION(Server, Reliable)
	void ServerJoinParty(const FString& TargetNickname);

	// 클라이언트의 파티 탈퇴 요청을 서버에서 처리
	UFUNCTION(Server, Reliable)
	void ServerLeaveParty();

	// 서버가 특정 클라이언트에 메시지를 전달할 때 사용하는 RPC
	UFUNCTION(Client, Reliable)
	void ClientReceiveMessage(const FMultiplayerChatMessage& Message);

	// 서버 처리 결과를 소유 클라이언트의 시스템 메시지로 전달
	void SendSystemMessageToOwner(const FString& MessageText);

	// 서버에서 도배 방지를 계산하기 위해 사용하는 마지막 전송 시각
	double LastAcceptedMessageTime = -1.0;

	// 서버가 관리하는 현재 파티의 고유 식별자
	// 빈 문자열이면 파티에 속하지 않은 상태입니다.
	FString CurrentPartyId;

	// 명령어가 아닌 일반 메시지를 전송할 기본 채널
	EMultiplayerChatChannel ActiveChatChannel = EMultiplayerChatChannel::Global;

	// 파티 명령 반복 요청을 제한하기 위한 마지막 처리 시각
	double LastPartyCommandTime = -1.0;

	// 컴포넌트가 기본 채팅 UI를 자동 생성할지 결정
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Chat|UI")
	bool bAutoCreateChatWidget = true;

	// 화면에 생성할 기본 채팅 위젯 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Chat|UI")
	TSubclassOf<UUserWidget> ChatWidgetClass;

	// 현재 로컬 플레이어 화면에 생성된 채팅 위젯
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ChatWidget;

	// 현재 로컬 플레이어가 채팅을 입력 중인지 나타냅니다.
	bool bIsChatInputActive = false;

	// 로컬 플레이어용 채팅 입력을 생성하고 입력 스택에 등록
	void SetupLocalChatInput();

	// 생성한 채팅 입력을 입력 스택에서 제거하고 정리
	void TeardownLocalChatInput();

	// 자동 바인딩된 활성화 키를 처리
	void HandleActivateChatInput();

	// 자동 바인딩된 취소 키를 처리
	void HandleCancelChatInput();

	// 플러그인이 채팅 입력 키를 자동으로 등록할지 결정
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Chat|Input")
	bool bAutoBindChatInput = true;

	// 채팅 입력을 활성화하는 키
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Chat|Input")
	FKey ActivateChatKey = EKeys::Enter;

	// 채팅 입력을 취소하는 키
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Chat|Input")
	FKey CancelChatKey = EKeys::Escape;

	// 다른 입력 컴포넌트보다 먼저 채팅 키를 처리하기 위한 우선순위
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Chat|Input")
	int32 ChatInputPriority = 100;

	// 로컬 플레이어의 채팅 키 바인딩을 보관하는 입력 컴포넌트
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> ChatInputComponent;

	// Escape 바인딩의 소비 설정을 변경하기 위한 인덱스
	int32 CancelInputBindingIndex = INDEX_NONE;

	// 프로젝트 Identity Provider 또는 PlayerState에서 플레이어 ID를 결정
	FString ResolvePlayerId(APlayerState* PlayerState) const;

	// 프로젝트 Identity Provider 또는 PlayerState에서 표시 이름을 결정
	FString ResolveDisplayName(APlayerState* PlayerState) const;

	// 소유 클라이언트가 서버에 닉네임 변경을 요청
	UFUNCTION(Server, Reliable)
	void ServerRequestNicknameChange(const FString& NewNickname);

	// 서버가 소유 클라이언트에 닉네임 변경 결과를 전달
	UFUNCTION(Client, Reliable)
	void ClientReceiveNicknameChangeResult(EMultiplayerChatNicknameResult Result,const FString& Nickname);

	// 플러그인이 클라이언트의 세션 닉네임 변경을 허용할지 결정
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Chat|Identity")
	bool bAllowClientNicknameChanges = true;

	// 한 번의 접속 세션에서 허용할 최대 닉네임 변경 성공 횟수
	// 0이면 변경 횟수를 제한하지 않습니다.
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Chat|Identity", meta = (ClampMin = "0"))
	int32 MaxNicknameChangesPerSession = 1;

	// 허용할 최소 닉네임 길이
	UPROPERTY(EditDefaultsOnly,Category = "Multiplayer Chat|Identity",meta = (ClampMin = "1"))
	int32 MinimumNicknameLength = 2;

	// 허용할 최대 닉네임 길이
	UPROPERTY(EditDefaultsOnly,Category = "Multiplayer Chat|Identity",meta = (ClampMin = "1"))
	int32 MaximumNicknameLength = 16;

	// 서버가 허용할 닉네임 변경 요청의 최소 간격
	UPROPERTY(EditDefaultsOnly,Category = "Multiplayer Chat|Identity",meta = (ClampMin = "0.0"))
	float MinimumNicknameChangeInterval = 3.0f;

	// 플레이어가 사용할 수 없는 예약 닉네임
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Chat|Identity")
	TArray<FString> ReservedNicknames;

	// 서버에서 관리하는 현재 세션의 닉네임 변경 성공 횟수
	int32 SuccessfulNicknameChangesThisSession = 0;

	// 서버에서 도배 방지를 계산하기 위한 마지막 닉네임 요청 시각
	double LastNicknameChangeRequestTime = -1.0;
};

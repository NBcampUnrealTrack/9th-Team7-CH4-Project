// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/Ch4CharacterTypes.h"
#include "Ch4_multiGamePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UInputAction;	// [추가]

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class ACh4_multiGamePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACh4_multiGamePlayerController();

	// 마우스 회전 입력 가로채기 (마우스 감도 및 Y축 반전 적용)
	virtual void AddPitchInput(float Val) override;
	virtual void AddYawInput(float Val) override;
	virtual bool IsMoveInputIgnored() const override;
	virtual bool IsLookInputIgnored() const override;

	/**
	 * Safe development command for Hamachi direct-IP tests.
	 * Usage: JoinHamachi 25.x.x.x (an optional :7777 suffix is accepted).
	 */
	UFUNCTION(Exec, BlueprintCallable, Category="Network|Debug")
	void JoinHamachi(FString HostIPv4);

	/** Saves the local choice for travel and asks the server to update replicated PlayerState. */
	UFUNCTION(BlueprintCallable, Category="Player|Character")
	void RequestCharacterType(ECh4CharacterType CharacterType);

	/** Saves the local headwear choice for travel and asks the server to update replicated PlayerState. */
	UFUNCTION(BlueprintCallable, Category="Player|Character")
	void RequestHeadwear(FName HeadwearID);
	
	// [추가] PauseMenu 관련 공개 함수들 선언
	// Local menu only: ESC and the existing IA_Pause mappings toggle this widget.
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void TogglePauseMenu();
	
	// PauseMenu 열기
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void ShowPauseMenu();
	
	// PauseMenu 닫기 (Resume 버튼 클릭 시에도 호출)
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void HidePauseMenu();
	
	// 현재 PauseMenu가 켜져있는지 확인
	UFUNCTION(BlueprintPure, Category = "UI|Pause Menu")
	bool IsPauseMenuOpen() const;
	
	// 세션을 정리하고 메인 메뉴(L_MainMenu)로 돌아가기
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void ReturnToMainMenu();
	
	// 게임 완전히 종료하기
	UFUNCTION(BlueprintCallable, Category = "UI|Pause Menu")
	void QuitGame();

protected:
	/** Lobby animals own their movement mappings; only reuse menu/voice keys there. */
	bool bUseTemplateInputMappings = true;
	
	// [추가] 에디터에서 설정할 프로퍼티와 위젯 인스턴스 변수
	// 에디터에서 만든 IA_Pause를 넣어줄 변수
	UPROPERTY(EditAnywhere, Category = "Input|Pause Menu")
	TObjectPtr<UInputAction> PauseAction;
	
	// 에디터에서 디자인할 WBP_PauseMenu 위젯 클래스를 지정할 변수
	UPROPERTY(EditAnywhere, Category = "Input|Pause Menu")
	TSubclassOf<UUserWidget> PauseMenuWidgetClass;

	/** Deferred MVVM fallback; the game module explicitly includes this widget package during Cook. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Pause Menu")
	TSoftClassPtr<UUserWidget> PauseMenuWidgetAsset = TSoftClassPtr<UUserWidget>(
		FSoftObjectPath(TEXT("/Game/UI/WBP_PauseMenu.WBP_PauseMenu_C")));

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> PauseMenuMappingContext;
	
	// 화면에 생성된 실제 PauseMenu 위젯의 주소를 기억할 포인터 변수
	UPROPERTY()
	TObjectPtr<UUserWidget> PauseMenuWidget;

	// PauseMenu 전용 MVVM ViewModel 인스턴스
	UPROPERTY(BlueprintReadOnly, Category = "UI|Pause Menu")
	TObjectPtr<class UCh4PauseMenuViewModel> PauseMenuViewModel;

	// ── HUD ────────────────────────────────────────────────────────────
	// 에디터에서 WBP_HUD 클래스를 지정할 변수
	UPROPERTY(EditAnywhere, Category = "UI|HUD")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	// 생성된 HUD 위젯 인스턴스
	UPROPERTY()
	TObjectPtr<UUserWidget> HUDWidget;

	// HUD 전용 MVVM ViewModel 인스턴스
	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD")
	TObjectPtr<class UCh4HUDViewModel> HUDViewModel;

	// ── Voice Mute Toggle (V Key) ──────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "Input|Voice")
	TObjectPtr<UInputAction> VoiceToggleAction;

	UFUNCTION(BlueprintCallable, Category = "UI|Voice")
	void ToggleVoice();

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FCh4PauseMenuRuntimeCommand;
#endif
	bool bPauseInputCaptured = false;
	double LastPauseToggleTime = 0.0;
	bool bInputBlockedBeforePause = false;
	TWeakObjectPtr<UInputComponent> PauseBlockedInputComponent;

	UFUNCTION(Server, Reliable)
	void ServerRequestCharacterType(ECh4CharacterType CharacterType);

	void ApplyServerCharacterType(ECh4CharacterType CharacterType);

	UFUNCTION(Server, Reliable)
	void ServerRequestHeadwear(FName HeadwearID);

	void ApplyServerHeadwear(FName HeadwearID);

	void SynchronizeCharacterSelectionForCurrentWorld();

	bool bSubmittedPersistedCharacterType = false;
	bool bSubmittedPersistedHeadwear = false;

};

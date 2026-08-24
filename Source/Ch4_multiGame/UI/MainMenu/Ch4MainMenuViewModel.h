#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "OnlineSessionSettings.h"
#include "Components/SlateWrapperTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "UI/MainMenu/Ch4MainMenuTypes.h"
#include "Ch4MainMenuViewModel.generated.h"

// 메인 메뉴 전체를 제어하는 ViewModel
// 패널 전환, 방 만들기/찾기, 게임 종료 로직을 담당
UCLASS(BlueprintType)
class UCh4MainMenuViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	// FieldNotify 속성 - View Binding 연결 대상
	// 현재 보여줄 패널 (Main / RoomSelection / RoomList)
	UPROPERTY(FieldNotify, Setter, Getter, BlueprintReadOnly, Category = "Menu|State")
	EMenuPanel CurrentPanel = EMenuPanel::Main;
	
	// 버튼 아래 표시되는 상태 텍스트 ("방 생성중...", "검색 중..." 등)
	UPROPERTY(FieldNotify, Setter, Getter, BlueprintReadOnly, Category = "Menu|State")
	FText StatusText;
	
	// true일 때 방 만들기/찾기 버튼 비활성화
	UPROPERTY(FieldNotify, Setter, Getter, BlueprintReadOnly, Category = "Menu|State")
	bool bIsLoading = false;
	
	// 패널별 VIsibility (View Binding 1:1 직결용)
	UPROPERTY(FieldNotify, Getter, BlueprintReadOnly, Category = "Menu|State")
	ESlateVisibility MainPanelVisibility;
	
	UPROPERTY(FieldNotify, Getter, BlueprintReadOnly, Category = "Menu|State")
	ESlateVisibility RoomSelectionVisibility;
	
	UPROPERTY(FieldNotify, Getter, BlueprintReadOnly, Category = "Menu|State")
	ESlateVisibility RoomListVisibility;
	
	// 버튼 클릭 가능 여부 (!bIsLoading)
	UPROPERTY(FieldNotify, Getter, BlueprintReadOnly, Category = "Menu|State")
	bool bCanInteract = true;
	
	// 방이 선택되었을 때만 true (Join 버튼 활성화용)
	UPROPERTY(FieldNotify, Setter, Getter, BlueprintReadOnly, Category = "Menu|State")
	bool bCanJoinRoom = false;
	
	// ----------------------
	// 방 목록 데이터 & 선택 상태
	// ----------------------
	// 검색된 방 목록 (ListView에 바인딩할 데이터 배열)
	UPROPERTY(FieldNotify, Getter, BlueprintReadOnly, Category = "Menu|Session")
	TArray<TObjectPtr<UCh4RoomEntryData>> RoomList;
	
	// 현재 선택된 방의 인덱스 (-1이면 미선택)
	UPROPERTY(BlueprintReadOnly, Category = "Menu|Session")
	int32 SelectedRoomIndex = -1;
	
	// 버튼이 호출할 함수들
	// [게임 시작] 버튼 -> RoomSelection 패널로 전환
	UFUNCTION(BlueprintCallable, Category = "Menu|Navigation")
	void ShowRoomSelection();
	
	// [뒤로 가기] 버튼 -> Main 패널로 복귀
	UFUNCTION(BlueprintCallable, Category = "Menu|Navigation")
	void ShowMainMenu();
	
	// [방 만들기] 버튼 -> 호스트로 L_Lobby 이동
	UFUNCTION(BlueprintCallable, Category = "Menu|Session")
	void HostGame();
	
	// [방 찾기] 버튼 -> LAN 세션 검색
	UFUNCTION(BlueprintCallable, Category = "Menu|Session")
	void FindRooms();
	
	// [방 선택] 버튼 -> 목록에서 아이템 클릭 시 호출
	UFUNCTION(BlueprintCallable, Category = "Menu|Session")
	void SelectRoom(int32 Index);
	
	UFUNCTION(BlueprintCallable, Category = "Menu|Session")
	void SelectRoomEntry(UCh4RoomEntryData* RoomEntry);
	
	// [방 참가] 버튼 -> 선택한 방으로 접속
	UFUNCTION(BlueprintCallable, Category = "Menu|Session")
	void JoinSelectedRoom();
	
	// [게임 종료] 버튼
	UFUNCTION(BlueprintCallable, Category = "Menu|System")
	void QuitGame();

private:
	// FieldNotify Setter/Getter (MVVM 내부 규칙)
	void SetCurrentPanel(EMenuPanel NewPanel);
	EMenuPanel GetCurrentPanel() const { return CurrentPanel; }
	
	void SetStatusText(FText NewText);
	FText GetStatusText() const { return StatusText; }
	
	void SetbIsLoading(bool bNewIsLoading);
	bool GetbIsLoading() const { return bIsLoading; }
	
	void SetbCanJoinRoom(bool bNewCanJoin);
	bool GetbCanJoinRoom() const { return bCanJoinRoom; }
	
	TArray<TObjectPtr<UCh4RoomEntryData>> GetRoomList() const { return RoomList; }
	
	ESlateVisibility GetMainPanelVisibility() const { return CurrentPanel == EMenuPanel::Main ? ESlateVisibility::Visible : ESlateVisibility::Collapsed; }
	ESlateVisibility GetRoomSelectionVisibility() const { return CurrentPanel == EMenuPanel::RoomSelection ? ESlateVisibility::Visible : ESlateVisibility::Collapsed; }
	ESlateVisibility GetRoomListVisibility() const { return CurrentPanel == EMenuPanel::RoomList ? ESlateVisibility::Visible : ESlateVisibility::Collapsed; }
	bool GetbCanInteract() const { return !bIsLoading; }
	
	// 세션 비동기 콜백 함수들
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	
	// 세션 검색 설정 보관용
	TSharedPtr<FOnlineSessionSearch> SearchSettings;
};
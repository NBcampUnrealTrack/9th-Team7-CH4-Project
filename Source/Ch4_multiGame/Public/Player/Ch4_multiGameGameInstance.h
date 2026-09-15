#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Player/Ch4CharacterTypes.h"
#include "Network/Ch4SteamSessionTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Ch4_multiGameGameInstance.generated.h"

class ACh4_PlayerCharacter;
class UCh4HatUnlockConfigDataAsset;
class UCh4LoadingScreenDataAsset;
class UCh4PlayerProgressSaveGame;
class UTexture2D;
class UCh4RoomEntryData;
class SWidget;
struct FCh4GameResult;
struct FWorldContext;

using FCh4SteamSessionAvailabilityCompletion = TFunction<void(bool)>;

/**
 * Keeps this process's local player's final selection across non-seamless travel
 * and owns the local map-loading screen lifecycle.
 * Replicated PlayerState remains the authoritative in-world source of truth.
 */
UCLASS()
class CH4_MULTIGAME_API UCh4_multiGameGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UCh4_multiGameGameInstance();
	virtual void Init() override;
	virtual void Shutdown() override;

	/** True means the asynchronous request was accepted; observe OnSteamSessionComplete. */
	UFUNCTION(BlueprintCallable, Category="Online|Steam")
	bool HostSteamGame();

	UFUNCTION(BlueprintCallable, Category="Online|Steam")
	bool FindSteamGames();

	/** Only accepts an entry from the latest completed search owned by this GameInstance. */
	UFUNCTION(BlueprintCallable, Category="Online|Steam")
	bool JoinSteamGame(UCh4RoomEntryData* Room);

	/** Leave/close the room, wait for Destroy completion, then open the configured main menu. */
	UFUNCTION(BlueprintCallable, Category="Online|Steam")
	bool DestroySteamSession();

	UFUNCTION(BlueprintPure, Category="Online|Steam")
	bool IsSteamSessionBusy() const { return SteamOperation != ECh4SteamSessionOperation::Idle; }

	UFUNCTION(BlueprintPure, Category="Online|Steam")
	bool HasActiveSteamSession() const;

	/** Host only: hide the existing room and reject every new join before Gameplay travel. */
	bool SetSteamSessionGameplayAvailability(FCh4SteamSessionAvailabilityCompletion Completion);

	/** Host only: publish the existing room after the Lobby world has initialized. */
	bool RestoreSteamSessionLobbyAvailability();

	UFUNCTION(BlueprintPure, Category="Online|Steam")
	FText GetSteamSessionStatus() const { return SteamSessionStatus; }

	UFUNCTION(BlueprintPure, Category="Online|Steam")
	TArray<UCh4RoomEntryData*> GetSteamRooms() const;

	UPROPERTY(BlueprintAssignable, Category="Online|Steam")
	FCh4SteamSessionChanged OnSteamSessionChanged;

	UPROPERTY(BlueprintAssignable, Category="Online|Steam")
	FCh4SteamSessionComplete OnSteamSessionComplete;

	/** Explicit process-wide legacy mode: -Ch4DirectIP -nosteam. Never an automatic fallback. */
	bool IsDirectIPDebugEnabled() const { return bDirectIPDebugEnabled; }
	void HandleSteamConnectionFailure(UWorld* FailedWorld, const FString& Message);
	void LogMatchTravel(const UWorld* World, const FString& Destination, bool bSeamless) const;

	/** Records the owning local player's newest choice before its server RPC is confirmed. */
	bool StoreLocalCharacterRequest(ECh4CharacterType CharacterType);

	/**
	 * Caches a value confirmed by the server. A late older replication cannot overwrite
	 * a newer local request that is still waiting for its matching confirmation.
	 */
	bool CacheAuthoritativeCharacterType(ECh4CharacterType CharacterType);
	bool TryGetLocalCharacterType(ECh4CharacterType& OutCharacterType) const;
	bool HasPendingCharacterRequest() const { return bHasPendingCharacterRequest; }

	/** Records the owning local player's newest headwear choice before its server RPC is confirmed. */
	bool StoreLocalHeadwearRequest(FName HeadwearID);
	bool CacheAuthoritativeHeadwear(FName HeadwearID);
	bool TryGetLocalHeadwear(FName& OutHeadwearID) const;
	bool HasPendingHeadwearRequest() const { return bHasPendingHeadwearRequest; }

	/** Stores a new personal best from a replicated server result and saves only when either record improves. */
	bool RecordGameResult(const FCh4GameResult& Result);

	/** Prints the local runtime profile, disk SaveGame, requirements, and all four unlock decisions. */
	UFUNCTION(BlueprintCallable, Category="Player|Progress|Debug")
	void DumpHatUnlockState() const;

	UFUNCTION(BlueprintPure, Category="Player|Progress")
	int32 GetBestSingleGameScore() const;

	UFUNCTION(BlueprintPure, Category="Player|Progress")
	int32 GetBestSingleGameDeliveredCargo() const;

	/** None and hats not listed by the achievement policy are always available. */
	UFUNCTION(BlueprintPure, Category="Player|Progress|Hat Unlocks")
	bool IsHeadwearUnlocked(FName HeadwearID) const;

	UFUNCTION(BlueprintPure, Category="Player|Progress|Hat Unlocks")
	FText GetHeadwearRequirementText(FName HeadwearID) const;

	UFUNCTION(BlueprintPure, Category="Player|Progress|Hat Unlocks")
	UCh4HatUnlockConfigDataAsset* GetHatUnlockConfig() const;

	UFUNCTION(BlueprintPure, Category="Player|Character")
	TSubclassOf<ACh4_PlayerCharacter> LoadCharacterClass(ECh4CharacterType CharacterType) const;

	int32 GetCharacterClassCount() const { return CharacterClasses.Num(); }

private:
	void InitializeSteamSessions();
	void ShutdownSteamSessions();
	bool EnsureSteamReady();
	bool CheckSteamMenuRequest();
	bool RejectSteamRequest(const FString& Message);
	void SetSteamOperation(ECh4SteamSessionOperation Operation, const FText& Message);
	void CompleteSteamOperation(bool bSucceeded, const FText& Message);
	void ClearSteamOperationDelegates();
	void ClearSteamUpdateDelegate();
	bool BeginSteamCreate();
	bool BeginSteamJoin(const FOnlineSessionSearchResult& Result);
	bool BeginSteamDestroy();
	void FailSteamOperation(const FText& Message);
	void TravelToSteamMainMenu();
	void HandleSteamPostLoadMap(UWorld* LoadedWorld);
	void HandleSteamCreateComplete(FName SessionName, bool bSucceeded);
	void HandleSteamFindComplete(bool bSucceeded);
	void HandleSteamJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleSteamDestroyComplete(FName SessionName, bool bSucceeded);
	void HandleSteamUpdateComplete(FName SessionName, bool bSucceeded);
	void HandleSteamInviteAccepted(bool bSucceeded, int32 LocalUserNum, FUniqueNetIdPtr UserId,
		const FOnlineSessionSearchResult& Result);
	bool UpdateSteamSessionMatchState(
		ECh4SteamMatchState MatchState,
		FCh4SteamSessionAvailabilityCompletion Completion = {});

	IOnlineSessionPtr SteamSessionInterface;
	TSharedPtr<FOnlineSessionSearch> SteamSearch;
	FDelegateHandle SteamCreateHandle, SteamFindHandle, SteamJoinHandle, SteamDestroyHandle, SteamUpdateHandle, SteamInviteHandle;
	FDelegateHandle SteamPostLoadHandle;
	ECh4SteamSessionOperation SteamOperation = ECh4SteamSessionOperation::Idle;
	FText SteamSessionStatus;
	FText SteamPendingFailure;
	FString SteamLobbyPackage;
	bool bSteamRehostAfterDestroy = false;
	bool bSteamLeaveRequested = false;
	bool bSteamTravelIsHost = false;
	bool bSteamUpdateInProgress = false;
	bool bDirectIPDebugEnabled = false;
	bool bSteamShuttingDown = false;
	ECh4SteamMatchState PendingSteamMatchState = ECh4SteamMatchState::Lobby;
	FCh4SteamSessionAvailabilityCompletion PendingSteamUpdateCompletion;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCh4RoomEntryData>> SteamRooms;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FCh4SteamSessionGuardsTest;
#endif

	void CacheLoadingScreenAssets();
	void CacheHatUnlockConfig();
	void LoadPlayerProgress();
	bool SavePlayerProgress() const;
	void HandlePreLoadMap(const FWorldContext& LoadContext, const FString& MapName);
	void HandleSeamlessTravelStart(UWorld* World, const FString& MapName);
	void HandleSeamlessTravelTransition(UWorld* World);
	void FinishSeamlessLoadingScreen();
	void HandlePostLoadMap(UWorld* LoadedWorld);

	UPROPERTY(Transient)
	TObjectPtr<UCh4LoadingScreenDataAsset> CachedLoadingScreenData;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> CachedLoadingScreenImages;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CachedLoadingScreenFallbackImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CachedLoadingScreenImage;

	UPROPERTY(Transient)
	TObjectPtr<UCh4HatUnlockConfigDataAsset> CachedHatUnlockConfig;

	UPROPERTY(Transient)
	TObjectPtr<UCh4PlayerProgressSaveGame> PlayerProgress;

	bool bLoadingScreenPrepared = false;
	bool bSeamlessLoadingScreen = false;
	double SeamlessLoadingScreenStarted = 0.0;
	float SeamlessLoadingScreenMinimumTime = 0.0f;
	TSharedPtr<SWidget> SeamlessLoadingWidget;

	/** Shared appearance catalog. Array order follows ECh4CharacterType. */
	UPROPERTY(EditDefaultsOnly, Category="Player|Character")
	TArray<TSoftClassPtr<ACh4_PlayerCharacter>> CharacterClasses;

	/** Local transfer cache only; the server validates and stores the world state in PlayerState. */
	UPROPERTY(Transient)
	ECh4CharacterType LocalCharacterType = ECh4CharacterType::Invalid;

	UPROPERTY(Transient)
	bool bHasPendingCharacterRequest = false;

	UPROPERTY(Transient)
	FName LocalHeadwearID = NAME_None;

	UPROPERTY(Transient)
	bool bHasPendingHeadwearRequest = false;

	UPROPERTY(Transient)
	bool bHasStoredHeadwear = false;
};

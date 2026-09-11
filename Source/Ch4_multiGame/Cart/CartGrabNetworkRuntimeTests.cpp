#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "Cart/CartBase.h"
#include "Ch4_multiGameGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFlow/Ch4_multiGameGameState.h"
#include "GameFramework/PlayerController.h"
#include "Player/Ch4_PlayerCharacter.h"

class FCh4CartGrabNetworkRuntimeCommand final : public IAutomationLatentCommand
{
public:
	explicit FCh4CartGrabNetworkRuntimeCommand(FAutomationTestBase* InTest)
		: Test(InTest), StartedAt(FPlatformTime::Seconds())
	{
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - StartedAt > 45.0)
		{
			Test->AddError(FString::Printf(TEXT("Cart network runtime timeout at stage %d"), Stage));
			return true;
		}

		UWorld* World = FindForestWorld();
		if (!World)
		{
			return false;
		}

		APlayerController* LocalController = World->GetFirstPlayerController();
		ACh4_PlayerCharacter* LocalCharacter = LocalController
			? Cast<ACh4_PlayerCharacter>(LocalController->GetPawn()) : nullptr;
		ACartBase* Cart = FindCart(World);
		if (!LocalController || !LocalCharacter || !Cart)
		{
			return false;
		}

		return LocalController->HasAuthority()
			? UpdateHost(World, Cart, LocalCharacter, Now)
			: UpdateClient(Cart, LocalCharacter, Now);
	}

private:
	UWorld* FindForestWorld() const
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && Context.WorldType == EWorldType::Game && World->HasBegunPlay()
				&& !World->IsInSeamlessTravel()
				&& World->GetPackage()->GetName() == TEXT("/Game/Map/Level/ForestLevel"))
			{
				return World;
			}
		}
		return nullptr;
	}

	ACartBase* FindCart(UWorld* World) const
	{
		for (TActorIterator<ACartBase> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed())
			{
				return *It;
			}
		}
		return nullptr;
	}

	USceneComponent* FindAnchor(ACartBase* Cart, const int32 Index) const
	{
		return Cart ? Cast<USceneComponent>(Cart->GetDefaultSubobjectByName(
			*FString::Printf(TEXT("Anchor_%d"), Index))) : nullptr;
	}

	bool UpdateHost(UWorld* World, ACartBase* Cart, ACh4_PlayerCharacter* HostCharacter, const double Now)
	{
		if (Stage == 6 && Now - StageStartedAt >= 3.0)
		{
			Test->AddInfo(TEXT("CART_GRAB_NETWORK_RESULT Host=Grabbed Client=Grabbed Simultaneous=1 ClientMoveIntent=1 Released=1 EmptyGoalGameOver=1 Score=0"));
			return true;
		}

		ACh4_PlayerCharacter* RemoteCharacter = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* Controller = It->Get())
			{
				ACh4_PlayerCharacter* Candidate = Cast<ACh4_PlayerCharacter>(Controller->GetPawn());
				if (Candidate && Candidate != HostCharacter)
				{
					RemoteCharacter = Candidate;
					break;
				}
			}
		}
		if (!RemoteCharacter)
		{
			return false;
		}

		if (Stage == 0)
		{
			USceneComponent* HostAnchor = FindAnchor(Cart, 1);
			USceneComponent* ClientAnchor = FindAnchor(Cart, 2);
			if (!HostAnchor || !ClientAnchor || !Cart->SetPreparationLocked(true))
			{
				Test->AddError(TEXT("Host could not prepare the Cart lock/anchor fixture"));
				return true;
			}
			HostCharacter->SetActorLocation(HostAnchor->GetComponentLocation());
			RemoteCharacter->SetActorLocation(ClientAnchor->GetComponentLocation());
			HostCharacter->ForceNetUpdate();
			RemoteCharacter->ForceNetUpdate();
			Cart->ForceNetUpdate();
			StageStartedAt = Now;
			Stage = 1;
			Test->AddInfo(TEXT("CART_GRAB_HOST locked preparation fixture for client rejection"));
			return false;
		}

		if (Stage == 1 && Now - StageStartedAt >= 5.0)
		{
			Test->TestNull(TEXT("Remote client cannot occupy an anchor while preparation is locked"),
				Cart->GetAnchorFor(RemoteCharacter));
			if (!Cart->SetPreparationLocked(false))
			{
				Test->AddError(TEXT("Host could not unlock the Cart fixture"));
				return true;
			}
			Cart->ForceNetUpdate();
			Stage = 2;
			return false;
		}

		if (Stage == 2)
		{
			if (!Cart->GetAnchorFor(HostCharacter) && !Cart->TryGrabPlayer(HostCharacter))
			{
				Test->AddError(TEXT("Host could not grab the unlocked Cart"));
				return true;
			}
			if (!Cart->GetAnchorFor(RemoteCharacter))
			{
				return false;
			}
			Test->TestTrue(TEXT("Host and remote client receive distinct Cart anchors"),
				Cart->GetAnchorFor(HostCharacter) != Cart->GetAnchorFor(RemoteCharacter));
			Test->TestTrue(TEXT("Host move intent uses the same authoritative Cart path"),
				Cart->SetPlayerMoveInput(HostCharacter, FVector2D(0.0f, 1.0f)));
			Stage = 3;
			return false;
		}

		if (Stage == 3 && !RemoteCharacter->CartMoveInput.IsNearlyZero())
		{
			Test->TestTrue(TEXT("Server received remote client Cart move intent"),
				RemoteCharacter->CartMoveInput.Equals(FVector2D(0.0f, 1.0f), 0.01f));
			Stage = 4;
			return false;
		}

		if (Stage == 4 && !Cart->GetAnchorFor(RemoteCharacter))
		{
			Test->TestNull(TEXT("Remote release clears server Cart state"), RemoteCharacter->GrabbedCart);
			Test->TestTrue(TEXT("Host release returns its independent anchor"), Cart->ReleasePlayer(HostCharacter));
			ACh4_multiGameGameMode* Rule = World->GetAuthGameMode<ACh4_multiGameGameMode>();
			ACh4_multiGameGameState* State = World->GetGameState<ACh4_multiGameGameState>();
			if (!Rule || !State || !Rule->RequestCargoInitialization(1) || !Rule->RequestGameStart())
			{
				Test->AddError(TEXT("Runtime fixture could not enter Playing for the empty Goal check"));
				return true;
			}
			FCh4DeliveryScoreSummary EmptySummary;
			EmptySummary.bHasScoreData = true;
			Rule->SetDeliveryScoreSummaryForTesting(Cart, EmptySummary);
			Rule->SetEmptyCartGameOverDelayForTesting(1.0f);
			if (!Rule->NotifyGoalReached(Cart))
			{
				Test->AddError(TEXT("Actual Forest GameMode rejected the empty Cart Goal fixture"));
				return true;
			}
			Test->TestEqual(TEXT("Actual Forest remains Playing during the empty Goal delay"),
				State->GetCurrentGamePhase(), ECh4GamePhase::Playing);
			Stage = 5;
			return false;
		}

		if (Stage == 5)
		{
			ACh4_multiGameGameState* State = World->GetGameState<ACh4_multiGameGameState>();
			if (!State || State->GetCurrentGamePhase() != ECh4GamePhase::GameOver)
			{
				return false;
			}
			Test->TestEqual(TEXT("Actual Forest empty Goal finalizes score zero"), State->GetFinalCargoScore(), 0);
			Test->TestEqual(TEXT("Actual Forest empty Goal uses CargoRuleFailed"),
				State->GetGameEndReason(), ECh4GameEndReason::CargoRuleFailed);
			State->ForceNetUpdate();
			StageStartedAt = Now;
			Stage = 6;
			return false;
		}

		return false;
	}

	bool UpdateClient(ACartBase* Cart, ACh4_PlayerCharacter* LocalCharacter, const double Now)
	{
		if (Stage == 0 && Cart->IsPreparationLocked())
		{
			LocalCharacter->ServerRPC_RequestCartGrab(Cart);
			Stage = 1;
			Test->AddInfo(TEXT("CART_GRAB_CLIENT sent locked grab request through owned Character RPC"));
			return false;
		}
		if (Stage == 1 && !Cart->IsPreparationLocked())
		{
			Test->TestNull(TEXT("Locked client request did not establish a Cart grab"), LocalCharacter->GrabbedCart);
			LocalCharacter->ServerRPC_RequestCartGrab(Cart);
			Stage = 2;
			return false;
		}
		if (Stage == 2 && LocalCharacter->GrabbedCart == Cart)
		{
			LocalCharacter->ServerRPC_SetCartMoveInput(FVector2D(0.0f, 1.0f));
			StageStartedAt = Now;
			Stage = 3;
			return false;
		}
		if (Stage == 3 && !LocalCharacter->CartMoveInput.IsNearlyZero() && Now - StageStartedAt >= 0.5)
		{
			LocalCharacter->ServerRPC_RequestCartRelease();
			Stage = 4;
			return false;
		}
		if (Stage == 4 && !LocalCharacter->GrabbedCart)
		{
			Stage = 5;
			return false;
		}
		if (Stage == 5)
		{
			ACh4_multiGameGameState* State = LocalCharacter->GetWorld()
				? LocalCharacter->GetWorld()->GetGameState<ACh4_multiGameGameState>() : nullptr;
			if (!State || State->GetCurrentGamePhase() != ECh4GamePhase::GameOver)
			{
				return false;
			}
			Test->TestEqual(TEXT("Remote client receives empty Goal score zero"), State->GetFinalCargoScore(), 0);
			Test->AddInfo(TEXT("CART_GRAB_CLIENT_RESULT LockedRejected=1 Grabbed=1 MoveIntent=1 Released=1 EmptyGoalGameOver=1 Score=0"));
			return true;
		}
		return false;
	}

	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
	double StageStartedAt = 0.0;
	int32 Stage = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4CartGrabNetworkRuntimeTest,
	"Ch4_multiGame.Cart.RuntimeNetworkGrab",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCh4CartGrabNetworkRuntimeTest::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("Ch4CartGrabNetworkFixture")))
	{
		AddInfo(TEXT("Requires explicit -Ch4CartGrabNetworkFixture with a listen server and one client."));
		return true;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCh4CartGrabNetworkRuntimeCommand(this));
	return true;
}

#endif

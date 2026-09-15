#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Map/LevelFloorBase.h"
#include "Map/RoadBase.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "Player/Ch4CharacterTypes.h"
#include "Player/Ch4_PlayerCharacter.h"

namespace Ch4RagdollRuntime
{
	constexpr TCHAR ForestMap[] = TEXT("/Game/Map/Level/ForestLevel");

	UWorld* FindForestWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && Context.WorldType == EWorldType::Game && World->HasBegunPlay()
				&& !World->IsInSeamlessTravel() && World->GetPackage()->GetName() == ForestMap)
			{
				return World;
			}
		}
		return nullptr;
	}

	void FindCharacters(UWorld* World, TArray<ACh4_PlayerCharacter*>& OutCharacters)
	{
		OutCharacters.Reset();
		for (TActorIterator<ACh4_PlayerCharacter> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed())
			{
				OutCharacters.Add(*It);
			}
		}
	}

	void FindRoads(UWorld* World, TArray<ARoadBase*>& OutRoads)
	{
		OutRoads.Reset();
		for (TActorIterator<ARoadBase> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed())
			{
				OutRoads.Add(*It);
			}
		}
		OutRoads.Sort([](const ARoadBase& Left, const ARoadBase& Right)
		{
			return Left.GetName() < Right.GetName();
		});
	}

	void VerifyDeterministicMapPartOwnership(FAutomationTestBase* Test, UWorld* World, const TCHAR* ObserverRole)
	{
		for (TActorIterator<ARoadBase> It(World); It; ++It)
		{
			Test->TestFalse(FString::Printf(TEXT("%s road %s does not replicate its deterministic transform"),
				ObserverRole, *It->GetName()), It->IsReplicatingMovement());
		}

		for (TActorIterator<ALevelFloorBase> It(World); It; ++It)
		{
			Test->TestFalse(FString::Printf(TEXT("%s environment %s does not replicate its deterministic transform"),
				ObserverRole, *It->GetName()), It->IsReplicatingMovement());
		}
	}

	bool GetRoadTestLocation(const ARoadBase* Road, FVector& OutLocation)
	{
		if (!Road)
		{
			return false;
		}

		TArray<UPrimitiveComponent*> Primitives;
		Road->GetComponents<UPrimitiveComponent>(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive || Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision
				|| Primitive->Bounds.BoxExtent.IsNearlyZero())
			{
				continue;
			}

			OutLocation = Primitive->Bounds.Origin;
			OutLocation.Z += Primitive->Bounds.BoxExtent.Z + 150.0;
			return true;
		}
		return false;
	}

	void AddCharacterSample(FAutomationTestBase* Test, const TCHAR* ObserverRole,
		const TCHAR* Segment, const ACh4_PlayerCharacter* Character)
	{
		const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		const UObject* Base = Character ? Character->GetMovementBaseObject() : nullptr;
		const UActorComponent* BaseComponent = Cast<UActorComponent>(Base);
		const AActor* BaseActor = BaseComponent ? BaseComponent->GetOwner() : Cast<AActor>(Base);
		const FBodyInstance* RootBody = Mesh && Character
			? Mesh->GetBodyInstance(TEXT("chest")) : nullptr;

		Test->AddInfo(FString::Printf(
			TEXT("RAGDOLL_NETWORK_SAMPLE Observer=%s Segment=%s Character=%s Local=%d Authority=%d State=%d Initialized=%d AnyPhysics=%d RootSim=%d RootAwake=%d RootBlend=%.3f BlendPhysics=%d MovementMode=%s BaseActor=%s BaseComponent=%s MeshCollision=%s CapsuleCollision=%s Anim=%s"),
			ObserverRole,
			Segment,
			*GetNameSafe(Character),
			Character && Character->IsLocallyControlled() ? 1 : 0,
			Character && Character->HasAuthority() ? 1 : 0,
			Character && Character->IsRagdollEnabledForDiagnostics() ? 1 : 0,
			Mesh && RootBody ? 1 : 0,
			Mesh && Mesh->IsAnySimulatingPhysics() ? 1 : 0,
			RootBody && RootBody->IsInstanceSimulatingPhysics() ? 1 : 0,
			RootBody && RootBody->IsInstanceAwake() ? 1 : 0,
			RootBody ? RootBody->PhysicsBlendWeight : -1.0f,
			Mesh && Mesh->bBlendPhysics ? 1 : 0,
			Movement ? *UEnum::GetValueAsString(Movement->MovementMode) : TEXT("Invalid"),
			*GetNameSafe(BaseActor),
			*GetNameSafe(Base),
			Mesh ? *Mesh->GetCollisionProfileName().ToString() : TEXT("None"),
			Character && Character->GetCapsuleComponent()
				? *UEnum::GetValueAsString(Character->GetCapsuleComponent()->GetCollisionEnabled()) : TEXT("Invalid"),
			Mesh ? *GetNameSafe(Mesh->GetAnimInstance()) : TEXT("None")));

		Test->TestTrue(FString::Printf(TEXT("%s %s keeps replicated ragdoll state"), Segment, *GetNameSafe(Character)),
			Character && Character->IsRagdollEnabledForDiagnostics());
		Test->TestTrue(FString::Printf(TEXT("%s %s keeps partial mesh physics"), Segment, *GetNameSafe(Character)),
			Mesh && Mesh->IsAnySimulatingPhysics());
		Test->TestTrue(FString::Printf(TEXT("%s %s keeps chest body simulation"), Segment, *GetNameSafe(Character)),
			RootBody && RootBody->IsInstanceSimulatingPhysics());
	}

	void VerifyVisualRagdollState(FAutomationTestBase* Test, const TCHAR* ObserverRole,
		const ACh4_PlayerCharacter* Character)
	{
		static const FName VisualRagdollBones[] =
		{
			TEXT("chest"), TEXT("head"),
			TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
			TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r")
		};

		const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		Test->TestNotNull(FString::Printf(TEXT("%s %s has a skeletal mesh"),
			ObserverRole, *GetNameSafe(Character)), Mesh);
		if (!Mesh)
		{
			return;
		}

		Test->TestEqual(FString::Printf(TEXT("%s %s refreshes remote bone transforms even when culled"),
			ObserverRole, *GetNameSafe(Character)), Mesh->VisibilityBasedAnimTickOption,
			EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones);
		Test->TestFalse(FString::Printf(TEXT("%s %s does not use update-rate optimization"),
			ObserverRole, *GetNameSafe(Character)), Mesh->bEnableUpdateRateOptimizations);

		const UPhysicalAnimationComponent* PhysicalAnimation =
			Character->FindComponentByClass<UPhysicalAnimationComponent>();
		Test->TestTrue(FString::Printf(TEXT("%s %s has PhysicalAnimation bound to the rendered mesh"),
			ObserverRole, *GetNameSafe(Character)),
			PhysicalAnimation && PhysicalAnimation->GetSkeletalMesh() == Mesh);

		for (const FName BoneName : VisualRagdollBones)
		{
			const FBodyInstance* Body = Mesh->GetBodyInstance(BoneName);
			const FString Prefix = FString::Printf(TEXT("%s %s Bone=%s"),
				ObserverRole, *GetNameSafe(Character), *BoneName.ToString());
			Test->TestNotNull(Prefix + TEXT(" has a PhysicsAsset body"), Body);
			if (!Body || !Body->IsValidBodyInstance())
			{
				continue;
			}

			Test->TestTrue(Prefix + TEXT(" simulates"), Body->IsInstanceSimulatingPhysics());
			Test->TestTrue(Prefix + TEXT(" is awake"), Body->IsInstanceAwake());
			Test->TestTrue(Prefix + TEXT(" has full physics blend"), Body->PhysicsBlendWeight >= 0.99f);

			const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
			Test->TestTrue(Prefix + TEXT(" exists in the skeleton"), BoneIndex != INDEX_NONE);
			if (BoneIndex == INDEX_NONE)
			{
				continue;
			}

			const FTransform RenderWorld = Mesh->GetBoneTransform(BoneIndex);
			const FTransform PhysicsWorld = Body->GetUnrealWorldTransform();
			const double PositionError = FVector::Distance(RenderWorld.GetLocation(), PhysicsWorld.GetLocation());
			const double RotationError = FMath::RadiansToDegrees(
				RenderWorld.GetRotation().AngularDistance(PhysicsWorld.GetRotation()));
			Test->TestTrue(Prefix + FString::Printf(TEXT(" reaches render pose (position error %.3f cm)"), PositionError),
				PositionError <= 1.0);
			Test->TestTrue(Prefix + FString::Printf(TEXT(" reaches render pose (rotation error %.3f deg)"), RotationError),
				RotationError <= 2.0);
		}
	}

}

class FCh4RagdollNetworkRuntimeCommand final : public IAutomationLatentCommand
{
public:
	explicit FCh4RagdollNetworkRuntimeCommand(FAutomationTestBase* InTest)
		: Test(InTest), StartedAt(FPlatformTime::Seconds())
	{
		FParse::Value(FCommandLine::Get(), TEXT("Ch4RagdollExpectedPlayers="), ExpectedPlayers);
		ExpectedPlayers = FMath::Max(ExpectedPlayers, 2);

		int32 CharacterTypeIndex = INDEX_NONE;
		if (FParse::Value(FCommandLine::Get(), TEXT("Ch4RagdollCharacterType="), CharacterTypeIndex))
		{
			RequestedCharacterType = Ch4Character::FromIndex(CharacterTypeIndex);
		}
	}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - StartedAt > 90.0)
		{
			Test->AddError(FString::Printf(TEXT("Ragdoll network fixture timed out at segment %d"), SegmentIndex));
			return true;
		}

		UWorld* World = Ch4RagdollRuntime::FindForestWorld();
		if (!World)
		{
			return false;
		}

		TArray<ACh4_PlayerCharacter*> Characters;
		Ch4RagdollRuntime::FindCharacters(World, Characters);
		if (Characters.Num() < ExpectedPlayers)
		{
			return false;
		}

		TArray<ARoadBase*> Roads;
		Ch4RagdollRuntime::FindRoads(World, Roads);
		if (Roads.IsEmpty())
		{
			return false;
		}

		APlayerController* LocalController = World->GetFirstPlayerController();
		const bool bHost = LocalController && LocalController->HasAuthority();
		if (Ch4Character::IsValidType(RequestedCharacterType) && !bRequestedAppearanceApplied)
		{
			for (ACh4_PlayerCharacter* Character : Characters)
			{
				Character->ApplyCharacterType(RequestedCharacterType);
			}
			bRequestedAppearanceApplied = true;
			StageAt = Now;
			Test->AddInfo(FString::Printf(TEXT("RAGDOLL_FIXTURE_CHARACTER_TYPE Type=%s"),
				*UEnum::GetValueAsString(RequestedCharacterType)));
			return false;
		}
		if (bRequestedAppearanceApplied && !bBaselineLogged && Now - StageAt < 1.0)
		{
			return false;
		}
		if (!bBaselineLogged)
		{
			Ch4RagdollRuntime::VerifyDeterministicMapPartOwnership(Test, World, bHost ? TEXT("Host") : TEXT("Client"));
			for (ACh4_PlayerCharacter* Character : Characters)
			{
				Ch4RagdollRuntime::AddCharacterSample(Test, bHost ? TEXT("Host") : TEXT("Client"), TEXT("Baseline"), Character);
				Character->DumpRagdollBoneDiagnosticState(bHost ? TEXT("HostBaseline") : TEXT("ClientBaseline"));
				Ch4RagdollRuntime::VerifyVisualRagdollState(Test, bHost ? TEXT("Host") : TEXT("Client"), Character);
			}
			for (const ARoadBase* Road : Roads)
			{
				Test->AddInfo(FString::Printf(TEXT("RAGDOLL_ROAD_STATE Observer=%s Road=%s Replicated=%d ReplicateMovement=%d AlwaysRelevant=%d RootMobility=%s"),
					bHost ? TEXT("Host") : TEXT("Client"), *Road->GetName(), Road->GetIsReplicated() ? 1 : 0,
					Road->IsReplicatingMovement() ? 1 : 0, Road->bAlwaysRelevant ? 1 : 0,
					Road->GetRootComponent() ? *UEnum::GetValueAsString(Road->GetRootComponent()->Mobility) : TEXT("Invalid")));
			}
			bBaselineLogged = true;
			StageAt = Now;
			if (FParse::Param(FCommandLine::Get(), TEXT("Ch4RagdollVisualOnly")))
			{
				return true;
			}
			return false;
		}

		return bHost ? UpdateHost(Roads, Characters, Now) : UpdateClient(Roads, Characters, Now);
	}

private:
	bool UpdateHost(const TArray<ARoadBase*>& Roads, const TArray<ACh4_PlayerCharacter*>& Characters, const double Now)
	{
		if (!Roads.IsValidIndex(SegmentIndex))
		{
			if (FinalLingerStartedAt <= 0.0)
			{
				FinalLingerStartedAt = Now;
			}
			return Now - FinalLingerStartedAt >= 5.0;
		}

		// The client automation controller starts after its network join. Keep the
		// first segment stable long enough for both processes to begin sampling.
		if (SegmentIndex == 0 && !bTeleportIssued && Now - StageAt < 12.0)
		{
			return false;
		}

		ARoadBase* Road = Roads[SegmentIndex];
		if (!bTeleportIssued)
		{
			FVector TestLocation;
			if (!Ch4RagdollRuntime::GetRoadTestLocation(Road, TestLocation))
			{
				Test->AddError(FString::Printf(TEXT("No colliding primitive found for %s"), *Road->GetName()));
				return true;
			}

			for (int32 Index = 0; Index < Characters.Num(); ++Index)
			{
				ACh4_PlayerCharacter* Character = Characters[Index];
				Character->SetRagdollEnabled(true);
				Character->SetActorLocation(TestLocation + FVector(0.0, Index * 250.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
				Character->ForceNetUpdate();
			}
			bTeleportIssued = true;
			StageAt = Now;
			Test->AddInfo(FString::Printf(TEXT("RAGDOLL_ROAD_TELEPORT Road=%s Location=%s"),
				*Road->GetName(), *TestLocation.ToString()));
			return false;
		}

		if (Now - StageAt < 2.0)
		{
			return false;
		}

		for (ACh4_PlayerCharacter* Character : Characters)
		{
			Ch4RagdollRuntime::AddCharacterSample(Test, TEXT("Host"), *Road->GetName(), Character);
		}
		++SegmentIndex;
		bTeleportIssued = false;
		StageAt = Now;
		return false;
	}

	bool UpdateClient(const TArray<ARoadBase*>& Roads, const TArray<ACh4_PlayerCharacter*>& Characters, const double Now)
	{
		ACh4_PlayerCharacter* LocalCharacter = nullptr;
		for (ACh4_PlayerCharacter* Character : Characters)
		{
			if (Character->IsLocallyControlled())
			{
				LocalCharacter = Character;
				break;
			}
		}
		if (!LocalCharacter)
		{
			return false;
		}

		const ARoadBase* ClosestRoad = nullptr;
		double ClosestDistanceSq = TNumericLimits<double>::Max();
		for (const ARoadBase* Road : Roads)
		{
			FVector TestLocation;
			if (Ch4RagdollRuntime::GetRoadTestLocation(Road, TestLocation))
			{
				const double DistanceSq = FVector::DistSquared(LocalCharacter->GetActorLocation(), TestLocation);
				if (DistanceSq < ClosestDistanceSq)
				{
					ClosestDistanceSq = DistanceSq;
					ClosestRoad = Road;
				}
			}
		}

		if (ClosestRoad && ClosestDistanceSq < FMath::Square(2000.0)
			&& ClosestRoadName != ClosestRoad->GetFName())
		{
			ClosestRoadName = ClosestRoad->GetFName();
			ObservedRoads.Add(ClosestRoadName);
			for (ACh4_PlayerCharacter* Character : Characters)
			{
				Ch4RagdollRuntime::AddCharacterSample(Test, TEXT("Client"), *ClosestRoad->GetName(), Character);
			}
			StageAt = Now;
		}

		if (ObservedRoads.Num() >= Roads.Num())
		{
			return true;
		}

		return false;
	}

	FAutomationTestBase* Test = nullptr;
	double StartedAt = 0.0;
	double StageAt = 0.0;
	double FinalLingerStartedAt = 0.0;
	int32 ExpectedPlayers = 2;
	int32 SegmentIndex = 0;
	bool bBaselineLogged = false;
	bool bTeleportIssued = false;
	bool bRequestedAppearanceApplied = false;
	ECh4CharacterType RequestedCharacterType = ECh4CharacterType::Invalid;
	FName ClosestRoadName = NAME_None;
	TSet<FName> ObservedRoads;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCh4RagdollNetworkRuntimeTest,
	"Ch4_multiGame.Player.Ragdoll.NetworkMapParts",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCh4RagdollNetworkRuntimeTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("Ch4RagdollNetworkFixture")))
	{
		AddInfo(TEXT("Requires explicit -Ch4RagdollNetworkFixture with a listen server and one client."));
		return true;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCh4RagdollNetworkRuntimeCommand(this));
	return true;
}

#endif

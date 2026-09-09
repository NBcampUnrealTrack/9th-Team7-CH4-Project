#pragma once

#include "CoreMinimal.h"

namespace Ch4CartStabilization
{
	struct FCorrectionResult
	{
		FVector AngularAcceleration = FVector::ZeroVector;
		float TiltAngleDegrees = 0.0f;
		float AssistAlpha = 0.0f;
		bool bIsValid = false;
	};

	inline float CalculateAssistAlpha(
		const float TiltAngleDegrees,
		const float DeadZoneDegrees,
		const float FullAssistDegrees)
	{
		const float SafeDeadZone = FMath::Max(DeadZoneDegrees, 0.0f);
		const float SafeFullAssist = FMath::Max(FullAssistDegrees, SafeDeadZone + UE_KINDA_SMALL_NUMBER);
		const float NormalizedAngle = FMath::Clamp(
			(TiltAngleDegrees - SafeDeadZone) / (SafeFullAssist - SafeDeadZone), 0.0f, 1.0f);

		return NormalizedAngle * NormalizedAngle * (3.0f - 2.0f * NormalizedAngle);
	}

	inline bool TryCalculateAverageGroundNormal(
		const TArray<FVector>& ContactNormals,
		const int32 MinimumContactCount,
		const float MinimumWorldUpDot,
		FVector& OutGroundNormal)
	{
		FVector NormalSum = FVector::ZeroVector;
		int32 ValidContactCount = 0;

		for (const FVector& ContactNormal : ContactNormals)
		{
			if (ContactNormal.ContainsNaN())
			{
				continue;
			}

			const FVector SafeNormal = ContactNormal.GetSafeNormal();
			if (SafeNormal.IsNearlyZero()
				|| FVector::DotProduct(SafeNormal, FVector::UpVector) < MinimumWorldUpDot)
			{
				continue;
			}

			NormalSum += SafeNormal;
			++ValidContactCount;
		}

		if (ValidContactCount < FMath::Max(MinimumContactCount, 1))
		{
			OutGroundNormal = FVector::ZeroVector;
			return false;
		}

		OutGroundNormal = NormalSum.GetSafeNormal();
		return !OutGroundNormal.IsNearlyZero();
	}

	inline FCorrectionResult CalculateCorrection(
		const FVector& CartUp,
		const FVector& TargetUp,
		const FVector& AngularVelocityRadians,
		const float DeadZoneDegrees,
		const float FullAssistDegrees,
		const float ProportionalGain,
		const float DerivativeGain,
		const float MaximumAngularAcceleration)
	{
		FCorrectionResult Result;
		if (CartUp.ContainsNaN() || TargetUp.ContainsNaN() || AngularVelocityRadians.ContainsNaN())
		{
			return Result;
		}

		const FVector SafeCartUp = CartUp.GetSafeNormal();
		const FVector SafeTargetUp = TargetUp.GetSafeNormal();
		if (SafeCartUp.IsNearlyZero() || SafeTargetUp.IsNearlyZero())
		{
			return Result;
		}

		const float ClampedDot = FMath::Clamp(FVector::DotProduct(SafeCartUp, SafeTargetUp), -1.0f, 1.0f);
		const float TiltAngleRadians = FMath::Acos(ClampedDot);
		Result.TiltAngleDegrees = FMath::RadiansToDegrees(TiltAngleRadians);
		Result.AssistAlpha = CalculateAssistAlpha(Result.TiltAngleDegrees, DeadZoneDegrees, FullAssistDegrees);
		Result.bIsValid = true;

		if (Result.AssistAlpha <= 0.0f || MaximumAngularAcceleration <= 0.0f)
		{
			return Result;
		}

		const FVector TiltAxis = FVector::CrossProduct(SafeCartUp, SafeTargetUp).GetSafeNormal();
		const FVector AngleError = TiltAxis * TiltAngleRadians;
		const FVector RollPitchAngularVelocity = AngularVelocityRadians
			- SafeTargetUp * FVector::DotProduct(AngularVelocityRadians, SafeTargetUp);

		const FVector AngularAcceleration = (
			AngleError * FMath::Max(ProportionalGain, 0.0f)
			- RollPitchAngularVelocity * FMath::Max(DerivativeGain, 0.0f)) * Result.AssistAlpha;

		Result.AngularAcceleration = AngularAcceleration.GetClampedToMaxSize(MaximumAngularAcceleration);
		return Result;
	}
}

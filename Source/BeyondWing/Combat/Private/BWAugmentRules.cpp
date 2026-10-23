#include "BWAugmentRules.h"

FBWAugmentTotals UBWAugmentRules::Aggregate(const TArray<FBWAugmentData>& Augments)
{
	FBWAugmentTotals T;
	for (const FBWAugmentData& A : Augments)
	{
		switch (A.Effect)
		{
		case EBWAugmentEffect::SpeedMaxUp:        T.SpeedMaxBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::SpeedMinUp:        T.SpeedMinBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::DuelReroll:        T.DuelRerollsPerBattle += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::StartBlock:        T.StartBlockBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::HandSizeUp:        T.HandSizeBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::EffectCardLimitUp: T.EffectLimitBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::AttackMaxUp:       T.AttackMaxMultiplier *= 1.0f + A.Value; break;
		case EBWAugmentEffect::CritChanceUp:      T.CritChanceBonus += A.Value; break;
		case EBWAugmentEffect::MPRegenUp:         T.MPPerTurnBonus += FMath::RoundToInt(A.Value); break;
		case EBWAugmentEffect::ThornArmor:        T.ThornFraction += A.Value; break;
		case EBWAugmentEffect::ReversedHymn:      T.bReversedHymn = true; break;
		case EBWAugmentEffect::SelfBlessing:      T.bSelfBlessing = true; break;
		case EBWAugmentEffect::DoubleOpening:
			T.DuelLimitBonus += FMath::Max(1, FMath::RoundToInt(A.Value));
			T.AttackCardLimitBonus += FMath::Max(1, FMath::RoundToInt(A.Value));
			break;
		}
	}
	return T;
}

bool UBWAugmentRules::IsEligible(const FBWAugmentData& Augment, FName RowName, int32 Act, EClassType PlayerClass, const TArray<FName>& Owned)
{
	if (Owned.Contains(RowName)) return false;
	if (Act < Augment.MinAct || Act > Augment.MaxAct) return false;
	return Augment.ClassType == EClassType::Common || Augment.ClassType == PlayerClass;
}

TArray<FName> UBWAugmentRules::PickCandidates(const TArray<FName>& RowNames, const TArray<FBWAugmentData>& Rows, int32 Act, EClassType PlayerClass,
	const TArray<FName>& Owned, const TArray<FName>& Exclude, int32 Count, FRandomStream& Stream)
{
	TArray<FName> Fresh, Repeat;
	for (int32 i = 0; i < FMath::Min(RowNames.Num(), Rows.Num()); ++i)
	{
		if (!IsEligible(Rows[i], RowNames[i], Act, PlayerClass, Owned)) continue;
		(Exclude.Contains(RowNames[i]) ? Repeat : Fresh).Add(RowNames[i]);
	}

	auto Shuffle = [&Stream](TArray<FName>& Names)
	{
		for (int32 i = Names.Num() - 1; i > 0; --i) Names.Swap(i, Stream.RandRange(0, i));
	};
	Shuffle(Fresh);
	Shuffle(Repeat);

	// 리롤하면 새 후보를 우선하고, 모자라면 이전 후보로 채운다
	TArray<FName> Result;
	for (const FName& Name : Fresh) { if (Result.Num() >= Count) break; Result.Add(Name); }
	for (const FName& Name : Repeat) { if (Result.Num() >= Count) break; Result.Add(Name); }
	return Result;
}

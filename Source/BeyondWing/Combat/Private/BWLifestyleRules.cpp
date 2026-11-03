#include "BWLifestyleRules.h"

bool UBWLifestyleRules::IsRaging(int32 CurrentHP, int32 MaxHP)
{
	return MaxHP > 0 && CurrentHP > 0 && CurrentHP <= FMath::FloorToInt(MaxHP * RageHPFraction);
}

FBWDiceRange UBWLifestyleRules::RageBonus(int32 PHA, bool bBloodRevenge)
{
	const int32 Mult = bBloodRevenge ? 3 : 1;
	return FBWDiceRange(FMath::FloorToInt(PHA * RageMinPerPHA) * Mult, FMath::FloorToInt(PHA * RageMaxPerPHA) * Mult);
}

int32 UBWLifestyleRules::ScaleCardHeal(int32 Amount, bool bRaging)
{
	return bRaging ? Amount / 2 : Amount;
}

FBWDiceRange UBWLifestyleRules::WillBonus(int32 Wounds)
{
	const int32 Steps = FMath::Max(0, Wounds) / 3;
	return FBWDiceRange(Steps, Steps * 2);
}

int32 UBWLifestyleRules::MagicMaxBonus(int32 MagicCountBefore, bool bDouble)
{
	return FMath::Max(0, MagicCountBefore) * MagicMaxPerShot * (bDouble ? 2 : 1);
}

float UBWLifestyleRules::MagicDamageMultiplier(int32 ShotNumber)
{
	if (ShotNumber >= SeventhShot) return SeventhDamageMultiplier * 1.1f;
	return ShotNumber >= 3 ? 1.1f : 1.0f;
}

int32 UBWLifestyleRules::HolyHealAmount(const FBWCardData& Card, int32 FTH, int32 CurrentHP, int32 MaxHP, int32& OutDarkGain)
{
	int32 Heal = Card.EffectHeal + FMath::FloorToInt(FTH * Card.FTHScale);
	OutDarkGain = 1 + Card.SpecialValue;
	if (MaxHP > 0 && CurrentHP * 2 <= MaxHP)
	{
		Heal += FMath::FloorToInt(FTH * HolyLowHPBonusPerFTH);
		OutDarkGain += 1;
	}
	return FMath::Max(0, Heal);
}

FBWCardData UBWLifestyleRules::MakeTenebrisAttack(const FBWCardData& HealCard, int32 FTH)
{
	if (HealCard.bTenebrisForm) return HealCard;
	FBWCardData Out = HealCard;
	const int32 Heal = FMath::Max(1, HealCard.EffectHeal + FMath::FloorToInt(FTH * HealCard.FTHScale));
	Out.OrigSkillMin = HealCard.SkillMin;
	Out.OrigSkillMax = HealCard.SkillMax;
	Out.OrigRange = HealCard.Range;
	Out.bTenebrisForm = true;
	Out.CardKind = ECardKind::Attack;
	Out.SkillMin = Heal / 2;
	Out.SkillMax = Heal;
	Out.Range = TenebrisRange;
	// 공격 범위에 이미 FTH가 들어 있으므로 계수는 쓰지 않는다 (되돌릴 때는 카드 표 값을 다시 쓴다)
	Out.FTHScale = 0.0f;
	Out.OrigFTHScale = HealCard.FTHScale;
	return Out;
}

FBWCardData UBWLifestyleRules::RestoreFromTenebris(const FBWCardData& Card)
{
	if (!Card.bTenebrisForm) return Card;
	FBWCardData Out = Card;
	Out.bTenebrisForm = false;
	Out.CardKind = ECardKind::Effect;
	Out.SkillMin = Card.OrigSkillMin;
	Out.SkillMax = Card.OrigSkillMax;
	Out.Range = Card.OrigRange;
	Out.FTHScale = Card.OrigFTHScale;
	return Out;
}

int32 UBWLifestyleRules::ExplosionExtraMP(int32 MPAfterCost)
{
	return FMath::Clamp(MPAfterCost, 0, ExplosionExtraMPCap);
}

FBWDiceRange UBWLifestyleRules::ApplyExplosion(const FBWDiceRange& Range, int32 ExtraMP)
{
	const float Mult = 1.0f + FMath::Max(0, ExtraMP) * ExplosionPerMP;
	return FBWDiceRange(FMath::FloorToInt(Range.Min * Mult), FMath::FloorToInt(Range.Max * Mult));
}

FBWDiceRange UBWLifestyleRules::ApplyCondition(const FBWDiceRange& Range, EBWCardCondition Condition, int32 Value, const FBWConditionContext& C)
{
	FBWDiceRange R = Range;
	switch (Condition)
	{
	case EBWCardCondition::Rage:           if (C.bRaging) R.Min += Value; break;
	case EBWCardCondition::WoundsMax:      R.Max += (FMath::Max(0, C.Wounds) / 5) * Value; break;
	case EBWCardCondition::LostHPMax:      R.Max += (FMath::Max(0, C.LostHP) / 10) * Value; break;
	case EBWCardCondition::EnemyLowHP:     if (C.bTargetLow) { R.Min += Value; R.Max += Value; } break;
	case EBWCardCondition::NoMoveThisTurn: if (!C.bMoved) R.Max += Value; break;
	case EBWCardCondition::DarkMax:        R.Max += FMath::Max(0, C.Dark) * Value; break;
	case EBWCardCondition::BlockMax:       R.Max += FMath::Max(0, C.Block); break;
	case EBWCardCondition::Liberated:      if (C.bLiberated) { R.Min += Value; R.Max += Value; } break;
	case EBWCardCondition::TenebrisDouble: if (C.bTenebris) { R.Min *= 2; R.Max *= 2; } break;
	default: break;
	}
	R.Min = FMath::Clamp(R.Min, 0, FMath::Max(0, R.Max));
	return R;
}

FString UBWLifestyleRules::Describe(EClassType ClassType, const FBWLifestyleState& S, bool bRaging)
{
	TArray<FString> Parts;
	switch (ClassType)
	{
	case EClassType::PHA:
		if (S.Wounds > 0) Parts.Add(FString::Printf(TEXT("상처 %d"), S.Wounds));
		if (bRaging) Parts.Add(S.bBloodRevenge ? TEXT("피의 복수") : TEXT("광기"));
		if (S.bRageGuard) Parts.Add(TEXT("격노"));
		break;
	case EClassType::TEC:
		if (S.bLoaded) Parts.Add(TEXT("장전"));
		if (S.MagicCount > 0) Parts.Add(FString::Printf(TEXT("마탄 %d/7"), S.MagicCount));
		if (S.bPossessed || S.bPossessedNext) Parts.Add(TEXT("빙의"));
		break;
	case EClassType::INT:
		Parts.Add(S.bLiberated ? FString(TEXT("해방")) : FString::Printf(TEXT("명상 %d/%d"), S.Liberation, LiberationNeeded));
		if (S.ChantReady > 0) Parts.Add(FString::Printf(TEXT("영창 +%d"), S.ChantReady));
		else if (S.ChantPending > 0) Parts.Add(FString::Printf(TEXT("영창 중 +%d"), S.ChantPending));
		break;
	case EClassType::FTH:
		if (S.Dark > 0) Parts.Add(FString::Printf(TEXT("어둠 %d"), S.Dark));
		if (S.bTenebris) Parts.Add(TEXT("테네브리스"));
		break;
	case EClassType::HP:
		if (S.bGaleGuard) Parts.Add(TEXT("질풍노도"));
		if (S.bStormReady || S.bStormPending) Parts.Add(TEXT("폭풍 질주"));
		break;
	default: break;
	}
	if (S.Counter > 0) Parts.Add(FString::Printf(TEXT("반격 %d"), S.Counter));
	return FString::Join(Parts, TEXT(" · "));
}

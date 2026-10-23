#include "BWDuelRules.h"

FBWDiceRange UBWDuelRules::ComputeCardSkillRange(const FBWCardData& Card, const FBWSkillStats& Stats)
{
	const float StatTerm =
		Stats.PHA * Card.PHAScale +
		Stats.TEC * Card.TECScale +
		Stats.INT * Card.INTScale +
		Stats.FTH * Card.FTHScale +
		Stats.MaxHP * Card.HPScale +
		(Stats.PHA + Stats.TEC) * Card.ATKScale;

	int32 Max = Card.SkillMax + FMath::FloorToInt(StatTerm);
	int32 Min = Card.SkillMin + FMath::FloorToInt(StatTerm * 0.5f);

	Max = FMath::Clamp(Max, 0, Card.SkillMax * 3);
	Min = FMath::Clamp(Min, 0, Max);
	return FBWDiceRange(Min, Max);
}

void UBWDuelRules::ComputeCritFromTEC(int32 TEC, float& OutChance, float& OutMultiplier)
{
	OutChance = FMath::Min(0.05f + TEC * 0.005f, 0.5f);
	OutMultiplier = FMath::Min(1.5f + TEC * 0.01f, 2.5f);
}

int32 UBWDuelRules::RollRange(const FBWDiceRange& Range, FRandomStream& Stream)
{
	return Stream.RandRange(Range.Min, FMath::Max(Range.Min, Range.Max));
}

int32 UBWDuelRules::RollSide(const FBWDuelSide& Side, FRandomStream& Stream)
{
	const int32 First = RollRange(Side.SkillRange, Stream);
	if (Side.bConfused == Side.bFocused) return First; // 없음, 또는 혼란·집중 상쇄

	const int32 Second = RollRange(Side.SkillRange, Stream);
	return Side.bConfused ? FMath::Min(First, Second) : FMath::Max(First, Second);
}

FBWDiceRange UBWDuelRules::ScaleRangeMax(const FBWDiceRange& Range, float Multiplier)
{
	const int32 Max = FMath::Max(0, FMath::FloorToInt(Range.Max * Multiplier));
	return FBWDiceRange(FMath::Min(Range.Min, Max), Max);
}

int32 UBWDuelRules::ApplyDamageModifiers(int32 Roll, const FBWDuelSide& Winner, FRandomStream& Stream, bool& bOutCritical, float LoserIncomingMultiplier)
{
	float Damage = (float)Roll;

	bOutCritical = Winner.CritChance > 0.0f && Stream.GetFraction() < Winner.CritChance;
	if (bOutCritical) Damage *= Winner.CritMultiplier;

	Damage *= Winner.DamageMultiplier * LoserIncomingMultiplier;
	return FMath::Max(0, FMath::FloorToInt(Damage));
}

FBWDuelResult UBWDuelRules::ResolveDuel(const FBWDuelSide& Attacker, const FBWDuelSide& Defender, FRandomStream& Stream)
{
	FBWDuelResult Result;
	Result.AttackerRoll = RollSide(Attacker, Stream);
	Result.DefenderRoll = RollSide(Defender, Stream);

	while (true)
	{
		// 동점이면 둘 다 다시 굴린다. 3번 연속 동점이면 피해 없이 끝
		if (Result.AttackerRoll == Result.DefenderRoll)
		{
			if (++Result.TieRerolls >= MaxConsecutiveTies)
			{
				Result.bDraw = true;
				return Result;
			}
			Result.AttackerRoll = RollSide(Attacker, Stream);
			Result.DefenderRoll = RollSide(Defender, Stream);
			continue;
		}

		// 증강 '재굴림': 선공이 지면 자기 굴림만 한 번 다시
		if (Result.AttackerRoll < Result.DefenderRoll && Attacker.LosingRerolls > 0 && !Result.bAttackerRerolled)
		{
			Result.bAttackerRerolled = true;
			Result.AttackerRoll = RollSide(Attacker, Stream);
			continue;
		}
		break;
	}

	Result.bAttackerWon = Result.AttackerRoll > Result.DefenderRoll;
	const FBWDuelSide& Winner = Result.bAttackerWon ? Attacker : Defender;
	const FBWDuelSide& Loser = Result.bAttackerWon ? Defender : Attacker;
	const int32 WinningRoll = Result.bAttackerWon ? Result.AttackerRoll : Result.DefenderRoll;
	Result.FinalDamage = ApplyDamageModifiers(WinningRoll, Winner, Stream, Result.bCritical, Loser.IncomingDamageMultiplier);
	return Result;
}

FBWDuelResult UBWDuelRules::ResolveEnemyStrike(const FBWDuelSide& Enemy, const FBWDuelSide& Target, FRandomStream& Stream)
{
	FBWDuelResult Result;
	Result.bAttackerWon = true;
	Result.AttackerRoll = RollSide(Enemy, Stream);
	Result.FinalDamage = ApplyDamageModifiers(Result.AttackerRoll, Enemy, Stream, Result.bCritical, Target.IncomingDamageMultiplier);
	return Result;
}

FBWCardData UBWDuelRules::MakeBasicStrike(EClassType ClassType)
{
	FBWCardData Strike;
	Strike.CardName = TEXT("기본 공격");
	Strike.Description = TEXT("언제나 쓸 수 있는 삶의 방식 고유의 공격. 공격카드를 쓰면 이 공격이 강해진다");
	Strike.CardKind = ECardKind::Attack;
	Strike.ClassType = ClassType;
	Strike.SkillMin = 1;
	Strike.SkillMax = 3;

	// 임시 수치 (밸런스 단계에서 조정): PHA·TEC·HP는 주 스탯 비례 추가 피해, INT·FTH는 스탯 비례 없이 약하다
	switch (ClassType)
	{
	case EClassType::PHA: Strike.PHAScale = 0.5f; break;
	case EClassType::TEC: Strike.TECScale = 0.5f; break;
	case EClassType::HP:  Strike.HPScale = 0.03f; break;
	default: break;
	}
	return Strike;
}

float UBWDuelRules::MakeClassCardPower(EClassType ClassType)
{
	switch (ClassType)
	{
	case EClassType::PHA:
	case EClassType::TEC: return 0.5f;
	case EClassType::HP:  return 0.8f;
	case EClassType::INT:
	case EClassType::FTH: return 1.3f;
	default:              return 1.0f;
	}
}

FBWDiceRange UBWDuelRules::ScaleCardBoost(const FBWDiceRange& CardRange, float CardPower)
{
	const float Power = FMath::Max(0.0f, CardPower);
	FBWDiceRange Boost(FMath::FloorToInt(CardRange.Min * Power), FMath::FloorToInt(CardRange.Max * Power));
	Boost.Min = FMath::Min(Boost.Min, Boost.Max);
	return Boost;
}

FBWDiceRange UBWDuelRules::AddCardBoost(const FBWDiceRange& BasicRange, const FBWDiceRange& CardRange, float CardPower)
{
	const FBWDiceRange Boost = ScaleCardBoost(CardRange, CardPower);
	return FBWDiceRange(BasicRange.Min + Boost.Min, BasicRange.Max + Boost.Max);
}

FBWClassCombatProfile UBWDuelRules::MakeClassProfile(EClassType ClassType)
{
	FBWClassCombatProfile Profile;
	Profile.Defense = MakeClassDefense(ClassType);

	switch (ClassType)
	{
	case EClassType::INT:
	case EClassType::FTH: // FTH는 보류 중이라 INT와 같은 MP 체계로 시작
		Profile.bUsesMP = true;
		Profile.StartMP = 6;
		Profile.MPPerTurn = 3;
		Profile.MaxMP = 20;
		break;
	case EClassType::HP:
		Profile.StartBlock = 3; // 기획서에 수치가 없어 턴 종료 방어도와 같게 시작 (밸런스 단계에서 조정)
		Profile.BlockPerTurnEnd = 3;
		break;
	default:
		break;
	}
	return Profile;
}

FBWClassStartStats UBWDuelRules::MakeClassStartStats(EClassType ClassType)
{
	// 임시 수치: 주 능력치 8, 나머지 4. 최대 HP는 체력 직업이 가장 높고 마법이 가장 낮다
	FBWClassStartStats Stats;
	switch (ClassType)
	{
	case EClassType::PHA: Stats.PHA = 8; Stats.MaxHP = 100; break;
	case EClassType::TEC: Stats.TEC = 8; Stats.MaxHP = 90;  break;
	case EClassType::INT: Stats.INT = 8; Stats.MaxHP = 85;  break;
	case EClassType::FTH: Stats.FTH = 8; Stats.MaxHP = 90;  break;
	case EClassType::HP:  Stats.PHA = 5; Stats.MaxHP = 130; break;
	default: break;
	}
	return Stats;
}

void UBWDuelRules::ApplyTurnStartResources(const FBWClassCombatProfile& Profile, int32 TurnNumber, int32& InOutMP)
{
	if (!Profile.bUsesMP || TurnNumber <= 1) return;
	InOutMP = FMath::Clamp(InOutMP + Profile.MPPerTurn, 0, Profile.MaxMP);
}

void UBWDuelRules::ApplyTurnEndResources(const FBWClassCombatProfile& Profile, int32& InOutBlock)
{
	InOutBlock += FMath::Max(0, Profile.BlockPerTurnEnd);
}

FBWDefenseStats UBWDuelRules::MakeClassDefense(EClassType ClassType)
{
	FBWDefenseStats Defense;
	switch (ClassType)
	{
	case EClassType::PHA: Defense.Type = EDefenseType::Flat; Defense.FlatReduction = 2; break;
	case EClassType::HP:  Defense.Type = EDefenseType::Flat; Defense.FlatReduction = 3; break;
	case EClassType::TEC: Defense.Type = EDefenseType::Evade; Defense.EvadeChance = 0.25f; break;
	case EClassType::INT:
	case EClassType::FTH: Defense.Type = EDefenseType::MagicShield; break;
	default: break;
	}
	return Defense;
}

int32 UBWDuelRules::GetShieldBlock(const FBWDefenseStats& Defense, int32 Spend)
{
	return FMath::Max(0, FMath::FloorToInt(FMath::Max(0, Spend) * Defense.ShieldPerMP + KINDA_SMALL_NUMBER));
}

FBWDefenseResult UBWDuelRules::ApplyDefense(int32 Damage, const FBWDefenseStats& Defense, int32& InOutMP, FRandomStream& Stream, int32 ShieldMP)
{
	FBWDefenseResult Result;
	Result.DamageAfterDefense = FMath::Max(0, Damage);
	if (Result.DamageAfterDefense == 0) return Result;

	switch (Defense.Type)
	{
	case EDefenseType::Flat:
		Result.DamageAfterDefense = FMath::Max(0, Result.DamageAfterDefense - Defense.FlatReduction);
		break;

	case EDefenseType::Evade:
		if (Stream.GetFraction() < FMath::Clamp(Defense.EvadeChance, 0.0f, MaxEvadeChance))
		{
			Result.bEvaded = true;
			Result.DamageAfterDefense = 0;
		}
		break;

	case EDefenseType::MagicShield:
		// [수정] 가드에 걸기로 고른 MP만 쓴다. 걸어 둔 MP는 피해보다 많아도 모두 사라진다 (얼마나 걸지가 선택)
		Result.MPSpent = FMath::Min3(FMath::Max(0, ShieldMP), FMath::Max(0, InOutMP), FMath::Max(0, Defense.MagicShieldMaxMP));
		InOutMP -= Result.MPSpent;
		Result.ShieldBlocked = FMath::Min(Result.DamageAfterDefense, GetShieldBlock(Defense, Result.MPSpent));
		Result.DamageAfterDefense -= Result.ShieldBlocked;
		break;
	}
	return Result;
}

void UBWDuelRules::ApplyDamageToBlockAndHP(int32 Damage, bool bIgnoreBlock, int32& InOutBlock, int32& InOutHP)
{
	int32 Remaining = FMath::Max(0, Damage);
	if (!bIgnoreBlock)
	{
		const int32 Absorbed = FMath::Min(InOutBlock, Remaining);
		InOutBlock -= Absorbed;
		Remaining -= Absorbed;
	}
	InOutHP = FMath::Max(0, InOutHP - Remaining);
}

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
	if (Side.bMaxRoll) return Side.SkillRange.Max; // 세트 아스트린 4개
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
		// [추가] 세트 겔레온 4개: 지지 않는다
		if (Attacker.bNeverLose && Result.AttackerRoll <= Result.DefenderRoll) Result.AttackerRoll = Result.DefenderRoll + 1;

		// [추가] 세트 아스트린: 동점이면 선공이 이긴다
		if (Result.AttackerRoll == Result.DefenderRoll && Attacker.bWinsTies) break;

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

	Result.bAttackerWon = Result.AttackerRoll > Result.DefenderRoll || (Attacker.bWinsTies && Result.AttackerRoll == Result.DefenderRoll);

	// [추가] 방어 카드: 막아 내면 아무 일도 없다
	if (Defender.bGuard && !Result.bAttackerWon)
	{
		Result.bDraw = true;
		Result.bGuardHeld = true;
		return Result;
	}
	const FBWDuelSide& Winner = Result.bAttackerWon ? Attacker : Defender;
	const FBWDuelSide& Loser = Result.bAttackerWon ? Defender : Attacker;
	const int32 WinningRoll = Result.bAttackerWon ? Result.AttackerRoll : Result.DefenderRoll;
	Result.FinalDamage = ApplyDamageModifiers(WinningRoll, Winner, Stream, Result.bCritical, Loser.IncomingDamageMultiplier);
	// [추가] 방어 카드: 뚫려도 굴린 수만큼 피해가 깎인다
	if (Defender.bGuard && !Attacker.bIgnoreGuard)
	{
		Result.GuardReduced = FMath::Min(Result.FinalDamage, Result.DefenderRoll);
		Result.FinalDamage -= Result.GuardReduced;
	}
	return Result;
}

namespace BWForecast
{
	// 한쪽 굴림의 분포 (혼란 = 두 번 중 낮은 값, 집중 = 높은 값)
	static TArray<double> RollDistribution(const FBWDuelSide& Side, int32& OutMin)
	{
		OutMin = Side.SkillRange.Min;
		const int32 N = FMath::Max(Side.SkillRange.Min, Side.SkillRange.Max) - Side.SkillRange.Min + 1;
		TArray<double> P;
		P.SetNumZeroed(N);
		const bool bTwice = Side.bConfused != Side.bFocused;
		const double N2 = (double)N * N;
		for (int32 K = 0; K < N; ++K)
		{
			if (!bTwice) P[K] = 1.0 / N;
			else if (Side.bFocused) P[K] = ((K + 1.0) * (K + 1.0) - (double)K * K) / N2;
			else P[K] = (((double)N - K) * (N - K) - (N - K - 1.0) * (N - K - 1.0)) / N2;
		}
		return P;
	}

	static double At(const TArray<double>& P, int32 Min, int32 Value)
	{
		const int32 K = Value - Min;
		return P.IsValidIndex(K) ? P[K] : 0.0;
	}

	static FBWDiceRange DamageRange(int32 LowRoll, int32 HighRoll, float Multiplier)
	{
		if (HighRoll < LowRoll || HighRoll <= 0) return FBWDiceRange(0, 0);
		return FBWDiceRange(FMath::Max(0, FMath::FloorToInt(LowRoll * Multiplier)), FMath::Max(0, FMath::FloorToInt(HighRoll * Multiplier)));
	}
}

FBWDuelForecast UBWDuelRules::ForecastDuel(const FBWDuelSide& Attacker, const FBWDuelSide& Defender)
{
	using namespace BWForecast;
	int32 AMin = 0, DMin = 0;
	const TArray<double> PA = RollDistribution(Attacker, AMin);
	const TArray<double> PD = RollDistribution(Defender, DMin);
	const int32 AMax = AMin + PA.Num() - 1;
	const int32 DMax = DMin + PD.Num() - 1;

	// 한 번 굴렸을 때 승·무·패, 그리고 '지면 내 굴림만 다시' (재굴림 증강)의 결과
	double W0 = 0, T0 = 0, L0 = 0, RW = 0, RT = 0, RL = 0;
	for (int32 D = DMin; D <= DMax; ++D)
	{
		const double Pd = At(PD, DMin, D);
		double Below = 0, Equal = At(PA, AMin, D), Above = 0;
		for (int32 A = AMin; A <= AMax; ++A)
		{
			if (A < D) Below += At(PA, AMin, A);
			else if (A > D) Above += At(PA, AMin, A);
		}
		W0 += Pd * Above;
		T0 += Pd * Equal;
		L0 += Pd * Below;
		RW += Pd * Below * Above;
		RT += Pd * Below * Equal;
		RL += Pd * Below * Below;
	}

	// 동점은 MaxConsecutiveTies번째에 무승부. 재굴림은 한 번만 (뒤에서부터 계산)
	struct FOutcome { double W = 0, L = 0, D = 0; };
	FOutcome Next[2]; // Next[u] = 동점 다음 상태 (u = 재굴림을 이미 씀)
	for (int32 U = 0; U < 2; ++U) Next[U].D = 1.0; // 마지막 동점 = 무승부
	for (int32 Tie = MaxConsecutiveTies - 1; Tie >= 0; --Tie)
	{
		FOutcome Cur[2];
		for (int32 U = 0; U < 2; ++U)
		{
			const bool bCanReroll = Attacker.LosingRerolls > 0 && U == 0;
			FOutcome& O = Cur[U];
			O.W = W0 + T0 * Next[U].W;
			O.L = T0 * Next[U].L;
			O.D = T0 * Next[U].D;
			if (bCanReroll)
			{
				O.W += RW + RT * Next[1].W;
				O.L += RL + RT * Next[1].L;
				O.D += RT * Next[1].D;
			}
			else O.L += L0;
		}
		Next[0] = Cur[0];
		Next[1] = Cur[1];
	}

	FBWDuelForecast Out;
	Out.WinChance = (float)Next[0].W;
	Out.LoseChance = (float)Next[0].L;
	Out.DrawChance = (float)Next[0].D;
	if (Out.WinChance > 0.0f) Out.WinDamage = DamageRange(FMath::Max(AMin, DMin + 1), AMax, Attacker.DamageMultiplier * Defender.IncomingDamageMultiplier);
	if (Out.LoseChance > 0.0f) Out.LoseDamage = DamageRange(FMath::Max(DMin, AMin + 1), DMax, Defender.DamageMultiplier * Attacker.IncomingDamageMultiplier);
	if (Defender.bGuard)
	{
		Out.bGuarded = true;
		Out.LoseDamage = FBWDiceRange(0, 0);
		if (!Attacker.bIgnoreGuard) Out.WinDamage = FBWDiceRange(FMath::Max(0, Out.WinDamage.Min - DMax), FMath::Max(0, Out.WinDamage.Max - DMin));
	}
	Out.CritChance = Attacker.CritChance;
	Out.CritMultiplier = Attacker.CritMultiplier;
	return Out;
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

	// 밸런스기획 3장: PHA·TEC·HP는 주 스탯 비례 추가 피해, INT·FTH는 스탯 비례 없이 약하다
	switch (ClassType)
	{
	case EClassType::PHA: Strike.PHAScale = 0.5f; break;
	case EClassType::TEC: Strike.TECScale = 0.75f; break;
	case EClassType::HP:  Strike.HPScale = 0.04f; break;
	default: break;
	}
	return Strike;
}

float UBWDuelRules::MakeClassCardPower(EClassType ClassType)
{
	switch (ClassType)
	{
	case EClassType::PHA:
	case EClassType::TEC: return 0.6f;
	case EClassType::HP:  return 0.8f;
	case EClassType::INT: return 1.1f;
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
	case EClassType::FTH:
		Profile.bUsesMP = true;
		Profile.StartMP = ClassType == EClassType::INT ? 5 : 6;
		Profile.MPPerTurn = 3;
		Profile.MaxMP = 20;
		break;
	case EClassType::HP:
		Profile.StartBlock = 0;
		Profile.BlockPerTurnEnd = 1; // 철혈: 합에서 이길 때도 방어도 +1 (BWLifestyleRules)
		break;
	case EClassType::TEC:
		Profile.CritChanceBonus = 0.1f; // 사수의 눈
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
	case EClassType::HP:  Defense.Type = EDefenseType::Flat; Defense.FlatReduction = 1; break;
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
		// [추가] 마나를 안 걸어도 기본 방어는 든다 (FlatReduction, 전사보다 낮은 값)
		Result.DamageAfterDefense = FMath::Max(0, Result.DamageAfterDefense - Defense.FlatReduction);
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

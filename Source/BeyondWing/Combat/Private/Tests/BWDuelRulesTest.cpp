#include "Misc/AutomationTest.h"
#include "BWDuelRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BWDuelTest
{
	// 월드가 필요 없는 빠른 순수 함수 테스트라 SmokeFilter로 둔다
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter;

	FBWDuelSide MakeSide(int32 Min, int32 Max, float CritChance = 0.0f, float CritMultiplier = 1.5f, float DamageMultiplier = 1.0f)
	{
		FBWDuelSide Side;
		Side.SkillRange = FBWDiceRange(Min, Max);
		Side.CritChance = CritChance;
		Side.CritMultiplier = CritMultiplier;
		Side.DamageMultiplier = DamageMultiplier;
		return Side;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDuelSkillRangeTest, "BeyondWing.Combat.Duel.SkillRange", BWDuelTest::Flags)
bool FBWDuelSkillRangeTest::RunTest(const FString& Parameters)
{
	FBWCardData Card;
	Card.SkillMin = 1;
	Card.SkillMax = 4;
	Card.PHAScale = 1.0f;

	FBWSkillStats Stats;
	Stats.PHA = 5;
	FBWDiceRange Range = UBWDuelRules::ComputeCardSkillRange(Card, Stats);
	TestEqual(TEXT("PHA 5: min = 1 + floor(5 * 0.5)"), Range.Min, 3);
	TestEqual(TEXT("PHA 5: max = 4 + 5"), Range.Max, 9);

	Stats.PHA = 20;
	Range = UBWDuelRules::ComputeCardSkillRange(Card, Stats);
	TestEqual(TEXT("max is capped at 3x base max"), Range.Max, 12);
	TestEqual(TEXT("PHA 20: min = 1 + 10"), Range.Min, 11);

	Card.SkillMin = 1;
	Card.SkillMax = 2;
	Stats.PHA = 20;
	Range = UBWDuelRules::ComputeCardSkillRange(Card, Stats);
	TestEqual(TEXT("max capped at 6"), Range.Max, 6);
	TestEqual(TEXT("min never exceeds max"), Range.Min, 6);

	// ATK = PHA + TEC, 소수점 내림
	FBWCardData AtkCard;
	AtkCard.SkillMin = 1;
	AtkCard.SkillMax = 10;
	AtkCard.ATKScale = 0.5f;
	FBWSkillStats AtkStats;
	AtkStats.PHA = 3;
	AtkStats.TEC = 4;
	Range = UBWDuelRules::ComputeCardSkillRange(AtkCard, AtkStats);
	TestEqual(TEXT("ATK term 3.5: max = 10 + 3"), Range.Max, 13);
	TestEqual(TEXT("ATK term 3.5: min = 1 + floor(1.75)"), Range.Min, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDuelCritStatsTest, "BeyondWing.Combat.Duel.CritFromTEC", BWDuelTest::Flags)
bool FBWDuelCritStatsTest::RunTest(const FString& Parameters)
{
	float Chance = 0.0f, Mult = 0.0f;
	UBWDuelRules::ComputeCritFromTEC(0, Chance, Mult);
	TestEqual(TEXT("TEC 0 chance"), Chance, 0.05f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("TEC 0 multiplier"), Mult, 1.5f, KINDA_SMALL_NUMBER);

	UBWDuelRules::ComputeCritFromTEC(20, Chance, Mult);
	TestEqual(TEXT("TEC 20 chance"), Chance, 0.15f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("TEC 20 multiplier"), Mult, 1.7f, KINDA_SMALL_NUMBER);

	UBWDuelRules::ComputeCritFromTEC(500, Chance, Mult);
	TestEqual(TEXT("chance capped at 50%"), Chance, 0.5f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("multiplier capped at 2.5"), Mult, 2.5f, KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDuelWinnerTest, "BeyondWing.Combat.Duel.WinnerDealsRoll", BWDuelTest::Flags)
bool FBWDuelWinnerTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(1234);

	FBWDuelResult Result = UBWDuelRules::ResolveDuel(BWDuelTest::MakeSide(10, 10), BWDuelTest::MakeSide(1, 1), Stream);
	TestTrue(TEXT("higher roll wins"), Result.bAttackerWon);
	TestFalse(TEXT("not a draw"), Result.bDraw);
	TestEqual(TEXT("winner's roll becomes damage"), Result.FinalDamage, 10);

	Result = UBWDuelRules::ResolveDuel(BWDuelTest::MakeSide(2, 2), BWDuelTest::MakeSide(7, 7), Stream);
	TestFalse(TEXT("defender wins with higher roll"), Result.bAttackerWon);
	TestEqual(TEXT("defender's roll becomes damage"), Result.FinalDamage, 7);

	// 범위 안에서만 굴린다
	for (int32 i = 0; i < 200; ++i)
	{
		Result = UBWDuelRules::ResolveDuel(BWDuelTest::MakeSide(3, 6), BWDuelTest::MakeSide(1, 4), Stream);
		if (Result.bDraw) continue;
		if (Result.AttackerRoll < 3 || Result.AttackerRoll > 6 || Result.DefenderRoll < 1 || Result.DefenderRoll > 4)
		{
			AddError(FString::Printf(TEXT("roll out of range: %d vs %d"), Result.AttackerRoll, Result.DefenderRoll));
			break;
		}
		const int32 Expected = Result.bAttackerWon ? Result.AttackerRoll : Result.DefenderRoll;
		if (Result.FinalDamage != Expected)
		{
			AddError(FString::Printf(TEXT("damage %d != winning roll %d"), Result.FinalDamage, Expected));
			break;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDuelTieTest, "BeyondWing.Combat.Duel.ThreeTiesIsDraw", BWDuelTest::Flags)
bool FBWDuelTieTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(42);
	const FBWDuelResult Result = UBWDuelRules::ResolveDuel(BWDuelTest::MakeSide(5, 5), BWDuelTest::MakeSide(5, 5), Stream);
	TestTrue(TEXT("three ties end the duel"), Result.bDraw);
	TestEqual(TEXT("tie count"), Result.TieRerolls, 3);
	TestEqual(TEXT("no damage on a draw"), Result.FinalDamage, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDuelModifierTest, "BeyondWing.Combat.Duel.CritAndMultipliers", BWDuelTest::Flags)
bool FBWDuelModifierTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(7);
	bool bCrit = false;

	// 치명타 확정: 7 × 1.5 = 10.5 → 10
	int32 Damage = UBWDuelRules::ApplyDamageModifiers(7, BWDuelTest::MakeSide(1, 1, 1.0f, 1.5f), Stream, bCrit);
	TestTrue(TEXT("crit fires at 100%"), bCrit);
	TestEqual(TEXT("crit damage floors"), Damage, 10);

	// 치명타 + 겔레온 1.5 + 취약 1.25: 7 × 1.5 × 1.875 = 19.6875 → 19
	Damage = UBWDuelRules::ApplyDamageModifiers(7, BWDuelTest::MakeSide(1, 1, 1.0f, 1.5f, 1.5f * 1.25f), Stream, bCrit);
	TestEqual(TEXT("crit then multipliers, floored"), Damage, 19);

	// 치명타 0%: 배율만 적용 7 × 1.5 = 10.5 → 10
	Damage = UBWDuelRules::ApplyDamageModifiers(7, BWDuelTest::MakeSide(1, 1, 0.0f, 1.5f, 1.5f), Stream, bCrit);
	TestFalse(TEXT("no crit at 0%"), bCrit);
	TestEqual(TEXT("multiplier only"), Damage, 10);

	Damage = UBWDuelRules::ApplyDamageModifiers(-3, BWDuelTest::MakeSide(1, 1), Stream, bCrit);
	TestEqual(TEXT("negative damage becomes 0"), Damage, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDuelEnemyStrikeTest, "BeyondWing.Combat.Duel.EnemyStrike", BWDuelTest::Flags)
bool FBWDuelEnemyStrikeTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(99);
	const FBWDuelResult Result = UBWDuelRules::ResolveEnemyStrike(BWDuelTest::MakeSide(4, 4), FBWDuelSide(), Stream);
	TestTrue(TEXT("enemy-opened duel always lands"), Result.bAttackerWon);
	TestEqual(TEXT("enemy roll becomes damage"), Result.FinalDamage, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDuelBlockTest, "BeyondWing.Combat.Duel.BlockThenHP", BWDuelTest::Flags)
bool FBWDuelBlockTest::RunTest(const FString& Parameters)
{
	int32 Block = 4, HP = 20;
	UBWDuelRules::ApplyDamageToBlockAndHP(10, false, Block, HP);
	TestEqual(TEXT("block absorbs first"), Block, 0);
	TestEqual(TEXT("rest goes to HP"), HP, 14);

	Block = 4; HP = 20;
	UBWDuelRules::ApplyDamageToBlockAndHP(10, true, Block, HP);
	TestEqual(TEXT("ignore-block keeps block"), Block, 4);
	TestEqual(TEXT("ignore-block hits HP directly"), HP, 10);

	Block = 15; HP = 20;
	UBWDuelRules::ApplyDamageToBlockAndHP(10, false, Block, HP);
	TestEqual(TEXT("block left over"), Block, 5);
	TestEqual(TEXT("HP untouched"), HP, 20);

	Block = 0; HP = 5;
	UBWDuelRules::ApplyDamageToBlockAndHP(50, false, Block, HP);
	TestEqual(TEXT("HP floors at 0"), HP, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDefenseClassTest, "BeyondWing.Combat.Defense.ClassDefaults", BWDuelTest::Flags)
bool FBWDefenseClassTest::RunTest(const FString& Parameters)
{
	FBWDefenseStats D = UBWDuelRules::MakeClassDefense(EClassType::PHA);
	TestTrue(TEXT("PHA flat 2"), D.Type == EDefenseType::Flat && D.FlatReduction == 2);
	D = UBWDuelRules::MakeClassDefense(EClassType::HP);
	TestTrue(TEXT("HP flat 3"), D.Type == EDefenseType::Flat && D.FlatReduction == 3);
	D = UBWDuelRules::MakeClassDefense(EClassType::TEC);
	TestTrue(TEXT("TEC evade 25%"), D.Type == EDefenseType::Evade && FMath::IsNearlyEqual(D.EvadeChance, 0.25f));
	D = UBWDuelRules::MakeClassDefense(EClassType::INT);
	TestTrue(TEXT("INT magic shield, 5 MP per duel"), D.Type == EDefenseType::MagicShield && D.MagicShieldMaxMP == 5);
	D = UBWDuelRules::MakeClassDefense(EClassType::FTH);
	TestTrue(TEXT("FTH magic shield"), D.Type == EDefenseType::MagicShield);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDefenseFlatTest, "BeyondWing.Combat.Defense.Flat", BWDuelTest::Flags)
bool FBWDefenseFlatTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(1);
	int32 MP = 0;
	const FBWDefenseStats PHA = UBWDuelRules::MakeClassDefense(EClassType::PHA);
	const FBWDefenseStats HP = UBWDuelRules::MakeClassDefense(EClassType::HP);
	TestEqual(TEXT("PHA: 5 - 2"), UBWDuelRules::ApplyDefense(5, PHA, MP, Stream).DamageAfterDefense, 3);
	TestEqual(TEXT("PHA: 1 - 2 floors at 0"), UBWDuelRules::ApplyDefense(1, PHA, MP, Stream).DamageAfterDefense, 0);
	TestEqual(TEXT("HP: 5 - 3"), UBWDuelRules::ApplyDefense(5, HP, MP, Stream).DamageAfterDefense, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDefenseEvadeTest, "BeyondWing.Combat.Defense.Evade", BWDuelTest::Flags)
bool FBWDefenseEvadeTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(2026);
	int32 MP = 0;
	const int32 Trials = 4000;

	auto EvadeRate = [&](const FBWDefenseStats& Defense)
	{
		int32 Evaded = 0;
		for (int32 i = 0; i < Trials; ++i)
		{
			const FBWDefenseResult R = UBWDuelRules::ApplyDefense(10, Defense, MP, Stream);
			if (R.bEvaded)
			{
				++Evaded;
				if (R.DamageAfterDefense != 0) { AddError(TEXT("evaded hit still dealt damage")); break; }
			}
			else if (R.DamageAfterDefense != 10) { AddError(TEXT("non-evaded hit was reduced")); break; }
		}
		return (float)Evaded / Trials;
	};

	const float BaseRate = EvadeRate(UBWDuelRules::MakeClassDefense(EClassType::TEC));
	TestTrue(FString::Printf(TEXT("TEC evades about 25%% (got %.3f)"), BaseRate), FMath::Abs(BaseRate - 0.25f) < 0.03f);

	FBWDefenseStats Boosted = UBWDuelRules::MakeClassDefense(EClassType::TEC);
	Boosted.EvadeChance = 0.9f;
	const float CappedRate = EvadeRate(Boosted);
	TestTrue(FString::Printf(TEXT("evade is capped at 60%% (got %.3f)"), CappedRate), FMath::Abs(CappedRate - 0.6f) < 0.03f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDefenseMagicShieldTest, "BeyondWing.Combat.Defense.MagicShield", BWDuelTest::Flags)
bool FBWDefenseMagicShieldTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(3);
	const FBWDefenseStats IntDefense = UBWDuelRules::MakeClassDefense(EClassType::INT);
	TestEqual(TEXT("1 MP blocks 2"), UBWDuelRules::GetShieldBlock(IntDefense, 1), 2);

	int32 MP = 10;
	FBWDefenseResult R = UBWDuelRules::ApplyDefense(8, IntDefense, MP, Stream);
	TestEqual(TEXT("no MP committed: no shield"), R.DamageAfterDefense, 8);
	TestEqual(TEXT("nothing spent"), MP, 10);

	R = UBWDuelRules::ApplyDefense(8, IntDefense, MP, Stream, 3);
	TestEqual(TEXT("3 MP spent"), R.MPSpent, 3);
	TestEqual(TEXT("blocks 6"), R.ShieldBlocked, 6);
	TestEqual(TEXT("8 - 6"), R.DamageAfterDefense, 2);
	TestEqual(TEXT("MP left"), MP, 7);

	R = UBWDuelRules::ApplyDefense(2, IntDefense, MP, Stream, 4);
	TestEqual(TEXT("committed MP is all spent even if the hit is small"), R.MPSpent, 4);
	TestEqual(TEXT("fully absorbed"), R.DamageAfterDefense, 0);
	TestEqual(TEXT("blocked only the damage"), R.ShieldBlocked, 2);

	MP = 20;
	R = UBWDuelRules::ApplyDefense(30, IntDefense, MP, Stream, 9);
	TestEqual(TEXT("at most 5 MP per guard"), R.MPSpent, 5);

	MP = 2;
	R = UBWDuelRules::ApplyDefense(6, IntDefense, MP, Stream, 4);
	TestEqual(TEXT("short on MP: spends what it has"), R.MPSpent, 2);
	TestEqual(TEXT("6 - 4"), R.DamageAfterDefense, 2);
	TestEqual(TEXT("MP drained"), MP, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWClassProfileTest, "BeyondWing.Combat.Class.Profiles", BWDuelTest::Flags)
bool FBWClassProfileTest::RunTest(const FString& Parameters)
{
	const FBWClassCombatProfile PHA = UBWDuelRules::MakeClassProfile(EClassType::PHA);
	TestTrue(TEXT("PHA: flat 2, no MP, no stacking block"), PHA.Defense.FlatReduction == 2 && !PHA.bUsesMP && PHA.BlockPerTurnEnd == 0);

	const FBWClassCombatProfile TEC = UBWDuelRules::MakeClassProfile(EClassType::TEC);
	TestTrue(TEXT("TEC: evade, no MP"), TEC.Defense.Type == EDefenseType::Evade && !TEC.bUsesMP);

	const FBWClassCombatProfile IntProfile = UBWDuelRules::MakeClassProfile(EClassType::INT);
	TestTrue(TEXT("INT: magic shield + MP 6 / +3 / max 20"), IntProfile.Defense.Type == EDefenseType::MagicShield && IntProfile.bUsesMP &&
		IntProfile.StartMP == 6 && IntProfile.MPPerTurn == 3 && IntProfile.MaxMP == 20);

	const FBWClassCombatProfile FTH = UBWDuelRules::MakeClassProfile(EClassType::FTH);
	TestTrue(TEXT("FTH starts on the INT MP system"), FTH.bUsesMP && FTH.StartMP == 6 && FTH.MPPerTurn == 3 && FTH.MaxMP == 20);

	const FBWClassCombatProfile HP = UBWDuelRules::MakeClassProfile(EClassType::HP);
	TestTrue(TEXT("HP: flat 3, block at start and +3 per turn end"), HP.Defense.FlatReduction == 3 && HP.StartBlock > 0 && HP.BlockPerTurnEnd == 3 && !HP.bUsesMP);

	// 시작 능력치: 주 능력치가 가장 높고, 체력 직업의 최대 HP가 가장 높다
	TestTrue(TEXT("PHA start: PHA is the main stat"), UBWDuelRules::MakeClassStartStats(EClassType::PHA).PHA > UBWDuelRules::MakeClassStartStats(EClassType::PHA).TEC);
	TestTrue(TEXT("TEC start: TEC is the main stat"), UBWDuelRules::MakeClassStartStats(EClassType::TEC).TEC > UBWDuelRules::MakeClassStartStats(EClassType::TEC).PHA);
	TestTrue(TEXT("INT start: INT is the main stat"), UBWDuelRules::MakeClassStartStats(EClassType::INT).INT > UBWDuelRules::MakeClassStartStats(EClassType::INT).PHA);
	const int32 HPClassHP = UBWDuelRules::MakeClassStartStats(EClassType::HP).MaxHP;
	for (EClassType Other : { EClassType::PHA, EClassType::TEC, EClassType::INT, EClassType::FTH })
	{
		TestTrue(TEXT("HP class has the highest max HP"), HPClassHP > UBWDuelRules::MakeClassStartStats(Other).MaxHP);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWClassResourcesTest, "BeyondWing.Combat.Class.TurnResources", BWDuelTest::Flags)
bool FBWClassResourcesTest::RunTest(const FString& Parameters)
{
	// INT: 1턴 6 (전투 시작값 그대로), 2턴 9, 3턴 12 ... 최대 20에서 멈춤
	const FBWClassCombatProfile IntProfile = UBWDuelRules::MakeClassProfile(EClassType::INT);
	int32 MP = IntProfile.StartMP;
	UBWDuelRules::ApplyTurnStartResources(IntProfile, 1, MP);
	TestEqual(TEXT("turn 1 keeps the starting MP"), MP, 6);
	UBWDuelRules::ApplyTurnStartResources(IntProfile, 2, MP);
	TestEqual(TEXT("turn 2 +3"), MP, 9);
	for (int32 Turn = 3; Turn <= 10; ++Turn) UBWDuelRules::ApplyTurnStartResources(IntProfile, Turn, MP);
	TestEqual(TEXT("MP caps at 20"), MP, 20);

	// PHA는 MP를 얻지 않는다
	const FBWClassCombatProfile PHA = UBWDuelRules::MakeClassProfile(EClassType::PHA);
	int32 NoMP = 0;
	UBWDuelRules::ApplyTurnStartResources(PHA, 2, NoMP);
	TestEqual(TEXT("PHA gains no MP"), NoMP, 0);

	// HP: 턴 종료마다 방어도 +3, 다른 직업은 그대로
	const FBWClassCombatProfile HP = UBWDuelRules::MakeClassProfile(EClassType::HP);
	int32 Block = HP.StartBlock;
	UBWDuelRules::ApplyTurnEndResources(HP, Block);
	UBWDuelRules::ApplyTurnEndResources(HP, Block);
	TestEqual(TEXT("HP block stacks by 3 per turn end"), Block, HP.StartBlock + 6);

	int32 PhaBlock = 0;
	UBWDuelRules::ApplyTurnEndResources(PHA, PhaBlock);
	TestEqual(TEXT("PHA gets no turn-end block"), PhaBlock, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWDuelDeterminismTest, "BeyondWing.Combat.Duel.SameSeedSameResult", BWDuelTest::Flags)
bool FBWDuelDeterminismTest::RunTest(const FString& Parameters)
{
	const FBWDuelSide Attacker = BWDuelTest::MakeSide(2, 9, 0.3f, 1.8f, 1.5f);
	const FBWDuelSide Defender = BWDuelTest::MakeSide(1, 6);

	FRandomStream StreamA(2026);
	FRandomStream StreamB(2026);
	for (int32 i = 0; i < 500; ++i)
	{
		const FBWDuelResult A = UBWDuelRules::ResolveDuel(Attacker, Defender, StreamA);
		const FBWDuelResult B = UBWDuelRules::ResolveDuel(Attacker, Defender, StreamB);
		if (A.AttackerRoll != B.AttackerRoll || A.DefenderRoll != B.DefenderRoll || A.bCritical != B.bCritical ||
			A.FinalDamage != B.FinalDamage || A.bDraw != B.bDraw || A.TieRerolls != B.TieRerolls)
		{
			AddError(FString::Printf(TEXT("duel %d diverged with the same seed"), i));
			break;
		}
	}
	return true;
}

#endif

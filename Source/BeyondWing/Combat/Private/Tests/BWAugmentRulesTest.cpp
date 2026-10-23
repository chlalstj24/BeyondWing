#include "Misc/AutomationTest.h"
#include "BWAugmentRules.h"
#include "BWDuelRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BWAugmentTest
{
	// 월드가 필요 없는 빠른 순수 함수 테스트라 SmokeFilter로 둔다
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter;

	FBWAugmentData Make(EBWAugmentEffect Effect, float Value, EClassType Class, int32 MinAct, int32 MaxAct)
	{
		FBWAugmentData A;
		A.Effect = Effect;
		A.Value = Value;
		A.ClassType = Class;
		A.MinAct = MinAct;
		A.MaxAct = MaxAct;
		return A;
	}

	// 기획서 10장 표와 같은 구성
	void MakePool(TArray<FName>& Names, TArray<FBWAugmentData>& Rows)
	{
		auto Add = [&](const TCHAR* Name, EBWAugmentEffect E, float V, EClassType C, int32 Min, int32 Max) { Names.Add(Name); Rows.Add(Make(E, V, C, Min, Max)); };
		Add(TEXT("Sprint"), EBWAugmentEffect::SpeedMaxUp, 1, EClassType::Common, 1, 3);
		Add(TEXT("CarefulStep"), EBWAugmentEffect::SpeedMinUp, 1, EClassType::Common, 1, 3);
		Add(TEXT("Reroll"), EBWAugmentEffect::DuelReroll, 1, EClassType::Common, 1, 3);
		Add(TEXT("HardShell"), EBWAugmentEffect::StartBlock, 8, EClassType::Common, 1, 2);
		Add(TEXT("GenerousHand"), EBWAugmentEffect::HandSizeUp, 1, EClassType::Common, 2, 3);
		Add(TEXT("Combo"), EBWAugmentEffect::EffectCardLimitUp, 1, EClassType::Common, 2, 3);
		Add(TEXT("HeavyBlow"), EBWAugmentEffect::AttackMaxUp, 0.3f, EClassType::PHA, 2, 3);
		Add(TEXT("WeakSpot"), EBWAugmentEffect::CritChanceUp, 0.15f, EClassType::TEC, 2, 3);
		Add(TEXT("ManaSpring"), EBWAugmentEffect::MPRegenUp, 2, EClassType::INT, 2, 3);
		Add(TEXT("ThornArmor"), EBWAugmentEffect::ThornArmor, 0.3f, EClassType::HP, 2, 3);
		Add(TEXT("ReversedHymn"), EBWAugmentEffect::ReversedHymn, 0, EClassType::FTH, 2, 3);
		Add(TEXT("SelfBlessing"), EBWAugmentEffect::SelfBlessing, 0, EClassType::FTH, 2, 3);
		Add(TEXT("DoubleOpening"), EBWAugmentEffect::DoubleOpening, 1, EClassType::Common, 3, 3);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWAugmentEligibilityTest, "BeyondWing.Combat.Augment.Eligibility", BWAugmentTest::Flags)
bool FBWAugmentEligibilityTest::RunTest(const FString& Parameters)
{
	TArray<FName> Names;
	TArray<FBWAugmentData> Rows;
	BWAugmentTest::MakePool(Names, Rows);
	FRandomStream Stream(3);

	// 1막 PHA: 공용 1막 증강 4개만 (질주, 신중한 발걸음, 재굴림, 단단한 껍질)
	for (int32 Trial = 0; Trial < 50; ++Trial)
	{
		const TArray<FName> Picks = UBWAugmentRules::PickCandidates(Names, Rows, 1, EClassType::PHA, {}, {}, 3, Stream);
		TestEqual(TEXT("3 candidates"), Picks.Num(), 3);
		for (const FName& P : Picks)
		{
			const int32 Idx = Names.IndexOfByKey(P);
			if (Rows[Idx].ClassType != EClassType::Common || Rows[Idx].MinAct > 1) { AddError(FString::Printf(TEXT("act 1 offered %s"), *P.ToString())); return true; }
		}
		if (Picks[0] == Picks[1] || Picks[1] == Picks[2] || Picks[0] == Picks[2]) { AddError(TEXT("duplicate candidate")); return true; }
	}

	// 3막 TEC: 단단한 껍질(1~2막)과 다른 직업 전용은 안 나오고, 이중 개시·급소는 나올 수 있다
	TSet<FName> Seen;
	for (int32 Trial = 0; Trial < 200; ++Trial)
	{
		for (const FName& P : UBWAugmentRules::PickCandidates(Names, Rows, 3, EClassType::TEC, {}, {}, 3, Stream)) Seen.Add(P);
	}
	TestFalse(TEXT("act 3 never offers HardShell"), Seen.Contains(FName(TEXT("HardShell"))));
	TestFalse(TEXT("TEC never offered PHA HeavyBlow"), Seen.Contains(FName(TEXT("HeavyBlow"))));
	TestTrue(TEXT("TEC can get WeakSpot"), Seen.Contains(FName(TEXT("WeakSpot"))));
	TestTrue(TEXT("act 3 can offer DoubleOpening"), Seen.Contains(FName(TEXT("DoubleOpening"))));

	// 이미 가진 증강은 다시 나오지 않는다
	const TArray<FName> Owned = { TEXT("Sprint"), TEXT("CarefulStep") };
	const TArray<FName> Picks = UBWAugmentRules::PickCandidates(Names, Rows, 1, EClassType::PHA, Owned, {}, 3, Stream);
	TestEqual(TEXT("only 2 act-1 augments left"), Picks.Num(), 2);
	TestFalse(TEXT("owned Sprint excluded"), Picks.Contains(FName(TEXT("Sprint"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWAugmentRerollTest, "BeyondWing.Combat.Augment.RerollPrefersNew", BWAugmentTest::Flags)
bool FBWAugmentRerollTest::RunTest(const FString& Parameters)
{
	TArray<FName> Names;
	TArray<FBWAugmentData> Rows;
	BWAugmentTest::MakePool(Names, Rows);
	FRandomStream Stream(8);

	// 2막 PHA 후보: 공용 6개(질주·신중·재굴림·껍질·넉넉한 손·연계) + 중량타 = 7개 → 리롤하면 이전 3개와 겹치지 않는 3개
	const TArray<FName> First = UBWAugmentRules::PickCandidates(Names, Rows, 2, EClassType::PHA, {}, {}, 3, Stream);
	const TArray<FName> Second = UBWAugmentRules::PickCandidates(Names, Rows, 2, EClassType::PHA, {}, First, 3, Stream);
	TestEqual(TEXT("reroll still gives 3"), Second.Num(), 3);
	for (const FName& N : Second) TestFalse(FString::Printf(TEXT("%s is new"), *N.ToString()), First.Contains(N));

	// 새 후보가 모자라면 이전 후보로 채운다 (1막 PHA는 4개뿐)
	const TArray<FName> A1 = UBWAugmentRules::PickCandidates(Names, Rows, 1, EClassType::PHA, {}, {}, 3, Stream);
	const TArray<FName> A2 = UBWAugmentRules::PickCandidates(Names, Rows, 1, EClassType::PHA, {}, A1, 3, Stream);
	TestEqual(TEXT("filled back to 3"), A2.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWAugmentAggregateTest, "BeyondWing.Combat.Augment.Aggregate", BWAugmentTest::Flags)
bool FBWAugmentAggregateTest::RunTest(const FString& Parameters)
{
	TArray<FName> Names;
	TArray<FBWAugmentData> Rows;
	BWAugmentTest::MakePool(Names, Rows);

	const FBWAugmentTotals T = UBWAugmentRules::Aggregate(Rows);
	TestEqual(TEXT("sprint"), T.SpeedMaxBonus, 1);
	TestEqual(TEXT("careful step"), T.SpeedMinBonus, 1);
	TestEqual(TEXT("reroll"), T.DuelRerollsPerBattle, 1);
	TestEqual(TEXT("hard shell"), T.StartBlockBonus, 8);
	TestEqual(TEXT("generous hand"), T.HandSizeBonus, 1);
	TestEqual(TEXT("combo"), T.EffectLimitBonus, 1);
	TestEqual(TEXT("heavy blow"), T.AttackMaxMultiplier, 1.3f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("weak spot"), T.CritChanceBonus, 0.15f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("mana spring"), T.MPPerTurnBonus, 2);
	TestEqual(TEXT("thorns"), T.ThornFraction, 0.3f, KINDA_SMALL_NUMBER);
	TestTrue(TEXT("FTH flags"), T.bReversedHymn && T.bSelfBlessing);
	TestTrue(TEXT("double opening: duels and attack cards +1"), T.DuelLimitBonus == 1 && T.AttackCardLimitBonus == 1);

	const FBWAugmentTotals Empty = UBWAugmentRules::Aggregate({});
	TestEqual(TEXT("no augments: x1"), Empty.AttackMaxMultiplier, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWAugmentDuelRerollTest, "BeyondWing.Combat.Augment.DuelReroll", BWAugmentTest::Flags)
bool FBWAugmentDuelRerollTest::RunTest(const FString& Parameters)
{
	// 1~10 vs 6 고정: 재굴림 없이 이길 확률 40%(7~10), 지면 한 번 더 → 1 - 0.5×(5/10 패배 중 다시 패) ≈ 0.4 + 0.5×0.4 + 동점 처리
	FRandomStream Stream(21);
	const int32 Trials = 4000;
	auto WinRate = [&](int32 Rerolls, int32& OutRerollsUsed)
	{
		FBWDuelSide Attacker;
		Attacker.SkillRange = FBWDiceRange(1, 10);
		Attacker.LosingRerolls = Rerolls;
		FBWDuelSide Defender;
		Defender.SkillRange = FBWDiceRange(6, 6);
		int32 Wins = 0;
		OutRerollsUsed = 0;
		for (int32 i = 0; i < Trials; ++i)
		{
			const FBWDuelResult R = UBWDuelRules::ResolveDuel(Attacker, Defender, Stream);
			Wins += R.bAttackerWon && !R.bDraw;
			OutRerollsUsed += R.bAttackerRerolled;
		}
		return (float)Wins / Trials;
	};

	int32 Used0 = 0, Used1 = 0;
	const float Base = WinRate(0, Used0);
	const float WithReroll = WinRate(1, Used1);
	TestEqual(TEXT("no rerolls without the augment"), Used0, 0);
	TestTrue(TEXT("reroll is used only after losing"), Used1 > 0 && Used1 < Trials);
	TestTrue(FString::Printf(TEXT("reroll raises the win rate (%.3f -> %.3f)"), Base, WithReroll), WithReroll > Base + 0.15f);
	return true;
}

#endif

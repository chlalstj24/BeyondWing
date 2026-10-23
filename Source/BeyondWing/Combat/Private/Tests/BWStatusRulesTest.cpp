#include "Misc/AutomationTest.h"
#include "BWStatusRules.h"
#include "BWDuelRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BWStatusTest
{
	// 월드가 필요 없는 빠른 순수 함수 테스트라 SmokeFilter로 둔다
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter;

	FBWDuelSide MakeSide(int32 Min, int32 Max)
	{
		FBWDuelSide Side;
		Side.SkillRange = FBWDiceRange(Min, Max);
		return Side;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWStatusApplyTest, "BeyondWing.Combat.Status.ApplyAndRefresh", BWStatusTest::Flags)
bool FBWStatusApplyTest::RunTest(const FString& Parameters)
{
	TArray<FBWStatusInstance> S;
	UBWStatusRules::ApplyStatus(S, EStatusEffect::Bleed, 2, 3);
	TestTrue(TEXT("bleed applied"), UBWStatusRules::HasStatus(S, EStatusEffect::Bleed));
	TestEqual(TEXT("bleed value"), UBWStatusRules::GetStatusValue(S, EStatusEffect::Bleed), 3);

	// 다시 걸리면 지속 턴은 갱신, 수치는 겹치지 않음
	UBWStatusRules::ApplyStatus(S, EStatusEffect::Bleed, 3, 2);
	TestEqual(TEXT("still one bleed entry"), S.Num(), 1);
	TestEqual(TEXT("duration refreshed to the new value"), S[0].RemainingTurns, 3);
	TestEqual(TEXT("values do not stack (larger kept)"), S[0].Value, 3);

	UBWStatusRules::ApplyStatus(S, EStatusEffect::None, 2, 0);
	UBWStatusRules::ApplyStatus(S, EStatusEffect::Stun, 0, 0);
	TestEqual(TEXT("None or 0-turn statuses are ignored"), S.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWStatusTickTest, "BeyondWing.Combat.Status.TickAndBleed", BWStatusTest::Flags)
bool FBWStatusTickTest::RunTest(const FString& Parameters)
{
	TArray<FBWStatusInstance> S;
	UBWStatusRules::ApplyStatus(S, EStatusEffect::Bleed, 2, 4);
	UBWStatusRules::ApplyStatus(S, EStatusEffect::Stun, 1, 0);

	TestEqual(TEXT("own turn 1: bleed 4"), UBWStatusRules::TickEndOfOwnTurn(S), 4);
	TestFalse(TEXT("1-turn stun expires after one own turn"), UBWStatusRules::HasStatus(S, EStatusEffect::Stun));
	TestTrue(TEXT("bleed still active"), UBWStatusRules::HasStatus(S, EStatusEffect::Bleed));

	TestEqual(TEXT("own turn 2: bleed 4"), UBWStatusRules::TickEndOfOwnTurn(S), 4);
	TestEqual(TEXT("2-turn bleed is gone"), S.Num(), 0);
	TestEqual(TEXT("nothing left to bleed"), UBWStatusRules::TickEndOfOwnTurn(S), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWStatusRangeTest, "BeyondWing.Combat.Status.SlowAndWeak", BWStatusTest::Flags)
bool FBWStatusRangeTest::RunTest(const FString& Parameters)
{
	FBWDiceRange Slowed = UBWStatusRules::ApplySlow(FBWDiceRange(3, 6));
	TestTrue(TEXT("slow: 3~6 -> 3~5"), Slowed.Min == 3 && Slowed.Max == 5);
	Slowed = UBWStatusRules::ApplySlow(FBWDiceRange(2, 2));
	TestTrue(TEXT("slow never drops max below min"), Slowed.Min == 2 && Slowed.Max == 2);

	FBWDiceRange Weak = UBWStatusRules::ApplyWeak(FBWDiceRange(3, 9));
	TestTrue(TEXT("weak: max 9 * 0.8 = 7.2 -> 7"), Weak.Min == 3 && Weak.Max == 7);
	Weak = UBWStatusRules::ApplyWeak(FBWDiceRange(4, 4));
	TestTrue(TEXT("weak: min clamped to new max 3"), Weak.Min == 3 && Weak.Max == 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWStatusVulnerableTest, "BeyondWing.Combat.Status.Vulnerable", BWStatusTest::Flags)
bool FBWStatusVulnerableTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(5);
	FBWDuelSide Loser = BWStatusTest::MakeSide(1, 1);
	Loser.IncomingDamageMultiplier = UBWStatusRules::VulnerableMultiplier;

	// 10 × 1.25 = 12.5 → 12
	FBWDuelResult R = UBWDuelRules::ResolveDuel(BWStatusTest::MakeSide(10, 10), Loser, Stream);
	TestEqual(TEXT("vulnerable defender takes +25%"), R.FinalDamage, 12);

	// 겔레온 1.5와 함께: 7 × 1.5 × 1.25 = 13.125 → 13 (곱한 뒤 한 번만 내림)
	FBWDuelSide Geleon = BWStatusTest::MakeSide(7, 7);
	Geleon.DamageMultiplier = 1.5f;
	R = UBWDuelRules::ResolveDuel(Geleon, Loser, Stream);
	TestEqual(TEXT("multipliers combine before flooring"), R.FinalDamage, 13);

	// 적이 선공인 합에서도 대상의 취약이 적용된다
	R = UBWDuelRules::ResolveEnemyStrike(BWStatusTest::MakeSide(8, 8), Loser, Stream);
	TestEqual(TEXT("enemy strike on vulnerable target: 8 * 1.25"), R.FinalDamage, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWStatusConfusionFocusTest, "BeyondWing.Combat.Status.ConfusionAndFocus", BWStatusTest::Flags)
bool FBWStatusConfusionFocusTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(77);
	const int32 Trials = 4000;

	auto AverageRoll = [&](bool bConfused, bool bFocused)
	{
		FBWDuelSide Side = BWStatusTest::MakeSide(1, 10);
		Side.bConfused = bConfused;
		Side.bFocused = bFocused;
		int64 Sum = 0;
		for (int32 i = 0; i < Trials; ++i) Sum += UBWDuelRules::RollSide(Side, Stream);
		return (double)Sum / Trials;
	};

	// 1~10 균등: 한 번 5.5, 두 번 중 낮은 값 ≈ 3.85, 높은 값 ≈ 7.15
	const double Normal = AverageRoll(false, false);
	const double Confused = AverageRoll(true, false);
	const double Focused = AverageRoll(false, true);
	const double Both = AverageRoll(true, true);
	TestTrue(FString::Printf(TEXT("normal avg ~5.5 (%.2f)"), Normal), FMath::Abs(Normal - 5.5) < 0.25);
	TestTrue(FString::Printf(TEXT("confusion takes the lower roll (%.2f)"), Confused), FMath::Abs(Confused - 3.85) < 0.25);
	TestTrue(FString::Printf(TEXT("focus takes the higher roll (%.2f)"), Focused), FMath::Abs(Focused - 7.15) < 0.25);
	TestTrue(FString::Printf(TEXT("confusion + focus cancel out (%.2f)"), Both), FMath::Abs(Both - 5.5) < 0.25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWStatusDescribeTest, "BeyondWing.Combat.Status.Describe", BWStatusTest::Flags)
bool FBWStatusDescribeTest::RunTest(const FString& Parameters)
{
	TArray<FBWStatusInstance> S;
	UBWStatusRules::ApplyStatus(S, EStatusEffect::Stun, 1, 0);
	UBWStatusRules::ApplyStatus(S, EStatusEffect::Bleed, 2, 3);
	TestEqual(TEXT("HUD text"), UBWStatusRules::DescribeStatuses(S), FString(TEXT("기절 1, 출혈(3) 2")));
	return true;
}

#endif

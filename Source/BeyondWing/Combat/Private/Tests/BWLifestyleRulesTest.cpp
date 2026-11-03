#include "Misc/AutomationTest.h"
#include "BeyondWing/Combat/Public/BWLifestyleRules.h"

namespace BWLifestyleTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWLifestyleBerserkerTest, "BeyondWing.Combat.Lifestyle.Berserker", BWLifestyleTest::Flags)
bool FBWLifestyleBerserkerTest::RunTest(const FString& Parameters)
{
	// 광기: HP 60% 이하
	TestTrue(TEXT("60/100 rages"), UBWLifestyleRules::IsRaging(60, 100));
	TestFalse(TEXT("61/100 does not"), UBWLifestyleRules::IsRaging(61, 100));
	TestFalse(TEXT("dead does not"), UBWLifestyleRules::IsRaging(0, 100));

	// PHA 8: 최소 +1, 최대 +2. 피의 복수면 3배
	const FBWDiceRange Rage = UBWLifestyleRules::RageBonus(8, false);
	TestEqual(TEXT("rage min"), Rage.Min, 1);
	TestEqual(TEXT("rage max"), Rage.Max, 2);
	TestEqual(TEXT("blood revenge max"), UBWLifestyleRules::RageBonus(8, true).Max, 6);

	TestEqual(TEXT("card heal halves in rage"), UBWLifestyleRules::ScaleCardHeal(7, true), 3);
	TestEqual(TEXT("card heal full otherwise"), UBWLifestyleRules::ScaleCardHeal(7, false), 7);

	// 철의 의지: 상처 3당 최소 +1·최대 +2
	const FBWDiceRange Will = UBWLifestyleRules::WillBonus(10);
	TestEqual(TEXT("will min"), Will.Min, 3);
	TestEqual(TEXT("will max"), Will.Max, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWLifestyleGunnerTest, "BeyondWing.Combat.Lifestyle.Gunner", BWLifestyleTest::Flags)
bool FBWLifestyleGunnerTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("first bullet no bonus"), UBWLifestyleRules::MagicMaxBonus(0, false), 0);
	TestEqual(TEXT("after 3 bullets +6"), UBWLifestyleRules::MagicMaxBonus(3, false), 6);
	TestEqual(TEXT("double"), UBWLifestyleRules::MagicMaxBonus(3, true), 12);
	TestEqual(TEXT("2nd shot x1"), UBWLifestyleRules::MagicDamageMultiplier(2), 1.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("3rd shot x1.1"), UBWLifestyleRules::MagicDamageMultiplier(3), 1.1f, KINDA_SMALL_NUMBER);
	TestTrue(TEXT("7th shot at least x3"), UBWLifestyleRules::MagicDamageMultiplier(7) >= 3.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWLifestyleCasterTest, "BeyondWing.Combat.Lifestyle.Caster", BWLifestyleTest::Flags)
bool FBWLifestyleCasterTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("extra MP capped at 10"), UBWLifestyleRules::ExplosionExtraMP(14), 10);
	TestEqual(TEXT("no negative extra"), UBWLifestyleRules::ExplosionExtraMP(-2), 0);
	const FBWDiceRange Boom = UBWLifestyleRules::ApplyExplosion(FBWDiceRange(20, 40), 10);
	TestEqual(TEXT("explosion +80% min"), Boom.Min, 36);
	TestEqual(TEXT("explosion +80% max"), Boom.Max, 72);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWLifestyleSaintTest, "BeyondWing.Combat.Lifestyle.Saint", BWLifestyleTest::Flags)
bool FBWLifestyleSaintTest::RunTest(const FString& Parameters)
{
	FBWCardData Whisper;
	Whisper.CardKind = ECardKind::Effect;
	Whisper.Special = EBWCardSpecial::HolyHeal;
	Whisper.EffectHeal = 5;
	Whisper.FTHScale = 0.5f;
	Whisper.SkillMin = 0;
	Whisper.SkillMax = 0;
	Whisper.Range = 1;

	// FTH 8: 5 + 4 = 9, 어둠 +1. HP 절반 이하면 +2 더 회복(8×0.25)하고 어둠 +2
	int32 Dark = 0;
	TestEqual(TEXT("normal heal"), UBWLifestyleRules::HolyHealAmount(Whisper, 8, 80, 90, Dark), 9);
	TestEqual(TEXT("normal dark"), Dark, 1);
	TestEqual(TEXT("low HP heal"), UBWLifestyleRules::HolyHealAmount(Whisper, 8, 40, 90, Dark), 11);
	TestEqual(TEXT("low HP dark"), Dark, 2);

	// 테네브리스: 치유량 9 → 공격 4~9, 사거리 3. 되돌리면 원래대로
	const FBWCardData Attack = UBWLifestyleRules::MakeTenebrisAttack(Whisper, 8);
	TestTrue(TEXT("becomes attack"), Attack.CardKind == ECardKind::Attack && Attack.bTenebrisForm);
	TestEqual(TEXT("attack min"), Attack.SkillMin, 4);
	TestEqual(TEXT("attack max"), Attack.SkillMax, 9);
	TestEqual(TEXT("range 3"), Attack.Range, 3);
	const FBWCardData Back = UBWLifestyleRules::RestoreFromTenebris(Attack);
	TestTrue(TEXT("back to effect"), Back.CardKind == ECardKind::Effect && !Back.bTenebrisForm);
	TestEqual(TEXT("scale restored"), Back.FTHScale, 0.5f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("range restored"), Back.Range, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBWLifestyleConditionTest, "BeyondWing.Combat.Lifestyle.Conditions", BWLifestyleTest::Flags)
bool FBWLifestyleConditionTest::RunTest(const FString& Parameters)
{
	FBWConditionContext C;
	C.Wounds = 12;
	C.LostHP = 35;
	C.Dark = 3;
	C.Block = 9;
	const FBWDiceRange Base(3, 6);
	TestEqual(TEXT("wounds 12 -> +4"), UBWLifestyleRules::ApplyCondition(Base, EBWCardCondition::WoundsMax, 2, C).Max, 10);
	TestEqual(TEXT("lost 35 -> +6"), UBWLifestyleRules::ApplyCondition(Base, EBWCardCondition::LostHPMax, 2, C).Max, 12);
	TestEqual(TEXT("dark 3 -> +6"), UBWLifestyleRules::ApplyCondition(Base, EBWCardCondition::DarkMax, 2, C).Max, 12);
	TestEqual(TEXT("block -> +9"), UBWLifestyleRules::ApplyCondition(Base, EBWCardCondition::BlockMax, 0, C).Max, 15);
	TestEqual(TEXT("not raging -> unchanged"), UBWLifestyleRules::ApplyCondition(Base, EBWCardCondition::Rage, 3, C).Min, 3);
	C.bRaging = true;
	TestEqual(TEXT("raging -> min +3"), UBWLifestyleRules::ApplyCondition(Base, EBWCardCondition::Rage, 3, C).Min, 6);
	C.bTenebris = true;
	TestEqual(TEXT("tenebris double"), UBWLifestyleRules::ApplyCondition(Base, EBWCardCondition::TenebrisDouble, 0, C).Max, 12);
	return true;
}

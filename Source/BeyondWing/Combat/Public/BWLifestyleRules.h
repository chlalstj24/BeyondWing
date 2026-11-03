#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWLifestyleRules.generated.h"

// 삶의 방식 고유 자원 (삶의방식카드기획 3~7장). 전투마다 처음부터 시작한다
USTRUCT(BlueprintType)
struct FBWLifestyleState
{
	GENERATED_BODY()

	// 광전사: 이번 전투에서 자해·출혈·고정 피해로 잃은 HP의 누계, 철의 의지 사용 횟수, 격노
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") int32 Wounds = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") int32 WillUses = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bRageUsed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bRageGuard = false;   // 이번 적 턴 받는 피해 -90%
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bRageGuardHit = false;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bBloodRevenge = false; // 이번 턴 광기 보너스 3배

	// 마탄의 사수: 장전, 이번 전투에 쏜 마탄 수, 들림(이번 턴), 다음 턴 들림
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bLoaded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") int32 MagicCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bPossessed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bPossessedNext = false;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bFiredMagicThisTurn = false;

	// 매직 캐스터: 명상 횟수(해방), 해방, 영창 (걸어 둔 값 / 이번 턴 쓸 수 있는 값)
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") int32 Liberation = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bLiberated = false;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") int32 ChantPending = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") int32 ChantReady = 0;

	// 어둠의 성자: 어둠, 테네브리스
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") int32 Dark = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bTenebris = false;

	// 철혈의 분쇄자: 질풍노도(이번 적 턴 피해 절반) → 폭풍 질주(다음 턴 첫 합), 재사용 대기, 반격
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bGaleGuard = false;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bStormPending = false; // 다음 턴에 폭풍 질주
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bStormReady = false;   // 이번 턴 첫 합이 폭풍 질주
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") int32 GaleCooldown = 0;

	// 공통 '이번 적 턴' 버프: 반격, 회피 +, 방어 +
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") int32 Counter = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") float EvadeBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") int32 DefenseBonus = 0;

	// 공통 '이번 턴': 치명타 +, 합 불가, 이동하지 않음
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") float TurnCritBonus = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bNoDuelThisTurn = false;
	UPROPERTY(BlueprintReadOnly, Category = "Lifestyle") bool bMovedThisTurn = false;
};

// 카드 조건부 보너스를 계산할 때 넘기는 지금 상황
struct FBWConditionContext
{
	bool bRaging = false;
	int32 Wounds = 0;
	int32 LostHP = 0;
	bool bTargetLow = false;
	bool bMoved = false;
	int32 Dark = 0;
	int32 Block = 0;
	bool bLiberated = false;
	bool bTenebris = false;
};

UCLASS()
class BEYONDWING_API UBWLifestyleRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// --- 수치 (밸런스기획 3장) ---
	static constexpr float RageHPFraction = 0.6f;     // 광기: HP 60% 이하
	static constexpr float RageMinPerPHA = 0.2f;
	static constexpr float RageMaxPerPHA = 0.35f;
	static constexpr float WillHealFraction = 0.25f;
	static constexpr float UnkillableHealFraction = 0.15f;
	static constexpr int32 RageWillUses = 2;
	static constexpr float RageGuardMultiplier = 0.1f;
	static constexpr int32 MagicMaxPerShot = 2;       // 마탄 횟수 1당 최댓값 +2
	static constexpr int32 SeventhShot = 7;
	static constexpr float SeventhDamageMultiplier = 3.0f;
	static constexpr int32 PossessedPenaltyPerShot = 3;
	static constexpr int32 LiberationNeeded = 3;
	static constexpr float LiberatedSpellMultiplier = 1.4f;
	static constexpr int32 LiberatedMaxMPBonus = 10;
	static constexpr int32 ExplosionExtraMPCap = 10;
	static constexpr float ExplosionPerMP = 0.08f;
	static constexpr int32 TenebrisDarkNeeded = 3;
	static constexpr int32 TenebrisMaxPerDark = 3;     // [수정] 2→3 (자해가 커진 만큼 강림의 대가로 세게)
	static constexpr int32 TenebrisSelfDamagePerDark = 3; // [수정] 1→3 (회복해도 딜하려면 HP를 크게 잃게)
	static constexpr float TenebrisLifesteal = 0.3f;
	static constexpr int32 TenebrisRange = 3;
	static constexpr float HolyLowHPBonusPerFTH = 0.25f;
	static constexpr int32 IronBloodBlock = 1;       // 철혈: 이기면 방어도 +1
	static constexpr float GaleGuardMultiplier = 0.5f;
	static constexpr int32 GaleCooldownTurns = 2;

	// 광전사 광기 (HP 비율로)
	static bool IsRaging(int32 CurrentHP, int32 MaxHP);
	// 광기 보너스 (피의 복수면 3배)
	static FBWDiceRange RageBonus(int32 PHA, bool bBloodRevenge);
	// 광기 중 회복 카드는 절반 (불사·철의 의지 제외)
	static int32 ScaleCardHeal(int32 Amount, bool bRaging);
	// 철의 의지: 상처 3당 최소 +1·최대 +2
	static FBWDiceRange WillBonus(int32 Wounds);

	// 마탄: 쏘기 전 마탄 횟수로 최댓값 보너스, 이번 마탄이 몇 번째인지로 피해 배율
	static int32 MagicMaxBonus(int32 MagicCountBefore, bool bDouble);
	static float MagicDamageMultiplier(int32 ShotNumber);

	// 치유 카드의 회복량과 쌓이는 어둠 (HP 50% 이하면 FTH×0.25 더, 어둠 +2)
	static int32 HolyHealAmount(const FBWCardData& Card, int32 FTH, int32 CurrentHP, int32 MaxHP, int32& OutDarkGain);
	// 테네브리스에서 치유 카드가 될 공격카드 (범위 = 치유량의 절반~치유량)
	static FBWCardData MakeTenebrisAttack(const FBWCardData& HealCard, int32 FTH);
	static FBWCardData RestoreFromTenebris(const FBWCardData& Card);

	// 폭발: 더 거는 MP와 그 배율
	static int32 ExplosionExtraMP(int32 MPAfterCost);
	static FBWDiceRange ApplyExplosion(const FBWDiceRange& Range, int32 ExtraMP);

	// 카드 조건부 보너스 (상황 값을 넘겨받는다)
	static FBWDiceRange ApplyCondition(const FBWDiceRange& Range, EBWCardCondition Condition, int32 Value, const FBWConditionContext& Context);

	// 상태 한 줄 (머리 위 표시): "상처 12 · 광기", "장전 · 마탄 3", "해방 2/4", "어둠 3 · 테네브리스", "폭풍 질주"
	static FString Describe(EClassType ClassType, const FBWLifestyleState& State, bool bRaging);
};

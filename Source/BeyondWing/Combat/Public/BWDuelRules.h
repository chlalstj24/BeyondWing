#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWDuelRules.generated.h"

// 합 한쪽의 굴림 조건 (숫자 범위, 치명타, 피해 배율)
USTRUCT(BlueprintType)
struct FBWDuelSide
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") FBWDiceRange SkillRange = FBWDiceRange(1, 4);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float CritChance = 0.0f;     // 0~1
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float CritMultiplier = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float DamageMultiplier = 1.0f; // 이 쪽이 주는 피해 배율 (겔레온 1.5 등)

	// [추가] 이 쪽이 지면 받는 피해 배율 (취약 1.25). 5-3 3단계에서 이긴 쪽 배율과 함께 곱한 뒤 내림
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") float IncomingDamageMultiplier = 1.0f;

	// [추가] 혼란: 두 번 굴려 낮은 값 / 집중: 두 번 굴려 높은 값 (둘 다면 상쇄되어 한 번)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") bool bConfused = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") bool bFocused = false;

	// [추가] 이 쪽이 이겨 피해를 주면 상대에게 거는 상태이상
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") EStatusEffect AppliedStatus = EStatusEffect::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") int32 StatusDuration = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") int32 StatusValue = 0;

	// [추가] 이 쪽이 이기면 상대를 1칸 민다 (밀기 효과가 붙은 스킬)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") bool bPushesTarget = false;

	// [추가] 증강 '재굴림': 이 쪽(선공)이 지면 자기 굴림을 한 번 다시 굴린다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") int32 LosingRerolls = 0;
};

// 스킬 숫자 범위 계산에 쓰는 스탯 (ATK는 PHA + TEC로 파생)
USTRUCT(BlueprintType)
struct FBWSkillStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") int32 PHA = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") int32 TEC = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") int32 INT = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") int32 FTH = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel") int32 MaxHP = 0;
};

// 내가 받는 피해에 적용하는 직업별 방어 (전투 기획서 7장). 적에게는 없다
USTRUCT(BlueprintType)
struct FBWDefenseStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense") EDefenseType Type = EDefenseType::Flat;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense") int32 FlatReduction = 0;       // 방어: 받는 피해 고정 감소
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense") float EvadeChance = 0.0f;      // 회피: 0~1, 상한 MaxEvadeChance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense") int32 MagicShieldMaxMP = 5;    // 매직실드: 한 번 가드에 쓸 수 있는 MP 상한
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defense") float ShieldPerMP = 2.0f;      // [추가] 매직실드: MP 1당 막는 피해 (내림)
};

// 방어 적용 결과
USTRUCT(BlueprintType)
struct FBWDefenseResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Defense") int32 DamageAfterDefense = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Defense") bool bEvaded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Defense") int32 MPSpent = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Defense") int32 ShieldBlocked = 0; // [추가] 매직실드가 실제로 막은 피해
};

// 직업별 전투 수치 (전투 기획서 8장). 속도는 UBWGameInstance::ClassSpeedDice가 맡는다
USTRUCT(BlueprintType)
struct FBWClassCombatProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") FBWDefenseStats Defense;

	// INT·FTH: 전투 시작 MP, 턴 시작마다 회복량(첫 턴 제외), 최대 MP
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") bool bUsesMP = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 StartMP = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 MPPerTurn = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 MaxMP = 0;

	// HP: 전투 시작 방어도와 턴 종료마다 쌓이는 방어도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 StartBlock = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 BlockPerTurnEnd = 0;
};

// 직업을 고를 때 받는 시작 능력치 (기획서에 수치가 없어 임시: 주 능력치 8, 나머지 4)
USTRUCT(BlueprintType)
struct FBWClassStartStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 PHA = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 TEC = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 INT = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 FTH = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class") int32 MaxHP = 100;
};

// 합(숫자 대결) 규칙. 상태가 없고, 모든 무작위는 넘겨받은 FRandomStream에서 뽑는다 (같은 시드 = 같은 결과)
UCLASS()
class BEYONDWING_API UBWDuelRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 3번 연속 동점이면 둘 다 피해 없이 합이 끝난다
	static constexpr int32 MaxConsecutiveTies = 3;

	// 6-2: 최소 = 기본 최소 + 스탯 항 × 0.5, 최대 = 기본 최대 + 스탯 항 (내림). 최대는 기본 최대의 3배까지, 최소는 최대 이하
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static FBWDiceRange ComputeCardSkillRange(const FBWCardData& Card, const FBWSkillStats& Stats);

	// 6-3: 치명타 확률 5% + TEC × 0.5% (상한 50%), 배수 1.5 + TEC × 0.01 (상한 2.5)
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static void ComputeCritFromTEC(int32 TEC, float& OutChance, float& OutMultiplier);

	UFUNCTION(BlueprintCallable, Category = "BW Duel")
	static int32 RollRange(const FBWDiceRange& Range, UPARAM(ref) FRandomStream& Stream);

	// 합 한쪽의 굴림. 혼란이면 두 번 굴려 낮은 값, 집중이면 높은 값, 둘 다면 한 번
	UFUNCTION(BlueprintCallable, Category = "BW Duel")
	static int32 RollSide(const FBWDuelSide& Side, UPARAM(ref) FRandomStream& Stream);

	// 최댓값에 배율을 곱해 내림, 최솟값은 최댓값 이하 (파트로바스, 중량타 등)
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static FBWDiceRange ScaleRangeMax(const FBWDiceRange& Range, float Multiplier);

	// 5-1: 양쪽이 굴려 큰 쪽이 이기고, 이긴 쪽 숫자가 5-3의 1~3단계(치명타, 배율)를 거쳐 피해가 된다
	UFUNCTION(BlueprintCallable, Category = "BW Duel")
	static FBWDuelResult ResolveDuel(const FBWDuelSide& Attacker, const FBWDuelSide& Defender, UPARAM(ref) FRandomStream& Stream);

	// 5-2: 적이 선공인 합. 적이 굴린 숫자가 그대로 피해가 되고 나는 카드를 내지 않는다 (Target의 받는 피해 배율은 적용)
	UFUNCTION(BlueprintCallable, Category = "BW Duel")
	static FBWDuelResult ResolveEnemyStrike(const FBWDuelSide& Enemy, const FBWDuelSide& Target, UPARAM(ref) FRandomStream& Stream);

	// 5-3 1~3단계: 굴림 N → 치명타 → (이긴 쪽 배율 × 진 쪽 받는 피해 배율) → 내림 (0 미만은 0)
	UFUNCTION(BlueprintCallable, Category = "BW Duel")
	static int32 ApplyDamageModifiers(int32 Roll, const FBWDuelSide& Winner, UPARAM(ref) FRandomStream& Stream, bool& bOutCritical, float LoserIncomingMultiplier = 1.0f);

	// [수정] 언제나 쓸 수 있는 삶의 방식 고유의 기본 공격 (DT_Classes가 없을 때의 기본값).
	// PHA·TEC·HP는 주 스탯 비례 추가 피해가 붙고, INT·FTH는 약한 대신 카드 강화 배율이 크다 (MakeClassCardPower)
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static FBWCardData MakeBasicStrike(EClassType ClassType);

	// [추가] 공격카드가 기본 공격에 더해 주는 몫의 배율 (DT_Classes가 없을 때의 기본값): PHA·TEC 0.5, HP 0.8, INT·FTH 1.3
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static float MakeClassCardPower(EClassType ClassType);

	// [추가] 카드가 더해 주는 몫 = 카드 숫자 범위 × 배율 (각각 내림, 최소는 최대 이하)
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static FBWDiceRange ScaleCardBoost(const FBWDiceRange& CardRange, float CardPower);

	// [추가] 카드로 강화한 공격 = 기본 공격 + 카드 몫
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static FBWDiceRange AddCardBoost(const FBWDiceRange& BasicRange, const FBWDiceRange& CardRange, float CardPower);

	// 8장 직업별 수치: INT·FTH는 MP 시작 6, 턴마다 +3, 최대 20. HP는 전투 시작과 턴 종료마다 방어도 3
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static FBWClassCombatProfile MakeClassProfile(EClassType ClassType);

	// 새 판을 시작할 때 직업별 시작 능력치와 최대 HP
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static FBWClassStartStats MakeClassStartStats(EClassType ClassType);

	// 4-1 3단계: 턴 시작 MP. TurnNumber는 1부터 (첫 턴은 전투 시작 MP 그대로)
	UFUNCTION(BlueprintCallable, Category = "BW Duel")
	static void ApplyTurnStartResources(const FBWClassCombatProfile& Profile, int32 TurnNumber, UPARAM(ref) int32& InOutMP);

	// 4-4 3단계: 턴 종료 방어도
	UFUNCTION(BlueprintCallable, Category = "BW Duel")
	static void ApplyTurnEndResources(const FBWClassCombatProfile& Profile, UPARAM(ref) int32& InOutBlock);

	// 회피 확률 상한 60%
	static constexpr float MaxEvadeChance = 0.6f;

	// 7장 직업별 기본 방어: PHA 방어 2, HP 방어 3, TEC 회피 25%, INT·FTH 매직실드
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static FBWDefenseStats MakeClassDefense(EClassType ClassType);

	// 5-3 4단계: 내가 받는 피해에 방어(고정 감소) / 회피 / 매직실드 적용.
	// [수정] 매직실드는 자동이 아니라 가드에 걸기로 고른 MP(ShieldMP)만 쓴다: 고른 MP는 (상한·보유량 안에서) 모두 소모되고 MP 1당 ShieldPerMP만큼 막는다
	UFUNCTION(BlueprintCallable, Category = "BW Duel")
	static FBWDefenseResult ApplyDefense(int32 Damage, const FBWDefenseStats& Defense, UPARAM(ref) int32& InOutMP, UPARAM(ref) FRandomStream& Stream, int32 ShieldMP = 0);

	// [추가] 매직실드에 MP를 Spend만큼 걸었을 때 막는 피해
	UFUNCTION(BlueprintPure, Category = "BW Duel")
	static int32 GetShieldBlock(const FBWDefenseStats& Defense, int32 Spend);

	// 5-3 5단계: 방어도를 먼저 깎고 남은 만큼 HP. 방어 무시(타이가)면 방어도를 건너뛴다
	UFUNCTION(BlueprintCallable, Category = "BW Duel")
	static void ApplyDamageToBlockAndHP(int32 Damage, bool bIgnoreBlock, UPARAM(ref) int32& InOutBlock, UPARAM(ref) int32& InOutHP);
};

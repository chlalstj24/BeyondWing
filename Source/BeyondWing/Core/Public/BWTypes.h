#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BWTypes.generated.h"

USTRUCT(BlueprintType)
struct FBWTooltipConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	bool bEnableTooltip = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip", meta = (EditCondition = "bEnableTooltip"))
	FString Title = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip", meta = (EditCondition = "bEnableTooltip", MultiLine = true))
	FString Description = TEXT("");
};

// [추가] 전투 안내 문구의 종류 (화면 위쪽 안내판의 색과 효과가 달라진다)
UENUM(BlueprintType)
enum class EBWPromptStyle : uint8
{
	Info,     // 다음에 할 일 (금빛)
	Highlight,// 눈여겨볼 일: 적과 맞닿음, 턴 시작 (밝은 청록, 살짝 커졌다 돌아옴)
	Warning,  // 할 수 없는 일 (붉은빛, 흔들림)
	Result    // 합 결과 같은 알림 (흰빛)
};

UENUM(BlueprintType)
enum class ETileState : uint8
{
	None,
	Hovered,
	MoveRange,
	AttackRange,
	EnemyAttackRange,
	AttackTarget,
	AllyZone,
	EnemyZone,
	Wall, // [추가] 베렌드 벽 밀치기용 장애물 타일
	EnemyMovePath, // [추가] 적이 이번 턴 돌진할 경로 (내 이동 범위 파란색과 구분)
	EnemyUnit,     // [추가] 적이 서 있는 칸
	PlayerUnit     // [추가] 내 말이 서 있는 칸
};

UENUM(BlueprintType)
enum class ESkillShape : uint8
{
	Single, Manhattan, Cross, T_Shape, Line, HorizontalLine, VerticalLine,
	Square3x3, Square5x5, FrontSquare3x3, Ring, CrossDiagonal, Cone, Arrowhead,
	Checkerboard, Surround, FrontRow, BackRow, AllEnemies, AllAllies, Random, Self
};

UENUM(BlueprintType)
enum class EClassType : uint8
{
	Common, PHA, INT, FTH, TEC, HP
};

UENUM(BlueprintType)
enum class EBWEnemyActionType : uint8
{
	Attack, Heal, BuffAttack
};

// [추가] 카드 종류: 공격카드는 합을 걸 때만, 효과카드는 전투 중·지도에서 사용
UENUM(BlueprintType)
enum class ECardKind : uint8
{
	Attack, Effect
};

// [추가] 직업별 방어 방식 (PHA·HP 고정 감소 / TEC 회피 / INT·FTH 매직실드)
UENUM(BlueprintType)
enum class EDefenseType : uint8
{
	Flat, Evade, MagicShield
};

// [추가] 상태이상 7종
UENUM(BlueprintType)
enum class EStatusEffect : uint8
{
	None,
	Stun,       // 기절: 다음 턴 이동·합 불가
	Slow,       // 둔화: 속도 최댓값 -1
	Weak,       // 약화: 스킬 최댓값 -20%
	Vulnerable, // 취약: 받는 피해 +25%
	Bleed,      // 출혈: 턴 종료마다 고정 피해
	Confusion,  // 혼란: 두 번 굴려 낮은 값
	Focus       // 집중: 두 번 굴려 높은 값
};

// [추가] 말에 걸린 상태이상 하나 (같은 종류는 하나만, 다시 걸리면 지속 턴 갱신)
USTRUCT(BlueprintType)
struct FBWStatusInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status") EStatusEffect Type = EStatusEffect::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status") int32 RemainingTurns = 0; // 걸린 말의 남은 자기 턴 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status") int32 Value = 0;          // 출혈 피해량 등
};

// [추가] 증강 효과 종류 (전투 기획서 10장)
UENUM(BlueprintType)
enum class EBWAugmentEffect : uint8
{
	SpeedMaxUp,         // 질주
	SpeedMinUp,         // 신중한 발걸음
	DuelReroll,         // 재굴림
	StartBlock,         // 단단한 껍질
	HandSizeUp,         // 넉넉한 손
	EffectCardLimitUp,  // 연계
	AttackMaxUp,        // 중량타 (PHA)
	CritChanceUp,       // 급소 (TEC)
	MPRegenUp,          // 마나 샘 (INT)
	ThornArmor,         // 가시 갑옷 (HP)
	ReversedHymn,       // 역전된 성가 (FTH)
	SelfBlessing,       // 자기 축복 (FTH)
	DoubleOpening       // 이중 개시 (3막 전용)
};

// [추가] 정수 굴림 범위 (속도, 스킬 숫자 범위 공용)
USTRUCT(BlueprintType)
struct FBWDiceRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice") int32 Min = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dice") int32 Max = 3;

	FBWDiceRange() {}
	FBWDiceRange(int32 InMin, int32 InMax) : Min(InMin), Max(InMax) {}
};

// [추가] 합 한 번의 결과
USTRUCT(BlueprintType)
struct FBWDuelResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Duel") bool bAttackerWon = false;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") bool bDraw = false; // 3연속 동점으로 피해 없이 종료
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 AttackerRoll = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 DefenderRoll = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 TieRerolls = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") bool bCritical = false;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 FinalDamage = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Duel") bool bAttackerRerolled = false; // [추가] 재굴림 증강을 썼는지
};

// [추가] 증강 데이터 테이블 행
USTRUCT(BlueprintType)
struct FBWAugmentData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") FString AugmentName = TEXT("");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment", meta = (MultiLine = true)) FString Description = TEXT("");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") EBWAugmentEffect Effect = EBWAugmentEffect::SpeedMaxUp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") float Value = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") EClassType ClassType = EClassType::Common; // Common = 공용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") int32 MinAct = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") int32 MaxAct = 3;
};

USTRUCT(BlueprintType)
struct FBWEnemyPattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	EBWEnemyActionType ActionType = EBWEnemyActionType::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString ActionName = TEXT("기본 행동"); // [추가] 툴팁용 이름

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString Description = TEXT("피해를 {V} 줍니다."); // [추가] 툴팁용 템플릿 문구 ({V}, {D}, {R} 지원)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 Value = 10;

	// [추가] 합에서 굴리는 숫자 범위
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Duel")
	FBWDiceRange SkillRange = FBWDiceRange(1, 4);

	// [추가] 이 스킬이 거는 상태이상과 지속 턴(기본 2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Duel")
	EStatusEffect AppliedStatus = EStatusEffect::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Duel")
	int32 StatusDuration = 2;

	// [추가] 출혈처럼 수치가 있는 상태이상의 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Duel")
	int32 StatusValue = 0;

	// [추가] 합에서 이기면 상대를 1칸 민다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Duel")
	bool bPushesTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	ESkillShape SkillShape = ESkillShape::Single;

	// [추가] 시각 및 효과음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	TSoftObjectPtr<class UParticleSystem> ActionEffect = nullptr;

	// [추가] Niagara 이펙트 (있으면 ActionEffect 대신 쓴다). 맞는 쪽 칸에서 터진다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	TSoftObjectPtr<class UNiagaraSystem> ActionNiagara = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<class USoundBase> ActionSound = nullptr;
};

USTRUCT(BlueprintType)
struct FBWEnemyTurnPattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TArray<FBWEnemyPattern> Actions;
};

USTRUCT(BlueprintType)
struct FBWEnemyData : public FTableRowBase
{
	GENERATED_BODY()

	// [최종] 데이터 테이블 칸 순서와 1:1 대응 (오프셋 꼬임 방지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EnemyDisplayName = TEXT("Default Enemy");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class ABWCharacter> EnemyClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<class USkeletalMesh> EnemyMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<class UTexture2D> Portrait = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHP = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TArray<FBWEnemyTurnPattern> CombatPatterns;

	// [추가] 전투 시작 방어도(추가 체력)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 StartingBlock = 0;

	// [추가] 속도 (1막 일반 1~3, 엘리트는 최댓값 +1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FBWDiceRange SpeedDice = FBWDiceRange(1, 3);
};

USTRUCT(BlueprintType)
struct FBWCardData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") FString CardName = TEXT("Unnamed Card");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") FString Description = TEXT("");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") float BaseDamage = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") FString SubStatName = TEXT("");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") FString SubStatValue = TEXT("");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") ECardKind CardKind = ECardKind::Attack; // [추가]
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") int32 SkillMin = 1; // [추가] 합 숫자 범위 기본 최소
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") int32 SkillMax = 4; // [추가] 합 숫자 범위 기본 최대
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") int32 MPCost = 0; // [추가] INT·FTH 마법 카드만 사용
	// [추가] 합에서 이겨 피해를 주면 상대에게 거는 상태이상 (지속 기본 2턴, 출혈은 Value가 피해량)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Status") EStatusEffect AppliedStatus = EStatusEffect::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Status") int32 StatusDuration = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Status") int32 StatusValue = 0;
	// [추가] 합에서 이기면 상대를 1칸 민다 (베렌드에서는 벽에 닿으면 추가 피해 + 기절)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Status") bool bPushesTarget = false;

	// [추가] 효과카드: 고르는 즉시 나에게 적용된다 (6-1 회복, 강화, 이동 보조)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Effect") int32 EffectHeal = 0;            // HP 회복
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Effect") int32 EffectBlock = 0;           // 방어도 획득
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Effect") int32 EffectMove = 0;            // 이번 턴 남은 이동 +
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Effect") int32 EffectMP = 0;              // MP 획득
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Effect") int32 EffectDraw = 0;            // 카드 뽑기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Effect") int32 EffectAttackMaxBonus = 0;  // 이번 턴 공격카드 최댓값 +
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Effect") EStatusEffect SelfStatus = EStatusEffect::None; // 나에게 거는 상태 (집중 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Effect") int32 SelfStatusDuration = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") TSoftObjectPtr<UTexture2D> AboveImage = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") TSoftObjectPtr<UTexture2D> BodyImage = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") TSoftObjectPtr<UTexture2D> BelowImage = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") ESkillShape SkillShape = ESkillShape::Single;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") int32 Range = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") ETileState DisplayColor = ETileState::AttackRange;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") EClassType ClassType = EClassType::Common;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") TSoftObjectPtr<USoundBase> CardSound = nullptr;
	
	// [추가] 시각 효과
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Visual")
	TSoftObjectPtr<class UParticleSystem> SkillEffect = nullptr;

	// [추가] Niagara 이펙트 (있으면 SkillEffect 대신 쓴다). 공격카드는 진 쪽 칸, 효과카드는 내 칸
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Visual")
	TSoftObjectPtr<class UNiagaraSystem> NiagaraEffect = nullptr;

	// [수정] 멀티 스탯 스케일링 지원
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float PHAScale = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float TECScale = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float INTScale = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float FTHScale = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float HPScale = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float ATKScale = 0.0f;

};
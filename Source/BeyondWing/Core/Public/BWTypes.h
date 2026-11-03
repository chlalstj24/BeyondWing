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
	PlayerUnit,    // [추가] 내 말이 서 있는 칸
	ManaLine       // [추가] 파트로바스 마력선
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
	Attack, Heal, BuffAttack,
	Block,  // 효과: 방어도 Value (제자리)
	Hex,    // 효과: 사거리 안의 나에게 상태이상만 건다 (피해 없음)
	Summon, // 효과: 호위를 불러낸다 (FBWEnemyData.SummonEnemy)
	Chant,  // 효과: 영창. 다음 카드가 나오기 전에 피해를 입으면 다음 카드가 사라진다
	Devour, // 효과: 호위를 삼켜 HP Value 회복. 호위가 없으면 공격
	Reckoning // 공격: 먹은 카드 1장당 범위 +Value
};

// [추가] 내 턴에 공격받았을 때 적이 내는 카드
UENUM(BlueprintType)
enum class EBWEnemyCounterKind : uint8
{
	Attack, // 맞굴림, 진 쪽이 맞는다
	Guard   // 맞굴림, 내가 이겨도 피해에서 적이 굴린 수만큼 깎이고, 내가 지면 아무 일도 없다
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

// [추가] 증강 종류 (증강기획 1-1). 후보 3개는 종류별 하나씩
UENUM(BlueprintType)
enum class EBWAugmentKind : uint8 { Stable, Change, Gamble };

// [추가] 증강 세트 (증강기획 1-4)
UENUM(BlueprintType)
enum class EBWAugmentSet : uint8
{
	None, Astrin, Sylpira, Soltheris, Urska, Lirin, Lunaris,
	Berend, Taiga, Geleon, Patrovas, Megenberta, Radiant,
	Lifestyle, // 삶의 방식 전용 증강. 효과는 삶의 방식마다 다르다
	Count UMETA(Hidden)
};

// [추가] 증강이 바꾸는 값. 한 증강이 여러 개를 가질 수 있고 값은 모두 더해진다 (DamageMult만 곱)
UENUM(BlueprintType)
enum class EBWAugStat : uint8
{
	// 전투 시작·항상
	SpeedMax, SpeedMin, SpeedMinIsOne, StartBlock, StartHeal, StartMP, MaxMP, FirstDraw, HandMax, TurnDraw, BattleDraw,
	EffectLimit, AttackLimit, DuelLimit, Rerolls, MaxHPFlat, MaxHPPct, DefenseUp, NoDefense, EffectNoMP, NoEffectCards, MPPerTurn,
	// 턴
	TurnHeal, TurnHPLoss, TurnEndBlock, EvenTurnBlock, FirstTurnFocus, FirstTurnMove, FirstTurnEffect, NoContactBlock,
	// 공격 범위
	AttackMin, AttackMax, AttackMaxPct, BasicMin, BasicMax, BasicMul, MeleeMin, RangedMin, RangedMax, CardMaxPct, MinIsOne,
	FullHPMax, LowHPMinMax, FirstDuelMinMax, FirstCardMax, BehindMin, NoMoveMinMax, MovedMaxPerTile, MoveLeftMin, LowHandMax,
	StatusedMin, PerStatusMax, NoDebuffMin, OddTurnMax, GoldMinPer100, NoEffectMax, Early3MinMax, Late4MinMax, Block10Max,
	TurnFirstFocus, HealAttackMax, EffectMaxPer, TieWin, MaxRoll, RangedNeverLose, ReachUp, RangedReachUp,
	// 피해
	DamagePct, DamageMult, WinPerLostHP, RangedDamagePct, EarlyDamagePct, LastEnemyPct, TakenPct, LoseTakenPct, CrowdTakenPct, FlatTaken, CritChance, CritMul, RangedCrit, AlwaysCrit, FirstAttackCrit, EnemyMax, EnemyMaxPct,
	// 격돌 뒤
	Lifesteal, WinBleed, WinVuln, WinSlow, MeleeWinBlock, AfterDuelMove, KillHeal, KillMove, KillDraw, KillRefund, Thorn, ZeroHitFixed, ZeroHitDraw, MirrorHit,
	// 상태·지형
	NoConfuse, DebuffImmune, DebuffShort, BleedTakenDown, StunGuard, WeakVulnSoft, VulnSoft, ThornImmune, ThicketImmune, WallImmune, PushImmune, EnemiesConfused,
	// 회복·자해
	HealPct, NoHeal, NoCardHeal, OverhealBlock, HealToBlock, HealToAoE, SelfDamageDown, SelfDamageBlock, SelfDamageHeals, BlockNoLoss, TwoEffectDraw, LastStand, Last30Heal,
	// 세트 빠진 효과·삶의 방식 핵심
	FirstCritX2, EffectThreeMax, IronBloodUp, BlockToBasic, LiberationDown, ShieldPerUp,
	// 지도
	WinHeal, EliteHeal, RestPct, NoRestHeal, RestMaxHP, CardChoices, ShopPct, MoveGold,
	// 규칙을 바꾸는 증강
	FistCharge, ReturnCard, DuelKeepMove, BlockBurst, MoveSlash, ChainHit, WinExtraDuel,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FBWAugmentMod
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") EBWAugStat Stat = EBWAugStat::AttackMax;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") float Value = 0.0f;
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
	UPROPERTY(BlueprintReadOnly, Category = "Duel") bool bGuardHeld = false; // 방어 카드에 막혀 아무 일도 없었다 (bDraw도 켜짐)
	UPROPERTY(BlueprintReadOnly, Category = "Duel") int32 GuardReduced = 0;  // 이겼지만 방어 카드에 깎인 피해
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
	// [추가] 개편안: 종류·세트·빌드 이름(전용 증강)·대가 문구·효과 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") EBWAugmentKind Kind = EBWAugmentKind::Stable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") EBWAugmentSet Set = EBWAugmentSet::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") FString BuildName = TEXT("");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") FString CostText = TEXT("");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") TArray<FBWAugmentMod> Mods;
	// [추가] 증강 선택 카드의 아이콘 (비면 이름 첫 글자를 보여 준다)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment") TSoftObjectPtr<class UTexture2D> Icon;
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

	// [추가] 내 턴에 공격받으면 내는 반격 카드. 범위가 비어 있으면(최댓값 0) 행동 카드의 범위로 공격 반격한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Counter")
	EBWEnemyCounterKind CounterKind = EBWEnemyCounterKind::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Counter")
	FString CounterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Counter")
	FBWDiceRange CounterRange = FBWDiceRange(0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Counter")
	EStatusEffect CounterStatus = EStatusEffect::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Counter")
	int32 CounterStatusDuration = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Counter")
	int32 CounterStatusValue = 0;

	// [추가] 방어 반격으로 막아 내면 그 자리에서 주는 피해
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Counter")
	int32 CounterRiposte = 0;

	// [추가] 이 공격이 맞으면 손패의 카드 1장을 이번 전투에서 먹는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bEatsCard = false;

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

	// [추가] 손을 얹으면 나오는 설명
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EnemyDescription;

	// [추가] 적 모델. 있으면 BP의 기본 메시를 숨기고 이 모델을 ModelHeight 높이로 세운다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
	TSoftObjectPtr<class UStaticMesh> ModelMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
	float ModelHeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
	float ModelYaw = 0.0f;

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

	// [추가] '강화 후 공격'으로 쌓이는 값의 상한 (기준 적 표)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxAttackBuff = 99;

	// [추가] 속도 (1막 일반 1~3, 엘리트는 최댓값 +1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FBWDiceRange SpeedDice = FBWDiceRange(1, 3);

	// [추가] 행동 카드의 사거리. 2 이상이면 그 거리에 맞춰 멈추고 더 다가오지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 ActionRange = 1;

	// [추가] 내가 이 거리 안에 있으면 물러난다 (0이면 도망가지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 KeepAwayBelow = 0;

	// [추가] 보스: HP가 이 비율 이하로 떨어지면 패턴을 Phase2Patterns로 바꾸고 방어도를 얻는다 (0이면 없음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Boss")
	float Phase2HPFraction = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Boss")
	TArray<FBWEnemyTurnPattern> Phase2Patterns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Boss")
	int32 Phase2Block = 0;

	// [추가] Summon 행동으로 부르는 적 (적 표의 행 이름)과 동시에 살아 있을 수 있는 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Boss")
	FName SummonEnemy = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Boss")
	int32 MaxSummons = 2;

	// [추가] 3층 기믹
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Boss") float Phase3HPFraction = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Boss") TArray<FBWEnemyTurnPattern> Phase3Patterns;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Boss") int32 Phase3Block = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") float ReviveFraction = 0.0f;  // 미회수: 쓰러지면 다음 적 턴에 이 비율로 일어난다 (1회)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") int32 TurnSelfDamage = 0;     // 수명·거래: 자기 턴 끝에 HP를 잃는다
	// 격노: 자기 턴을 EnrageTurn번 치른 뒤부터 턴마다 공격 +EnrageBuff (상한 없음, 소모전을 끝낸다. 0이면 없음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") int32 EnrageTurn = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") int32 EnrageBuff = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") int32 TurnSelfBuff = 0;       // 거래: 자기 턴 끝에 공격 +
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") int32 GoldSteal = 0;          // 맞히면 골드를 훔친다. 쓰러뜨리면 돌려받는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") bool bBreaksObstacles = false; // 돌진 경로의 장애물을 부순다 (부술 때마다 공격 +BreakBuff)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") int32 BreakBuff = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") bool bMirrorCounter = false;  // 본뜨기: 공격 반격의 범위가 내 공격과 같다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") float LastWishFraction = 0.0f; // 마지막 기원: 이 비율 아래에서 받는 피해 절반, LastWishTurns 턴 뒤 쓰러짐
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Gimmick") int32 LastWishTurns = 3;
};

// [추가] 카드 등급: 시작 카드는 보상에 나오지 않고, 일반·고급·희귀 순으로 보상에 드물게 나온다
UENUM(BlueprintType)
enum class EBWCardRarity : uint8
{
	Common, Uncommon, Rare, Starter
};

// [추가] 삶의 방식 고유 규칙에 묶인 카드 (삶의방식카드기획 3~7장)
UENUM(BlueprintType)
enum class EBWCardSpecial : uint8
{
	None,
	Shot,          // 마탄의 사수: 이 카드로 강화한 합에서 이기면 장전
	MagicBullet,   // 마탄의 사수: 장전이 있어야 쓴다. 마탄 횟수 +1
	Load,          // 마탄의 사수: 바로 장전
	ClearPossess,  // 마탄의 사수: 들림 제거
	Meditate,      // 매직 캐스터: 이번 턴 이동 0, 해방 +1 (+SpecialValue)
	Chant,         // 매직 캐스터: 다음 턴 첫 마법 최댓값 +SpecialValue (피해를 받으면 깨짐)
	Explosion,     // 매직 캐스터: 남은 MP를 최대 10까지 더 걸고 1당 +8%
	HolyHeal,      // 어둠의 성자: 치유 (EffectHeal + FTH×FTHScale). 어둠이 쌓이고, 테네브리스에서는 공격카드가 된다
	Descend,       // 어둠의 성자: 어둠 3 이상이면 테네브리스에 들어가거나 나온다
	Gale,          // 철혈의 분쇄자: 질풍노도 (이번 턴 합 불가, 받는 피해 절반, 다음 턴 폭풍 질주)
	Storm,         // 철혈의 분쇄자: 폭풍 질주 중이면 이 카드 범위도 두 배
	WillOfIron,    // 광전사: 상처 3당 최소 +1·최대 +2, 합 뒤 상처 0·HP 25% 회복·출혈 제거
	Unkillable,    // 광전사: 최대 HP 15% 회복 (광기 감소 없음), 출혈 제거
	Rage           // 광전사: 철의 의지 2번 뒤 전투당 1번, 이번 적 턴 받는 피해 -90%
};

// [추가] 카드의 조건부 보너스 (ConditionValue와 함께)
UENUM(BlueprintType)
enum class EBWCardCondition : uint8
{
	None,
	Rage,            // 광기면 최소 +값
	WoundsMax,       // 상처 5당 최댓값 +값
	LostHPMax,       // 잃은 HP 10당 최댓값 +값
	EnemyLowHP,      // 대상 HP 30% 이하면 최소·최대 +값
	NoMoveThisTurn,  // 이번 턴 이동하지 않았으면 최댓값 +값
	DarkMax,         // 어둠 1당 최댓값 +값
	BlockMax,        // 최댓값 + 현재 방어도, 합 뒤 방어도 절반
	Liberated,       // 해방 중이면 최소·최대 +값
	TenebrisDouble,  // 테네브리스면 최소·최대 두 배
	MagicDouble      // 마탄 횟수 보너스 두 배
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

	// --- [추가] 삶의 방식 카드 (삶의방식카드기획) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") EBWCardRarity Rarity = EBWCardRarity::Common;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") EBWCardSpecial Special = EBWCardSpecial::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 SpecialValue = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") EBWCardCondition Condition = EBWCardCondition::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 ConditionValue = 0;
	// 자해: 쓰는 순간 고정 피해 (방어 무시, 방어도는 깎임). 지면 자해: 이 카드로 강화한 합에서 지면
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 SelfDamage = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 LoseSelfDamage = 0;
	// 공격카드: 이 카드로 강화한 합에서 이기면
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 WinHeal = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 WinBlock = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 WinMP = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 WinDraw = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 WinDark = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") float WinLifesteal = 0.0f; // 준 피해의 비율만큼 회복
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") float CritChanceBonus = 0.0f;  // 공격카드: 이 합의 치명타 확률 +, 효과카드: 이번 턴 +
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") float CritMultiplierBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") bool bIgnoreTargetBlock = false;
	// 효과카드: 이번 적 턴 동안 (반격 n, 회피 +%, 방어 +n), 어둠 +n, 이번 턴 합 불가, 이번 턴 이동 끝, 나쁜 상태 제거
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 EffectCounter = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") float EffectEvadeBonus = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 EffectDefenseBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 EffectDark = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") bool bNoDuelThisTurn = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") bool bEndMoveThisTurn = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") bool bCleanse = false;
	// 공격카드가 쓰는 순간 나에게 거는 상태 (살육의 출혈 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Lifestyle") int32 SelfStatusValue = 0;

	// 테네브리스로 공격카드가 된 치유 카드 (원래 모습으로 되돌릴 때 쓴다, 저장하지 않음)
	bool bTenebrisForm = false;
	int32 OrigSkillMin = 0;
	int32 OrigSkillMax = 0;
	int32 OrigRange = 1;
	float OrigFTHScale = 0.0f;
};
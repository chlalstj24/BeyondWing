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
	EnemyZone
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 MoveDistance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	ESkillShape SkillShape = ESkillShape::Single;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 Range = 1;

	// [추가] 시각 및 효과음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	TSoftObjectPtr<class UParticleSystem> ActionEffect = nullptr;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BaseAttackPower = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TArray<FBWEnemyTurnPattern> CombatPatterns;
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card") int32 APCost = 1;
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

	// [수정] 멀티 스탯 스케일링 지원
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float PHAScale = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float TECScale = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float INTScale = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float FTHScale = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float HPScale = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") float ATKScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card|Combat") int32 MovePointsGained = 0;
};
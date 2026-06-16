#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWGameInstance.generated.h"

// 4개 국가(차원) 정의
UENUM(BlueprintType)
enum class EDimensionType : uint8
{
	None,
	Patrovas, Berend, Geleon, Taiga
};

// 노드 타입 정의
UENUM(BlueprintType)
enum class EMapNodeType : uint8
{
	Battle, Elite, Mystery, Rest, Shop, Chest, Boss
};

// 개별 몬스터의 배치 정보 (ID + 좌표)
USTRUCT(BlueprintType)
struct FEnemyPositionInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName EnemyID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Coordinate;
};

// 전투 조합(인카운터) 데이터 테이블 행 구조 (출현 목록)
USTRUCT(BlueprintType)
struct FEncounterData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Stage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDimensionType Dimension = EDimensionType::Patrovas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMapNodeType EncounterType = EMapNodeType::Battle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FEnemyPositionInfo> EnemyGroup;
};

USTRUCT(BlueprintType)
struct FEnemySpawnRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName EnemyID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Coordinate;
};

USTRUCT(BlueprintType)
struct FMapNode
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EMapNodeType NodeType = EMapNodeType::Battle;
	UPROPERTY(BlueprintReadOnly) EDimensionType Dimension = EDimensionType::None;
	UPROPERTY(BlueprintReadOnly) bool bIsVisited = false;
	int32 StepIdx = 0;
	int32 DimIdx = 0;
};

USTRUCT(BlueprintType)
struct FMapStep
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FMapNode> Nodes;
};

UENUM(BlueprintType)
enum class ERewardType : uint8
{
	Gold, CardSelection, Relic, CardRemoval, Heal, Equipment
};

USTRUCT(BlueprintType)
struct FRewardItem
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ERewardType Type = ERewardType::Gold;
	UPROPERTY(BlueprintReadOnly) int32 Amount = 0;
	UPROPERTY(BlueprintReadOnly) FString RewardName = "";
	UPROPERTY(BlueprintReadOnly) FString Description = "";
};

UCLASS()
class BEYONDWING_API UBWGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UBWGameInstance();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Database") UDataTable* EnemyDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Database") UDataTable* EncounterDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Progress") int32 CurrentStage = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Progress") int32 CurrentGold = 0;

	// 플레이어 영구 스탯
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerMaxHP = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerCurrentHP = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerPHA = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerINT = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerFTH = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerTEC = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerBaseMovePoints = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Player Stats") int32 PlayerDeckCount = 10;

	UFUNCTION(BlueprintCallable, Category = "BW Combat") void PrepareEncounter(EMapNodeType Type, EDimensionType Dim);
	FName FindEnemyIDByDisplayName(FString DisplayName);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Combat") TArray<FEnemySpawnRequest> PendingEnemyRequests;

	UPROPERTY(BlueprintReadOnly, Category = "BW Map") TArray<FMapStep> WorldMapData;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 CurrentStep = -1;
	UPROPERTY(BlueprintReadOnly, Category = "BW Map") int32 CurrentDimIdx = -1;
	UPROPERTY(BlueprintReadOnly, Category = "BW Combat") EMapNodeType CurrentEncounterType = EMapNodeType::Battle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Map") int32 VisibilityRange = 2;
	UPROPERTY(BlueprintReadOnly, Category = "BW Reward") TArray<FRewardItem> PendingRewards;

	UFUNCTION(BlueprintCallable, Category = "BW Map") void GetNodeVisibilityState(int32 Step, EMapNodeType Type, bool& bOutIsVisible, bool& bOutIsFogged);
	UFUNCTION(BlueprintCallable, Category = "BW Map") void GenerateWorldMap(int32 TotalSteps = 15);
	UFUNCTION(BlueprintCallable, Category = "BW Map") bool CanMoveToNode(int32 TargetStep, int32 TargetDimIdx);
	UFUNCTION(BlueprintCallable, Category = "BW Map") void SelectMapNode(int32 Step, int32 DimIdx);
	UFUNCTION(BlueprintCallable, Category = "BW Reward") void GenerateCombatRewards(EMapNodeType EncounterType);
	UFUNCTION(BlueprintCallable, Category = "BW Reward") void ClaimReward(int32 Index);

	UPROPERTY(BlueprintReadOnly, Category = "BW Stats") TMap<EDimensionType, int32> ConsecutiveCounts;
	UPROPERTY(BlueprintReadOnly, Category = "BW Stats") TMap<EDimensionType, int32> NeglectedCounts;
	void GetCountryStats(EDimensionType Dim, int32& OutConsecutive, int32& OutNeglected);

private:
	void ResetStats();
};
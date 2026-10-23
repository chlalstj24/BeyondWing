#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "GridManager.generated.h"

class AGridTile;
class ABWCharacter;

UCLASS()
class BEYONDWING_API AGridManager : public AActor
{
	GENERATED_BODY()
	
public:	
	AGridManager();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
	int32 Width;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
	int32 Height;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
	float TileSpacing;

	// [추가] 왼쪽부터 아군 배치 구역으로 쓰는 열 수 (나머지는 적 구역)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
	int32 AllyZoneColumns = 4;

	UFUNCTION(BlueprintPure, Category = "Grid Placement")
	bool IsAllyZoneColumn(int32 X) const { return X >= 0 && X < AllyZoneColumns; }

	// [추가] 켜면 칸의 X(아군→적)를 화면 가로(월드 Y)로 깐다. 기본은 꺼짐: 아군이 아래(카메라 쪽), 적이 위
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
	bool bColumnsAlongWorldY = false;

	// 칸 좌표 → 월드 위치
	UFUNCTION(BlueprintPure, Category = "Grid Placement")
	FVector GetTileWorldLocation(FIntPoint Coord) const;

	// 월드 위치 → 칸 좌표 (그리드 밖이면 false)
	UFUNCTION(BlueprintPure, Category = "Grid Placement")
	bool WorldToGrid(FVector WorldLocation, FIntPoint& OutCoord) const;

	// 칸 방향(예: (1,0) = 적 쪽)을 바라보는 회전
	UFUNCTION(BlueprintPure, Category = "Grid Placement")
	FRotator GetFacingRotation(FIntPoint GridDir) const;

	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	TSubclassOf<AGridTile> TileClass;

	virtual void OnConstruction(const FTransform& Transform) override;
	
	UFUNCTION(CallInEditor, Category = "Grid Settings")
	void GenerateGridInEditor();

	UFUNCTION(BlueprintCallable, Category = "Grid Placement")
	void DrawPlacementGuidelines();

	UFUNCTION(BlueprintCallable, Category = "Grid Placement")
	bool IsValidAllyPlacementArea(FIntPoint Coord);

	UFUNCTION(BlueprintCallable, Category = "Grid Placement")
	bool TryPlaceCharacter(FIntPoint TargetCoord, TSubclassOf<ABWCharacter> CharClass);

	UFUNCTION(BlueprintCallable, Category = "Grid Placement")
	bool IsGridReady() const { return bIsGridReady; }

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void DrawSkillPreview(FIntPoint OriginIdx, FIntPoint TargetIdx, FBWCardData CardData);

	// 플레이어 기준으로 MoveRange 안에서 실제로 갈 수 있는 칸을 표시 (접촉 칸에서 멈춤)
	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void DrawMovementPreview(FIntPoint OriginIdx, int32 MoveRange = 1);

	// [추가] 전투의 모든 굴림(합, 회피 등)이 쓰는 무작위 스트림. BeginPlay에서 시드를 정한다
	FRandomStream& GetCombatRandom() { return CombatRandom; }

	// --- [추가] 벽 (맵 가장자리 + 장애물 타일) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid Walls")
	TArray<FIntPoint> WallCoords;

	// 장애물 벽을 지정하고 타일 색을 갱신한다 (전투 시작 시 GameInstance의 PendingWallTiles로 자동 호출)
	UFUNCTION(BlueprintCallable, Category = "Grid Walls")
	void SetWallTiles(const TArray<FIntPoint>& Coords);

	// 맵 밖이거나 장애물 벽이면 true (베렌드 벽 밀치기의 '벽')
	UFUNCTION(BlueprintPure, Category = "Grid Walls")
	bool IsWallOrOutside(FIntPoint Coord) const;

	// Target을 Dir 방향으로 1칸 민다. 벽이면 밀리지 않고 bOutHitWall=true, 다른 말에 막히면 아무 일도 없다. 실제로 밀렸으면 true
	UFUNCTION(BlueprintCallable, Category = "Grid Walls")
	bool TryPushCharacter(ABWCharacter* Target, FIntPoint Dir, bool& bOutHitWall);

	// --- [추가] 속도 이동 ---
	// 그리드 안이고, 벽이 아니며, 다른 말이 없는 칸
	UFUNCTION(BlueprintCallable, Category = "Grid Movement")
	bool IsTileWalkable(FIntPoint Coord);

	// Coord의 상하좌우에 상대 진영의 살아 있는 말이 있으면 그 말을 돌려준다 (대각선은 접촉 아님)
	UFUNCTION(BlueprintCallable, Category = "Grid Movement")
	ABWCharacter* FindContactOpponent(FIntPoint Coord, bool bMoverIsEnemy);

	// Origin에서 Target까지 MaxSteps 이내의 최단 경로 (Origin 제외, Target 포함). 갈 수 없으면 빈 배열.
	// 상대와 접촉하는 칸에서는 더 나아가지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Grid Movement")
	TArray<FIntPoint> FindMovePath(FIntPoint Origin, FIntPoint Target, int32 MaxSteps, bool bMoverIsEnemy);

	// [추가] 4-3 돌진: Target과 맞닿는 칸까지의 최단 경로를 MaxSteps만큼 자른다. 이미 맞닿아 있으면 빈 배열.
	// 막혀서 닿을 수 없으면 Target에 가장 가까워지는 칸으로 간다
	UFUNCTION(BlueprintCallable, Category = "Grid Movement")
	TArray<FIntPoint> FindRushPath(FIntPoint Origin, FIntPoint Target, int32 MaxSteps, bool bMoverIsEnemy);

	// Origin에서 MaxSteps 이내로 도달 가능한 모든 칸과 거리 (Origin은 거리 0)
	void BuildMoveField(FIntPoint Origin, int32 MaxSteps, bool bMoverIsEnemy, TMap<FIntPoint, int32>& OutDistance, TMap<FIntPoint, FIntPoint>& OutParent);

	// 적의 의도(이동 및 공격) 누적 표시 (기존 표시를 지우지 않음)
	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void AddEnemyIntentPreview(FIntPoint OriginIdx, FIntPoint TargetIdx, ESkillShape Shape, int32 Range, bool bMoveBeforeAttack);

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void ExecuteSkill(FIntPoint OriginIdx, FIntPoint TargetIdx, FBWCardData CardData);

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void ClearAllRanges();

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void RefreshEnemyIntents();

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void CheckVictoryCondition();

	TArray<FIntPoint> GetAffectedCoordinates(FIntPoint OriginIdx, FIntPoint TargetIdx, ESkillShape Shape, int32 Range);

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	AGridTile* GetTileAtCoordinate(FIntPoint Coord);

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	ABWCharacter* GetCharacterAtCoordinate(FIntPoint Coord);
	// [추가] 쓰러진 말을 판에서 뺀다 (쓰러지는 연출 동안 칸을 막거나 판정에 끼지 않게)
	void UnregisterCharacter(ABWCharacter* Character) { DeployedCharacters.Remove(Character); }

	FIntPoint CalculateDirection(FIntPoint Origin, FIntPoint Target);

	// --- 연출 및 최적화 설정 ---
	UPROPERTY(EditAnywhere, Category = "Grid Effects")
	USoundBase* TileSpawnSound;

	UPROPERTY(EditAnywhere, Category = "Grid Effects")
	float TotalSpawnDuration = 0.75f; // 2배 빨라진 기본값

	UPROPERTY(EditAnywhere, Category = "Grid Effects")
	float TileSpawnHeightOffset = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Grid Effects")
	USoundBase* CharacterPlacementSound;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TArray<AGridTile*> TilePool;

	UPROPERTY()
	TMap<FIntPoint, AGridTile*> GridMap;

	UPROPERTY()
	TArray<ABWCharacter*> DeployedCharacters;

	void GenerateGrid();
	void StartDiagonalWave();
	void SpawnWeightGroup(int32 CurrentWeight);
	
	// --- 새로운 시퀀스 로직 ---
	void SpawnEnemies();
	void FinalizeGridColors();

	bool bIsGridReady = false;

	FRandomStream CombatRandom;

public:
	// 승리 또는 패배가 정해지면 true (그 뒤의 적 행동·판정을 멈춘다)
	UPROPERTY(BlueprintReadOnly, Category = "Grid Operations")
	bool bBattleOver = false;
};
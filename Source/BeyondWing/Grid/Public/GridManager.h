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

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void DrawMovementPreview(FIntPoint OriginIdx, int32 MoveRange = 1);

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
};
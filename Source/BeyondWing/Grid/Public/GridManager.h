#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridTile.h"    // 수정: 하부 경로 제거
#include "BWCharacter.h" // 수정: 하부 경로 제거
#include "GridManager.generated.h"

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

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void DrawSkillPreview(FIntPoint OriginIdx, FIntPoint TargetIdx, FCardData CardData);

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void ExecuteSkill(FIntPoint OriginIdx, FIntPoint TargetIdx, FCardData CardData);

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	void ClearAllRanges();

	UFUNCTION(BlueprintCallable, Category = "Grid Operations")
	AGridTile* GetTileAtCoordinate(FIntPoint Coord);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TMap<FIntPoint, AGridTile*> GridMap;

	UPROPERTY()
	TArray<ABWCharacter*> DeployedCharacters;

	void GenerateGrid();
	FIntPoint CalculateDirection(FIntPoint Origin, FIntPoint Target);
};
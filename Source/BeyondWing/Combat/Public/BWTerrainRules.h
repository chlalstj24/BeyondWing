#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondWing/Core/Public/BWMapTypes.h"
#include "BWTerrainRules.generated.h"

// 전투 칸의 지형 (차원마다 한 종류). 장애물(벽)은 따로 WallCoords로 다룬다
UENUM(BlueprintType)
enum class EBWTerrainType : uint8
{
	None,
	Resonance, // 파트로바스 마력 공명석: 이 칸에서 합을 굴리면 최댓값 +ResonanceMaxBonus
	Thorns,    // 베렌드 가시덤불: 들어서거나 밀려 들어오면 고정 피해 ThornsDamage
	Cover,     // 겔레온 사격 방벽: 자기 턴 끝에 서 있으면 방어도를 CoverBlock까지 채운다 (쌓이지 않음)
	Thicket    // 타이가 깊은 수풀: 들어서면 이번 이동이 그 칸에서 멈춘다
};

// 차원마다 다른 장애물 모양 (게임 규칙은 모두 '벽'으로 같고 생김새만 다르다)
UENUM(BlueprintType)
enum class EBWObstacleStyle : uint8
{
	Rock,          // 차원 없음: 바위
	Crystal,       // 파트로바스: 보랏빛 마력 수정
	BreakingRock,  // 베렌드: 거암
	MarblePillar,  // 겔레온: 대리석 기둥·금빛 장식
	JungleTree     // 타이가: 밀림 거목·이끼 바위
};

// 한 전투의 배치
USTRUCT(BlueprintType)
struct FBWTerrainLayout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Terrain") TArray<FIntPoint> Walls;
	UPROPERTY(BlueprintReadOnly, Category = "Terrain") TMap<FIntPoint, EBWTerrainType> Terrain;
};

// 전투 지형 규칙. 상태가 없고, 무작위는 넘겨받은 스트림에서 뽑는다
UCLASS()
class BEYONDWING_API UBWTerrainRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 수치 (기획 수치 없음, 임시)
	static constexpr int32 ResonanceMaxBonus = 2;
	static constexpr int32 ThornsDamage = 3;
	static constexpr int32 CoverBlock = 3;
	static constexpr int32 MinObstacles = 3;
	static constexpr int32 MaxObstacles = 5;
	static constexpr int32 MinTerrain = 3;
	static constexpr int32 MaxTerrain = 4;

	UFUNCTION(BlueprintPure, Category = "BW Terrain") static EBWTerrainType GetTerrainForDimension(EDimensionType Dimension);
	UFUNCTION(BlueprintPure, Category = "BW Terrain") static EBWObstacleStyle GetObstacleStyle(EDimensionType Dimension);

	// Width×Height 판에 장애물과 지형을 놓는다.
	// - Reserved(적 배치 칸)에는 아무것도 놓지 않는다
	// - 장애물은 가장자리 줄·아군 맨 뒤 열을 피하고, 벽이 아닌 칸이 모두 이어져 있게 한다
	// - 이미 정해진 벽(PresetWalls, 인카운터 지정)이 있으면 장애물은 새로 만들지 않는다
	static FBWTerrainLayout GenerateLayout(EDimensionType Dimension, int32 Width, int32 Height, int32 AllyZoneColumns,
		const TArray<FIntPoint>& Reserved, const TArray<FIntPoint>& PresetWalls, FRandomStream& Stream);

	// 벽이 아닌 칸이 모두 상하좌우로 이어져 있는지
	static bool IsOpenAreaConnected(int32 Width, int32 Height, const TArray<FIntPoint>& Walls);

	UFUNCTION(BlueprintPure, Category = "BW Terrain") static FText GetTerrainName(EBWTerrainType Type);
	UFUNCTION(BlueprintPure, Category = "BW Terrain") static FText GetTerrainDescription(EBWTerrainType Type);
	UFUNCTION(BlueprintPure, Category = "BW Terrain") static FText GetObstacleName(EBWObstacleStyle Style);
};

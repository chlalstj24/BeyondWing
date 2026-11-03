#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BeyondWing/Combat/Public/BWTerrainRules.h"
#include "BWBattlefieldDecor.generated.h"

class AGridManager;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

// 전투 판의 차원별 풍경: 칸 위 장애물·지형 모형 + 판 바깥 꾸밈 + 바닥 색.
// 모양은 기본 도형·StarterContent 메시를 조합해 코드로 만든다 (충돌 없음, 칸 고르기를 막지 않는다).
// 진짜 모델이 생기면 AddPart를 부르는 레시피만 바꾸면 된다
UCLASS()
class BEYONDWING_API ABWBattlefieldDecor : public AActor
{
	GENERATED_BODY()

public:
	ABWBattlefieldDecor();

	void Build(AGridManager* Grid, EDimensionType Dimension, const FBWTerrainLayout& Layout, int32 Seed);
	// [추가] 부서진 장애물을 치운다
	void RemoveObstacle(FIntPoint Coord);

	// 색을 바꿔 쓰는 기본 재질 (Color·Emissive 벡터, Metallic·Roughness 스칼라)
	UPROPERTY(EditAnywhere, Category = "Decor") TSoftObjectPtr<UMaterialInterface> PropMaterial;
	// 판 바깥 꾸밈 개수, 판에서 떨어진 거리(칸 단위)
	UPROPERTY(EditAnywhere, Category = "Decor") int32 SceneryCount = 34;
	UPROPERTY(EditAnywhere, Category = "Decor") float SceneryMinDistance = 1.3f;
	UPROPERTY(EditAnywhere, Category = "Decor") float SceneryMaxDistance = 6.0f;
	// 레시피의 발광 값에 곱하는 배율 (차원 분위기의 노출과 함께 맞춘 값)
	UPROPERTY(EditAnywhere, Category = "Decor") float EmissiveScale = 0.35f;
	// 솟아오르는 등장 연출
	UPROPERTY(EditAnywhere, Category = "Decor") float GrowTime = 0.35f;

	virtual void Tick(float DeltaSeconds) override;

private:
	struct FPart
	{
		TWeakObjectPtr<UStaticMeshComponent> Comp;
		FVector BaseLocation;
		FVector BaseScale;
		FRotator BaseRotation;
		float Delay = 0.0f;
		float BobAmplitude = 0.0f;
		float SpinSpeed = 0.0f;
		float Phase = 0.0f;
	};

	TMap<FIntPoint, TArray<TWeakObjectPtr<UStaticMeshComponent>>> ObstacleParts;

	struct FLook
	{
		FLinearColor Color = FLinearColor(0.5f, 0.5f, 0.5f);
		FLinearColor Emissive = FLinearColor::Black;
		float Metallic = 0.0f;
		float Roughness = 0.7f;
	};

	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLook& Look,
		float Delay, float Bob = 0.0f, float Spin = 0.0f);

	// 칸 위 (Origin = 칸 중심 바닥)
	void BuildObstacle(EBWObstacleStyle Style, const FVector& Origin, float Delay);
	void BuildTerrain(EBWTerrainType Type, const FVector& Origin, float Delay);
	// 판 바깥 (Big = 판에서 멀수록 크게)
	void BuildScenery(EDimensionType Dimension, const FVector& Origin, float Big, float Delay);

	// 판 너머(화면 위쪽) 풍경을 만드는 중인지. 키 큰 모델은 윗부분이 화면 밖으로 잘린다
	bool bSceneryFarSide = false;
	void BuildGround(EDimensionType Dimension, const FVector& Center);

	UPROPERTY(Transient) USceneComponent* Root = nullptr;
	UPROPERTY(Transient) UMaterialInterface* LoadedMaterial = nullptr;
	UPROPERTY(Transient) UStaticMesh* Cube = nullptr;
	UPROPERTY(Transient) UStaticMesh* Cylinder = nullptr;
	UPROPERTY(Transient) UStaticMesh* Cone = nullptr;
	UPROPERTY(Transient) UStaticMesh* Sphere = nullptr;
	UPROPERTY(Transient) UStaticMesh* Plane = nullptr;
	UPROPERTY(Transient) UStaticMesh* Rock = nullptr;
	UPROPERTY(Transient) UStaticMesh* Bush = nullptr;
	UPROPERTY(Transient) UStaticMesh* Stone = nullptr;

	TArray<FPart> Parts;
	FRandomStream Stream;
	float Elapsed = 0.0f;
	float TileSize = 120.0f;
};

#include "BeyondWing/Grid/Public/BWBattlefieldDecor.h"
#include "BeyondWing/Grid/Public/GridManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Framework/Docking/TabManager.h"

namespace BWDecor
{
#if WITH_EDITOR
	// 에디터 재시작 뒤 트리포 Bridge 탭을 열어 수신 서버를 켠다 (서버는 탭이 열릴 때 시작된다)
	static FAutoConsoleCommand OpenTripoBridgeCommand(
		TEXT("bw.OpenTripoBridge"),
		TEXT("Opens the Tripo Bridge tab."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FTabId(FName(TEXT("TripoWebSocketTab"))));
		}));
#endif

	UStaticMesh* LoadMesh(const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path); }

	// 트리포로 만든 모델. 없으면 기본 도형 레시피로 돌아간다
	const TCHAR* CrystalModelPaths[] =
	{
		TEXT("/Game/BattleMap/Patrovas/SM_Patrovas_CrystalCluster_A.SM_Patrovas_CrystalCluster_A"),
		TEXT("/Game/BattleMap/Patrovas/SM_Patrovas_CrystalCluster_B.SM_Patrovas_CrystalCluster_B"),
		TEXT("/Game/BattleMap/Patrovas/SM_Patrovas_CrystalCluster_C.SM_Patrovas_CrystalCluster_C"),
	};

	// 트리포 모델의 정면(그림에서 보던 쪽)이 카메라를 보는 방향
	const float ModelFrontYaw = 180.0f;

	const TCHAR* OrbitShardPath = TEXT("/Game/BattleMap/Patrovas/SM_Patrovas_OrbitShard_A.SM_Patrovas_OrbitShard_A");
	const TCHAR* GiantCrystalPath = TEXT("/Game/BattleMap/Patrovas/SM_Patrovas_GiantCrystal_A.SM_Patrovas_GiantCrystal_A");
	const TCHAR* FloatingRockPath = TEXT("/Game/BattleMap/Patrovas/SM_Patrovas_FloatingRock_A.SM_Patrovas_FloatingRock_A");
	const TCHAR* CrystalDebrisPath = TEXT("/Game/BattleMap/Patrovas/SM_Patrovas_CrystalDebris_A.SM_Patrovas_CrystalDebris_A");
	const TCHAR* CommonRockPath = TEXT("/Game/BattleMap/Common/SM_Common_Rock_A.SM_Common_Rock_A");
	const TCHAR* BreakingRockPath = TEXT("/Game/BattleMap/Berend/SM_Berend_BreakingRock_A.SM_Berend_BreakingRock_A");
	const TCHAR* GiantTreePath = TEXT("/Game/BattleMap/Berend/SM_Berend_GiantTree_A.SM_Berend_GiantTree_A");
	const TCHAR* ThornMatPath = TEXT("/Game/BattleMap/Berend/SM_Berend_ThornMat_A.SM_Berend_ThornMat_A");
	const TCHAR* MarblePillarPath = TEXT("/Game/BattleMap/Geleon/SM_Geleon_MarblePillar_A.SM_Geleon_MarblePillar_A");
	const TCHAR* JungleTreePath = TEXT("/Game/BattleMap/Taiga/SM_Taiga_JungleTree_A.SM_Taiga_JungleTree_A");
	const TCHAR* MarbleBlockPath = TEXT("/Game/BattleMap/Geleon/SM_Geleon_MarbleBlock_A.SM_Geleon_MarbleBlock_A");
	const TCHAR* MossRockPath = TEXT("/Game/BattleMap/Taiga/SM_Taiga_MossRock_A.SM_Taiga_MossRock_A");
	const TCHAR* ThicketPath = TEXT("/Game/BattleMap/Taiga/SM_Taiga_Thicket_A.SM_Taiga_Thicket_A");
	const TCHAR* BarricadePath = TEXT("/Game/BattleMap/Geleon/SM_Geleon_Barricade_B.SM_Geleon_Barricade_B");
	const TCHAR* ResonanceShardPath = TEXT("/Game/BattleMap/Patrovas/SM_Patrovas_ResonanceShard_A.SM_Patrovas_ResonanceShard_A");
	const TCHAR* SkyArchPath = TEXT("/Game/BattleMap/Geleon/SM_Geleon_SkyArch_A.SM_Geleon_SkyArch_A");
	const TCHAR* LanternPostPath = TEXT("/Game/BattleMap/Taiga/SM_Taiga_LanternPost_A.SM_Taiga_LanternPost_A");
	const TCHAR* TorchPostPath = TEXT("/Game/BattleMap/Berend/SM_Berend_TorchPost_A.SM_Berend_TorchPost_A");
	const TCHAR* SkyIslePath = TEXT("/Game/BattleMap/Geleon/SM_Geleon_SkyIsle_A.SM_Geleon_SkyIsle_A");
	const TCHAR* TwistedTreePath = TEXT("/Game/BattleMap/Patrovas/SM_Patrovas_TwistedTree_B.SM_Patrovas_TwistedTree_B");

	UStaticMesh* LoadCrystalModel(FRandomStream& Stream)
	{
		const int32 Count = UE_ARRAY_COUNT(CrystalModelPaths);
		const int32 Start = Stream.RandRange(0, Count - 1);
		for (int32 i = 0; i < Count; ++i)
		{
			if (UStaticMesh* Model = LoadMesh(CrystalModelPaths[(Start + i) % Count])) return Model;
		}
		return nullptr;
	}

	float EaseOutBack(float T)
	{
		const float C1 = 1.70158f, C3 = C1 + 1.0f;
		const float X = T - 1.0f;
		return 1.0f + C3 * X * X * X + C1 * X * X;
	}
}

ABWBattlefieldDecor::ABWBattlefieldDecor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	PropMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/System/Material/M_BWProp.M_BWProp")));
}

UStaticMeshComponent* ABWBattlefieldDecor::AddPart(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLook& Look,
	float Delay, float Bob, float Spin)
{
	if (!Mesh) return nullptr;
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetStaticMesh(Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetupAttachment(Root);
	Comp->RegisterComponent();
	Comp->SetWorldLocationAndRotation(Location, Rotation);
	Comp->SetWorldScale3D(FVector(0.01f));

	if (LoadedMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(LoadedMaterial, Comp);
		MID->SetVectorParameterValue(TEXT("Color"), Look.Color);
		MID->SetVectorParameterValue(TEXT("Emissive"), Look.Emissive * EmissiveScale);
		MID->SetScalarParameterValue(TEXT("Metallic"), Look.Metallic);
		MID->SetScalarParameterValue(TEXT("Roughness"), Look.Roughness);
		for (int32 i = 0; i < FMath::Max(1, Comp->GetNumMaterials()); ++i) Comp->SetMaterial(i, MID);
	}

	FPart Part;
	Part.Comp = Comp;
	Part.BaseLocation = Location;
	Part.BaseScale = Scale;
	Part.BaseRotation = Rotation;
	Part.Delay = Delay;
	Part.BobAmplitude = Bob;
	Part.SpinSpeed = Spin;
	Part.Phase = Stream.FRandRange(0.0f, 6.28f);
	Parts.Add(Part);
	return Comp;
}

void ABWBattlefieldDecor::RemoveObstacle(FIntPoint Coord)
{
	if (const TArray<TWeakObjectPtr<UStaticMeshComponent>>* Found = ObstacleParts.Find(Coord))
	{
		for (const TWeakObjectPtr<UStaticMeshComponent>& Comp : *Found)
		{
			if (Comp.IsValid()) Comp->SetVisibility(false);
		}
	}
}

void ABWBattlefieldDecor::Build(AGridManager* Grid, EDimensionType Dimension, const FBWTerrainLayout& Layout, int32 Seed)
{
	if (!Grid) return;
	Stream.Initialize(Seed);
	TileSize = Grid->TileSpacing;
	LoadedMaterial = PropMaterial.LoadSynchronous();
	Cube = BWDecor::LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cylinder = BWDecor::LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Cone = BWDecor::LoadMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	Sphere = BWDecor::LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Plane = BWDecor::LoadMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	Rock = BWDecor::LoadMesh(TEXT("/Game/StarterContent/Props/SM_Rock.SM_Rock"));
	Bush = BWDecor::LoadMesh(TEXT("/Game/StarterContent/Props/SM_Bush.SM_Bush"));
	Stone = BWDecor::LoadMesh(TEXT("/Game/Free_Magic/Mesh/SM_Free_Magic_Stone1.SM_Free_Magic_Stone1"));

	const float GroundZ = Grid->GetActorLocation().Z + 4.0f;
	auto TileGround = [Grid, GroundZ](const FIntPoint& Coord)
	{
		FVector L = Grid->GetTileWorldLocation(Coord);
		L.Z = GroundZ;
		return L;
	};

	// 칸 위: 장애물·지형은 칸이 내려앉는 물결 순서대로 솟는다
	const EBWObstacleStyle Style = UBWTerrainRules::GetObstacleStyle(Dimension);
	for (const FIntPoint& Wall : Layout.Walls)
	{
		const int32 First = Parts.Num();
		BuildObstacle(Style, TileGround(Wall), 0.05f * (Wall.X + Wall.Y));
		for (int32 i = First; i < Parts.Num(); ++i) ObstacleParts.FindOrAdd(Wall).Add(Parts[i].Comp);
	}
	for (const TPair<FIntPoint, EBWTerrainType>& Cell : Layout.Terrain) BuildTerrain(Cell.Value, TileGround(Cell.Key), 0.05f * (Cell.Key.X + Cell.Key.Y) + 0.1f);

	// 판 바깥: 판 사각형 둘레의 띠에 흩뿌린다. 카메라 쪽 가장자리는 판을 가리므로 비운다
	const FVector A = TileGround(FIntPoint(0, 0));
	const FVector B = TileGround(FIntPoint(Grid->Width - 1, Grid->Height - 1));
	const FVector Min(FMath::Min(A.X, B.X) - TileSize * 0.5f, FMath::Min(A.Y, B.Y) - TileSize * 0.5f, GroundZ);
	const FVector Max(FMath::Max(A.X, B.X) + TileSize * 0.5f, FMath::Max(A.Y, B.Y) + TileSize * 0.5f, GroundZ);
	const FVector Center = (Min + Max) * 0.5f;

	FVector CameraForward(1.0f, 0.0f, 0.0f);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			const FVector ToCenter = Center - PC->PlayerCameraManager->GetCameraLocation();
			if (!ToCenter.IsNearlyZero()) CameraForward = FVector(ToCenter.X, ToCenter.Y, 0.0f).GetSafeNormal();
		}
	}

	BuildGround(Dimension, Center);

	TArray<FVector> Placed;
	for (int32 Attempt = 0; Attempt < SceneryCount * 12 && Placed.Num() < SceneryCount; ++Attempt)
	{
		const float Distance = Stream.FRandRange(SceneryMinDistance, SceneryMaxDistance) * TileSize;
		const FVector P(Stream.FRandRange(Min.X - Distance, Max.X + Distance), Stream.FRandRange(Min.Y - Distance, Max.Y + Distance), GroundZ);
		const float OutX = FMath::Max3<double>(Min.X - P.X, P.X - Max.X, 0.0);
		const float OutY = FMath::Max3<double>(Min.Y - P.Y, P.Y - Max.Y, 0.0);
		const float Out = FMath::Max(OutX, OutY);
		if (Out < SceneryMinDistance * TileSize) continue;

		// 카메라 쪽 띠는 비운다 (판 앞을 가림)
		const FVector Rel = P - Center;
		const float HalfAlong = FMath::Abs(CameraForward.X) * (Max.X - Min.X) * 0.5f + FMath::Abs(CameraForward.Y) * (Max.Y - Min.Y) * 0.5f;
		if (FVector::DotProduct(Rel, CameraForward) < -HalfAlong + TileSize * 0.5f) continue;

		const float Big = 1.0f + FMath::Clamp((Out / TileSize - SceneryMinDistance) / (SceneryMaxDistance - SceneryMinDistance), 0.0f, 1.0f) * 1.3f;
		bool bTooClose = false;
		for (const FVector& Other : Placed)
		{
			if (FVector::Dist2D(Other, P) < TileSize * 0.9f * Big) { bTooClose = true; break; }
		}
		if (bTooClose) continue;
		Placed.Add(P);
		bSceneryFarSide = FVector::DotProduct(Rel, CameraForward) > HalfAlong;
		BuildScenery(Dimension, P, Big, 0.25f + Stream.FRandRange(0.0f, 0.5f));
	}
	SetActorTickEnabled(true);
}

void ABWBattlefieldDecor::BuildGround(EDimensionType Dimension, const FVector& Center)
{
	FLook Look;
	switch (Dimension)
	{
	case EDimensionType::Patrovas: Look.Color = FLinearColor(0.05f, 0.035f, 0.08f); Look.Roughness = 0.5f; break;
	case EDimensionType::Berend:   Look.Color = FLinearColor(0.055f, 0.075f, 0.035f); Look.Roughness = 0.95f; break;
	case EDimensionType::Geleon:   Look.Color = FLinearColor(0.42f, 0.42f, 0.44f); Look.Roughness = 0.35f; break;
	case EDimensionType::Taiga:    Look.Color = FLinearColor(0.025f, 0.05f, 0.03f); Look.Roughness = 0.9f; break;
	default: return; // 차원 없음: 레벨 바닥 그대로
	}
	// 바닥은 등장 연출 없이 바로 깐다
	if (UStaticMeshComponent* Ground = AddPart(Plane, FVector(Center.X, Center.Y, Center.Z - 3.5f), FRotator::ZeroRotator, FVector(80.0f, 80.0f, 1.0f), Look, 0.0f))
	{
		Ground->SetWorldScale3D(FVector(80.0f, 80.0f, 1.0f));
		Parts.Last().Delay = -1.0f;
	}
}

void ABWBattlefieldDecor::BuildObstacle(EBWObstacleStyle Style, const FVector& O, float Delay)
{
	const float Yaw = Stream.FRandRange(0.0f, 360.0f);
	auto At = [&O](float X, float Y, float Z) { return O + FVector(X, Y, Z); };
	// 트리포 모델을 칸 안에 맞춰 놓는다 (폭 88cm, 높이는 수정 장애물과 같은 약 135cm). 모델이 없으면 false
	auto AddObstacleModel = [&](const TCHAR* Path)
	{
		UStaticMesh* Model = BWDecor::LoadMesh(Path);
		if (!Model) return false;
		const FVector ModelSize = Model->GetBoundingBox().GetSize();
		const float HeightScale = 135.0f / static_cast<float>(FMath::Max(ModelSize.Z, 1.0));
		// 가는 모델(기둥)을 칸 폭까지 늘리면 비율이 깨지므로 높이 배율을 넘기지 않는다
		const float WidthScale = FMath::Min(88.0f / static_cast<float>(FMath::Max3(ModelSize.X, ModelSize.Y, 1.0)), HeightScale);
		if (UStaticMeshComponent* Comp = AddPart(Model, O, FRotator(0, BWDecor::ModelFrontYaw, 0), FVector(WidthScale, WidthScale, HeightScale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
		return true;
	};

	switch (Style)
	{
	case EBWObstacleStyle::Crystal:
	{
		FLook Crystal; Crystal.Color = FLinearColor(0.32f, 0.12f, 0.6f); Crystal.Emissive = FLinearColor(0.9f, 0.25f, 1.8f); Crystal.Roughness = 0.15f;
		FLook Base; Base.Color = FLinearColor(0.08f, 0.06f, 0.1f);
		if (UStaticMesh* Model = BWDecor::LoadCrystalModel(Stream))
		{
			// 트리포 모델. 폭을 88cm로 맞추고 세로는 늘리지 않아 뒤 칸의 말을 가리지 않는다
			const FVector ModelSize = Model->GetBoundingBox().GetSize();
			const float WidthScale = 88.0f / static_cast<float>(FMath::Max3(ModelSize.X, ModelSize.Y, 1.0));
			if (UStaticMeshComponent* Comp = AddPart(Model, O, FRotator(0, BWDecor::ModelFrontYaw, 0), FVector(WidthScale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
			if (UStaticMesh* Shard = BWDecor::LoadMesh(BWDecor::OrbitShardPath))
			{
				// 약 98cm 모델 → 지시서의 30cm. 수정 꼭대기 바로 위를 맴돈다
				if (UStaticMeshComponent* Comp = AddPart(Shard, At(0, 0, 118), FRotator::ZeroRotator, FVector(0.3f), FLook(), Delay + 0.1f, 8.0f, 60.0f)) Comp->EmptyOverrideMaterials();
			}
			else
			{
				AddPart(Cone, At(0, 0, 165), FRotator(180, 0, 0), FVector(0.1f, 0.1f, 0.25f), Crystal, Delay + 0.1f, 8.0f, 60.0f);
			}
			break;
		}
		AddPart(Stone, At(0, 0, 10), FRotator(0, Yaw, 0), FVector(0.55f, 0.55f, 0.18f), Base, Delay);
		AddPart(Cone, At(0, 0, 85), FRotator(4, Yaw, 3), FVector(0.62f, 0.62f, 1.6f), Crystal, Delay);
		AddPart(Cone, At(28, 16, 45), FRotator(-24, Yaw + 40, 10), FVector(0.36f, 0.36f, 0.9f), Crystal, Delay + 0.05f);
		AddPart(Cone, At(-24, -22, 38), FRotator(20, Yaw - 30, -14), FVector(0.3f, 0.3f, 0.75f), Crystal, Delay + 0.08f);
		// 머리 위를 맴도는 작은 조각
		AddPart(Cone, At(0, 0, 190), FRotator(180, 0, 0), FVector(0.1f, 0.1f, 0.25f), Crystal, Delay + 0.1f, 8.0f, 60.0f);
		break;
	}
	case EBWObstacleStyle::BreakingRock:
	{
		// 바위 깨기 대회의 거암: 큰 바위 + 쪼개진 틈 + 떨어져 나온 조각
		if (AddObstacleModel(BWDecor::BreakingRockPath)) break;
		FLook Rock1; Rock1.Color = FLinearColor(0.3f, 0.29f, 0.27f); Rock1.Roughness = 0.95f;
		FLook Rock2; Rock2.Color = FLinearColor(0.2f, 0.19f, 0.18f); Rock2.Roughness = 0.95f;
		FLook Crack; Crack.Color = FLinearColor(0.02f, 0.02f, 0.02f); Crack.Roughness = 1.0f;
		const FRotator Rot(0, Yaw, 0);
		AddPart(Rock, O, Rot, FVector(0.42f, 0.42f, 0.5f), Rock1, Delay);
		AddPart(Cube, O + FVector(0, 0, 70), Rot + FRotator(0, 0, 12), FVector(0.04f, 0.8f, 0.7f), Crack, Delay + 0.05f);
		AddPart(Rock, O + Rot.RotateVector(FVector(38, -30, 0)), FRotator(0, Yaw + 70, 0), FVector(0.14f), Rock2, Delay + 0.08f);
		AddPart(Rock, O + Rot.RotateVector(FVector(-34, 32, 0)), FRotator(0, Yaw + 140, 0), FVector(0.11f), Rock2, Delay + 0.1f);
		break;
	}
	case EBWObstacleStyle::MarblePillar:
	{
		FLook Marble; Marble.Color = FLinearColor(0.82f, 0.8f, 0.76f); Marble.Roughness = 0.3f;
		FLook Gold; Gold.Color = FLinearColor(0.85f, 0.6f, 0.2f); Gold.Metallic = 1.0f; Gold.Roughness = 0.3f;
		FLook Lamp; Lamp.Color = FLinearColor(1.0f, 0.9f, 0.6f); Lamp.Emissive = FLinearColor(3.0f, 2.4f, 1.0f); Lamp.Roughness = 0.1f;
		if (Stream.FRand() < 0.55f)
		{
			// 대리석 기둥 + 금빛 테 + 빛나는 수정구 (대성당)
			if (AddObstacleModel(BWDecor::MarblePillarPath)) break;
			AddPart(Cylinder, At(0, 0, 8), FRotator::ZeroRotator, FVector(0.75f, 0.75f, 0.16f), Marble, Delay);
			AddPart(Cylinder, At(0, 0, 95), FRotator::ZeroRotator, FVector(0.46f, 0.46f, 1.7f), Marble, Delay + 0.03f);
			AddPart(Cylinder, At(0, 0, 175), FRotator::ZeroRotator, FVector(0.6f, 0.6f, 0.1f), Gold, Delay + 0.06f);
			AddPart(Sphere, At(0, 0, 195), FRotator::ZeroRotator, FVector(0.18f), Lamp, Delay + 0.1f, 4.0f, 0.0f);
		}
		else
		{
			// 무너진 공중 건축의 대리석 덩이
			if (AddObstacleModel(BWDecor::MarbleBlockPath)) break;
			const FRotator Rot(0, Yaw, 0);
			AddPart(Cube, O + FVector(0, 0, 38), Rot, FVector(0.78f, 0.78f, 0.76f), Marble, Delay);
			AddPart(Cube, O + Rot.RotateVector(FVector(6, -4, 0)) + FVector(0, 0, 100), Rot + FRotator(0, 17, 0), FVector(0.5f, 0.5f, 0.48f), Marble, Delay + 0.06f);
			AddPart(Cube, O + FVector(0, 0, 38), Rot, FVector(0.8f, 0.06f, 0.8f), Gold, Delay + 0.03f); // 금빛 띠
		}
		break;
	}
	case EBWObstacleStyle::JungleTree:
	{
		FLook Bark; Bark.Color = FLinearColor(0.09f, 0.055f, 0.03f); Bark.Roughness = 0.9f;
		FLook Canopy; Canopy.Color = FLinearColor(0.03f, 0.13f, 0.06f); Canopy.Roughness = 0.85f;
		FLook Moss; Moss.Color = FLinearColor(0.06f, 0.16f, 0.06f); Moss.Roughness = 0.95f;
		FLook Rock1; Rock1.Color = FLinearColor(0.08f, 0.085f, 0.08f); Rock1.Roughness = 0.9f;
		if (Stream.FRand() < 0.6f)
		{
			// 밀림 거목: 뿌리 + 줄기 + 늘어진 덩굴 + 낮은 수관
			if (AddObstacleModel(BWDecor::JungleTreePath)) break;
			AddPart(Sphere, At(0, 0, 4), FRotator::ZeroRotator, FVector(0.8f, 0.8f, 0.18f), Moss, Delay);
			AddPart(Cylinder, At(0, 0, 85), FRotator(3, Yaw, 2), FVector(0.24f, 0.24f, 1.7f), Bark, Delay + 0.03f);
			for (int32 i = 0; i < 3; ++i)
			{
				const FVector Dir = FRotator(0, Yaw + i * 120.0f, 0).Vector();
				AddPart(Cylinder, O + Dir * 34.0f + FVector(0, 0, 120), FRotator::ZeroRotator, FVector(0.025f, 0.025f, 0.9f), Moss, Delay + 0.06f + i * 0.02f);
			}
			AddPart(Sphere, At(0, 0, 175), FRotator(0, Yaw, 0), FVector(0.95f, 0.95f, 0.5f), Canopy, Delay + 0.12f);
		}
		else
		{
			// 이끼 낀 동굴 바위
			if (AddObstacleModel(BWDecor::MossRockPath)) break;
			AddPart(Rock, At(0, 0, 0), FRotator(0, Yaw, 0), FVector(0.34f, 0.34f, 0.4f), Rock1, Delay);
			AddPart(Sphere, At(0, 0, 4), FRotator::ZeroRotator, FVector(0.85f, 0.85f, 0.14f), Moss, Delay + 0.05f);
		}
		break;
	}
	default:
	{
		if (AddObstacleModel(BWDecor::CommonRockPath)) break;
		FLook Grey; Grey.Color = FLinearColor(0.3f, 0.29f, 0.27f); Grey.Roughness = 0.9f;
		AddPart(Rock, O, FRotator(0, Yaw, 0), FVector(0.36f, 0.36f, 0.4f), Grey, Delay);
		break;
	}
	}
}

void ABWBattlefieldDecor::BuildTerrain(EBWTerrainType Type, const FVector& O, float Delay)
{
	// 칸 전체를 덮는 덤불: 납작한 모델 다섯 덩이를 칸 가운데와 네 귀퉁이에 겹쳐 칸 밖으로 살짝 넘치게 한다
	auto AddTileCover = [&](const TCHAR* Path)
	{
		UStaticMesh* Model = BWDecor::LoadMesh(Path);
		if (!Model) return false;
		const FVector ModelSize = Model->GetBoundingBox().GetSize();
		const float Reach = TileSize * 0.27f;
		const FVector2D Offsets[] = { {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {-1.0f, -1.0f} };
		for (const FVector2D& Offset : Offsets)
		{
			const float Width = TileSize * Stream.FRandRange(0.72f, 0.82f);
			const FVector Scale(Width / FMath::Max(ModelSize.X, 1.0), Width / FMath::Max(ModelSize.Y, 1.0), Width / FMath::Max(ModelSize.X, 1.0) * 1.3);
			const FVector Spot = O + FVector(Offset.X * Reach, Offset.Y * Reach, 0.0f);
			if (UStaticMeshComponent* Comp = AddPart(Model, Spot, FRotator(0, Stream.FRandRange(0.0f, 360.0f), 0), Scale, FLook(), Delay)) Comp->EmptyOverrideMaterials();
		}
		return true;
	};

	switch (Type)
	{
	case EBWTerrainType::Resonance:
	{
		// 빛나는 마력 웅덩이 + 떠서 도는 수정
		FLook Pool; Pool.Color = FLinearColor(0.2f, 0.05f, 0.4f); Pool.Emissive = FLinearColor(0.7f, 0.15f, 1.5f); Pool.Roughness = 0.1f;
		FLook Ring; Ring.Color = FLinearColor(0.1f, 0.05f, 0.15f); Ring.Emissive = FLinearColor(0.35f, 0.1f, 0.8f);
		FLook Crystal; Crystal.Color = FLinearColor(0.5f, 0.25f, 0.9f); Crystal.Emissive = FLinearColor(1.5f, 0.6f, 3.0f); Crystal.Roughness = 0.1f;
		AddPart(Cylinder, O + FVector(0, 0, 1.5f), FRotator::ZeroRotator, FVector(0.9f, 0.9f, 0.025f), Ring, Delay);
		AddPart(Cylinder, O + FVector(0, 0, 2.5f), FRotator::ZeroRotator, FVector(0.7f, 0.7f, 0.025f), Pool, Delay + 0.03f);
		if (UStaticMesh* Shard = BWDecor::LoadMesh(BWDecor::ResonanceShardPath))
		{
			// 약 98cm 모델 → 약 33cm. 웅덩이 귀퉁이에 낮게 떠서 말을 가리지 않는다
			if (UStaticMeshComponent* Comp = AddPart(Shard, O + FVector(32, 32, 6), FRotator::ZeroRotator, FVector(0.34f), FLook(), Delay + 0.08f, 2.0f, 40.0f)) Comp->EmptyOverrideMaterials();
			break;
		}
		AddPart(Cone, O + FVector(0, 0, 150), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 0.22f), Crystal, Delay + 0.08f, 10.0f, 90.0f);
		AddPart(Cone, O + FVector(0, 0, 128), FRotator(180, 0, 0), FVector(0.12f, 0.12f, 0.22f), Crystal, Delay + 0.08f, 10.0f, 90.0f);
		break;
	}
	case EBWTerrainType::Thorns:
	{
		// 네모난 가시 덩굴 판 한 장을 칸 안에 맞춘다 (옆 칸을 넘지 않게)
		if (UStaticMesh* ThornMat = BWDecor::LoadMesh(BWDecor::ThornMatPath))
		{
			const FVector ModelSize = ThornMat->GetBoundingBox().GetSize();
			const float Fit = TileSize * 0.94f / FMath::Max3(ModelSize.X, ModelSize.Y, 1.0);
			const float Yaw = Stream.RandRange(0, 3) * 90.0f;
			if (UStaticMeshComponent* Comp = AddPart(ThornMat, O, FRotator(0, Yaw, 0), FVector(Fit), FLook(), Delay)) Comp->EmptyOverrideMaterials();
			break;
		}
		FLook Brush; Brush.Color = FLinearColor(0.07f, 0.13f, 0.04f); Brush.Roughness = 0.95f;
		FLook Thorn; Thorn.Color = FLinearColor(0.35f, 0.07f, 0.05f); Thorn.Emissive = FLinearColor(0.08f, 0.0f, 0.0f); Thorn.Roughness = 0.7f;
		AddPart(Bush, O, FRotator(0, Stream.FRandRange(0.0f, 360.0f), 0), FVector(0.55f, 0.55f, 0.3f), Brush, Delay);
		for (int32 i = 0; i < 9; ++i)
		{
			const float Angle = i * 40.0f + Stream.FRandRange(-15.0f, 15.0f);
			const float R = Stream.FRandRange(14.0f, 44.0f);
			const FVector P = O + FRotator(0, Angle, 0).Vector() * R;
			// 바깥쪽으로 눕힌 가시
			const FRotator Tilt = FRotationMatrix::MakeFromZ((FRotator(0, Angle, 0).Vector() * 0.7f + FVector(0, 0, 1)).GetSafeNormal()).Rotator();
			AddPart(Cone, P + FVector(0, 0, 18), Tilt, FVector(0.07f, 0.07f, Stream.FRandRange(0.3f, 0.5f)), Thorn, Delay + 0.02f * i);
		}
		break;
	}
	case EBWTerrainType::Cover:
	{
		// 사방이 막힌 낮은 네모 방벽 (사격 연합). 어느 쪽에서 봐도 같다
		if (UStaticMesh* Model = BWDecor::LoadMesh(BWDecor::BarricadePath))
		{
			const FVector ModelSize = Model->GetBoundingBox().GetSize();
			const float FitScale = 96.0f / static_cast<float>(FMath::Max3(ModelSize.X, ModelSize.Y, 1.0));
			if (UStaticMeshComponent* Comp = AddPart(Model, O, FRotator(0, Stream.RandRange(0, 3) * 90.0f, 0), FVector(FitScale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
			break;
		}
		FLook Plate; Plate.Color = FLinearColor(0.8f, 0.78f, 0.74f); Plate.Roughness = 0.35f;
		FLook Stripe; Stripe.Color = FLinearColor(0.9f, 0.65f, 0.15f); Stripe.Emissive = FLinearColor(0.25f, 0.17f, 0.03f); Stripe.Metallic = 1.0f; Stripe.Roughness = 0.35f;
		FLook Base; Base.Color = FLinearColor(0.3f, 0.3f, 0.34f); Base.Roughness = 0.4f;
		AddPart(Cube, O + FVector(0, 0, 1.5f), FRotator::ZeroRotator, FVector(0.95f, 0.95f, 0.02f), Base, Delay);
		for (int32 i = -1; i <= 1; ++i) AddPart(Cube, O + FVector(i * 26.0f - 8.0f, 0, 2.5f), FRotator(0, 45, 0), FVector(0.08f, 0.5f, 0.02f), Stripe, Delay + 0.02f);
		// 방벽은 칸의 적 쪽 가장자리 (+X 방향)
		AddPart(Cube, O + FVector(46, 0, 26), FRotator::ZeroRotator, FVector(0.1f, 0.95f, 0.52f), Plate, Delay + 0.05f);
		AddPart(Cube, O + FVector(40, 46, 30), FRotator::ZeroRotator, FVector(0.14f, 0.08f, 0.6f), Stripe, Delay + 0.07f);
		AddPart(Cube, O + FVector(40, -46, 30), FRotator::ZeroRotator, FVector(0.14f, 0.08f, 0.6f), Stripe, Delay + 0.07f);
		break;
	}
	case EBWTerrainType::Thicket:
	{
		// 발이 묶이는 수풀: 낮은 풀 더미 + 뾰족한 잎 (말이 서도 가리지 않게 낮게)
		if (AddTileCover(BWDecor::ThicketPath)) break;
		FLook Grass; Grass.Color = FLinearColor(0.04f, 0.17f, 0.07f); Grass.Roughness = 0.85f;
		FLook Blade; Blade.Color = FLinearColor(0.07f, 0.26f, 0.09f); Blade.Emissive = FLinearColor(0.0f, 0.02f, 0.01f); Blade.Roughness = 0.8f;
		AddPart(Sphere, O + FVector(0, 0, 2), FRotator::ZeroRotator, FVector(1.0f, 1.0f, 0.2f), Grass, Delay);
		for (int32 i = 0; i < 10; ++i)
		{
			const float Angle = i * 36.0f + Stream.FRandRange(-12.0f, 12.0f);
			const FVector P = O + FRotator(0, Angle, 0).Vector() * Stream.FRandRange(14.0f, 44.0f);
			const FRotator Tilt = FRotationMatrix::MakeFromZ((FRotator(0, Angle, 0).Vector() * 0.35f + FVector(0, 0, 1)).GetSafeNormal()).Rotator();
			AddPart(Cone, P + FVector(0, 0, 20), Tilt, FVector(0.06f, 0.06f, Stream.FRandRange(0.4f, 0.6f)), Blade, Delay + 0.02f * i);
		}
		break;
	}
	default: break;
	}
}

void ABWBattlefieldDecor::BuildScenery(EDimensionType Dimension, const FVector& O, float Big, float Delay)
{
	const float Yaw = Stream.FRandRange(0.0f, 360.0f);
	float Roll = Stream.FRand();
	auto S = [Big](float X, float Y, float Z) { return FVector(X, Y, Z) * Big; };

	switch (Dimension)
	{
	case EDimensionType::Patrovas:
	{
		FLook Crystal; Crystal.Color = FLinearColor(0.3f, 0.1f, 0.55f); Crystal.Emissive = FLinearColor(0.8f, 0.2f, 1.6f); Crystal.Roughness = 0.15f;
		FLook Teal = Crystal; Teal.Color = FLinearColor(0.1f, 0.35f, 0.5f); Teal.Emissive = FLinearColor(0.15f, 0.8f, 1.2f);
		FLook Rock1; Rock1.Color = FLinearColor(0.07f, 0.05f, 0.1f); Rock1.Roughness = 0.8f;
		// 판 옆 칸 크기와 어울리게 멀리 있어도 너무 커지지 않게 한다
		const float Size = FMath::Min(Big, 1.3f);
		// 판 너머의 나무는 윗부분이 잘려 뿌리만 보이므로 다른 풍경으로 바꾼다
		if (bSceneryFarSide && Roll < 0.3f) Roll += 0.6f;
		if (Roll < 0.3f)
		{
			// 마법의 숲: 뒤틀린 나무 + 빛나는 수관
			if (UStaticMesh* Model = BWDecor::LoadMesh(BWDecor::TwistedTreePath))
			{
				// 높이 약 97cm 모델 → 170~310cm
				const float TreeScale = Stream.FRandRange(1.8f, 2.4f) * Size;
				// 뒷면은 그림 정보가 없어 흐리므로 앞면이 카메라를 보게 둔다
				const float TreeYaw = BWDecor::ModelFrontYaw + Stream.FRandRange(-25.0f, 25.0f);
				if (UStaticMeshComponent* Comp = AddPart(Model, O, FRotator(0, TreeYaw, 0), FVector(TreeScale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
				break;
			}
			FLook Bark; Bark.Color = FLinearColor(0.06f, 0.04f, 0.07f); Bark.Roughness = 0.9f;
			FLook Canopy; Canopy.Color = FLinearColor(0.06f, 0.14f, 0.13f); Canopy.Emissive = FLinearColor(0.03f, 0.2f, 0.24f); Canopy.Roughness = 0.7f;
			const float H = Stream.FRandRange(1.6f, 2.8f);
			AddPart(Cylinder, O + FVector(0, 0, 50.0f * H * Big), FRotator(Stream.FRandRange(-8.0f, 8.0f), Yaw, Stream.FRandRange(-8.0f, 8.0f)), S(0.26f, 0.26f, H), Bark, Delay);
			AddPart(Sphere, O + FVector(0, 0, 105.0f * H * Big), FRotator(0, Yaw, 0), S(1.3f, 1.3f, 0.9f), Canopy, Delay + 0.06f);
		}
		else if (Roll < 0.5f)
		{
			// 수정 무리
			if (UStaticMesh* Giant = Stream.FRand() < 0.5f ? BWDecor::LoadMesh(BWDecor::GiantCrystalPath) : nullptr)
			{
				// 높이 약 97cm 모델 → 170~310cm
				const float H = Stream.FRandRange(1.8f, 2.4f) * Size;
				if (UStaticMeshComponent* Comp = AddPart(Giant, O, FRotator(0, Yaw, 0), FVector(H), FLook(), Delay)) Comp->EmptyOverrideMaterials();
				break;
			}
			if (UStaticMesh* Model = BWDecor::LoadCrystalModel(Stream))
			{
				const float H = Stream.FRandRange(1.2f, 1.8f) * Size;
				if (UStaticMeshComponent* Comp = AddPart(Model, O, FRotator(0, Yaw, 0), FVector(H * 0.8f, H * 0.8f, H), FLook(), Delay)) Comp->EmptyOverrideMaterials();
				break;
			}
			const FLook& Look = Stream.FRand() < 0.7f ? Crystal : Teal;
			const int32 N = Stream.RandRange(2, 4);
			for (int32 i = 0; i < N; ++i)
			{
				const FVector Off = FRotator(0, Yaw + i * 90.0f, 0).Vector() * (i == 0 ? 0.0f : 30.0f * Big);
				const float H = Stream.FRandRange(1.2f, 2.6f) * (i == 0 ? 1.0f : 0.6f);
				AddPart(Cone, O + Off + FVector(0, 0, 50.0f * H * Big * 0.9f), FRotator(Stream.FRandRange(-18.0f, 18.0f), Yaw, Stream.FRandRange(-18.0f, 18.0f)), S(0.35f, 0.35f, H), Look, Delay + i * 0.04f);
			}
		}
		else if (Roll < 0.8f)
		{
			// 떠 있는 바위 (아래에 빛나는 조각)
			const float Z = Stream.FRandRange(120.0f, 260.0f) * Big;
			if (UStaticMesh* Model = BWDecor::LoadMesh(BWDecor::FloatingRockPath))
			{
				// 폭 약 97cm 모델 → 110~200cm. 기준점이 바닥(뾰족한 끝)이다
				const float Scale = Stream.FRandRange(1.2f, 1.6f) * Size;
				if (UStaticMeshComponent* Comp = AddPart(Model, O + FVector(0, 0, Z), FRotator(0, Yaw, 0), FVector(Scale), FLook(), Delay, 14.0f, 6.0f)) Comp->EmptyOverrideMaterials();
				break;
			}
			AddPart(Stone, O + FVector(0, 0, Z),FRotator(Stream.FRandRange(-20.0f, 20.0f), Yaw, 180.0f), S(0.6f, 0.6f, 0.45f), Rock1, Delay, 14.0f, 6.0f);
			AddPart(Cone, O + FVector(0, 0, Z - 40.0f * Big), FRotator(180, 0, 0), S(0.15f, 0.15f, 0.4f), Crystal, Delay + 0.05f, 14.0f, 6.0f);
		}
		else if (UStaticMesh* Debris = Roll < 0.9f ? BWDecor::LoadMesh(BWDecor::CrystalDebrisPath) : nullptr)
		{
			// 깨진 수정 파편 무더기 (폭 약 97cm 모델)
			const float Scale = Stream.FRandRange(0.8f, 1.3f) * Size;
			if (UStaticMeshComponent* Comp = AddPart(Debris, O, FRotator(0, Yaw, 0), FVector(Scale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
		}
		else
		{
			// 바닥에 새겨진 빛나는 고리
			FLook Rune; Rune.Color = FLinearColor(0.1f, 0.03f, 0.2f); Rune.Emissive = FLinearColor(0.5f, 0.12f, 1.1f);
			AddPart(Cylinder, O + FVector(0, 0, 1), FRotator::ZeroRotator, S(1.4f, 1.4f, 0.02f), Rune, Delay);
			AddPart(Cylinder, O + FVector(0, 0, 2), FRotator::ZeroRotator, S(1.1f, 1.1f, 0.02f), Rock1, Delay);
			AddPart(Cone, O + FVector(0, 0, 90.0f * Big), FRotator::ZeroRotator, S(0.14f, 0.14f, 0.35f), Crystal, Delay + 0.05f, 12.0f, 50.0f);
		}
		break;
	}
	case EDimensionType::Berend:
	{
		// 베렌드: 하늘을 가리는 거대한 나무와 바위, 대회장의 횃불
		FLook Stone1; Stone1.Color = FLinearColor(0.27f, 0.26f, 0.24f); Stone1.Roughness = 0.95f;
		FLook Leaf; Leaf.Color = FLinearColor(0.05f, 0.14f, 0.04f); Leaf.Roughness = 0.9f;
		FLook Wood; Wood.Color = FLinearColor(0.13f, 0.08f, 0.04f); Wood.Roughness = 0.9f;
		if (Roll < 0.3f)
		{
			if (UStaticMesh* Model = BWDecor::LoadMesh(BWDecor::BreakingRockPath))
			{
				// 거암을 풍경 크기로 (높이 약 97cm 모델 → 250~400cm)
				const float RockScale = Stream.FRandRange(2.5f, 4.0f) * Big;
				if (UStaticMeshComponent* Comp = AddPart(Model, O, FRotator(0, Yaw, 0), FVector(RockScale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
				break;
			}
			AddPart(Rock, O, FRotator(0, Yaw, 0), S(0.5f, 0.5f, Stream.FRandRange(0.4f, 0.7f)), Stone1, Delay);
		}
		else if (UStaticMesh* GiantTree = Roll < 0.65f ? BWDecor::LoadMesh(BWDecor::GiantTreePath) : nullptr)
		{
			// 트리포 거목 (높이 약 97cm 모델 → 400~650cm)
			const float TreeScale = Stream.FRandRange(4.0f, 6.5f) * Big;
			if (UStaticMeshComponent* Comp = AddPart(GiantTree, O, FRotator(0, Yaw, 0), FVector(TreeScale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
		}
		else if (Roll < 0.65f)
		{
			// 거목
			const float H = Stream.FRandRange(2.4f, 3.8f);
			AddPart(Cylinder, O + FVector(0, 0, 50.0f * H * Big), FRotator::ZeroRotator, S(0.5f, 0.5f, H), Wood, Delay);
			AddPart(Sphere, O + FVector(0, 0, 100.0f * H * Big), FRotator(0, Yaw, 0), S(2.2f, 2.2f, 1.4f), Leaf, Delay + 0.06f);
		}
		else if (Roll < 0.88f)
		{
			AddPart(Bush, O, FRotator(0, Yaw, 0), S(0.7f, 0.7f, 0.6f), Leaf, Delay);
		}
		else
		{
			// 바위 무더기 (예전 횃불 기둥은 불꽃이 멈춘 모델이라 뺐다)
			if (UStaticMesh* Model = BWDecor::LoadMesh(BWDecor::CommonRockPath))
			{
				const float RockScale = Stream.FRandRange(1.1f, 1.7f) * Big;
				if (UStaticMeshComponent* Comp = AddPart(Model, O, FRotator(0, Yaw, 0), FVector(RockScale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
				break;
			}
			AddPart(Sphere, O + FVector(0, 0, 30.0f * Big), FRotator(0, Yaw, 0), S(0.9f, 0.8f, 0.6f), Wood, Delay);
		}
		break;
	}
	case EDimensionType::Geleon:
	{
		// 겔레온: 하늘에 뜬 대리석 건축과 금빛 장식
		FLook Marble; Marble.Color = FLinearColor(0.82f, 0.8f, 0.76f); Marble.Roughness = 0.3f;
		FLook Gold; Gold.Color = FLinearColor(0.85f, 0.6f, 0.2f); Gold.Metallic = 1.0f; Gold.Roughness = 0.3f;
		FLook Cloud; Cloud.Color = FLinearColor(0.9f, 0.92f, 0.96f); Cloud.Roughness = 0.9f;
		FLook Lamp; Lamp.Color = FLinearColor(1.0f, 0.9f, 0.6f); Lamp.Emissive = FLinearColor(4.0f, 3.2f, 1.4f);
		if (Roll < 0.3f)
		{
			// 무너진 대성당 아치 또는 첨탑
			if (UStaticMesh* Model = Stream.FRand() < 0.6f ? BWDecor::LoadMesh(BWDecor::SkyArchPath) : nullptr)
			{
				// 높이 약 97cm 모델 → 300~500cm
				const float ArchScale = Stream.FRandRange(3.0f, 5.0f) * Big;
				if (UStaticMeshComponent* Comp = AddPart(Model, O, FRotator(0, Yaw, 0), FVector(ArchScale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
				break;
			}
			const float H = Stream.FRandRange(2.5f, 4.5f);
			AddPart(Cylinder, O + FVector(0, 0, 50.0f * H * Big), FRotator::ZeroRotator, S(0.6f, 0.6f, H), Marble, Delay);
			AddPart(Cone, O + FVector(0, 0, 100.0f * H * Big + 45.0f * Big), FRotator::ZeroRotator, S(0.8f, 0.8f, 1.0f), Gold, Delay + 0.05f);
			AddPart(Sphere, O + FVector(0, 0, 100.0f * H * Big + 100.0f * Big), FRotator::ZeroRotator, S(0.15f, 0.15f, 0.15f), Lamp, Delay + 0.08f);
		}
		else if (Roll < 0.6f)
		{
			// 떠 있는 작은 섬과 건물
			const float Z = Stream.FRandRange(140.0f, 300.0f) * Big;
			if (UStaticMesh* Model = BWDecor::LoadMesh(BWDecor::SkyIslePath))
			{
				// 높이 약 97cm 모델 → 200~300cm. 기준점이 뾰족한 아래 끝이다
				const float IsleScale = Stream.FRandRange(2.0f, 3.0f) * Big;
				if (UStaticMeshComponent* Comp = AddPart(Model, O + FVector(0, 0, Z), FRotator(0, Yaw, 0), FVector(IsleScale), FLook(), Delay, 14.0f, 0.0f)) Comp->EmptyOverrideMaterials();
				break;
			}
			AddPart(Stone, O + FVector(0, 0, Z), FRotator(0, Yaw, 180.0f), S(0.8f, 0.8f, 0.45f), Marble, Delay, 14.0f, 0.0f);
			AddPart(Cube, O + FVector(0, 0, Z + 50.0f * Big), FRotator(0, Yaw, 0), S(0.5f, 0.5f, 0.7f), Marble, Delay + 0.05f, 14.0f, 0.0f);
		}
		else if (Roll < 0.85f)
		{
			// 구름 무리
			const int32 N = Stream.RandRange(2, 3);
			for (int32 i = 0; i < N; ++i)
			{
				AddPart(Sphere, O + FRotator(0, Yaw + i * 110.0f, 0).Vector() * (i == 0 ? 0.0f : 60.0f * Big) + FVector(0, 0, 30.0f * Big), FRotator::ZeroRotator, S(1.2f, 1.2f, 0.6f), Cloud, Delay + i * 0.05f, 6.0f, 0.0f);
			}
		}
		else
		{
			// 태양 고리 (솔테리스의 상징)
			AddPart(Cylinder, O + FVector(0, 0, 90.0f * Big), FRotator(0, Yaw, 90), S(1.6f, 1.6f, 0.12f), Gold, Delay);
			AddPart(Cylinder, O + FVector(0, 0, 90.0f * Big), FRotator(0, Yaw, 90), S(0.6f, 0.6f, 0.2f), Lamp, Delay + 0.05f);
		}
		break;
	}
	case EDimensionType::Taiga:
	{
		// 타이가: 달빛 아래의 정글, 주황 등불
		FLook Bark; Bark.Color = FLinearColor(0.07f, 0.045f, 0.03f); Bark.Roughness = 0.9f;
		FLook Canopy; Canopy.Color = FLinearColor(0.025f, 0.1f, 0.05f); Canopy.Roughness = 0.85f;
		FLook Fern; Fern.Color = FLinearColor(0.04f, 0.16f, 0.07f); Fern.Roughness = 0.8f;
		FLook Lantern; Lantern.Color = FLinearColor(1.0f, 0.45f, 0.1f); Lantern.Emissive = FLinearColor(4.0f, 1.6f, 0.2f);
		if (Roll < 0.5f)
		{
			// 밀림 나무
			if (UStaticMesh* Model = BWDecor::LoadMesh(BWDecor::JungleTreePath))
			{
				// 높이 약 97cm 모델 → 300~500cm
				const float TreeScale = Stream.FRandRange(3.0f, 5.0f) * Big;
				if (UStaticMeshComponent* Comp = AddPart(Model, O, FRotator(0, Yaw, 0), FVector(TreeScale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
				break;
			}
			const float H = Stream.FRandRange(2.0f, 3.2f);
			AddPart(Cylinder, O + FVector(0, 0, 50.0f * H * Big), FRotator::ZeroRotator, S(0.3f, 0.3f, H), Bark, Delay);
			AddPart(Sphere, O + FVector(0, 0, 100.0f * H * Big), FRotator(0, Yaw, 0), S(1.8f, 1.8f, 0.9f), Canopy, Delay + 0.05f);
			AddPart(Sphere, O + FRotator(0, Yaw, 0).Vector() * 70.0f * Big + FVector(0, 0, 75.0f * H * Big), FRotator(0, Yaw, 0), S(1.1f, 1.1f, 0.6f), Canopy, Delay + 0.08f);
		}
		else if (Roll < 0.8f)
		{
			// 커다란 잎 무리
			const int32 N = Stream.RandRange(4, 6);
			for (int32 i = 0; i < N; ++i)
			{
				const float Angle = Yaw + i * 360.0f / N;
				const FRotator Tilt = FRotationMatrix::MakeFromZ((FRotator(0, Angle, 0).Vector() * 0.8f + FVector(0, 0, 1)).GetSafeNormal()).Rotator();
				AddPart(Cone, O + FRotator(0, Angle, 0).Vector() * 30.0f * Big + FVector(0, 0, 50.0f * Big), Tilt, S(0.3f, 0.1f, 1.3f), Fern, Delay + i * 0.03f);
			}
		}
		else
		{
			// 길잡이 돌등
			if (UStaticMesh* Model = BWDecor::LoadMesh(BWDecor::LanternPostPath))
			{
				// 높이 약 97cm 모델 → 약 200cm
				const float PostScale = 2.0f * Big;
				if (UStaticMeshComponent* Comp = AddPart(Model, O, FRotator(0, Yaw, 0), FVector(PostScale), FLook(), Delay)) Comp->EmptyOverrideMaterials();
				break;
			}
			AddPart(Rock, O, FRotator(0, Yaw, 0), S(0.25f, 0.25f, 0.3f), Bark, Delay);
			AddPart(Sphere, O + FVector(0, 0, 70.0f * Big), FRotator::ZeroRotator, S(0.14f, 0.14f, 0.18f), Lantern, Delay + 0.05f, 2.0f);
		}
		break;
	}
	default:
	{
		FLook Grey; Grey.Color = FLinearColor(0.3f, 0.29f, 0.27f); Grey.Roughness = 0.9f;
		FLook Leaf; Leaf.Color = FLinearColor(0.12f, 0.2f, 0.07f); Leaf.Roughness = 0.9f;
		if (Roll < 0.5f) AddPart(Rock, O, FRotator(0, Yaw, 0), S(0.45f, 0.45f, 0.5f), Grey, Delay);
		else AddPart(Bush, O, FRotator(0, Yaw, 0), S(0.6f, 0.6f, 0.6f), Leaf, Delay);
		break;
	}
	}
}

void ABWBattlefieldDecor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;

	bool bAnyAnimating = false;
	for (const FPart& Part : Parts)
	{
		UStaticMeshComponent* Comp = Part.Comp.Get();
		if (!Comp) continue;

		if (Part.Delay < 0.0f)
		{
			Comp->SetWorldScale3D(Part.BaseScale);
			continue;
		}
		const float T = FMath::Clamp((Elapsed - Part.Delay) / FMath::Max(0.01f, GrowTime), 0.0f, 1.0f);
		const bool bLooping = Part.BobAmplitude > 0.0f || Part.SpinSpeed != 0.0f;
		bAnyAnimating |= T < 1.0f || bLooping;
		if (T >= 1.0f && !bLooping && Comp->GetComponentScale().Equals(Part.BaseScale)) continue;

		Comp->SetWorldScale3D(Part.BaseScale * FMath::Max(0.01f, BWDecor::EaseOutBack(T)));
		if (bLooping)
		{
			const FVector Loc = Part.BaseLocation + FVector(0, 0, FMath::Sin(Elapsed * 1.7f + Part.Phase) * Part.BobAmplitude);
			const FRotator Rot = Part.BaseRotation + FRotator(0, Elapsed * Part.SpinSpeed, 0);
			Comp->SetWorldLocationAndRotation(Loc, Rot);
		}
	}
	if (!bAnyAnimating) SetActorTickEnabled(false);
}

#include "GridManager.h"
#include "Components/BoxComponent.h"
#include "GridTile.h"

// 생성자 기본 설정
AGridManager::AGridManager()
{
	PrimaryActorTick.bCanEverTick = false;
	Width = 8;
	Height = 8;
	TileSpacing = 120.0f;
}

// 에디터 그리드 재생성 버튼
void AGridManager::GenerateGridInEditor()
{
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for (AActor* Actor : AttachedActors)
	{
		if (Actor && Actor->IsValidLowLevel())
		{
			Actor->Destroy();
		}
	}
	GridMap.Empty();

	GenerateGrid();
	DrawPlacementGuidelines();
}

// 그리드 맵 스폰 연산
void AGridManager::GenerateGrid()
{
	if (!TileClass || !GetWorld()) return;

	for (int32 x = 0; x < Width; ++x)
	{
		for (int32 y = 0; y < Height; ++y)
		{
			FVector SpawnLocation = GetActorLocation() + FVector(x * TileSpacing, y * TileSpacing, 0.0f);
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.bNoFail = true;

			AGridTile* NewTile = GetWorld()->SpawnActor<AGridTile>(TileClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
			if (NewTile)
			{
				NewTile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				NewTile->GridCoordinate = FIntPoint(x, y);
				NewTile->SetTileState(ETileState::None);
				
				if (NewTile->CollisionBox)
				{
					NewTile->CollisionBox->SetBoxExtent(FVector(TileSpacing * 0.5f, TileSpacing * 0.5f, 10.0f));
				}

				GridMap.Add(FIntPoint(x, y), NewTile);
			}
		}
	}
}

// 스킬 시전 범위 미리보기 드로우
void AGridManager::DrawSkillPreview(FIntPoint OriginIdx, FIntPoint TargetIdx, FCardData CardData)
{
	ClearAllRanges();

	FIntPoint Dir = CalculateDirection(OriginIdx, TargetIdx);

	for (auto& Elem : GridMap)
	{
		FIntPoint TileCoord = Elem.Key;
		int32 Distance = FMath::Abs(OriginIdx.X - TileCoord.X) + FMath::Abs(OriginIdx.Y - TileCoord.Y);

		switch (CardData.SkillShape)
		{
		case ESkillShape::Single:
		{
			// 마우스 방향에 맞춰 일직선 사거리 범위만 계산하여 표시
			for (int32 i = 1; i <= CardData.Range; ++i)
			{
				FIntPoint CheckCoord = OriginIdx + (Dir * i);
				if (TileCoord == CheckCoord)
				{
					Elem.Value->SetTileState(CardData.DisplayColor);
				}
			}
			break;
		}
		case ESkillShape::Manhattan:
		{
			if (Distance <= CardData.Range && Distance > 0)
				Elem.Value->SetTileState(CardData.DisplayColor);
			break;
		}
		case ESkillShape::Cross:
		{
			bool bIsHorizontal = (TileCoord.X == OriginIdx.X && FMath::Abs(TileCoord.Y - OriginIdx.Y) <= CardData.Range);
			bool bIsVertical = (TileCoord.Y == OriginIdx.Y && FMath::Abs(TileCoord.X - OriginIdx.X) <= CardData.Range);
			if (TileCoord != OriginIdx && (bIsHorizontal || bIsVertical))
				Elem.Value->SetTileState(CardData.DisplayColor);
			break;
		}
		case ESkillShape::T_Shape:
		{
			// 마우스 방향으로 줄기(Range) 생성
			FIntPoint EndPoint = OriginIdx + (Dir * CardData.Range);
			for (int32 i = 1; i <= CardData.Range; ++i)
			{
				if (TileCoord == OriginIdx + (Dir * i))
				{
					Elem.Value->SetTileState(CardData.DisplayColor);
				}
			}

			// 줄기 끝에서 수직으로 양옆 한 칸씩 추가 (T자 형태)
			FIntPoint Perpendicular = FIntPoint(-Dir.Y, Dir.X);
			if (TileCoord == EndPoint + Perpendicular || TileCoord == EndPoint - Perpendicular)
			{
				Elem.Value->SetTileState(CardData.DisplayColor);
			}
			break;
		}
		}
	}
}

// 게임 시작 시 초기 가이드라인 렌더링
void AGridManager::BeginPlay()
{
	Super::BeginPlay();
	DrawPlacementGuidelines();
}

// 액터 생성 시 컴포넌트 갱신
void AGridManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

// 배치 가능 구역 및 적 구역 가이드라인 정의
void AGridManager::DrawPlacementGuidelines()
{
	for (auto& Elem : GridMap)
	{
		if (!Elem.Value) continue;
        
		if (Elem.Key.X >= 0 && Elem.Key.X <= 2)
		{
			Elem.Value->DefaultZoneState = ETileState::AllyZone;
			Elem.Value->SetTileState(ETileState::AllyZone);
		}
		else
		{
			Elem.Value->DefaultZoneState = ETileState::EnemyZone;
			Elem.Value->SetTileState(ETileState::EnemyZone);
		}
	}
}

// 배치 가능 타일 검증
bool AGridManager::IsValidAllyPlacementArea(FIntPoint Coord)
{
	if (Coord.X >= 0 && Coord.X <= 2 && Coord.Y >= 0 && Coord.Y < Height)
	{
		for (ABWCharacter* Char : DeployedCharacters)
			if (Char && Char->OwnGridCoordinate == Coord) return false;
		return true;
	}
	return false;
}

// 캐릭터 배치 및 가이드라인 초기화 처리
bool AGridManager::TryPlaceCharacter(FIntPoint TargetCoord, TSubclassOf<ABWCharacter> CharClass)
{
	if (!CharClass || !IsValidAllyPlacementArea(TargetCoord)) return false;
	AGridTile* TargetTile = GetTileAtCoordinate(TargetCoord);
	if (!TargetTile) return false;

	ABWCharacter* NewChar = GetWorld()->SpawnActor<ABWCharacter>(CharClass, TargetTile->GetActorLocation() + FVector(0,0,20), FRotator::ZeroRotator);
	if (NewChar)
	{
		NewChar->OwnGridCoordinate = TargetCoord;
		DeployedCharacters.Add(NewChar);

		// 배치 완료 후 모든 타일의 기본 가이드라인 영구 제거 및 하얗게 변경
		for (auto& Elem : GridMap)
		{
			if (Elem.Value)
			{
				Elem.Value->DefaultZoneState = ETileState::None;
				Elem.Value->SetTileState(ETileState::None);
			}
		}
		return true;
	}
	return false;
}

// 스킬 실행 구현부
void AGridManager::ExecuteSkill(FIntPoint OriginIdx, FIntPoint TargetIdx, FCardData CardData) { }

// 타일 범위 상태 완전 초기화
void AGridManager::ClearAllRanges() 
{
	for (auto& Elem : GridMap) if (Elem.Value) Elem.Value->SetTileState(ETileState::None);
}

// 특정 좌표 타일 반환
AGridTile* AGridManager::GetTileAtCoordinate(FIntPoint Coord) 
{
	return GridMap.Contains(Coord) ? GridMap[Coord] : nullptr;
}

// 캐릭터와 커서 간의 주 방향 벡터 연산
FIntPoint AGridManager::CalculateDirection(FIntPoint Origin, FIntPoint Target) 
{
	int32 DeltaX = Target.X - Origin.X;
	int32 DeltaY = Target.Y - Origin.Y;
	if (FMath::Abs(DeltaX) >= FMath::Abs(DeltaY)) return FIntPoint(FMath::Sign(DeltaX), 0);
	return FIntPoint(0, FMath::Sign(DeltaY));
}
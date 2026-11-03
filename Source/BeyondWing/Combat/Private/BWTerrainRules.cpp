#include "BeyondWing/Combat/Public/BWTerrainRules.h"

EBWTerrainType UBWTerrainRules::GetTerrainForDimension(EDimensionType Dimension)
{
	switch (Dimension)
	{
	case EDimensionType::Patrovas: return EBWTerrainType::Resonance;
	case EDimensionType::Berend:   return EBWTerrainType::Thorns;
	case EDimensionType::Geleon:   return EBWTerrainType::Cover;
	case EDimensionType::Taiga:    return EBWTerrainType::Thicket;
	default:                       return EBWTerrainType::None;
	}
}

EBWObstacleStyle UBWTerrainRules::GetObstacleStyle(EDimensionType Dimension)
{
	switch (Dimension)
	{
	case EDimensionType::Patrovas: return EBWObstacleStyle::Crystal;
	case EDimensionType::Berend:   return EBWObstacleStyle::BreakingRock;
	case EDimensionType::Geleon:   return EBWObstacleStyle::MarblePillar;
	case EDimensionType::Taiga:    return EBWObstacleStyle::JungleTree;
	default:                       return EBWObstacleStyle::Rock;
	}
}

bool UBWTerrainRules::IsOpenAreaConnected(int32 Width, int32 Height, const TArray<FIntPoint>& Walls)
{
	TArray<FIntPoint> Open;
	for (int32 X = 0; X < Width; ++X)
	{
		for (int32 Y = 0; Y < Height; ++Y)
		{
			if (!Walls.Contains(FIntPoint(X, Y))) Open.Add(FIntPoint(X, Y));
		}
	}
	if (Open.Num() == 0) return true;

	static const FIntPoint Dirs[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	TSet<FIntPoint> Seen;
	TArray<FIntPoint> Queue = { Open[0] };
	Seen.Add(Open[0]);
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		for (const FIntPoint& Dir : Dirs)
		{
			const FIntPoint Next = Queue[Head] + Dir;
			if (Next.X < 0 || Next.X >= Width || Next.Y < 0 || Next.Y >= Height || Walls.Contains(Next) || Seen.Contains(Next)) continue;
			Seen.Add(Next);
			Queue.Add(Next);
		}
	}
	return Seen.Num() == Open.Num();
}

FBWTerrainLayout UBWTerrainRules::GenerateLayout(EDimensionType Dimension, int32 Width, int32 Height, int32 AllyZoneColumns,
	const TArray<FIntPoint>& Reserved, const TArray<FIntPoint>& PresetWalls, FRandomStream& Stream)
{
	FBWTerrainLayout Layout;
	Layout.Walls = PresetWalls;

	auto Shuffle = [&Stream](TArray<FIntPoint>& Cells)
	{
		for (int32 i = Cells.Num() - 1; i > 0; --i) Cells.Swap(i, Stream.RandRange(0, i));
	};
	auto NearOther = [](const TArray<FIntPoint>& Others, const FIntPoint& Cell)
	{
		for (const FIntPoint& O : Others)
		{
			if (FMath::Abs(O.X - Cell.X) + FMath::Abs(O.Y - Cell.Y) <= 1) return true;
		}
		return false;
	};

	// 1. 장애물: 두 진영 사이 가운데 띠(아군 구역 끝 2열 ~ 적 구역 앞 2열)에. 서로 붙지 않게 흩어 둔다
	if (PresetWalls.Num() == 0)
	{
		const int32 Count = Stream.RandRange(MinObstacles, MaxObstacles);
		TArray<FIntPoint> Candidates;
		for (int32 X = FMath::Max(1, AllyZoneColumns - 2); X <= FMath::Min(Width - 2, AllyZoneColumns + 1); ++X)
		{
			for (int32 Y = 0; Y < Height; ++Y)
			{
				if (!Reserved.Contains(FIntPoint(X, Y))) Candidates.Add(FIntPoint(X, Y));
			}
		}
		Shuffle(Candidates);
		for (const FIntPoint& Cell : Candidates)
		{
			if (Layout.Walls.Num() >= Count) break;
			if (NearOther(Layout.Walls, Cell)) continue;
			Layout.Walls.Add(Cell);
			if (!IsOpenAreaConnected(Width, Height, Layout.Walls)) Layout.Walls.Pop();
		}
	}

	// 2. 지형: 맨 앞·맨 뒤 열을 뺀 곳에, 서로·장애물과 붙지 않게
	const EBWTerrainType Type = GetTerrainForDimension(Dimension);
	if (Type != EBWTerrainType::None)
	{
		const int32 Count = Stream.RandRange(MinTerrain, MaxTerrain);
		TArray<FIntPoint> Candidates;
		for (int32 X = 1; X < Width - 1; ++X)
		{
			for (int32 Y = 0; Y < Height; ++Y)
			{
				const FIntPoint Cell(X, Y);
				if (!Reserved.Contains(Cell) && !Layout.Walls.Contains(Cell)) Candidates.Add(Cell);
			}
		}
		Shuffle(Candidates);
		TArray<FIntPoint> Placed;
		for (const FIntPoint& Cell : Candidates)
		{
			if (Placed.Num() >= Count) break;
			if (NearOther(Placed, Cell) || NearOther(Layout.Walls, Cell)) continue;
			Placed.Add(Cell);
			Layout.Terrain.Add(Cell, Type);
		}
	}
	return Layout;
}

FText UBWTerrainRules::GetTerrainName(EBWTerrainType Type)
{
	switch (Type)
	{
	case EBWTerrainType::Resonance: return FText::FromString(TEXT("마력 공명석"));
	case EBWTerrainType::Thorns:    return FText::FromString(TEXT("가시덤불"));
	case EBWTerrainType::Cover:     return FText::FromString(TEXT("사격 방벽"));
	case EBWTerrainType::Thicket:      return FText::FromString(TEXT("깊은 수풀"));
	default:                        return FText::GetEmpty();
	}
}

FText UBWTerrainRules::GetTerrainDescription(EBWTerrainType Type)
{
	switch (Type)
	{
	case EBWTerrainType::Resonance: return FText::FromString(FString::Printf(TEXT("이 칸에 선 말이 격돌에서 굴리면 최댓값 +%d"), ResonanceMaxBonus));
	case EBWTerrainType::Thorns:    return FText::FromString(FString::Printf(TEXT("들어서거나 밀려 들어오면 피해 %d (방어 무시)"), ThornsDamage));
	case EBWTerrainType::Cover:     return FText::FromString(FString::Printf(TEXT("자기 턴이 끝날 때 서 있으면 다음 자기 턴까지 피해 %d을 막는 보호막"), CoverBlock));
	case EBWTerrainType::Thicket:      return FText::FromString(TEXT("들어서면 이번 이동이 여기서 멈춘다"));
	default:                        return FText::GetEmpty();
	}
}

FText UBWTerrainRules::GetObstacleName(EBWObstacleStyle Style)
{
	switch (Style)
	{
	case EBWObstacleStyle::Crystal:     return FText::FromString(TEXT("마력 수정"));
	case EBWObstacleStyle::BreakingRock:  return FText::FromString(TEXT("거암"));
	case EBWObstacleStyle::MarblePillar: return FText::FromString(TEXT("대리석 기둥"));
	case EBWObstacleStyle::JungleTree:  return FText::FromString(TEXT("밀림 거목"));
	default:                            return FText::FromString(TEXT("바위"));
	}
}

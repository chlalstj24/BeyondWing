#include "BeyondWing/Core/Public/BWGameInstance.h"

UBWGameInstance::UBWGameInstance()
{
	ResetStats();
}

void UBWGameInstance::GenerateCombatRewards(EMapNodeType EncounterType)
{
	PendingRewards.Empty();

	// 1. 기본 골드 보상 (EncounterType에 따라 차등)
	FRewardItem GoldReward;
	GoldReward.Type = ERewardType::Gold;
	GoldReward.Amount = FMath::RandRange(20, 40);
	if (EncounterType == EMapNodeType::Elite) GoldReward.Amount *= 2;
	else if (EncounterType == EMapNodeType::Boss) GoldReward.Amount *= 5;
	GoldReward.RewardName = FString::Printf(TEXT("%d Gold"), GoldReward.Amount);
	PendingRewards.Add(GoldReward);

	// 2. 카드 선택 보상 (상자나 모닥불 아니면 기본 제공)
	if (EncounterType == EMapNodeType::Battle || EncounterType == EMapNodeType::Elite || EncounterType == EMapNodeType::Boss)
	{
		FRewardItem CardReward;
		CardReward.Type = ERewardType::CardSelection;
		CardReward.RewardName = TEXT("Add a Card to Deck");
		
		// [파트로바스 방치 효과] 보상이 강화된 채 등장
		if (NeglectedCounts[EDimensionType::Patrovas] >= 3)
		{
			CardReward.RewardName = TEXT("Add an ENHANCED Card to Deck");
		}
		PendingRewards.Add(CardReward);
	}

	// 3. [국가별 방치 특수 보상 주입]
	EDimensionType CurrentDimension = (EDimensionType)(CurrentDimIdx + 1);
	
	// 타이가 방치 2턴 이상: 카드 삭제 2회 + 장비
	if (CurrentDimension == EDimensionType::Taiga && NeglectedCounts[EDimensionType::Taiga] >= 2)
	{
		FRewardItem Removal;
		Removal.Type = ERewardType::CardRemoval;
		Removal.RewardName = TEXT("Remove 2 Basic Cards");
		PendingRewards.Add(Removal);

		FRewardItem Equip;
		Equip.Type = ERewardType::Equipment;
		Equip.RewardName = TEXT("Legendary Equipment");
		PendingRewards.Add(Equip);
	}

	// 베렌드 방치 3턴 이상: HP 20% 회복
	if (CurrentDimension == EDimensionType::Berend && NeglectedCounts[EDimensionType::Berend] >= 3)
	{
		FRewardItem Heal;
		Heal.Type = ERewardType::Heal;
		Heal.Amount = 20; // 20%
		Heal.RewardName = TEXT("Northern Primal Healing (20% HP)");
		PendingRewards.Add(Heal);
	}

	UE_LOG(LogTemp, Warning, TEXT("[BWReward] %d Rewards Generated for Encounter: %d"), PendingRewards.Num(), (uint8)EncounterType);
}

void UBWGameInstance::ClaimReward(int32 Index)
{
	if (!PendingRewards.IsValidIndex(Index)) return;

	FRewardItem Reward = PendingRewards[Index];
	
	// 여기서 실제 골드 추가, 카드 덱 수정 등의 로직 호출
	UE_LOG(LogTemp, Log, TEXT("[BWReward] Claimed: %s"), *Reward.RewardName);

	// 수령한 보상은 목록에서 제거
	PendingRewards.RemoveAt(Index);
}

FName UBWGameInstance::FindEnemyIDByDisplayName(FString DisplayName)
{
	if (!EnemyDataTable) return NAME_None;

	static const FString ContextString(TEXT("Display Name Search Context"));
	TArray<FName> RowNames = EnemyDataTable->GetRowNames();

	for (const FName& RowName : RowNames)
	{
		FBWEnemyData* Data = EnemyDataTable->FindRow<FBWEnemyData>(RowName, ContextString);
		if (Data && Data->EnemyDisplayName.Equals(DisplayName, ESearchCase::IgnoreCase))
		{
			return RowName;
		}
	}

	return NAME_None;
}

void UBWGameInstance::PrepareEncounter(EMapNodeType Type, EDimensionType Dim)
{
	PendingEnemyRequests.Empty();
	CurrentEncounterType = Type; // 보상 생성을 위해 저장

	if (!EncounterDataTable) 
	{
		UE_LOG(LogTemp, Error, TEXT("[BWGameInstance] EncounterDataTable is NULL! Please assign DT_Encounters in BP_BWGameInstance."));
		return;
	}

	TArray<FEncounterData*> EligibleEncounters;
	float TotalWeight = 0.0f;

	// 1. 현재 단계(Stage), 차원(Dimension), 타입(Battle/Elite/Boss)에 맞는 후보 필터링
	static const FString ContextString(TEXT("Encounter Filter Context"));
	EncounterDataTable->ForeachRow<FEncounterData>(ContextString, [&](const FName& Key, const FEncounterData& Row) {
		// [수정] 차원이 정확히 일치하거나, None(공통)인 경우 모두 후보에 포함
		bool bDimMatch = (Row.Dimension == Dim || Row.Dimension == EDimensionType::None);
		
		if (Row.Stage == CurrentStage && bDimMatch && Row.EncounterType == Type)
		{
			EligibleEncounters.Add(const_cast<FEncounterData*>(&Row));
			TotalWeight += Row.Weight;
		}
	});

	if (EligibleEncounters.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWGameInstance] No Eligible Encounters found for Stage %d, Dim %d, Type %d"), CurrentStage, (int32)Dim, (int32)Type);
		return;
	}

	// 2. 가중치 기반 랜덤 선택 (Weighted Random)
	float RandomRoll = FMath::FRandRange(0.0f, TotalWeight);
	float CurrentWeightSum = 0.0f;
	FEncounterData* ChosenEncounter = EligibleEncounters[0];

	for (FEncounterData* Encounter : EligibleEncounters)
	{
		CurrentWeightSum += Encounter->Weight;
		if (RandomRoll <= CurrentWeightSum)
		{
			ChosenEncounter = Encounter;
			break;
		}
	}

	// 3. 선택된 조합의 몬스터들을 스폰 목록에 추가
	for (const FEnemyPositionInfo& PosInfo : ChosenEncounter->EnemyGroup)
	{
		FEnemySpawnRequest Req;
		
		// [개선] 만약 입력한 ID가 실제 Row Name이 아니라면, Display Name에서 찾아서 보정
		FName FinalID = PosInfo.EnemyID;
		if (EnemyDataTable && !EnemyDataTable->FindRow<FBWEnemyData>(FinalID, TEXT("")))
		{
			FName SearchResult = FindEnemyIDByDisplayName(FinalID.ToString());
			if (SearchResult != NAME_None) FinalID = SearchResult;
		}

		Req.EnemyID = FinalID;
		Req.Coordinate = PosInfo.Coordinate;
		PendingEnemyRequests.Add(Req);
	}

	UE_LOG(LogTemp, Warning, TEXT("[BWGameInstance] Encounter Prepared: %d enemies will spawn."), PendingEnemyRequests.Num());
}

void UBWGameInstance::ResetStats()
{
	ConsecutiveCounts.Empty();
	NeglectedCounts.Empty();

	// 4개 국가 초기화 (1: Patrovas, 2: Geleon, 3: Berend, 4: Taiga)
	for (int32 i = 1; i <= 4; ++i)
	{
		EDimensionType Dim = (EDimensionType)i;
		ConsecutiveCounts.Add(Dim, 0);
		NeglectedCounts.Add(Dim, 0);
	}
}

void UBWGameInstance::GenerateWorldMap(int32 TotalSteps)
{
	WorldMapData.Empty();
	CurrentStep = -1;
	CurrentDimIdx = -1;
	ResetStats();

	// 고정 값: 15층으로 강제 (기획상 15층이 마지막)
	int32 MapLength = 15;

	for (int32 i = 0; i < MapLength; ++i)
	{
		FMapStep NewStep;
		for (int32 dim = 0; dim < 4; ++dim)
		{
			FMapNode Node;
			Node.StepIdx = i;
			Node.DimIdx = dim;
			Node.Dimension = (EDimensionType)(dim + 1);

			// --- [기획 규칙 1] 고정 층수 설정 ---
			if (i == 0)             Node.NodeType = EMapNodeType::Battle; // 1층: 일반몹
			else if (i == 8)        Node.NodeType = EMapNodeType::Chest;  // 9층: 상자
			else if (i == 13)       Node.NodeType = EMapNodeType::Rest;   // 14층: 모닥불
			else if (i == 14)       Node.NodeType = EMapNodeType::Boss;   // 15층: 보스
			
			// --- [기획 규칙 2] 나머지 층 확률 생성 ---
			else
			{
				int32 Rand = FMath::RandRange(0, 99);
				
				// 첫 5개층(0~4 index)에서는 엘리트 등장 불가 (0%)
				int32 EliteChance = (i < 5) ? 0 : 8;

				// 누적 확률 계산 (Shop 5, Rest 12, Mystery 22, Elite 8)
				if (Rand < 5)                      Node.NodeType = EMapNodeType::Shop;    // 5%
				else if (Rand < 5 + 12)            Node.NodeType = EMapNodeType::Rest;    // 12%
				else if (Rand < 5 + 12 + 22)       Node.NodeType = EMapNodeType::Mystery; // 22%
				else if (Rand < 5 + 12 + 22 + EliteChance) Node.NodeType = EMapNodeType::Elite;   // 8%
				else                               Node.NodeType = EMapNodeType::Battle;  // 나머지(약 53~61%) 일반전투
			}
			NewStep.Nodes.Add(Node);
		}
		WorldMapData.Add(NewStep);
	}
	UE_LOG(LogTemp, Log, TEXT("[BWMap] World Map Generated with strict rules (15 floors)."));
}

bool UBWGameInstance::CanMoveToNode(int32 TargetStep, int32 TargetDimIdx)
{
	// 1. 단계 체크 (반드시 다음 단계여야 함)
	if (TargetStep != CurrentStep + 1) return false;

	// 2. 인덱스 범위 체크
	if (TargetStep < 0 || TargetStep >= WorldMapData.Num()) return false;
	if (TargetDimIdx < 0 || TargetDimIdx >= 4) return false;

	// 3. 첫 시작인 경우 (CurrentStep == -1) 아무 노드나 선택 가능
	if (CurrentStep == -1) return true;

	// 4. [중요] 인접 차원 체크 (거리 1 이내)
	int32 Dist = FMath::Abs(CurrentDimIdx - TargetDimIdx);
	return Dist <= 1;
}

void UBWGameInstance::SelectMapNode(int32 Step, int32 DimIdx)
{
	if (!CanMoveToNode(Step, DimIdx)) return;

	CurrentStep = Step;
	CurrentDimIdx = DimIdx;
	
	// [추가] 선택한 노드를 '방문함' 상태로 영구 저장
	if (WorldMapData.IsValidIndex(Step) && WorldMapData[Step].Nodes.IsValidIndex(DimIdx))
	{
		WorldMapData[Step].Nodes[DimIdx].bIsVisited = true;
	}

	EDimensionType SelectedDim = (EDimensionType)(DimIdx + 1);

	// 통계 업데이트 (연속/방치 효과)
	for (int32 i = 1; i <= 4; ++i)
	{
		EDimensionType Dim = (EDimensionType)i;
		if (Dim == SelectedDim)
		{
			ConsecutiveCounts[Dim]++;
			NeglectedCounts[Dim] = 0; // 선택했으니 방치 초기화
		}
		else
		{
			ConsecutiveCounts[Dim] = 0; // 선택 안 했으니 연속 초기화
			NeglectedCounts[Dim]++;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[BWMap] Moved to Step %d, Dim %d (%s)"), Step, DimIdx, *UEnum::GetValueAsString(SelectedDim));
}

void UBWGameInstance::GetNodeVisibilityState(int32 Step, EMapNodeType Type, bool& bOutIsVisible, bool& bOutIsFogged)
{
	// 0. 게임 시작 전(CurrentStep == -1)일 때 1층(Step 0)은 무조건 보임
	if (CurrentStep == -1 && Step == 0)
	{
		bOutIsVisible = true;
		bOutIsFogged = false;
		return;
	}

	// 1. 이미 지난 층은 무조건 보임
	if (Step <= CurrentStep)
	{
		bOutIsVisible = true;
		bOutIsFogged = false;
		return;
	}

	// 2. 기획상 '고정된 층(Fixed Step)'의 노드들은 안개와 상관없이 처음부터 보임
	// 9층(Step 8): 고정 상자, 14층(Step 13): 고정 모닥불, 15층(Step 14): 고정 보스
	bool bIsFixedStep = (Step == 8 || Step == 13 || Step == 14);
	if (bIsFixedStep)
	{
		bOutIsVisible = true;
		bOutIsFogged = false;
		return;
	}

	// 3. 현재 위치에서 VisibilityRange 이내의 노드는 보임
	if (Step <= CurrentStep + VisibilityRange)
	{
		bOutIsVisible = true;
		bOutIsFogged = false;
	}
	else
	{
		// 안개에 가려짐
		bOutIsVisible = true; // 존재는 하되
		bOutIsFogged = true;  // 검은 덮개로 가림
	}
}

void UBWGameInstance::GetCountryStats(EDimensionType Dim, int32& OutConsecutive, int32& OutNeglected)
{
	OutConsecutive = ConsecutiveCounts.Contains(Dim) ? ConsecutiveCounts[Dim] : 0;
	OutNeglected = NeglectedCounts.Contains(Dim) ? NeglectedCounts[Dim] : 0;
}

#include "BeyondWing/UI/Public/BWMapNodeWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Kismet/GameplayStatics.h"

void UBWMapNodeWidget::InitNode(const FMapNode& InData)
{
	NodeData = InData;
	
	// 위젯 자체는 마우스를 통과시켜야 스크롤 박스가 드래그를 감지함
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	K2_SetupTypeVisuals();

	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI)
	{
		bool bNodeVisible = false;
		bool bIsHidden = false; 
		GI->GetNodeVisibilityState(NodeData.StepIdx, NodeData.NodeType, bNodeVisible, bIsHidden);

		bool bSelectable = GI->CanMoveToNode(NodeData.StepIdx, NodeData.DimIdx);
		
		// 안개 속 노드는 클릭 불가
		if (bIsHidden) bSelectable = false;

		// 연출 데이터 계산
		float FinalOpacity = 1.0f;
		FLinearColor FinalTint = FLinearColor::White;

		if (bIsHidden)
		{
			bool bIsFixed = (NodeData.StepIdx == 8 || NodeData.StepIdx == 13 || NodeData.StepIdx == 14);
			FinalOpacity = bIsFixed ? 0.1f : 0.0f; 
			FinalTint = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
		}
		else if (NodeData.bIsVisited)
		{
			FinalOpacity = 0.3f;
			FinalTint = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
		}

		// [강력한 조치] 안개가 아닌 노드는 블루프린트 설정과 상관없이 강제로 보이게 설정
		if (!bIsHidden)
		{
			SetVisibility(ESlateVisibility::Visible);
		}

		K2_UpdateStateVisuals(bSelectable, NodeData.bIsVisited, bIsHidden);
		
		SetRenderOpacity(FinalOpacity);
		SetColorAndOpacity(FinalTint);
	}
}

void UBWMapNodeWidget::HandleClick()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI && GI->CanMoveToNode(NodeData.StepIdx, NodeData.DimIdx))
	{
		// 1. 인카운터 데이터 준비 (랜덤 주사위 굴리기)
		GI->PrepareEncounter(NodeData.NodeType, NodeData.Dimension);

		// 2. 맵 노드 상태 업데이트
		GI->SelectMapNode(NodeData.StepIdx, NodeData.DimIdx);
		
		FString TargetLevelName = TEXT("BattleLevel");
		switch (NodeData.Dimension)
		{
		case EDimensionType::Patrovas: TargetLevelName = TEXT("Level_Patrovas"); break;
		case EDimensionType::Berend:   TargetLevelName = TEXT("Level_Berend"); break;
		case EDimensionType::Geleon:   TargetLevelName = TEXT("Level_Geleon"); break;
		case EDimensionType::Taiga:    TargetLevelName = TEXT("Level_Taiga"); break;
		default: break;
		}

		if (NodeData.NodeType == EMapNodeType::Boss) TargetLevelName = TEXT("Level_Boss");

		UGameplayStatics::OpenLevel(GetWorld(), FName(*TargetLevelName));
	}
}

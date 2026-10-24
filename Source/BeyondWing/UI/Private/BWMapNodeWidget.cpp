#include "BeyondWing/UI/Public/BWMapNodeWidget.h"
#include "BeyondWing/UI/Public/BWMapWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/Core/Public/BWMapRules.h"
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
			FinalOpacity = 0.0f;
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
	// [수정] 이동·도착 처리(차원문 → 방치 보너스 → 과부하 → 노드 진입)는 지도 화면이 한다
	if (UBWMapWidget* Map = GetTypedOuter<UBWMapWidget>())
	{
		Map->TryMoveTo(NodeData.StepIdx, NodeData.DimIdx);
	}
}

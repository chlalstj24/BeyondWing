#include "BeyondWing/UI/Public/BWMapWidget.h"
#include "BeyondWing/UI/Public/BWMapNodeWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"

void UBWMapWidget::RenderMap()
{
	if (!MapMainContainer || !NodeWidgetClass) return;

	// [강력한 조치] UI 전용 모드에서 탈출하여 게임 월드 입력을 허용
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

	SetIsFocusable(true);

	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	if (GI->WorldMapData.Num() == 0) GI->GenerateWorldMap(15);

	if (MapScrollBox)
	{
		MapScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		MapScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
		MapScrollBox->SetOrientation(Orient_Horizontal);
		MapScrollBox->SetAllowRightClickDragScrolling(true);
		MapScrollBox->SetWheelScrollMultiplier(ScrollSensitivity);
	}

	MapMainContainer->ClearChildren();
	MapMainContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	for (int32 step = 0; step < GI->WorldMapData.Num(); ++step)
	{
		UVerticalBox* ColumnBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (ColumnBox)
		{
			ColumnBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			const FMapStep& StepData = GI->WorldMapData[step];
			for (int32 dim = 0; dim < StepData.Nodes.Num(); ++dim)
			{
				USizeBox* NodeWrapper = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				NodeWrapper->SetWidthOverride(NodeSize.X);
				NodeWrapper->SetHeightOverride(NodeSize.Y);
				NodeWrapper->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

				UBWMapNodeWidget* NewNode = CreateWidget<UBWMapNodeWidget>(this, NodeWidgetClass);
				if (NewNode)
				{
					NewNode->InitNode(StepData.Nodes[dim]);
					NodeWrapper->AddChild(NewNode);
					
					UVerticalBoxSlot* NodeSlot = ColumnBox->AddChildToVerticalBox(NodeWrapper);
					if (NodeSlot)
					{
						NodeSlot->SetHorizontalAlignment(HAlign_Center);
						NodeSlot->SetVerticalAlignment(VAlign_Center);
						NodeSlot->SetPadding(FMargin(0.0f, RowSpacing / 2.0f));
					}
				}
			}
			
			UPanelSlot* ColumnSlot = MapMainContainer->AddChild(ColumnBox);
			if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(ColumnSlot))
			{
				HSlot->SetPadding(FMargin(ColumnSpacing / 2.0f, 0.0f));
				HSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
	ScrollToCurrentStep();
}

FReply UBWMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDraggingMap = true;
		LastMousePos = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UBWMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDraggingMap = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UBWMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDraggingMap && MapScrollBox)
	{
		FVector2D CurrentMousePos = InMouseEvent.GetScreenSpacePosition();
		FVector2D Delta = LastMousePos - CurrentMousePos;
		
		float NewOffset = MapScrollBox->GetScrollOffset() + Delta.X;
		MapScrollBox->SetScrollOffset(NewOffset);
		
		LastMousePos = CurrentMousePos;
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UBWMapWidget::ScrollToCurrentStep()
{
	if (!MapScrollBox) return;
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	float ScrollOffset = (GI->CurrentStep) * (NodeSize.X + ColumnSpacing);
	ScrollOffset = FMath::Max(0.0f, ScrollOffset - 200.0f);
	MapScrollBox->SetScrollOffset(ScrollOffset);
}

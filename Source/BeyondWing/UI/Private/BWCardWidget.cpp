#include "BWCardWidget.h"
#include "BWHandWidget.h"
#include "BWPlayerController.h"
#include "InputCoreTypes.h"

void UBWCardWidget::SetHovered(bool bInHovered)
{
	if (bIsHovered != bInHovered)
	{
		bIsHovered = bInHovered;
		OnHoverStateChanged(bIsHovered);

		if (OwningHand)
		{
			if (bIsHovered)
			{
				OwningHand->OnCardHovered(this);
			}
			else
			{
				OwningHand->OnCardUnhovered(this);
			}
		}
	}
}

void UBWCardWidget::SetSelected()
{
	UE_LOG(LogTemp, Warning, TEXT("[BWCardWidget] SetSelected Called for Index: %d (%s)"), HandIndex, *CardData.CardName);
	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->SelectCard(HandIndex);
	}
}

UTexture2D* UBWCardWidget::GetCardTexture(TSoftObjectPtr<UTexture2D> SoftPtr)
{
	if (SoftPtr.IsNull()) 
	{
		return nullptr;
	}

	UTexture2D* LoadedTex = SoftPtr.Get();
	if (!LoadedTex)
	{
		LoadedTex = SoftPtr.LoadSynchronous();
	}

	return LoadedTex;
}

FReply UBWCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// C++ 판정을 비우고 블루프린트 버튼이 처리하도록 함
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

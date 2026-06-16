#include "BeyondWing/UI/Public/BWTooltipWidget.h"
#include "Components/TextBlock.h"
#include "BeyondWing/Character/Public/BWCharacter.h"

void UBWTooltipWidget::SetTooltipTitle(const FString& InTitle)
{
	if (TXT_Title)
	{
		TXT_Title->SetText(FText::FromString(InTitle));
	}
}

void UBWTooltipWidget::SetTooltipDescription(const FString& RawDescription, int32 Value, int32 Dist, int32 Range)
{
	if (TXT_Description)
	{
		TXT_Description->SetAutoWrapText(true); // 자동 줄넘김 활성화
		TXT_Description->SetWrapTextAt(MaxWrapWidth); // [추가] 지정한 너비에서 줄바꿈
		TXT_Description->SetJustification(ETextJustify::Left);
		
		FString Formatted = FormatTemplate(RawDescription, Value, Dist, Range);
		TXT_Description->SetText(FText::FromString(Formatted));
	}
}

void UBWTooltipWidget::SetupFromCardData(const FBWCardData& CardData, ABWCharacter* OwnerChar)
{
	SetTooltipTitle(CardData.CardName);
	
	int32 FinalDamage = 0;
	if (OwnerChar)
	{
		bool bCrit;
		FinalDamage = OwnerChar->CalculateFinalCardDamage(CardData, bCrit);
	}
	else
	{
		FinalDamage = FMath::RoundToInt(CardData.BaseDamage);
	}

	SetTooltipDescription(CardData.Description, FinalDamage, 0, CardData.Range);
}

void UBWTooltipWidget::SetupFromEnemyPattern(const FBWEnemyPattern& Pattern)
{
	SetTooltipTitle(Pattern.ActionName);
	SetTooltipDescription(Pattern.Description, Pattern.Value, Pattern.MoveDistance, Pattern.Range);
}

FString UBWTooltipWidget::FormatTemplate(const FString& Raw, int32 V, int32 D, int32 R)
{
	FString Result = Raw;
	Result = Result.Replace(TEXT("{V}"), *FString::FromInt(V));
	Result = Result.Replace(TEXT("{D}"), *FString::FromInt(D));
	Result = Result.Replace(TEXT("{R}"), *FString::FromInt(R));
	return Result;
}

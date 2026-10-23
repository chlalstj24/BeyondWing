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
	// [수정] {V}=숫자 범위 최댓값, {R}=최솟값 (적 스킬은 합에서 이 범위로 굴린다)
	SetTooltipDescription(Pattern.Description, Pattern.SkillRange.Max, 0, Pattern.SkillRange.Min);
}

FString UBWTooltipWidget::FormatTemplate(const FString& Raw, int32 V, int32 D, int32 R)
{
	FString Result = Raw;
	Result = Result.Replace(TEXT("{V}"), *FString::FromInt(V));
	Result = Result.Replace(TEXT("{D}"), *FString::FromInt(D));
	Result = Result.Replace(TEXT("{R}"), *FString::FromInt(R));
	return Result;
}

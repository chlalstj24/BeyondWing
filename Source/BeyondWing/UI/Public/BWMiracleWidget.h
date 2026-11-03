#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWMiracleWidget.generated.h"

class UBorder;
class UTextBlock;

// 세트 4개 '기적' 전용 연출: 화면이 세트 색으로 번쩍이고, 어두워진 위로 이름이 내리찍힌 뒤 가로 띠가 벌어지며 사라진다.
// 입력을 막지 않고, 시간 배율과 무관하게 실제 시간으로 돈다.
UCLASS()
class BEYONDWING_API UBWMiracleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void PlayMiracle(const FString& Title, const FString& Sub, const FLinearColor& Accent);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildLayout();

	UPROPERTY() UBorder* Flash = nullptr;
	UPROPERTY() UBorder* Dim = nullptr;
	UPROPERTY() UBorder* BandTop = nullptr;
	UPROPERTY() UBorder* BandBottom = nullptr;
	UPROPERTY() UTextBlock* TXT_Title = nullptr;
	UPROPERTY() UTextBlock* TXT_Sub = nullptr;
	double StartedAt = -1.0;
};

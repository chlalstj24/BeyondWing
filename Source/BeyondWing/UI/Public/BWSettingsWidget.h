#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWSettingsWidget.generated.h"

class UButton;
class UTextBlock;

// 환경설정 화면 (타이틀에서 연다). 화면 모드·해상도·그래픽 품질·수직 동기화·프레임 제한·전체 음량.
// 값은 ◀ ▶로 고르고 '적용'을 눌러야 바뀐다. 화면 관련 값은 엔진의 GameUserSettings(Saved/Config/.../GameUserSettings.ini)에,
// 전체 음량은 같은 파일의 [/Script/BeyondWing.BWSettings] MasterVolume에 저장된다.
UCLASS()
class BEYONDWING_API UBWSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "BW Settings") void ApplySettings();
	UFUNCTION(BlueprintCallable, Category = "BW Settings") void ResetToDefaults();
	UFUNCTION(BlueprintCallable, Category = "BW Settings") void Close();

	// 값 하나를 앞/뒤로 바꾼다 (Row: 아래 EBWSettingRow 순서)
	UFUNCTION(BlueprintCallable, Category = "BW Settings") void StepOption(int32 Row, int32 Delta);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	struct FOptionRow
	{
		FText Label;
		TArray<FText> Choices;
		int32 Index = 0;
		TWeakObjectPtr<UTextBlock> ValueText;
	};
	TArray<FOptionRow> Rows;
	TArray<FIntPoint> Resolutions;

	void BuildRows();
	void ReadCurrentSettings();
	void RefreshRow(int32 Row);
	void BuildLayout();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;

	// ◀ ▶ 버튼은 행마다 두 개라 동적 바인딩 대신 버튼 → (행, 방향) 표로 찾는다
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> StepButtons;
	TArray<FIntPoint> StepButtonTargets; // (행, -1|+1)
	UFUNCTION() void OnStepPressed();
	UFUNCTION() void OnApplyClicked() { ApplySettings(); }
	UFUNCTION() void OnDefaultsClicked() { ResetToDefaults(); }
	UFUNCTION() void OnCloseClicked() { Close(); }
};

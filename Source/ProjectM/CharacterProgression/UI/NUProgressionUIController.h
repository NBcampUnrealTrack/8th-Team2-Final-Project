#pragma once

#include "CoreMinimal.h"
#include "UI/Controller/NUUIController.h"
#include "NUProgressionUIController.generated.h"

class UNUCharacterStatsWidget;
class UNUAdvancementSelectionWidget;
class UNUEquipmentComponent;
class UNUJobSelectionWidget;
class UNUPlayerSkillWindowWidget;
class UNUProgressionComponent;
class UNUProgressionDebugWidget;
class UNUProgressionScreenWidget;

// 캐릭터 성장 관련 UI의 생성, 열기/닫기, 이벤트 연결을 관리
UCLASS()
class PROJECTM_API UNUProgressionUIController : public UNUUIController
{
	GENERATED_BODY()

public:
	void OpenJobSelection(FSimpleDelegate InSelectionCompleted = FSimpleDelegate());
	void CloseJobSelection();
	void ToggleSkills();
	void OpenSkills();
	void CloseSkills();
	bool IsSkillsOpen() const;

	void ToggleCharacterStats();
	void ToggleProgressionGraph();
	void ToggleProgressionDebug();

	bool IsCharacterStatsOpen() const;

	// 스탯 창 위젯 --- 닫힌 뒤에도 캐시가 남으므로 IsCharacterStatsOpen과 같이 쓸 것.
	// (정보창의 [스탯 자세히]가 창을 자기 옆에 붙일 때 위치를 잡는 용도)
	UNUCharacterStatsWidget* GetCharacterStatsWidget() const { return CharacterStatsWidget; }

protected:
	// 플레이어가 바뀌면 기존 UI와 위젯 참조를 정리
	virtual void OnPlayerControllerChanged(APlayerController* NewPC) override;
	virtual void OnDeinitialize() override;

private:
	void ResetWindows();

	UPROPERTY(Transient)
	TObjectPtr<UNUJobSelectionWidget> JobSelectionWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNUPlayerSkillWindowWidget> SkillWindowWidget;

	UNUProgressionComponent* ResolveProgression() const;
	UNUEquipmentComponent* ResolveEquipment() const;

	void OpenCharacterStats();
	void CloseCharacterStats();
	void OpenProgressionGraph();
	void CloseProgressionGraph();
	bool IsProgressionGraphOpen() const;

	// 선택한 성장 노드에서 2차 전직 선택 창을 연다.
	void OpenAdvancementSelection(int32 NodeId);
	void CloseAdvancementSelection();
	void OpenProgressionDebug();
	void CloseProgressionDebug();
	bool IsProgressionDebugOpen() const;

	// 장비 변경 시 열린 스탯 창을 갱신하도록 이벤트를 연결
	void BindEquipment(UNUEquipmentComponent* Equipment);
	void UnbindEquipment();

	// 성장 그래프를 열기 전에 필요한 170 성장 응답 이벤트를 연결
	void BindPendingGraphRequest(UNUProgressionComponent* Progression);
	void UnbindPendingGraphRequest();

	// 스탯 창이 열려 있으면 장비 변경이 반영된 스탯을 다시 요청
	UFUNCTION()
	void HandleEquipmentChanged();

	// 대기 중인 성장 응답이 완료되면 성장 그래프를 연다.
	UFUNCTION()
	void HandleProgressionRequestStateChanged();

	UPROPERTY(Transient)
	TObjectPtr<UNUCharacterStatsWidget> CharacterStatsWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNUProgressionScreenWidget> ProgressionScreenWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNUAdvancementSelectionWidget> AdvancementSelectionWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNUProgressionDebugWidget> ProgressionDebugWidget;

	TWeakObjectPtr<UNUEquipmentComponent> BoundEquipment;
	TWeakObjectPtr<UNUProgressionComponent> PendingGraphProgression;
	bool bOpenProgressionGraphAfterProgressionResponse = false;
};

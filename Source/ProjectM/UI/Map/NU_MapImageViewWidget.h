// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Base/NU_BaseUserWidget.h"
#include "UI/Map/NUMapImageSettings.h"
#include "NU_MapImageViewWidget.generated.h"

class UCanvasPanel;
class UImage;
class ULevel;
class UNUWorldMarkerWidget;

/*
구운 지도 이미지 위에 마커를 얹는 "지도 뷰" 조각.

미니맵과 전체 지도가 같은 그림과 같은 좌표 변환을 쓰도록 조각 하나로 만들고,
두 WBP 가 각자 크기만 다르게 배치한다 --- 두 곳에 나뉘면 언젠가 어긋난다.

이름 계약 (WBP 에 같은 이름으로 배치)
  Img_Map         지도 텍스처가 들어갈 이미지. 텍스처가 정사각이므로 정사각 비율로 둔다
  Canvas_Markers  Img_Map 과 정확히 같은 영역을 덮는 캔버스. 마커는 전부 이 안에 있어야 한다
  Marker_Player   플레이어 위치 마커 (선택). 없으면 플레이어 표시를 건너뛴다

지도는 월드가 아니라 플레이어가 지금 있는 "공간"으로 고른다. 사냥터는 마을 월드에 스트리밍 레벨로
옮겨 붙으므로, 그 레벨 이름으로 표를 찾고 좌표는 레벨 배치를 되돌려서 쓴다.
공간 확인과 아이콘 갱신은 0.5초 타이머다 --- 지도 없는 공간에서 접혀 있는 동안은 Tick 이 안 돈다.
지도가 없는 공간이면 조각째 Collapsed 로 접는다 --- 빈 틀이 남지 않게.

아이콘: 지역 이동 포탈(건물 출입구 제외)과 월드 마커(NPC 머리 위 퀘스트 표시)를 지도에 비춘다.
월드 마커는 컴포넌트가 이미 정한 태그를 읽기만 한다 --- 판정이 두 곳에 생기지 않게. 어떤 태그를 어떤
그림으로 보일지는 아이콘 WBP(IconMarkerClass)의 태그별 텍스처 표가 정하고, 표에 없는 태그는 안 보인다.

마커는 캔버스 슬롯 "앵커"를 UV 로 쓰므로 조각의 화면 크기와 무관하게 맞고,
렌더 배율을 1/Zoom 으로 되돌려서 확대해도 마커 크기는 그대로다.
플레이어 마커 갱신은 Tick 이지만 슬롯 쓰기는 UV 가 실제로 바뀔 때만 한다.

미니맵 모드에서 EdgeTags 아이콘(보고 가능·목표 지점·받을 퀘스트)이 창 밖이면 테두리 안쪽(EdgeInset)에 붙이고,
아이콘 WBP 의 꼭지를 실제 방향으로 돌린다. bRotatePlayerMarker 면 플레이어 마커가 이동 방향으로 돈다.
*/
UCLASS()
class PROJECTM_API UNU_MapImageViewWidget : public UNU_BaseUserWidget
{
	GENERATED_BODY()

public:
	UNU_MapImageViewWidget(const FObjectInitializer& ObjectInitializer);

	// 월드 좌표의 마커를 지도 위 해당 자리에 놓는다. 마커는 Canvas_Markers 의 자식이어야 한다.
	// 지도 범위 밖이면 마커를 접는다
	UFUNCTION(BlueprintCallable, Category = "NU|Map")
	void PlaceMarkerAtWorld(UWidget* Marker, const FVector& WorldLocation);

	// 지금 공간에 지도가 있는지
	UFUNCTION(BlueprintPure, Category = "NU|Map")
	bool HasMapImage() const { return bHasEntry; }

	// 지도가 생기거나 사라질 때(공간 이동). 미니맵 틀이 자신을 접고 펴는 데 쓴다
	FSimpleMulticastDelegate OnMapImageChanged;

	// 휠 줌(커서 중심)과 확대 중 드래그 팬. 전체 지도 인스턴스에서만 켠다 --- 미니맵은 고정
	UPROPERTY(EditAnywhere, Category = "NU|Map")
	bool bEnableZoom = false;

	UPROPERTY(EditAnywhere, Category = "NU|Map", meta = (ClampMin = "1.0", EditCondition = "bEnableZoom"))
	float MaxZoom = 4.0f;

	// 플레이어를 화면 중앙에 고정하고 지도가 움직이는 미니맵 모드. bEnableZoom 과 같이 켜지 않는다
	UPROPERTY(EditAnywhere, Category = "NU|Map")
	bool bCenterOnPlayer = false;

	// 미니맵 모드의 고정 배율 --- 3이면 지도 전체의 1/3 폭이 창에 보인다
	UPROPERTY(EditAnywhere, Category = "NU|Map", meta = (ClampMin = "1.0", EditCondition = "bCenterOnPlayer"))
	float CenterZoom = 3.0f;

	// 포탈·퀘스트 아이콘 위젯. 월드 마커와 같은 베이스라 태그별 그림을 WBP 가 정한다. 비우면 아이콘을 안 그린다
	UPROPERTY(EditAnywhere, Category = "NU|Map")
	TSubclassOf<UNUWorldMarkerWidget> IconMarkerClass;

	// 아이콘의 화면 크기. 줌과 무관하게 이 크기로 보인다
	UPROPERTY(EditAnywhere, Category = "NU|Map")
	FVector2D IconMarkerSize = FVector2D(20.0f, 20.0f);

	// 플레이어 마커를 이동 방향으로 돌린다 (그림이 위를 가리키게 그려져 있어야 한다). 멈추면 마지막 방향을 유지
	UPROPERTY(EditAnywhere, Category = "NU|Map")
	bool bRotatePlayerMarker = false;

	// 방향이 따라붙는 빠르기 (1초에 남은 각도의 몇 배)
	UPROPERTY(EditAnywhere, Category = "NU|Map", meta = (ClampMin = "1.0", EditCondition = "bRotatePlayerMarker"))
	float PlayerTurnSpeed = 12.0f;

	// 미니맵 모드에서 창 밖이어도 가장자리에 붙여 보일 아이콘 태그. 비우면 붙이지 않는다
	UPROPERTY(EditAnywhere, Category = "NU|Map", meta = (EditCondition = "bCenterOnPlayer"))
	FGameplayTagContainer EdgeTags;

	// 가장자리에 붙일 때 창 테두리에서 들어올 거리 (화면 단위). 둥근 모서리에 가리지 않을 만큼
	UPROPERTY(EditAnywhere, Category = "NU|Map", meta = (EditCondition = "bCenterOnPlayer"))
	float EdgeInset = 15.0f;

protected:
	virtual void InitializeWidget() override;
	virtual void ReleaseWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void HandleRefreshTimer();

	// 지금 공간을 확인하고, 바뀌었으면 지도 그림·레벨 배치·줌을 새로 잡는다
	void RefreshSpace();

	// 포탈·월드 마커를 모아 아이콘을 놓는다. 아이콘 위젯은 풀로 재사용한다
	void RefreshIconMarkers();

	// 월드 좌표 => 지도 UV (레벨 배치를 되돌린 뒤 표의 변환)
	FVector2D WorldToUV(const FVector& WorldLocation) const;

	// UV 가 0~1 이면 앵커를 옮기고, 밖이면 마커를 접는다
	void PlaceMarkerAtUV(UWidget* Marker, const FVector2D& UV);

	// 마커 렌더 배율을 1/Zoom 으로 --- 확대해도 마커 크기가 그대로 보이게
	void ApplyMarkerScale();

	// 아이콘 하나를 실제 UV 에 놓는다. 미니맵 창 밖이고 EdgeTags 면 테두리 안쪽에 붙이고 꼭지를 돌린다
	void PlaceIcon(int32 Index);

	// 이동 방향을 따라 플레이어 마커를 돌린다
	void UpdatePlayerHeading(const APawn* Pawn, float DeltaTime);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_Map;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> Canvas_Markers;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Marker_Player;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUWorldMarkerWidget>> IconMarkers;

	// 아이콘마다 실제 지도 UV (가장자리에 붙이기 전). IconMarkers 와 같은 순서
	TArray<FVector2D> IconUVs;

	// 마지막으로 본 조각 크기 --- 타이머에서 가장자리를 계산할 때 쓴다
	FVector2D LastLocalSize = FVector2D::ZeroVector;

	float PlayerHeading = 0.0f;
	float AppliedPlayerHeading = 1000.0f;
	bool bPlayerHeadingSet = false;

	// 설정 표에서 복사해 둔 현재 공간 항목. CDO 배열의 포인터를 들고 있지 않는다 (에디터에서 표가 편집될 수 있다)
	FNUMapImageEntry CachedEntry;
	bool bHasEntry = false;

	// 마지막으로 판정한 공간과 그 레벨의 배치. 공간 비교는 레벨 포인터로 한다
	TWeakObjectPtr<const ULevel> CachedLevel;
	bool bSpaceResolved = false;
	FTransform LevelToWorld = FTransform::Identity;

	FTimerHandle RefreshTimer;

	// 마지막으로 적용한 플레이어 마커 UV --- 안 바뀌면 슬롯을 건너뛴다
	FVector2D LastPlayerUV = FVector2D(-1.0f, -1.0f);

	// 줌 상태. 렌더 변환(피벗 0,0)으로 적용해서 마커·이미지가 같이 움직인다
	// 화면점 = Pan + Zoom * 콘텐츠점
	float Zoom = 1.0f;
	FVector2D Pan = FVector2D::ZeroVector;
	bool bPanning = false;

	// 콘텐츠의 PivotLocal 지점이 화면에서 제자리에 있도록 팬을 다시 풀며 줌을 적용한다
	void ApplyZoom(const FGeometry& Geometry, float NewZoom, const FVector2D& PivotLocal);

	// 확대분이 조각 밖으로 빈틈을 만들지 않게 팬을 가둔다
	void ClampPan(const FVector2D& LocalSize);
};

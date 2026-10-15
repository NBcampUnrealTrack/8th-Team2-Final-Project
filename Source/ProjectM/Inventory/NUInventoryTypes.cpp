// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/NUInventoryTypes.h"

#include "Inventory/NUInventoryComponent.h"

DEFINE_LOG_CATEGORY(LogNUInventory);

namespace NUInventory
{
	FText GetResultMessage(int32 ResultCode)
	{
		// gRPC 가이드 결과 코드표 중 아이템 패킷(60~66)과 상점 패킷(180/181)이 실제로 받는 값들
		switch (ResultCode)
		{
		case 1204: return NSLOCTEXT("NUInventory", "RC1204", "캐릭터를 먼저 선택해야 합니다.");
		case 1300: return NSLOCTEXT("NUInventory", "RC1300", "그 저장소로는 옮길 수 없습니다.");
		case 1301: return NSLOCTEXT("NUInventory", "RC1301", "칸 번호가 올바르지 않습니다.");
		case 1302: return NSLOCTEXT("NUInventory", "RC1302", "집으려는 칸이 비어 있습니다.");
		case 1303: return NSLOCTEXT("NUInventory", "RC1303", "같은 칸으로는 옮길 수 없습니다.");
		case 1304: return NSLOCTEXT("NUInventory", "RC1304", "아이템 처리에 실패했습니다. 잠시 후 다시 시도해 주세요.");
		case 1305: return NSLOCTEXT("NUInventory", "RC1305", "빈 칸이 없습니다.");
		case 1306: return NSLOCTEXT("NUInventory", "RC1306", "장착할 수 없는 아이템입니다.");
		case 1307: return NSLOCTEXT("NUInventory", "RC1307", "부위가 맞지 않습니다.");
		case 1308: return NSLOCTEXT("NUInventory", "RC1308", "레벨이 낮아 장착할 수 없습니다.");
		case 1309: return NSLOCTEXT("NUInventory", "RC1309", "나눌 수 없는 아이템입니다.");
		case 1310: return NSLOCTEXT("NUInventory", "RC1310", "나눌 개수가 올바르지 않습니다.");
		case 1311: return NSLOCTEXT("NUInventory", "RC1311", "빈 칸에만 놓을 수 있습니다.");
		case 1312: return NSLOCTEXT("NUInventory", "RC1312", "가방 내용이 서버와 다릅니다.");
		case 1313: return NSLOCTEXT("NUInventory", "RC1313", "한 칸에 담을 수 있는 개수를 넘었습니다.");
		case 1314: return NSLOCTEXT("NUInventory", "RC1314", "그 칸에는 둘 수 없는 아이템입니다.");
		case 2200: return NSLOCTEXT("NUInventory", "RC2200", "상점 요청이 올바르지 않습니다.");
		case 2201: return NSLOCTEXT("NUInventory", "RC2201", "가방에 자리가 부족합니다.");
		case 2202: return NSLOCTEXT("NUInventory", "RC2202", "판매할 수량이 부족합니다.");
		case 2203: return NSLOCTEXT("NUInventory", "RC2203", "상점 처리에 실패했습니다. 잠시 후 다시 시도해 주세요.");
		case 2204: return NSLOCTEXT("NUInventory", "RC2204", "골드가 부족합니다.");
		case 2205: return NSLOCTEXT("NUInventory", "RC2205", "이 상점에서 팔지 않는 물건입니다.");
		case 2206: return NSLOCTEXT("NUInventory", "RC2206", "레벨이 낮아 구매할 수 없습니다.");
		case 2207: return NSLOCTEXT("NUInventory", "RC2207", "판매할 수 없는 아이템입니다.");
		case 2208: return NSLOCTEXT("NUInventory", "RC2208", "가격 정보가 서버와 다릅니다. 잠시 후 다시 시도해 주세요.");

		default:
			return FText::Format(
				NSLOCTEXT("NUInventory", "RCUnknown", "아이템 요청이 거부되었습니다. (코드 {0})"),
				FText::AsNumber(ResultCode));
		}
	}
}

void FNUInventoryList::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	for (const int32 Index : AddedIndices)
	{
		UE_LOG(LogNUInventory, Verbose, TEXT("[복제] 추가 %s x%d"), *Items[Index].ItemRowName.ToString(), Items[Index].Count);
	}
}

void FNUInventoryList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	for (const int32 Index : ChangedIndices)
	{
		UE_LOG(LogNUInventory, Verbose, TEXT("[복제] 변경 %s x%d"), *Items[Index].ItemRowName.ToString(), Items[Index].Count);
	}
}

void FNUInventoryList::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	for (const int32 Index : RemovedIndices)
	{
		UE_LOG(LogNUInventory, Log, TEXT("[복제] 제거 %s"), *Items[Index].ItemRowName.ToString());
	}
}

void FNUInventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (OwnerComponent != nullptr)
	{
		OwnerComponent->HandleInventoryReplicated();
	}
}

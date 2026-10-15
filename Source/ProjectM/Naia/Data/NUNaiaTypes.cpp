#include "Naia/Data/NUNaiaTypes.h"

#include "Naia/Component/NUNaiaRosterComponent.h"

void FNUNaiaList::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
    UE_LOG(LogTemp, Verbose, TEXT("보관함 항목 추가 %d개, 총 %d"), AddedIndices.Num(), FinalSize);
}

void FNUNaiaList::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
    UE_LOG(LogTemp, Verbose, TEXT("보관함 항목 변경 %d개, 총 %d"), ChangedIndices.Num(), FinalSize);
}

void FNUNaiaList::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
    UE_LOG(LogTemp, Verbose, TEXT("보관함 항목 제거 %d개, 총 %d"), RemovedIndices.Num(), FinalSize);
}

void FNUNaiaList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
    if (IsValid(OwnerComponent))
    {
        OwnerComponent->HandleStorageReplicated();
    }
}

#include "UI/HUD/NUFieldHUDVisibility.h"

#include "UObject/Object.h"

void FNUFieldHUDVisibility::RequestHidden(const UObject* Owner, FName Reason, bool bHide)
{
	PruneInvalidOwners();
	if (!IsValid(Owner) || Reason.IsNone())
	{
		return;
	}
	const int32 Index = Requests.IndexOfByPredicate([Owner, Reason](const FRequest& Entry)
	{
		return Entry.Owner == Owner && Entry.Reason == Reason;
	});
	if (bHide && Index == INDEX_NONE)
	{
		FRequest& Entry = Requests.AddDefaulted_GetRef();
		Entry.Owner = Owner;
		Entry.Reason = Reason;
	}
	else if (!bHide && Index != INDEX_NONE)
	{
		Requests.RemoveAtSwap(Index);
	}
}

void FNUFieldHUDVisibility::ReleaseOwner(const UObject* Owner)
{
	Requests.RemoveAll([Owner](const FRequest& Entry)
	{
		return !Entry.Owner.IsValid() || Entry.Owner == Owner;
	});
}

void FNUFieldHUDVisibility::PruneInvalidOwners()
{
	Requests.RemoveAll([](const FRequest& Entry) { return !Entry.Owner.IsValid(); });
}

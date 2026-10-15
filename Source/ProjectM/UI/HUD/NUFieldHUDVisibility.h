#pragma once

#include "CoreMinimal.h"

// Per-player preference plus temporary suppression. A feature can release only its own reasons.
class FNUFieldHUDVisibility
{
public:
	void SetUserVisible(bool bVisible) { bUserVisible = bVisible; }
	bool IsUserVisible() const { return bUserVisible; }
	bool IsVisible() const { return bUserVisible && !HasRequests(); }
	bool HasRequests() const { return !Requests.IsEmpty(); }

	void RequestHidden(const UObject* Owner, FName Reason, bool bHide);
	void ReleaseOwner(const UObject* Owner);
	void PruneInvalidOwners();
	// Temporary requests are scoped to the current player controller/session; preference survives.
	void ClearRequests() { Requests.Reset(); }

private:
	struct FRequest
	{
		TWeakObjectPtr<const UObject> Owner;
		FName Reason;
	};
	TArray<FRequest> Requests;
	bool bUserVisible = true;
};

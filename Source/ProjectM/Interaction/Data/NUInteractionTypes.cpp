#include "Interaction/Data/NUInteractionTypes.h"

bool FNUInteractionContext::IsValid() const
{
	return ::IsValid(Interactor) && ::IsValid(Target);
}

bool FNUInteractionOptionHandle::IsValid() const
{
	return OptionType.IsValid() && !OptionId.IsNone();
}

bool FNUInteractionOptionHandle::operator==(const FNUInteractionOptionHandle& Other) const
{
	return OptionType == Other.OptionType && OptionId == Other.OptionId;
}

bool FNUInteractionOption::IsValid() const
{
	return Handle.IsValid() && !DisplayText.IsEmpty();
}

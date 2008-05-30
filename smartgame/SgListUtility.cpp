//----------------------------------------------------------------------------
/** @file SgListUtility.cpp
    See SgListUtility.h
*/
//----------------------------------------------------------------------------

#include "SgSystem.h"
#include "SgListUtility.h"

#include <algorithm>

using namespace std;

//----------------------------------------------------------------------------

void SgListUtility::Intersection(SgList<int>* list, const SgList<int>& list2)
{
     SgList<int> newList;
     for (SgListIterator<int> it(*list); it; ++it)
     {
         // AR speed up by hash tags, if used in time-critical code
         if (list2.Contains(*it))
             newList.Append(*it);
     }
     newList.SwapWith(list);
}

void SgListUtility::Difference(SgList<int>* list, const SgList<int>& list2)
{
    // AR speed up by using tags, if used in time-critical code.
    for (SgListIterator<int> it(list2); it; ++it)
        list->Exclude(*it);
}

void SgListUtility::Reverse(SgList<int>* list)
{
    SgList<int> rev;
    for (SgListIterator<int> it(*list); it; ++it)
        rev.Push(*it);
    if (rev.NonEmpty())
    {
        SG_ASSERT(list->Top() == rev.Tail());
        SG_ASSERT(list->Tail() == rev.Top());
        rev.SwapWith(list);
    }
}

//----------------------------------------------------------------------------

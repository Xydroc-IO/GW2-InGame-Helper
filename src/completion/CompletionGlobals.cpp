#include "CompletionShared.h"

namespace CompletionDetail
{
	bool gAchFocus = false;
	bool gAchPlaceOnce = false;
	char gStatus[192]{};
	char gApSearch[64]{};
	int gApFilter = 0;
	int gApSelCatId = 0;
	int gApSelAchId = 0;
}

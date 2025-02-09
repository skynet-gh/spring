#include "FreeListMap.h"


CR_BIND_TEMPLATE_1TYPED(spring::FreeListMap, TVal, )
CR_REG_METADATA_TEMPLATE_1TYPED(spring::FreeListMap, TVal, (
	CR_IGNORED(values),
	CR_IGNORED(freeIDs),
	CR_IGNORED(nextId)
))

CR_BIND_TEMPLATE_1TYPED(spring::FreeListMapSaved, TVal, )
CR_REG_METADATA_TEMPLATE_1TYPED(spring::FreeListMapSaved, TVal, (
	CR_MEMBER(values),
	CR_MEMBER(freeIDs),
	CR_MEMBER(nextId)
))
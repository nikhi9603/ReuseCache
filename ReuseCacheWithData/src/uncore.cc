#include "uncore.h"

// uncore
UNCORE uncore;

// constructor
UNCORE::UNCORE()
{

#ifdef USE_REUSE_CACHE_LLC
    LLC->replacement_final_stats = static_cast<void (CACHE::*)()>(&REUSE_CACHE_LLC::reuse_cache_llc_replacement_final_stats);
#endif
}

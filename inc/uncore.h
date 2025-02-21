#ifndef UNCORE_H
#define UNCORE_H

#include "champsim.h"
#include "cache.h"
#include "reuse_cache_llc.h"
#include "dram_controller.h"
// #include "drc_controller.h"

// #define DRC_MSHR_SIZE 48

// uncore
class UNCORE
{
public:
// LLC
#ifdef USE_REUSE_CACHE_LLC
  CACHE *LLC = new REUSE_CACHE_LLC{"LLC", REUSE_CACHE_TAG_ARRAY_SET, REUSE_CACHE_TAG_ARRAY_WAYS, REUSE_CACHE_DATA_ARRAY_SET, REUSE_CACHE_DATA_ARRAY_WAYS, LLC_WQ_SIZE, LLC_RQ_SIZE, LLC_PQ_SIZE, LLC_MSHR_SIZE};
#else
  CACHE *LLC = new CACHE{"LLC", LLC_SET, LLC_WAY, LLC_SET *LLC_WAY, LLC_WQ_SIZE, LLC_RQ_SIZE, LLC_PQ_SIZE, LLC_MSHR_SIZE};
#endif
  // DRAM
  MEMORY_CONTROLLER DRAM{"DRAM"};

  UNCORE();
};

extern UNCORE uncore;

#endif // UNCORE_H

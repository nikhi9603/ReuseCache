#ifndef REUSE_CACHE_LLC_H
#define REUSE_CACHE_LLC_H

#include "cache.h"

class REUSE_CACHE_LLC : public CACHE
{
public:
    const uint32_t NUM_TAG_ARRAY_WAYS, NUM_TAG_ARRAY_SETS, NUM_DATA_ARRAY_WAYS, NUM_DATA_ARRAY_SETS;
    BLOCK **tag_array;
    BLOCK **data_array;

    uint64_t sim_llc_access[NUM_CPUS][NUM_TYPES],
        sim_llc_tag_miss[NUM_CPUS][NUM_TYPES],
        sim_llc_tag_hit[NUM_CPUS][NUM_TYPES],
        sim_llc_data_miss[NUM_CPUS][NUM_TYPES],
        sim_llc_data_hit[NUM_CPUS][NUM_TYPES];

    REUSE_CACHE_LLC(string name, uint32_t num_tag_array_ways, uint32_t num_tag_array_sets, uint32_t num_data_array_ways, uint32_t num_data_array_sets, uint32_t wq_size, uint32_t rq_size, uint32_t pq_size, uint32_t mshr_size)
        : CACHE(name, 0, 0, 0, wq_size, rq_size, pq_size, mshr_size), NUM_TAG_ARRAY_WAYS(num_tag_array_ways), NUM_TAG_ARRAY_SETS(num_tag_array_sets), NUM_DATA_ARRAY_WAYS(num_data_array_ways), NUM_DATA_ARRAY_SETS(num_data_array_sets)
    {
        tag_array = new BLOCK *[NUM_TAG_ARRAY_SETS];
        for (uint32_t i = 0; i < NUM_TAG_ARRAY_SETS; i++)
        {
            tag_array[i] = new BLOCK[NUM_TAG_ARRAY_WAYS];
        }

        data_array = new BLOCK *[NUM_DATA_ARRAY_SETS];
        for (uint32_t i = 0; i < NUM_DATA_ARRAY_SETS; i++)
        {
            data_array[i] = new BLOCK[NUM_DATA_ARRAY_WAYS];
        }

        for (uint32_t i = 0; i < NUM_CPUS; i++)
        {
            for (uint32_t j = 0; j < NUM_TYPES; j++)
            {
                sim_llc_access[i][j] = 0;
                sim_llc_tag_miss[i][j] = 0;
                sim_llc_tag_hit[i][j] = 0;
                sim_llc_data_miss[i][j] = 0;
                sim_llc_data_hit[i][j] = 0;
            }
        }
    }

    // checks the tag array
    uint32_t get_set(uint64_t address);

    void lru_update(uint32_t set, uint32_t way);
    int fill_cache_tag(uint32_t tag_array_set, PACKET *packet);
    int fill_cache_data(uint32_t tag_array_set, uint32_t tag_array_way, PACKET *packet);
    int check_tag_hit(PACKET *packet);
    int check_hit(PACKET *packet);
    int invalidate_entry(uint64_t inval_addr);

    void handle_fill();
    void handle_writeback();
    void handle_read();

    void reuse_cache_llc_initialize_replacement(),
        reuse_cache_llc_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit),
        reuse_cache_llc_replacement_final_stats();

    uint32_t reuse_cache_llc_find_tag_array_victim(uint32_t tag_array_set),
        reuse_cache_llc_find_data_array_victim(uint32_t data_array_set);
};

#endif
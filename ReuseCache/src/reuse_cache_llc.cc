#include "reuse_cache_llc.h"

void REUSE_CACHE_LLC::handle_fill()
{
    uint32_t fill_cpu = (MSHR.next_fill_index == MSHR_SIZE) ? NUM_CPUS : MSHR.entry[MSHR.next_fill_index].cpu;
    if (fill_cpu == NUM_CPUS)
        return;

    if (MSHR.next_fill_cycle <= current_core_cycle[fill_cpu])
    {
        auto mshr_index = MSHR.next_fill_index;

        auto tag_array_set = get_set(MSHR.entry[mshr_index].address);
        int tag_way = check_tag_hit(&MSHR.entry[mshr_index]);

        int filled = 0;
        bool isBypass = false;

                    
        if(MSHR.entry[mshr_index].type == PREFETCH || MSHR.entry[mshr_index].type == PREFETCH_TRANSLATION) 
        {
            isBypass = true;
        }

        if(isBypass == false)
        {
            // fills generated from misses. So the data is not present in the cache but tag may be present
            if (tag_way != NUM_TAG_ARRAY_WAYS)
            { // tag already present, but data not present

                filled = fill_cache_data(tag_array_set, tag_way, &MSHR.entry[mshr_index]);
                if (filled)
                {
                    // sim_llc_tag_hit[fill_cpu][MSHR.entry[mshr_index].type]++;
                    // sim_llc_data_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
                }
            }
            else
            { // tag not present
                filled = fill_cache_tag(tag_array_set, &MSHR.entry[mshr_index]);
                // if (filled)
                    // sim_llc_tag_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
            }
        }

        if (filled || isBypass)
        {
            if (MSHR.entry[mshr_index].fill_level < fill_level)
            {
                if (MSHR.entry[mshr_index].instruction)
                    upper_level_icache[fill_cpu]->return_data(&MSHR.entry[mshr_index]);
                else // data
                    upper_level_dcache[fill_cpu]->return_data(&MSHR.entry[mshr_index]);
            }

            if(isBypass == false)
            {
                // sim_llc_access[fill_cpu][MSHR.entry[mshr_index].type]++;

                ACCESS[MSHR.entry[mshr_index].type]++;
                MISS[MSHR.entry[mshr_index].type]++;  
                // cout << "data miss handle-fill " << "set = " << tag_array_set << " way = " << tag_way << "address = " << MSHR.entry[mshr_index].address << endl;
            }   
            
            MSHR.remove_queue(&MSHR.entry[mshr_index]);
            MSHR.num_returned--;

            update_fill_cycle();
        }
    }
}

void REUSE_CACHE_LLC::handle_writeback()
{
    if (WQ.occupancy == 0)
        return;

    uint32_t writeback_cpu = WQ.entry[WQ.head].cpu;
    if (writeback_cpu == NUM_CPUS)
        return;

    if ((WQ.entry[WQ.head].event_cycle <= current_core_cycle[writeback_cpu]) && (WQ.occupancy > 0))
    {
        uint32_t index = WQ.head;

        auto tag_array_set = get_set(WQ.entry[index].address);
        auto tag_way = check_tag_hit(&WQ.entry[index]);

        int filled = 0;
        bool tag_hit_data_miss = false;

        if (tag_way != NUM_TAG_ARRAY_WAYS) // tag hit
        {
            if (tag_array[tag_array_set][tag_way].hasData) // data hit
            {
                if(WQ.entry[index].type != WRITEBACK)
                {
                    tag_array[tag_array_set][tag_way].nrr = 0;          // dont consider hits for writeback to be usage..
                    tag_array[tag_array_set][tag_way].forward_backward_pointer->nru = 0;
                }
                tag_array[tag_array_set][tag_way].forward_backward_pointer->dirty = 1;
                WQ.entry[index].data = tag_array[tag_array_set][tag_way].forward_backward_pointer->data;
                filled = 1;
                sim_llc_data_hit[writeback_cpu][WQ.entry[index].type]++;
                // tag_array[tag_array_set][tag_way].forward_backward_pointer->num_uses++;
            }
            else // data miss
            {
                uint32_t data_array_set = (uint32_t)(tag_array_set & ((1 << lg2(NUM_DATA_ARRAY_SETS)) - 1));
                int data_array_victim_way = reuse_cache_llc_find_data_array_victim(data_array_set, false);
                uint64_t evicted_addr = data_array[data_array_set][data_array_victim_way].address;

                filled = fill_cache_data(tag_array_set, tag_way, &WQ.entry[index]);
                tag_hit_data_miss = true;

                if (filled)
                {
                    cpu = writeback_cpu;
                    WQ.entry[index].pf_metadata = llc_prefetcher_cache_fill(WQ.entry[index].address << LOG2_BLOCK_SIZE, data_array_set, data_array_victim_way, 0,
                                                                                evicted_addr << LOG2_BLOCK_SIZE, WQ.entry[index].pf_metadata);
                    cpu = 0;
                    tag_array[tag_array_set][tag_way].forward_backward_pointer->dirty = 1;
                    sim_llc_data_miss[writeback_cpu][WQ.entry[index].type]++;
                }
            }

            if (filled)
            {
                sim_llc_tag_hit[writeback_cpu][WQ.entry[index].type]++;
            }
        }
        else // tag miss
        {
            filled = fill_cache_tag(tag_array_set, &WQ.entry[index]);
            // filled = fill_cache_tag_data(tag_array_set, &WQ.entry[index]);

            if (filled)
            {
                sim_llc_tag_miss[writeback_cpu][WQ.entry[index].type]++;

                lower_level->add_wq(&WQ.entry[index]);
            }
        }

        if (filled)
        {
            if (WQ.entry[index].fill_level < fill_level)
            {
                if (WQ.entry[index].instruction)
                    upper_level_icache[writeback_cpu]->return_data(&WQ.entry[index]);
                if (WQ.entry[index].is_data)
                    upper_level_dcache[writeback_cpu]->return_data(&WQ.entry[index]);
            }

            sim_llc_access[writeback_cpu][WQ.entry[index].type]++;

            ACCESS[WQ.entry[index].type]++;
            if (tag_way != NUM_TAG_ARRAY_WAYS)
            {
                if(tag_hit_data_miss)
                {
                    // cout << "data miss tag-hit data-miss hw " << "set = " << tag_array_set << " way = " << tag_way << "address = " << WQ.entry[index].address << endl;
                    MISS[WQ.entry[index].type]++;
                }
                else
                {
                    // cout << "data hit hw " << "set = " << tag_array_set << " way = " << tag_way << "address = " << WQ.entry[index].address << endl;
                    HIT[WQ.entry[index].type]++;       // VERIFY_MODIFIED: tag present and data miss will be considered as MISS right?
                }
            }
            else
            {
                // cout << "data miss tag-hit data-miss hw " << "set = " << tag_array_set << " way = " << tag_way << "address = " << WQ.entry[index].address << endl;
                MISS[WQ.entry[index].type]++;
            }

            WQ.remove_queue(&WQ.entry[index]);
        }
    }
}

void REUSE_CACHE_LLC::handle_read()
{
    if (RQ.occupancy == 0)
        return;

    for (uint32_t i = 0; i < MAX_READ; i++)
    {
        uint32_t read_cpu = RQ.entry[RQ.head].cpu;
        if (read_cpu == NUM_CPUS)
            return;

        if ((RQ.entry[RQ.head].event_cycle <= current_core_cycle[read_cpu]) && (RQ.occupancy > 0))
        {
            uint32_t index = RQ.head;

            uint32_t tag_array_set = get_set(RQ.entry[index].address);
            int tag_way = check_tag_hit(&RQ.entry[index]);
            int way = check_hit(&RQ.entry[RQ.head]);

            if (way >= 0)
            {
                if(RQ.entry[index].type == LOAD)
                {
                    cpu = read_cpu;
                    llc_prefetcher_operate(tag_array[tag_array_set][way].forward_backward_pointer->address << LOG2_BLOCK_SIZE, RQ.entry[index].ip, 1, RQ.entry[index].type, 0);
                    cpu = 0;
                }

                tag_array[tag_array_set][way].used = 1;
                tag_array[tag_array_set][way].nrr = 0;
                tag_array[tag_array_set][way].forward_backward_pointer->nru = 0;

                if (RQ.entry[index].fill_level < fill_level)
                {
                    if (RQ.entry[index].instruction)
                    {
                        RQ.entry[index].data = tag_array[tag_array_set][way].forward_backward_pointer->data;
                        upper_level_icache[read_cpu]->return_data(&RQ.entry[index]);
                    }
                    else
                    {
                        RQ.entry[index].data = tag_array[tag_array_set][way].forward_backward_pointer->data;
                        upper_level_dcache[read_cpu]->return_data(&RQ.entry[index]);
                    }
                }

                if(tag_array[tag_array_set][way].forward_backward_pointer->prefetch)
                {
                    pf_useful++;
                    tag_array[tag_array_set][way].forward_backward_pointer->prefetch = 0;
                }

                sim_llc_access[read_cpu][RQ.entry[index].type]++;
                sim_llc_tag_hit[read_cpu][RQ.entry[index].type]++;
                sim_llc_data_hit[read_cpu][RQ.entry[index].type]++;

                // VERIFY_MODIFIED: HIT happens only when data is present right? Just tag is checked here, check_hit func checks bth tag +data
                ACCESS[RQ.entry[index].type]++;
                HIT[RQ.entry[index].type]++;
                // cout << "data hit hr: " << "set = " << tag_array_set << " way = " << way << "address = " << RQ.entry[index].address << endl;
                tag_array[tag_array_set][way].forward_backward_pointer->num_uses++;

                RQ.remove_queue(&RQ.entry[index]);
                reads_available_this_cycle--;
            }
            else
            {
                uint8_t miss_handled = 1;
                int mshr_index = check_nonfifo_queue(&MSHR, &RQ.entry[index], false);

                if (mshr_index == -2)
                {
                    // this is a data/instruction collision in the MSHR, so we have to wait before we can allocate this miss
                    miss_handled = 0;
                }

                if ((mshr_index == -1) && (MSHR.occupancy < MSHR_SIZE))
                {
                    if (lower_level->get_occupancy(1, RQ.entry[index].address) == lower_level->get_size(1, RQ.entry[index].address))
                    {
                        miss_handled = 0;
                    }
                    else
                    {
                        add_nonfifo_queue(&MSHR, &RQ.entry[index]); //@Vishal: Updated from add_mshr
                        if (lower_level)
                        {
                            lower_level->add_rq(&RQ.entry[index]);
                        }
                    }
                }
                else
                {
                    if ((mshr_index == -1 && MSHR.occupancy == MSHR_SIZE))
                    {
                        STALL[RQ.entry[index].type]++;
                        miss_handled = 0;
                    }
                    else if (mshr_index != -1)
                    {
                        if (RQ.entry[index].type == RFO)
                        {
                            if (RQ.entry[index].tlb_access)
                            {
                                // checking for dead code
                                assert(0);
                                uint32_t sq_index = RQ.entry[index].sq_index;
                                MSHR.entry[mshr_index].store_merged = 1;
                                MSHR.entry[mshr_index].sq_index_depend_on_me.insert(sq_index);
                                MSHR.entry[mshr_index].sq_index_depend_on_me.join(RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                            }

                            if (RQ.entry[index].load_merged)
                            {
                                // uint32_t lq_index = RQ.entry[index].lq_index;
                                MSHR.entry[mshr_index].load_merged = 1;
                                // MSHR.entry[mshr_index].lq_index_depend_on_me[lq_index] = 1;
                                MSHR.entry[mshr_index].lq_index_depend_on_me.join(RQ.entry[index].lq_index_depend_on_me, LQ_SIZE);
                            }
                        }
                        else
                        {
                            if (RQ.entry[index].instruction)
                            {
                                uint32_t rob_index = RQ.entry[index].rob_index;
                                DP(if (warmup_complete[MSHR.entry[mshr_index].cpu]) {
                                            //if(cache_type==IS_ITLB || cache_type==IS_DTLB || cache_type==IS_STLB)
                                            cout << "read request merged with MSHR entry -"<< MSHR.entry[mshr_index].type << endl; });
                                MSHR.entry[mshr_index].instr_merged = 1;
                                MSHR.entry[mshr_index].rob_index_depend_on_me.insert(rob_index);

                                DP(if (warmup_complete[MSHR.entry[mshr_index].cpu]) {
                                            cout << "[INSTR_MERGED] " << __func__ << " cpu: " << MSHR.entry[mshr_index].cpu << " instr_id: " << MSHR.entry[mshr_index].instr_id;
                                            cout << " merged rob_index: " << rob_index << " instr_id: " << RQ.entry[index].instr_id << endl; });

                                if (RQ.entry[index].instr_merged)
                                {
                                    MSHR.entry[mshr_index].rob_index_depend_on_me.join(RQ.entry[index].rob_index_depend_on_me, ROB_SIZE);
                                    DP(if (warmup_complete[MSHR.entry[mshr_index].cpu]) {
                                                cout << "[INSTR_MERGED] " << __func__ << " cpu: " << MSHR.entry[mshr_index].cpu << " instr_id: " << MSHR.entry[mshr_index].instr_id;
                                                cout << " merged rob_index: " << i << " instr_id: N/A" << endl; });
                                }
                            }
                            else
                            {
                                uint32_t lq_index = RQ.entry[index].lq_index;
                                MSHR.entry[mshr_index].load_merged = 1;
                                MSHR.entry[mshr_index].lq_index_depend_on_me.insert(lq_index);

                                DP(if (warmup_complete[read_cpu]) {
                                            cout << "[DATA_MERGED] " << __func__ << " cpu: " << read_cpu << " instr_id: " << RQ.entry[index].instr_id;
                                            cout << " merged rob_index: " << RQ.entry[index].rob_index << " instr_id: " << RQ.entry[index].instr_id << " lq_index: " << RQ.entry[index].lq_index << endl; });
                                MSHR.entry[mshr_index].lq_index_depend_on_me.join(RQ.entry[index].lq_index_depend_on_me, LQ_SIZE);
                                if (RQ.entry[index].store_merged)
                                {
                                    MSHR.entry[mshr_index].store_merged = 1;
                                    MSHR.entry[mshr_index].sq_index_depend_on_me.join(RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                                }
                            }
                        }

                        if (RQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
                        {
                            MSHR.entry[mshr_index].fill_level = RQ.entry[index].fill_level;
                            MSHR.entry[mshr_index].instruction = RQ.entry[index].instruction;
                        }

                        if ((RQ.entry[index].fill_l1i) && (MSHR.entry[mshr_index].fill_l1i != 1))
                        {
                            MSHR.entry[mshr_index].fill_l1i = 1;
                        }
                        if ((RQ.entry[index].fill_l1d) && (MSHR.entry[mshr_index].fill_l1d != 1))
                        {
                            MSHR.entry[mshr_index].fill_l1d = 1;
                        }

                        //VERIFY:: copy pasted from coventional cache. not sure if its really executes for llc
                        bool merging_already_done = false;

                        // update request
                        if ((MSHR.entry[mshr_index].type == PREFETCH && RQ.entry[index].type != PREFETCH) || (MSHR.entry[mshr_index].type == PREFETCH_TRANSLATION && RQ.entry[index].type != PREFETCH_TRANSLATION) || (MSHR.entry[mshr_index].type == TRANSLATION_FROM_L1D && RQ.entry[index].type != TRANSLATION_FROM_L1D))
                        {

                            merging_already_done = true;
                            uint8_t prior_returned = MSHR.entry[mshr_index].returned;
                            uint64_t prior_event_cycle = MSHR.entry[mshr_index].event_cycle;
                            uint64_t prior_data;

                            uint64_t prior_address = MSHR.entry[mshr_index].address;
                            uint64_t prior_full_addr = MSHR.entry[mshr_index].full_addr;
                            uint64_t prior_full_physical_address = MSHR.entry[mshr_index].full_physical_address;
                            uint8_t prior_fill_l1i = MSHR.entry[mshr_index].fill_l1i;
                            uint8_t prior_fill_l1d = MSHR.entry[mshr_index].fill_l1d;
                            // Neelu: Need to save instruction field as well.
                            uint8_t prior_instruction = MSHR.entry[mshr_index].instruction;

                            assert(MSHR.entry[mshr_index].type != TRANSLATION_FROM_L1D || MSHR.entry[mshr_index].type != PREFETCH_TRANSLATION);

                            if (RQ.entry[index].fill_level > MSHR.entry[mshr_index].fill_level)
                                RQ.entry[index].fill_level = MSHR.entry[mshr_index].fill_level;

                            ++pf_late; //@v Late prefetch-> on-demand requests hit in MSHR

                            MSHR.entry[mshr_index] = RQ.entry[index];

                            if (prior_fill_l1i && MSHR.entry[mshr_index].fill_l1i == 0)
                                MSHR.entry[mshr_index].fill_l1i = 1;
                            if (prior_fill_l1d && MSHR.entry[mshr_index].fill_l1d == 0)
                                MSHR.entry[mshr_index].fill_l1d = 1;

                            // Neelu: Need to save instruction field as well.
                            if (prior_instruction && MSHR.entry[mshr_index].instruction == 0)
                                MSHR.entry[mshr_index].instruction = 1;

                            // in case request is already returned, we should keep event_cycle and retunred variables
                            MSHR.entry[mshr_index].returned = prior_returned;
                            MSHR.entry[mshr_index].event_cycle = prior_event_cycle;
                            MSHR.entry[mshr_index].data = prior_data;
                        }

                        MSHR_MERGED[RQ.entry[index].type]++;
                    }
                    else
                    {
                        cerr << "[" << NAME << "] MSHR errors" << endl;
                        assert(0);
                    }
                }

                if (miss_handled)
                {
                    // update prefetcher on load instruction
                    if (RQ.entry[index].type == LOAD)
                    {
                        cpu = read_cpu;
                        llc_prefetcher_operate(RQ.entry[index].address << LOG2_BLOCK_SIZE, RQ.entry[index].ip, 0, RQ.entry[index].type, 0);
                        cpu = 0;  
                    }                  

                    sim_llc_access[read_cpu][RQ.entry[index].type]++;
                    if (tag_way != NUM_TAG_ARRAY_WAYS)
                    {
                        sim_llc_tag_hit[read_cpu][RQ.entry[index].type]++;
                        sim_llc_data_miss[read_cpu][RQ.entry[index].type]++;
                    }
                    else
                        sim_llc_tag_miss[read_cpu][RQ.entry[index].type]++;
                    RQ.remove_queue(&RQ.entry[index]);
                    reads_available_this_cycle--;
                }
            }
        }
        else
            return;

        if (reads_available_this_cycle == 0)
            return;
    }
}

void REUSE_CACHE_LLC::handle_prefetch()
{
    // handle prefetch

    for (uint32_t i = 0; i < MAX_READ; i++)
    {
        uint32_t prefetch_cpu = PQ.entry[PQ.head].cpu;
        if (prefetch_cpu == NUM_CPUS)
            return;

        // handle the oldest entry
        if ((PQ.entry[PQ.head].event_cycle <= current_core_cycle[prefetch_cpu]) && (PQ.occupancy > 0))
        {
            int index = PQ.head;
            uint32_t tag_array_set = get_set(PQ.entry[index].address);
            int tag_array_way = check_tag_hit(&PQ.entry[index]);
            int way = check_hit(&PQ.entry[index]);

            if(way >= 0)
            {
                // prefetch hit

                // update replacement policy
                // tag_array[tag_array_set][tag_array_way].nrr = 0;
                // tag_array[tag_array_set][tag_array_way].forward_backward_pointer->nru = 0;

                // COLLECT STATS
                sim_llc_tag_hit[prefetch_cpu][PQ.entry[index].type]++;
                sim_llc_data_hit[prefetch_cpu][PQ.entry[index].type]++;

                HIT[PQ.entry[index].type]++;
                ACCESS[PQ.entry[index].type]++;
                // cout << "data hit pw" << endl;
                // tag_array[tag_array_set][way].forward_backward_pointer->num_uses++;

                // run prefetcher on prefetches from higher caches
                if (PQ.entry[index].pf_origin_level < fill_level)
                {
                    cpu = prefetch_cpu;
                    if ((((PQ.entry[index].pf_metadata >> 17) & 1) | ((PQ.entry[index].pf_metadata >> 18) & 1)) == 1)
                        sending_hint_to_llc++;
                    PQ.entry[index].pf_metadata = llc_prefetcher_operate(tag_array[tag_array_set][way].forward_backward_pointer->address << LOG2_BLOCK_SIZE, PQ.entry[index].ip, 1, PREFETCH, PQ.entry[index].pf_metadata);
                    cpu = 0;
                }

                // check fill level
                // data should be updated (for TLBs) in case of hit
                if (PQ.entry[index].fill_level < fill_level)
                {
#ifdef SANITY_CHECK
                        if (PQ.entry[index].data == 0)
                            assert(0);
#endif
                    if (PQ.entry[index].instruction)
                    {
                        PQ.entry[index].data = tag_array[tag_array_set][tag_array_way].forward_backward_pointer->data;
                        upper_level_icache[prefetch_cpu]->return_data(&PQ.entry[index]);
                    }
                    else // data
                    {
                        PQ.entry[index].data = tag_array[tag_array_set][tag_array_way].forward_backward_pointer->data;
                        upper_level_dcache[prefetch_cpu]->return_data(&PQ.entry[index]);
                    }
                }

                // remove this entry from PQ
                PQ.remove_queue(&PQ.entry[index]);
                reads_available_this_cycle--;
            }
            else
            {  // prefetch miss
                
                // check mshr
                uint8_t miss_handled = 1;
                int mshr_index = check_nonfifo_queue(&MSHR, &PQ.entry[index], false); //@Vishal: Updated from check_mshr

                if ((mshr_index == -1) && (MSHR.occupancy < MSHR_SIZE))
                { // this is a new miss

                    // Neelu: checking fill level for prefetches going to lower level.
                    if (fill_level == PQ.entry[index].fill_level)
                        pf_same_fill_level++;
                    else if (PQ.entry[index].fill_level > fill_level)
                        pf_lower_fill_level++;

                    ++pf_lower_level; //@v Increment for new prefetch miss

                    if (lower_level)
                    {
                        if (lower_level->get_occupancy(1, PQ.entry[index].address) == lower_level->get_size(1, PQ.entry[index].address))
                            miss_handled = 0;
                        else
                        {
                            // run prefetcher on prefetches from higher caches
                            if (PQ.entry[index].pf_origin_level < fill_level)
                            {
                                if (cache_type == IS_LLC)
                                {
                                    cpu = prefetch_cpu;
                                    if ((((PQ.entry[index].pf_metadata >> 17) & 1) | ((PQ.entry[index].pf_metadata >> 18) & 1)) == 1)
                                        sending_hint_to_llc++;
                                    PQ.entry[index].pf_metadata = llc_prefetcher_operate(PQ.entry[index].address << LOG2_BLOCK_SIZE, PQ.entry[index].ip, 0, PREFETCH, PQ.entry[index].pf_metadata);
                                    cpu = 0;
                                }
                                // add it to MSHRs if this prefetch miss will be filled to this cache level
                                if (PQ.entry[index].fill_level <= fill_level)
                                    add_nonfifo_queue(&MSHR, &PQ.entry[index]); //@Vishal: Updated from add_mshr

                                lower_level->add_rq(&PQ.entry[index]); // add it to the DRAM RQ                                
                            }
                        }
                    }
                }
                else
                {
                    if ((mshr_index == -1) && (MSHR.occupancy == MSHR_SIZE))
                    { // not enough MSHR resource

                        // TODO: should we allow prefetching with lower fill level at this case?

                        // cannot handle miss request until one of MSHRs is available
                        miss_handled = 0;
                        STALL[PQ.entry[index].type]++;
                    }
                    else if (mshr_index != -1)
                    { // already in-flight miss
                        // no need to update request except fill_level
                        // update fill_level
                        if (PQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
                        {
                            //@Vasudha:STLB Prefetch packet can have instruction variable as 1 or 0. Update instruction variable when upper level TLB sends request.
                            MSHR.entry[mshr_index].instruction = PQ.entry[index].instruction;
                            MSHR.entry[mshr_index].fill_level = PQ.entry[index].fill_level;
                        }
                        if ((PQ.entry[index].fill_l1i) && (MSHR.entry[mshr_index].fill_l1i != 1))
                        {
                            MSHR.entry[mshr_index].fill_l1i = 1;
                        }
                        if ((PQ.entry[index].fill_l1d) && (MSHR.entry[mshr_index].fill_l1d != 1))
                        {
                            MSHR.entry[mshr_index].fill_l1d = 1;
                        }

                        MSHR_MERGED[PQ.entry[index].type]++;
                    }
                    else
                    {
                        cerr << "[" << NAME << "] MSHR errors" << endl;
                        assert(0);
                    }
                }
                
                if (miss_handled)
                {
                    sim_llc_access[prefetch_cpu][PQ.entry[index].type]++;
                    if(tag_array_way != NUM_TAG_ARRAY_WAYS)
                    {
                        sim_llc_data_miss[prefetch_cpu][PQ.entry[index].type]++;
                    }
                    else
                    {
                        sim_llc_tag_miss[prefetch_cpu][PQ.entry[index].type]++;
                    }

                    // MISS[PQ.entry[index].type]++;
                    // ACCESS[PQ.entry[index].type]++;

                    // remove this entry from PQ
                    PQ.remove_queue(&PQ.entry[index]);
                    reads_available_this_cycle--;
                }
            }
        }
    }
}

void REUSE_CACHE_LLC::reuse_cache_llc_replacement_final_stats()
{
    std::cout << "REUSE CACHE LLC REPLACEMENT FINAL STATS" << std::endl;

    for (uint32_t i = 0; i < NUM_CPUS; i++)
    {
        std::cout << "CPU: " << i << std::endl;
        int TOTAL_ACCESS = 0, TOTAL_TAG_MISS = 0, TOTAL_TAG_HIT = 0, TOTAL_DATA_MISS = 0, TOTAL_DATA_HIT = 0;
        int TOTAL_MISS = 0, TOTAL_HIT = 0;
        for (uint32_t j = 0; j < NUM_TYPES; j++)
        {
            TOTAL_ACCESS += ACCESS[j];
            TOTAL_MISS += MISS[j];
            TOTAL_HIT += HIT[j];
            TOTAL_TAG_MISS += sim_llc_tag_miss[i][j];
            TOTAL_TAG_HIT += sim_llc_tag_hit[i][j];
            TOTAL_DATA_MISS += sim_llc_data_miss[i][j];
            TOTAL_DATA_HIT += sim_llc_data_hit[i][j];
        }
        std::cout << "TOTAL ACCESS: " << TOTAL_ACCESS << " TOTAL MISS: " << TOTAL_MISS << " TOTAL HIT: " << TOTAL_HIT << std::endl;
        std::cout << "TAG MISS: " << TOTAL_TAG_MISS << " TAG HIT: " << TOTAL_TAG_HIT << std::endl;
        std::cout << "DATA MISS: " << TOTAL_DATA_MISS << " DATA HIT: " << TOTAL_DATA_HIT << std::endl;
        std::cout << "MISS_RATE: " << (double)TOTAL_MISS / TOTAL_ACCESS << std::endl;
        std::cout << "HIT_RATE: " << (double)TOTAL_HIT / TOTAL_ACCESS << std::endl;

        cout << "LLC LOAD      ACCESS: " << setw(10) << ACCESS[0] << "  HIT: " << setw(10) << HIT[0] << "  MISS: " << setw(10) << MISS[0] << "  HIT %: " << setw(10) << ((double)HIT[0] * 100 / ACCESS[0]) << "  MISS %: " << setw(10) << ((double)MISS[0] * 100 / ACCESS[0]) << endl; 
        cout << "LLC RFO      ACCESS: " << setw(10) << ACCESS[1] << "  HIT: " << setw(10) << HIT[1] << "  MISS: " << setw(10) << MISS[1] << "  HIT %: " << setw(10) << ((double)HIT[1] * 100 / ACCESS[1]) << "  MISS %: " << setw(10) << ((double)MISS[1] * 100 / ACCESS[1]) << endl; 
        cout << "LLC PREFETCH      ACCESS: " << setw(10) << ACCESS[2] << "  HIT: " << setw(10) << HIT[2] << "  MISS: " << setw(10) << MISS[2] << "  HIT %: " << setw(10) << ((double)HIT[2] * 100 / ACCESS[2]) << "  MISS %: " << setw(10) << ((double)MISS[2] * 100 / ACCESS[2]) << endl; 
        cout << "LLC WRITEBACK      ACCESS: " << setw(10) << ACCESS[3] << "  HIT: " << setw(10) << HIT[3] << "  MISS: " << setw(10) << MISS[3] << "  HIT %: " << setw(10) << ((double)HIT[3] * 100 / ACCESS[3]) << "  MISS %: " << setw(10) << ((double)MISS[3] * 100 / ACCESS[3]) << endl; 
        cout << "LLC LOAD TRANSLATION ACCESS: " << setw(10) << ACCESS[4] << "  HIT: " << setw(10) << HIT[4] << "  MISS: " << setw(10) << MISS[4] << "  HIT %: " << setw(10) << ((double)HIT[4] * 100 / ACCESS[4]) << "  MISS %: " << setw(10) << ((double)MISS[4] * 100 / ACCESS[4]) << endl; 
        cout << "LLC PREFETCH TRANSLATION ACCESS: " << setw(10) << ACCESS[5] << "  HIT: " << setw(10) << HIT[5] << "  MISS: " << setw(10) << MISS[5] << "  HIT %: " << setw(10) << ((double)HIT[5] * 100 / ACCESS[5]) << "  MISS %: " << setw(10) << ((double)MISS[5] * 100 / ACCESS[5]) << endl; 
        cout << "LLC TRANSLATION FROM L1D PREFETCHER ACCESS: " << setw(10) << ACCESS[6] << "  HIT: " << setw(10) << HIT[6] << "  MISS: " << setw(10) << MISS[6] << "  HIT %: " << setw(10) << ((double)HIT[6] * 100 / ACCESS[6]) << "  MISS %: " << setw(10) << ((double)MISS[6] * 100 / ACCESS[6]) << endl; 
        
        // update num uses after simulation(final cache lines states?)
        for(int i = 0; i < NUM_DATA_ARRAY_SETS; i++)
        {
            for(int j = 0; j < NUM_DATA_ARRAY_WAYS; j++)
            {
                if(data_array[i][j].valid == 1)
                {
                    num_uses_before_eviction[data_array[i][j].num_uses]++;
                }
            }
        }

        std::cout << "In Reuse Cache:" << std::endl;
        int totalLines = 0;
        for (auto &use : num_uses_before_eviction)
        {
            totalLines += use.second;
            if(use.second != 0) std::cout << use.second << " lines used " << use.first << " times before eviction" << std::endl;
        }
        std::cout << "Total lines: " << totalLines << std::endl;
    }
}

uint32_t REUSE_CACHE_LLC::get_set(uint64_t address)
{
    // VERIFY: address is full_addr chopped the offset ig
    return (uint32_t)(address & ((1 << lg2(NUM_TAG_ARRAY_SETS)) - 1));
}

int REUSE_CACHE_LLC::check_tag_hit(PACKET *packet)
{
    uint32_t set = get_set(packet->address);

    for (uint32_t i = 0; i < NUM_TAG_ARRAY_WAYS; i++)
    {
        if (tag_array[set][i].valid && tag_array[set][i].tag == packet->address)
        {
            return i;
        }
    }

    return NUM_TAG_ARRAY_WAYS;
}

// VERIFY: is it really required?? can't we just check hasData value after getting way in check_tag_hit since duplicates dont exists in tag array
int REUSE_CACHE_LLC::check_hit(PACKET *packet)
{
    uint32_t set = get_set(packet->address);

    for (uint32_t i = 0; i < NUM_TAG_ARRAY_WAYS; i++)
    {
        if (tag_array[set][i].valid && tag_array[set][i].tag == packet->address && tag_array[set][i].hasData)
        {
            return i;
        }
    }

    return -1;
}

int REUSE_CACHE_LLC::invalidate_entry(uint64_t inval_addr)
{
    uint32_t set = get_set(inval_addr);

    for (uint32_t i = 0; i < NUM_TAG_ARRAY_WAYS; i++)
    {
        if (tag_array[set][i].valid && tag_array[set][i].tag == inval_addr)
        {
            tag_array[set][i].valid = 0;
            tag_array[set][i].hasData = false;
            return 1;
        }
    }

    return 0;
}

int REUSE_CACHE_LLC::fill_cache_tag(uint32_t tag_array_set, PACKET *packet)
{
    uint32_t tag_victim = reuse_cache_llc_find_tag_array_victim(tag_array_set);
    // cout << "tag-victim= " << tag_victim << endl;

    if (tag_array[tag_array_set][tag_victim].valid && tag_array[tag_array_set][tag_victim].hasData)
    {
        if (tag_array[tag_array_set][tag_victim].forward_backward_pointer->dirty)
        {
            if (lower_level)
            {
                if (lower_level->get_occupancy(2, tag_array[tag_array_set][tag_victim].address) == lower_level->get_size(2, tag_array[tag_array_set][tag_victim].address))
                {
                    // lower level WQ is full, cannot replace this victim
                    lower_level->increment_WQ_FULL(tag_array[tag_array_set][tag_victim].address);
                    STALL[packet->type]++;
                    return 0;
                }
                else
                {
                    PACKET writeback_packet;

                    writeback_packet.fill_level = fill_level << 1;
                    writeback_packet.cpu = packet->cpu;
                    writeback_packet.address = tag_array[tag_array_set][tag_victim].address;
                    writeback_packet.full_addr = tag_array[tag_array_set][tag_victim].full_addr;
                    writeback_packet.data = tag_array[tag_array_set][tag_victim].forward_backward_pointer->data;
                    writeback_packet.instr_id = packet->instr_id;
                    writeback_packet.ip = 0; // writeback does not have ip
                    writeback_packet.type = WRITEBACK;
                    writeback_packet.event_cycle = current_core_cycle[packet->cpu];

                    lower_level->add_wq(&writeback_packet);
                }
            }
        }

        tag_array[tag_array_set][tag_victim].forward_backward_pointer->valid = 0;
        tag_array[tag_array_set][tag_victim].hasData = false;
    }

    tag_array[tag_array_set][tag_victim].valid = 1;
    tag_array[tag_array_set][tag_victim].dirty = 0;
    tag_array[tag_array_set][tag_victim].used = 0;
    tag_array[tag_array_set][tag_victim].prefetch = (packet->type == PREFETCH || packet->type == PREFETCH_TRANSLATION || packet->type == TRANSLATION_FROM_L1D) ? 1 : 0;

    tag_array[tag_array_set][tag_victim].tag = packet->address;
    tag_array[tag_array_set][tag_victim].address = packet->address;
    tag_array[tag_array_set][tag_victim].full_addr = packet->full_addr;
    tag_array[tag_array_set][tag_victim].cpu = packet->cpu;
    tag_array[tag_array_set][tag_victim].instr_id = packet->instr_id;

    tag_array[tag_array_set][tag_victim].delta = packet->delta;
    tag_array[tag_array_set][tag_victim].depth = packet->depth;
    tag_array[tag_array_set][tag_victim].signature = packet->signature;
    tag_array[tag_array_set][tag_victim].confidence = packet->confidence;

    tag_array[tag_array_set][tag_victim].hasData = false;
    tag_array[tag_array_set][tag_victim].nrr = 1;
    tag_array[tag_array_set][tag_victim].num_uses = 0;

    return 1;
}

int REUSE_CACHE_LLC::fill_cache_data(uint32_t tag_array_set, uint32_t tag_array_way, PACKET *packet)
{
    uint32_t data_array_set = (uint32_t)(tag_array_set & ((1 << lg2(NUM_DATA_ARRAY_SETS)) - 1));
    uint32_t data_array_way = reuse_cache_llc_find_data_array_victim(data_array_set);
    // cout << "data-victim= . random value =? = " << data_array_way << endl;

    if (data_array[data_array_set][data_array_way].valid && data_array[data_array_set][data_array_way].dirty)
    {
        if (lower_level)
        {
            if (lower_level->get_occupancy(2, data_array[data_array_set][data_array_way].address) == lower_level->get_size(2, data_array[data_array_set][data_array_way].address))
            {
                // lower level WQ is full, cannot replace this victim
                lower_level->increment_WQ_FULL(data_array[data_array_set][data_array_way].address);
                STALL[packet->type]++;
                return 0;
            }
            else
            {
                PACKET writeback_packet;

                writeback_packet.fill_level = fill_level << 1;
                writeback_packet.cpu = packet->cpu;
                writeback_packet.address = data_array[data_array_set][data_array_way].address;
                writeback_packet.full_addr = data_array[data_array_set][data_array_way].full_addr;
                writeback_packet.data = data_array[data_array_set][data_array_way].data;
                writeback_packet.instr_id = packet->instr_id;
                writeback_packet.ip = 0; // writeback does not have ip
                writeback_packet.type = WRITEBACK;
                writeback_packet.event_cycle = current_core_cycle[packet->cpu];

                lower_level->add_wq(&writeback_packet);

                data_array[data_array_set][data_array_way].valid = 0;
                data_array[data_array_set][data_array_way].forward_backward_pointer->hasData = false;
            }
        }
    }

    data_array[data_array_set][data_array_way].valid = 1;
    data_array[data_array_set][data_array_way].dirty = 0;
    data_array[data_array_set][data_array_way].used = 0;
    data_array[data_array_set][data_array_way].prefetch = (packet->type == PREFETCH || packet->type == PREFETCH_TRANSLATION || packet->type == TRANSLATION_FROM_L1D) ? 1 : 0;

    data_array[data_array_set][data_array_way].tag = packet->address;
    data_array[data_array_set][data_array_way].address = packet->address;
    data_array[data_array_set][data_array_way].data = packet->data;
    data_array[data_array_set][data_array_way].full_addr = packet->full_addr;
    data_array[data_array_set][data_array_way].cpu = packet->cpu;
    data_array[data_array_set][data_array_way].instr_id = packet->instr_id;

    data_array[data_array_set][data_array_way].delta = packet->delta;
    data_array[data_array_set][data_array_way].depth = packet->depth;
    data_array[data_array_set][data_array_way].signature = packet->signature;
    data_array[data_array_set][data_array_way].confidence = packet->confidence;

    tag_array[tag_array_set][tag_array_way].hasData = 1;
    tag_array[tag_array_set][tag_array_way].forward_backward_pointer = &data_array[data_array_set][data_array_way];
    data_array[data_array_set][data_array_way].forward_backward_pointer = &tag_array[tag_array_set][tag_array_way];

    if(packet->type != WRITEBACK)
    {
        tag_array[tag_array_set][tag_array_way].nrr = 0;   
    }
    data_array[data_array_set][data_array_way].nru = 1;
    data_array[data_array_set][data_array_way].num_uses = 0;

    return 1;
}

int REUSE_CACHE_LLC::fill_cache_tag_data(uint32_t tag_array_set, PACKET* packet)
{
    uint32_t writeback_cpu = WQ.entry[WQ.head].cpu;
    // For now, this func docuses fill in writeback case (filling tag and data together)
    // Writing in this way to fill to avoid wrong value of nrr in some case when only tag/data can be filled.... (this func is written in such a way that single core is supported)
    uint32_t tag_victim = reuse_cache_llc_find_tag_array_victim(tag_array_set, false);
    uint32_t data_array_set = (uint32_t)(tag_array_set & ((1 << lg2(NUM_DATA_ARRAY_SETS)) - 1));
    uint32_t data_array_way = reuse_cache_llc_find_data_array_victim(data_array_set, false);

    if (tag_array[tag_array_set][tag_victim].valid && tag_array[tag_array_set][tag_victim].hasData)
    {
        if (tag_array[tag_array_set][tag_victim].forward_backward_pointer->dirty)
        {
            if (lower_level)
            {
                if (lower_level->get_occupancy(2, tag_array[tag_array_set][tag_victim].address) == lower_level->get_size(2, tag_array[tag_array_set][tag_victim].address))
                {
                    // lower level WQ is full, cannot replace this victim
                    lower_level->increment_WQ_FULL(tag_array[tag_array_set][tag_victim].address);
                    STALL[packet->type]++;
                    return 0;
                }
            }
        }
    }
    else if(tag_array[tag_array_set][tag_victim].valid && tag_array[tag_array_set][tag_victim].hasData == 0) // checking to fill only when both tag and data can be filled together so tha nrr wont be set as 0 in the case if tag got filled and data got stalled
    {
        if (data_array[data_array_set][data_array_way].valid && data_array[data_array_set][data_array_way].dirty)
        {
            if (lower_level->get_occupancy(2, data_array[data_array_set][data_array_way].address) == lower_level->get_size(2, data_array[data_array_set][data_array_way].address))
            {
                // lower level WQ is full, cannot replace this victim
                lower_level->increment_WQ_FULL(data_array[data_array_set][data_array_way].address);
                STALL[packet->type]++;
                return 0;
            }
        }
    }

    // No stalls happened
    // FILL TAG
    tag_victim = reuse_cache_llc_find_tag_array_victim(tag_array_set);

    if (tag_array[tag_array_set][tag_victim].valid && tag_array[tag_array_set][tag_victim].hasData)
    {
        if (tag_array[tag_array_set][tag_victim].forward_backward_pointer->dirty) // no stall happened so lower level wq has space
        {
            PACKET writeback_packet;

            writeback_packet.fill_level = fill_level << 1;
            writeback_packet.cpu = packet->cpu;
            writeback_packet.address = tag_array[tag_array_set][tag_victim].address;
            writeback_packet.full_addr = tag_array[tag_array_set][tag_victim].full_addr;
            writeback_packet.data = tag_array[tag_array_set][tag_victim].forward_backward_pointer->data;
            writeback_packet.instr_id = packet->instr_id;
            writeback_packet.ip = 0; // writeback does not have ip
            writeback_packet.type = WRITEBACK;
            writeback_packet.event_cycle = current_core_cycle[packet->cpu];

            lower_level->add_wq(&writeback_packet);
        }
        
        tag_array[tag_array_set][tag_victim].forward_backward_pointer->valid = 0;
        tag_array[tag_array_set][tag_victim].hasData = false;
    }

    tag_array[tag_array_set][tag_victim].valid = 1;
    tag_array[tag_array_set][tag_victim].dirty = 0;
    tag_array[tag_array_set][tag_victim].used = 0;
    tag_array[tag_array_set][tag_victim].prefetch = (packet->type == PREFETCH || packet->type == PREFETCH_TRANSLATION || packet->type == TRANSLATION_FROM_L1D) ? 1 : 0;

    tag_array[tag_array_set][tag_victim].tag = packet->address;
    tag_array[tag_array_set][tag_victim].address = packet->address;
    tag_array[tag_array_set][tag_victim].full_addr = packet->full_addr;
    tag_array[tag_array_set][tag_victim].cpu = packet->cpu;
    tag_array[tag_array_set][tag_victim].instr_id = packet->instr_id;

    tag_array[tag_array_set][tag_victim].delta = packet->delta;
    tag_array[tag_array_set][tag_victim].depth = packet->depth;
    tag_array[tag_array_set][tag_victim].signature = packet->signature;
    tag_array[tag_array_set][tag_victim].confidence = packet->confidence;

    tag_array[tag_array_set][tag_victim].hasData = true;
    tag_array[tag_array_set][tag_victim].nrr = 1;
    tag_array[tag_array_set][tag_victim].num_uses = 0;

    // FILL DATA
    data_array_way = reuse_cache_llc_find_data_array_victim(data_array_set);

    // Here data_array_way would have been changed too from prev cal at start since there is a chance of tag victim having data and that is invalid now.
    // Stall didnt happen so lower wq has space .. If that was used by tag victim before this, then data victim would have been invalid block and following wont happen.
    if (data_array[data_array_set][data_array_way].valid && data_array[data_array_set][data_array_way].dirty)
    {
        PACKET writeback_packet;

        writeback_packet.fill_level = fill_level << 1;
        writeback_packet.cpu = packet->cpu;
        writeback_packet.address = data_array[data_array_set][data_array_way].address;
        writeback_packet.full_addr = data_array[data_array_set][data_array_way].full_addr;
        writeback_packet.data = data_array[data_array_set][data_array_way].data;
        writeback_packet.instr_id = packet->instr_id;
        writeback_packet.ip = 0; // writeback does not have ip
        writeback_packet.type = WRITEBACK;
        writeback_packet.event_cycle = current_core_cycle[packet->cpu];

        lower_level->add_wq(&writeback_packet);

        data_array[data_array_set][data_array_way].valid = 0;
        data_array[data_array_set][data_array_way].forward_backward_pointer->hasData = false;
    }

    cpu = writeback_cpu;
    WQ.entry[WQ.head].pf_metadata = llc_prefetcher_cache_fill(WQ.entry[WQ.head].address << LOG2_BLOCK_SIZE, data_array_set, data_array_way, 0,
                                                                                data_array[data_array_set][data_array_way].address << LOG2_BLOCK_SIZE, WQ.entry[WQ.head].pf_metadata);
    cpu = 0;
    data_array[data_array_set][data_array_way].valid = 1;
    data_array[data_array_set][data_array_way].dirty = 1;   // since for now, func handles writeback case only
    data_array[data_array_set][data_array_way].used = 0;
    data_array[data_array_set][data_array_way].prefetch = (packet->type == PREFETCH || packet->type == PREFETCH_TRANSLATION || packet->type == TRANSLATION_FROM_L1D) ? 1 : 0;

    data_array[data_array_set][data_array_way].tag = packet->address;
    data_array[data_array_set][data_array_way].address = packet->address;
    data_array[data_array_set][data_array_way].data = packet->data;
    data_array[data_array_set][data_array_way].full_addr = packet->full_addr;
    data_array[data_array_set][data_array_way].cpu = packet->cpu;
    data_array[data_array_set][data_array_way].instr_id = packet->instr_id;

    data_array[data_array_set][data_array_way].delta = packet->delta;
    data_array[data_array_set][data_array_way].depth = packet->depth;
    data_array[data_array_set][data_array_way].signature = packet->signature;
    data_array[data_array_set][data_array_way].confidence = packet->confidence;

    data_array[data_array_set][data_array_way].nru = 1;
    data_array[data_array_set][data_array_way].num_uses = 0;
    data_array[data_array_set][data_array_way].forward_backward_pointer = &tag_array[tag_array_set][tag_victim];

    tag_array[tag_array_set][tag_victim].forward_backward_pointer = &data_array[data_array_set][data_array_way];

    return 1;
}

uint32_t REUSE_CACHE_LLC::reuse_cache_llc_find_tag_array_victim(uint32_t tag_array_set, bool find_to_replace)
{
    uint8_t start = rr_ptr_tag_array[tag_array_set];

    for (int i = 0; i < NUM_TAG_ARRAY_WAYS; i++)
    {
        int way = (start + i) % NUM_TAG_ARRAY_WAYS;
        if (tag_array[tag_array_set][way].valid == 0)
        {
            if(find_to_replace) rr_ptr_tag_array[tag_array_set] = (way + 1) % NUM_TAG_ARRAY_WAYS;
            return way;
        }
    }

    for (int i = 0; i < NUM_TAG_ARRAY_WAYS; i++)
    {
        int way = (start + i) % NUM_TAG_ARRAY_WAYS;
        if (tag_array[tag_array_set][way].nrr == 1)
        {
            if(find_to_replace) rr_ptr_tag_array[tag_array_set] = (way + 1) % NUM_TAG_ARRAY_WAYS;
            return way;
        }
    }

    // cout << "-----------Tag array victim not found--------------" << endl;

    if(find_to_replace)
    {
        for(int i = 0; i < NUM_TAG_ARRAY_WAYS; i++)
        {
            tag_array[tag_array_set][i].nrr = 1;
        }
        rr_ptr_tag_array[tag_array_set] = (start + 1) % NUM_TAG_ARRAY_WAYS;
    }
    return start;
}

uint32_t REUSE_CACHE_LLC::reuse_cache_llc_find_data_array_victim(uint32_t data_array_set, bool find_to_replace)
{
    uint8_t start = rr_ptr_data_array[data_array_set];

    for (int i = 0; i < NUM_DATA_ARRAY_WAYS; i++)
    {
        int way = (start + i) % NUM_DATA_ARRAY_WAYS;
        if (data_array[data_array_set][way].valid == 0)
        {
            if(find_to_replace) rr_ptr_data_array[data_array_set] = (way + 1) % NUM_DATA_ARRAY_WAYS;
            return way;
        }
    }

    for (int i = 0; i < NUM_DATA_ARRAY_WAYS; i++)
    {
        int way = (start + i) % NUM_DATA_ARRAY_WAYS;

        if (data_array[data_array_set][way].nru == 1)
        {
            if(find_to_replace) 
            {
                num_uses_before_eviction[data_array[data_array_set][way].num_uses]++;
                rr_ptr_data_array[data_array_set] = (way + 1) % NUM_DATA_ARRAY_WAYS;
            }
            return way;
        }
    }
    // cout << "-----------Data array victim not found--------------" << endl;
    if(find_to_replace)
    {
        for(int i = 0; i < NUM_DATA_ARRAY_WAYS; i++)
        {
            data_array[data_array_set][i].nru = 1;
        }
        rr_ptr_data_array[data_array_set] = (start + 1) % NUM_DATA_ARRAY_WAYS;
    }
    return start;
}
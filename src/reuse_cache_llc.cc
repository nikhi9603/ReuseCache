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

        int filled;

        // fills generated from misses. So the data is not present in the cache but tag may be present
        if (tag_way != NUM_TAG_ARRAY_WAYS)
        { // tag already present, but data not present
            filled = fill_cache_data(tag_array_set, tag_way, &MSHR.entry[mshr_index]);
            sim_llc_tag_hit[fill_cpu][MSHR.entry[mshr_index].type]++;
        }
        else
        { // tag not present
            filled = fill_cache_tag(tag_array_set, &MSHR.entry[mshr_index]);
            sim_llc_tag_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
        }

        if (filled)
        {
            if (MSHR.entry[mshr_index].fill_level < fill_level)
            {
                if (MSHR.entry[mshr_index].instruction)
                    upper_level_icache[fill_cpu]->return_data(&MSHR.entry[mshr_index]);
                else // data
                    upper_level_dcache[fill_cpu]->return_data(&MSHR.entry[mshr_index]);
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

    if (WQ.entry[WQ.head].event_cycle <= current_core_cycle[writeback_cpu])
    {
        uint32_t index = WQ.head;

        auto tag_array_set = get_set(WQ.entry[index].address);
        auto tag_way = check_tag_hit(&WQ.entry[index]);

        int filled = 0;

        if (tag_way != NUM_TAG_ARRAY_WAYS)
        {
            if (tag_array[tag_array_set][tag_way].hasData)
            {
                tag_array[tag_array_set][tag_way].nrr = 0;
                tag_array[tag_array_set][tag_way].forward_backward_pointer->dirty = 1;
                tag_array[tag_array_set][tag_way].forward_backward_pointer->nru = 1;
                filled = 1;
            }
            else
            {
                filled = fill_cache_data(tag_array_set, tag_way, &WQ.entry[index]);

                if (filled)
                    tag_array[tag_array_set][tag_way].forward_backward_pointer->dirty = 1;
            }
        }
        else
        {
            if (lower_level)
            {
                if (lower_level->get_occupancy(2, WQ.entry[index].address) == lower_level->get_size(2, WQ.entry[index].address))
                {

                    // lower level WQ is full, cannot replace this victim
                    filled = 0;
                    lower_level->increment_WQ_FULL(WQ.entry[index].address);
                    STALL[WQ.entry[index].type]++;
                }
                else
                {
                    PACKET writeback_packet;

                    writeback_packet.fill_level = fill_level << 1;
                    writeback_packet.cpu = writeback_cpu;
                    writeback_packet.address = WQ.entry[index].address;
                    writeback_packet.full_addr = WQ.entry[index].full_addr;
                    writeback_packet.data = WQ.entry[index].data;
                    writeback_packet.instr_id = WQ.entry[index].instr_id;
                    writeback_packet.ip = 0;
                    writeback_packet.type = WRITEBACK;
                    writeback_packet.event_cycle = current_core_cycle[writeback_cpu];

                    lower_level->add_wq(&writeback_packet);

                    filled = 1;
                }
            }

            if (filled)
                filled &= fill_cache_tag(tag_array_set, &WQ.entry[index]);
        }

        if (filled)
        {
            WQ.remove_queue(&WQ.entry[index]);
        }
    }
}

uint32_t REUSE_CACHE_LLC::get_set(uint64_t address)
{
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

int REUSE_CACHE_LLC::check_hit(PACKET *packet)
{
    uint32_t set = get_set(packet->address);

    for (uint32_t i = 0; i < NUM_TAG_ARRAY_WAYS; i++)
    {
        if (tag_array[set][i].valid && tag_array[set][i].tag == packet->address && tag_array[set][i].hasData)
        {
            return 1;
        }
    }

    return 0;
}

int REUSE_CACHE_LLC::fill_cache_tag(uint32_t tag_array_set, PACKET *packet)
{
    uint32_t tag_victim = reuse_cache_llc_find_tag_array_victim(tag_array_set);

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
                    writeback_packet.data = tag_array[tag_array_set][tag_victim].data;
                    writeback_packet.instr_id = packet->instr_id;
                    writeback_packet.ip = 0; // writeback does not have ip
                    writeback_packet.type = WRITEBACK;
                    writeback_packet.event_cycle = current_core_cycle[packet->cpu];

                    lower_level->add_wq(&writeback_packet);
                }
            }
        }

        tag_array[tag_array_set][tag_victim].forward_backward_pointer->valid = 0;
    }

    tag_array[tag_array_set][tag_victim].valid = 1;
    tag_array[tag_victim][tag_array_set].dirty = 0;
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

    return 1;
}

int REUSE_CACHE_LLC::fill_cache_data(uint32_t tag_array_set, uint32_t tag_array_way, PACKET *packet)
{
    uint32_t data_array_set = (uint32_t)(tag_array_set & ((1 << lg2(NUM_DATA_ARRAY_SETS)) - 1));
    uint32_t data_array_way = reuse_cache_llc_find_data_array_victim(data_array_set);

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
                data_array[data_array_set][data_array_way].forward_backward_pointer->valid = 0;
            }
        }
    }

    data_array[data_array_set][data_array_way].valid = 1;
    data_array[data_array_set][data_array_way].dirty = 0;
    data_array[data_array_way][data_array_set].used = 0;
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

    tag_array[tag_array_set][tag_array_way].nrr = 0;
    data_array[data_array_set][data_array_way].nru = 1;

    return 1;
}

uint32_t REUSE_CACHE_LLC::reuse_cache_llc_find_tag_array_victim(uint32_t tag_array_set)
{
    for (int i = 0; i < NUM_TAG_ARRAY_WAYS; i++)
    {
        if (tag_array[tag_array_set][i].valid == 0)
        {
            return i;
        }
    }

    for (int i = 0; i < NUM_TAG_ARRAY_WAYS; i++)
    {
        if (tag_array[tag_array_set][i].nrr == 1)
        {
            return i;
        }
    }
}

uint32_t REUSE_CACHE_LLC::reuse_cache_llc_find_data_array_victim(uint32_t data_array_set)
{
    for (int i = 0; i < NUM_DATA_ARRAY_WAYS; i++)
    {
        if (data_array[data_array_set][i].valid == 0)
        {
            return i;
        }
    }

    for (int i = 0; i < NUM_DATA_ARRAY_WAYS; i++)
    {
        if (data_array[data_array_set][i].nru == 1)
        {
            return i;
        }
    }
}
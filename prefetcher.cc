#include "interface.hh"

#include <stdlib.h>
#include <stdint.h>


#define GHB_SIZE        128                                     // Number of entries in the GHB
#define MAX_ADDR_BITS   28                                      // log2 of MAX_PHYS_MEM_ADDR + 1
#define CZONE_SIZE      64*1024                                 // Number of bytes in each CZone
#define CZONE_BITS      12                                      // log2 of NUM_CZONES
#define PREFETCH_DEGREE 2                                       // Number of prefeches generated on a correlation hit
#define NUM_CZONES      (MAX_PHYS_MEM_ADDR + 1)/(CZONE_SIZE)    // Number of CZones


typedef struct GHB_entry_t{
    Addr                address;       // Address of memory access
    struct GHB_entry_t* next_instance; // Next in the linked list of accesses w/ in same CZone
    struct GHB_entry_t* prev_instance; // Prev in the linked list of accesses w/ in same CZone
} GHB_entry;

GHB_entry   GHB[GHB_SIZE];   // Global History Buffer
GHB_entry*  GHB_head;        // Current head of the GHB
GHB_entry*  IT[NUM_CZONES];  // Index table

int32_t delta_buffers[NUM_CZONES*(GHB_SIZE + 1)];  // Delta buffers for all CZones. It is more than NUM_CZONES times bigger than it needs to to keep cluttering in memory to a minimum
int32_t* delta_buffer_heads[NUM_CZONES];           // Pointers to the heads of the delta buffers

/**
 * Checks if the address of the provided GHB entry is
 * larger than the maximum to determine if tha memory
 * location has been used before or not
 */ 
int is_unused(GHB_entry* entry){
    return (entry->address > MAX_PHYS_MEM_ADDR);
}

/**
 * Generates czone tags for memory addresses by right-shifting them
 * and cutting off the most significant 32 bits (which should always
 * be zeroes)
 */ 
uint32_t generate_czone_tag(uint64_t miss_address){
    uint32_t tag = (uint32_t) (miss_address >> (MAX_ADDR_BITS - CZONE_BITS));
    return tag;
}

/**
 * Pushes a delta to the delta bufffer
 */ 
void deltabuffer_push(uint32_t czone_tag, int32_t delta){
    *(delta_buffer_heads[czone_tag]) = delta;
    ++delta_buffer_heads[czone_tag];
}

/**
 * Removes the least recently added delta in the delta buffer.
 */ 
void deltabuffer_remove(uint32_t czone_tag){
    if (delta_buffer_heads[czone_tag] != &delta_buffers[czone_tag*GHB_SIZE]){
        memcpy(&delta_buffers[czone_tag*GHB_SIZE], &delta_buffers[czone_tag*GHB_SIZE] + 1, 
                (delta_buffer_heads[czone_tag] - &delta_buffers[czone_tag*GHB_SIZE])*sizeof(int32_t));
        --(delta_buffer_heads[czone_tag]);
        *(delta_buffer_heads[czone_tag])  = 0;
    }
}


/**
 * Inserts a new GHB entry with the address a into the GHB
 * Inserts into delta buffers and removes if necessary
 * Sets the relevant pointers to previous and next entry in the GHB
 * of its CZone.
 */
void GHB_insert(Addr a){
    // Increment pointer to GHB head, and make sure it doesn't go out of bounds
    ++GHB_head;
    if (GHB_head > &GHB[GHB_SIZE-1]){
        GHB_head = GHB;
    }
   
    // Check if the current memory location has been used before
    if (!is_unused(GHB_head)){
        uint32_t evict_tag = generate_czone_tag(GHB_head->address);
        GHB_entry* entry_to_evict = IT[evict_tag];
        
        // If element to be evicted has a representative in the IT
        if (entry_to_evict != NULL){
            deltabuffer_remove(evict_tag);
        }

        // If the entry to evict is the only one from its CZone in the GHB
        if (entry_to_evict == GHB_head){
            IT[evict_tag] = NULL;
        }
    }

    // Set address and linked list pointers of the new entry in the GHB
    GHB_head->address       = a;
    GHB_head->prev_instance = NULL;

    uint32_t czone_tag = generate_czone_tag(a);
    GHB_entry* prev_czone_access = IT[czone_tag];


    if (prev_czone_access != NULL){ // If new entry is not the only one from its CZone in the GHB
        GHB_head->next_instance = prev_czone_access;
        prev_czone_access->prev_instance = GHB_head;
        deltabuffer_push(czone_tag, (int32_t)prev_czone_access->address - (int32_t)a);
    } else {
        GHB_head->next_instance = NULL;
        delta_buffer_heads[czone_tag] = &delta_buffers[czone_tag*GHB_SIZE];
    }
    IT[czone_tag] = GHB_head;
}


/**
* Searches for any delta pair in the delta buffers
* matching the most recent delta pair.
*/
void calculate_correlation_hit(int32_t* delta_buffer_head, int32_t* delta_buffer_tail, int32_t* hit_p, int32_t* prefetch_start){
    *hit_p = 0;
    int32_t key_register[2];
    key_register[0] = *(delta_buffer_head - 2);
    key_register[1] = *(delta_buffer_head - 1); // Correlation key register
        
    int32_t* iterator = delta_buffer_head - 4;
    if (iterator <= delta_buffer_tail){
        return;
    }

    int32_t comparison_register[2];
    comparison_register[0] = *iterator;
    comparison_register[1] = *(iterator + 1);   // Comparison register

    while(iterator >= delta_buffer_tail){
        if(comparison_register[0] == key_register[0] && comparison_register[1] == key_register[1]){
            *hit_p = 1;
            prefetch_start = iterator + 2;
            return;
        }
        
        --iterator;
        comparison_register[0] = *iterator;
        comparison_register[1] = *(iterator + 1);        
    }
}

/**
* Issues the prefetches from a given start point in the delta buffer
* Also checks if the the given prefetch is in the mshr queue or cache already
* and drops it if it is.
*/
void CDC_issue_prefetches(int32_t* pf_delta_start, int32_t* delta_buffer_head, int32_t* delta_buffer_tail, Addr pf_addr_start){
    Addr current_address = pf_addr_start;
    int32_t* local_pf_delta_start = pf_delta_start;
    int it = 0;

    for(int i = 0; i < PREFETCH_DEGREE; i++){

        if(!in_cache(current_address) && !in_mshr_queue(current_address)){
            issue_prefetch(current_address);
        }        
        if(local_pf_delta_start + it >= delta_buffer_head){
            local_pf_delta_start = delta_buffer_tail;
            it = 0;
        }
        current_address += *(local_pf_delta_start + it);

        it++;
    }
}


/**
 * Initializes the GHB entries, GHB_head, the Index table entries
 * and the delta buffer head pointers
 * Also sets the address member to an address larger than what is 
 * possible in the simulator, to make unused GHB entries easy to
 * detect.
 */ 
void prefetch_init(void){
    for (int i = 0; i < GHB_SIZE; i++){
        GHB[i].prev_instance = NULL;
        GHB[i].next_instance = NULL;
        GHB[i].address       = MAX_PHYS_MEM_ADDR + 1;
    };

    GHB_head = GHB;

    for (int i = 0; i < NUM_CZONES; i++){
        IT[i] = NULL;
        delta_buffer_heads[i] = &delta_buffers[i*GHB_SIZE];
    }

    //DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
}

/**
 * Called on every memory acces to the L2 cache
 * 
 * If the access is a miss, it's added to the GHB, the deltabuffer,
 * and a search for correlating delta pairs is begun. If a pair is found
 * the prefetches are generated
 * 
 * If the access is a hit with its prefetch bit set, the function behaves
 * as if it were a miss, but it also clears the prefetch bit of the cache
 * line.
 * 
 * If the access is anything else, the function does nothing
 */
void prefetch_access(AccessStat stat){
    if (stat.miss || (!stat.miss && get_prefetch_bit(stat.mem_addr))){
        if (get_prefetch_bit(stat.mem_addr)){
            clear_prefetch_bit(stat.mem_addr);
        }
        
        GHB_insert(stat.mem_addr); //updates IT, inserts into GHB, updates DB
        
        int32_t *hit_p, *pf_delta_start;
        int32_t hit = 0;
        int32_t pf_d_start = 0;
        hit_p    = &hit;
        pf_delta_start = &pf_d_start;

        uint32_t czone_tag = generate_czone_tag(stat.mem_addr);

        calculate_correlation_hit(delta_buffer_heads[czone_tag], &delta_buffers[czone_tag*GHB_SIZE], hit_p, pf_delta_start);
        
        if(*hit_p){
            CDC_issue_prefetches(pf_delta_start, delta_buffer_heads[czone_tag], &delta_buffers[czone_tag*GHB_SIZE], GHB_head->address);
        }
    }
}

/**
* Sets the prefetch bit of a cache line, so that 
* prefetches are generated on the next reference 
* to even though it is in the cache already.
*/
void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
    set_prefetch_bit(addr);
}

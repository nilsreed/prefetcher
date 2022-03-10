#include "interface.hh"

#include <stdlib.h>
#include <stdint.h>
// TODO: REMOVE BEFORE SUMBISSION
/*
#define CZONE_256B    256
#define CZONE_64KB    64*1024
#define CZONE_128KB   128*1024
#define CZONE_16MB    16*1024*1024
#define PF_DEG_2      2
#define PF_DEG_4      4
#define PF_DEG_8      8
#define PF_DEG_16     16 
*/
#define GHB_SIZE        256
#define MAX_ADDR_BITS   28            // log2 of MAX_PHYS_MEM_ADDR + 1
#define CZONE_SIZE      64*1024
#define CZONE_BITS      16
#define PREFETCH_DEGREE 2
#define NUM_CZONES      (MAX_PHYS_MEM_ADDR + 1)/(CZONE_SIZE)

typedef struct GHB_entry_t{
    Addr                address;
    struct GHB_entry_t* next_instance; // Next in the linked list of accesses w/ in same CZone
    struct GHB_entry_t* prev_instance; // Prev in the linked list of accesses w/ in same CZone
    int32_t*            delta_buffer_head;
    int32_t*            delta_buffer_tail;
} GHB_entry;

GHB_entry   GHB[GHB_SIZE]; //Global History Buffer
GHB_entry*  GHB_head;
GHB_entry*  IT[NUM_CZONES];  //Index table

int32_t delta_buffers[NUM_CZONES*(GHB_SIZE + 1)]; //pointer to int arrays


uint32_t generate_czone_tag(uint64_t miss_address){
    uint32_t tag = (uint32_t) (miss_address >> (MAX_ADDR_BITS - CZONE_BITS));
    return tag;
}

void deltabuffer_push(GHB_entry* entry, int32_t delta){ //add delta to top of czone specific deltabuffer
    *(entry->delta_buffer_head) = delta;
    ++(entry->delta_buffer_head);
}

void deltabuffer_remove(GHB_entry* entry){ //remove delta from bottom of czone specific deltauffer    
    if(entry->delta_buffer_head != entry->delta_buffer_tail){
        memcpy(entry->delta_buffer_tail, entry->delta_buffer_tail + 1, (entry->delta_buffer_head - entry->delta_buffer_tail)*sizeof(int32_t));
        --(entry->delta_buffer_head);
        *(entry->delta_buffer_head) = 0;
    }
}


void GHB_insert(Addr a){
    //TODO: Verify
    //GHB_head = GHB + ((sizeof(GHB_head) + 1) % GHB_SIZE);
    ++GHB_head;
    if (GHB_head > GHB + GHB_SIZE){
        GHB_head = GHB;
    } 
    //om IT peika på det elementet som skal ut
    
    uint32_t evict_address = generate_czone_tag(GHB_head->address);
    GHB_entry* entry_to_evict = IT[evict_address];
    
    if (entry_to_evict != NULL){
        deltabuffer_remove(entry_to_evict);
    }

    if (entry_to_evict == GHB_head){
        IT[generate_czone_tag(GHB_head->address)] = NULL;
    }

    GHB_head->address       = a;
    GHB_head->prev_instance = NULL;

    //In some form or fashion, access Index table (hash table, TODO), and acquire ptr.
    uint32_t czone_tag = generate_czone_tag(a);
    GHB_entry* prev_czone_access = IT[czone_tag]; //Placeholder

    //Remember to assign deltabuffer head and tail pointers!
    if (prev_czone_access != NULL){
        GHB_head->next_instance = prev_czone_access;
        prev_czone_access->prev_instance = GHB_head;
        GHB_head->delta_buffer_head = prev_czone_access->delta_buffer_head;
        GHB_head->delta_buffer_tail = prev_czone_access->delta_buffer_tail;
        //TODO: insert delta calculator here
        deltabuffer_push(GHB_head, (int32_t)prev_czone_access->address - (int32_t)a);
    } else {
        //Add addr to Index table
        GHB_head->next_instance = NULL;
        GHB_head->delta_buffer_head = delta_buffers + czone_tag*GHB_SIZE;
        GHB_head->delta_buffer_tail = delta_buffers + czone_tag*GHB_SIZE;
    }
    IT[czone_tag] = GHB_head;
}


/*
CorrelationHit-kalkulator, som beskrevet i AC/DC. Hardkodet til correlation pairs,
men gitt problemstillingen vår bør vi kanskje vurdere å utvide til triplets eller
generisk/dynamisk størrelse
*/
void calculate_correlation_hit(int32_t* delta_buffer_head, int32_t* delta_buffer_tail, int32_t* hit_p, int32_t* prefetch_start){
    *hit_p = 0;
    int32_t key_register[2];
    key_register[0] = *(delta_buffer_head - 1);
    key_register[1] = *delta_buffer_head; //Correlation key register
        
    int32_t* iterator = delta_buffer_head - 3;
    if (iterator <= delta_buffer_tail){
        return;
    }

    int32_t comparison_register[2];
    comparison_register[0] = *iterator;
    comparison_register[1] = *(iterator + 1);

    while(iterator >= delta_buffer_tail){
        
        //comparison_register[0] == key_register[0] && comparison_register[1] == key_register[1]
        if(!(memcmp(key_register, comparison_register, sizeof(key_register)))){
            *hit_p = 1;
            prefetch_start = iterator + 2;
            return;
        }
        
        --iterator;
        comparison_register[0] = *iterator;
        comparison_register[1] = *(iterator + 1);        
    }

    //TODO, ikkje gjer nåke om der ikkje e nok prefetches!
}

void CDC_issue_prefetches(int32_t* pf_delta_start, int32_t* delta_buffer_head, int32_t* delta_buffer_tail, Addr pf_addr_start){
    Addr current_address = pf_addr_start;
    int32_t* local_pf_delta_start = pf_delta_start;
    for(int i = 0; i < PREFETCH_DEGREE; i++){

        if(!in_cache(current_address) && !in_mshr_queue(current_address)){
            issue_prefetch(current_address);
        }        
        if(local_pf_delta_start + i >= delta_buffer_head){
            local_pf_delta_start = delta_buffer_tail;
        }
        current_address += *(local_pf_delta_start + i);//delta_buffers[czone][*pf_delta_start+i]; //??
    }
}

/*
The real deal, to be called from the given prefetch access
TODO:
    On cache miss:

    Record miss address via interface function

    Generate czone tag from miss address

    Get pointer to czone GHB "head" from czone tag via index table

    Update GHB head and and index table with pointer to miss address

    Update Delta buffer for relevant czone using czone tag

    Calculate correlation hit(??)

    Use correlation hit pattern and agressivity to decide which adressess to prefetch

    Prefetch addresses via interface function
*/
void commit_prefetch(AccessStat stat){
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

        calculate_correlation_hit(GHB_head->delta_buffer_head, GHB_head->delta_buffer_tail, hit_p, pf_delta_start);
        
        if(*hit_p){
            CDC_issue_prefetches(pf_delta_start, GHB_head->delta_buffer_head, GHB_head->delta_buffer_tail, GHB_head->address);
        }
    }
}

void prefetch_init(void)
{
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */
    //GHB = (GHB_entry*) malloc(sizeof(GHB_entry)*GHB_SIZE);
    for (int i = 0; i < GHB_SIZE; i++){
        GHB[i].prev_instance = NULL;
        GHB[i].next_instance = NULL;
    };

    GHB_head = GHB;

    for (int i = 0; i < NUM_CZONES; i++){
        IT[i] = NULL;
    }

    //IT  = (GHB_entry**) malloc(sizeof(GHB_entry*)*NUM_CZONES);

    //delta_buffers = (int32_t**) malloc(sizeof(int32_t*)*NUM_CZONES);

//    for (int i = 0; i < NUM_CZONES; i++){
//        delta_buffers[i] = (int32_t*) malloc(sizeof(int32_t)*GHB_SIZE);
//    }
    
    
    //DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
}

void prefetch_access(AccessStat stat)
{
    commit_prefetch(stat);
    /* pf_addr is now an address within the _next_ cache block */
    //Addr pf_addr = stat.mem_addr + BLOCK_SIZE;

    /*
     * Issue a prefetch request if a demand miss occured,
     * and the block is not already in cache.
     */
    //if (stat.miss && !in_cache(pf_addr)) {
    //    issue_prefetch(pf_addr);
    //}
}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
    set_prefetch_bit(addr);
}

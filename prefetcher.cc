#include "interface.hh"

#define CZONE_256B    256
#define CZONE_64KB    64*1024
#define CZONE_128KB   128*1024
#define CZONE_16MB    16*1024*1024
#define PF_DEG_2      2
#define PF_DEG_4      4
#define PF_DEG_8      8
#define PF_DEG_16     16
#define GHB_SIZE      0
#define MAX_ADDR_BITS 28            // log2 of MAX_PHYS_MEM_ADDR + 1

typedef struct GHB_entry_t{
    Addr                address;
    struct GHB_entry_t* next_instance; // Next in the linked list of accesses w/ in same CZone
    struct GHB_entry_t* prev_instance; // Prev in the linked list of accesses w/ in same CZone
    int32_t*            delta_buffer_head;
    int32_t*            delta_buffer_tail;
} GHB_entry;

GHB_entry* GHB; //Global History Buffer
GHB_entry* GHB_HEAD;
GHB_entry* GHB_TAIL;
GHB_entry* IT;  //Index table

int32_t** delta_buffers; //pointer to int arrays



uint32_t g_czone_size = CZONE_64KB;
uint8_t g_prefetch_degree = PF_DEG_4;
uint8_t g_czone_bits;


uint32_t generate_czone_tag(uint64_t miss_address){
    uint32_t tag = miss_address >> (MAX_ADDR_BITS - g_czone_bits);
    return tag;
}

void deltabuffer_push(int32_t* deltabuffer_head, int32_t delta){ //add delta to top of czone specific deltabuffer
    ++deltabuffer_head;
    *deltabuffer_head = delta;
}

void deltabuffer_remove(int32_t* deltabuffer_head, int32_t* deltabuffer_tail){ //remove delta from bottom of czone specific deltauffer    
    if(deltabuffer_head != deltabuffer_tail){
        memcpy(deltabuffer_tail, deltabuffer_tail + 1, (deltabuffer_head - deltabuffer_tail)*sizeof(int32_t));
        *deltabuffer_head = 0;
        --deltabuffer_head;
    }
}


void GHB_insert(Addr a){
    GHB_HEAD = GHB + ((GHB_HEAD + 1) % GHB_SIZE);
    
    //om IT peika på det elementet som skal ut
    
    uint32_t evict_address = generate_czone_tag(GHB_HEAD->address);
    GHB_entry* entry_to_evict = IT[evict_address];

    deltabuffer_remove(entry_to_evict.deltabuffer_head, entry_to_evict.deltabuffer_tail);

    if (entry_to_evict == GHB_HEAD){
        IT[generate_czone_tag(GHB_HEAD->address)] = NULL;
    }

    GHB_HEAD->address       = a;
    GHB_HEAD->prev_instance = NULL;

    //In some form or fashion, access Index table (hash table, TODO), and acquire ptr.
    uint32_t czone_tag = generate_czone_tag(a);
    GHB_entry* prev_czone_access = IT[czone_tag]; //Placeholder

    //Remember to assign deltabuffer head and tail pointers!
    if (prev_czone_access != NULL){
        GHB_HEAD->next_instance = prev_czone_access;
        prev_czone_access->prev_instance = GHB_HEAD;
        GHB_HEAD->deltabuffer_head = prev_czone_access.deltabuffer_head;
        GHB_HEAD->deltabuffer_tail = prev_czone_access.deltabuffer_tail;
        //TODO: insert delta calculator here
        deltabuffer_push(GHB_HEAD->deltabuffer_head, ptr->address - a)
    } else {
        //Add addr to Index table
        GHB_HEAD->next_instance = NULL;
        GHB_HEAD->deltabuffer_head = deltabuffers[czone_tag];
        GHB_HEAD->deltabuffer_tail = deltabuffers[czone_tag];
    }
    IT[czone_tag] = GHB_HEAD;
}

/**
 * @brief log2 for powers of 2
 * 
 * Calculates log2 of a number that is known to be a power of 2
 * 
 * @param num a power, unknown, of 2
 * @return uint8_t
 */
uint8_t num_bits_2p(uint64_t num){
    uint8_t bits = 0;
    uint64_t local = num;
    while((local & (uint64_t)1) != 1){
        bits++;
        local = local >> 1;
    }
    return bits;
}

/*
CorrelationHit-kalkulator, som beskrevet i AC/DC. Hardkodet til correlation pairs,
men gitt problemstillingen vår bør vi kanskje vurdere å utvide til triplets eller
generisk/dynamisk størrelse
*/
void calculate_correlation_hit(int32_t* deltabuffer_head, int32_t* deltabuffer_tail, int32_t* hit_p, int* prefetch_start){
    *hit_p = 0;
    int32_t[2] key_register = {*(deltabuffer_head - 1), *deltabuffer_head}; //Correlation key register
        
    int32_t* iterator = deltabuffer_head - 3;
    int32_t[2] comparison_register = {iterator, iterator + 1};
    while(iterator <= deltabuffer_tail){
        
        //comparison_register[0] == key_register[0] && comparison_register[1] == key_register[1]
        if(!(memcmp(key_register, comparison_register, sizeof(key_register)))){
            *hit_p = 1;
            prefetch_start = iterator + 2;
            return;
        }
        
        --iterator;
        comparison_register = {iterator, iterator + 1};        
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
    uint32_t czone = generate_czone_tag(stat.mem_addr);


    
}

void prefetch_init(void)
{
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */
    GHB = GHB_entry[GHB_SIZE];
    for (int i = 0; i < GHB_SIZE; i++){
        GHB[i].prev_instance = NULL;
        GHB[i].next_instance = NULL;
    };

    IT  = malloc(sizeof(GHB_entry*)*(MAX_PHYS_MEM_ADDR + 1/(g_czone_size))));
    g_czone_bits = num_bits_2p(MAX_PHYS_MEM_ADDR + 1/(g_czone_size));
    
    //DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
}

void prefetch_access(AccessStat stat)
{
    /* pf_addr is now an address within the _next_ cache block */
    Addr pf_addr = stat.mem_addr + BLOCK_SIZE;

    /*
     * Issue a prefetch request if a demand miss occured,
     * and the block is not already in cache.
     */
    if (stat.miss && !in_cache(pf_addr)) {
        issue_prefetch(pf_addr);
    }
}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
    set_prefetch_bit(addr);
}

#define NO_MALLOC_REDIRECT

#include "dryos.h"
#include "bmp.h"
#include "property.h"
#include "lens.h"
#include "raw.h"

extern void* _malloc(size_t size);
extern void _free(void* ptr);

/* semaphore from mem.c, used for these routines as well */
extern struct semaphore *mem_sem;

/* the CBRs are called from Canon's RscMgr task */
THREAD_ROLE(RscMgr);

/* semaphore routines used more like event flags or message queues
 * (not checked for thread safety)
 */
static inline NO_THREAD_SAFETY_ANALYSIS
int take_semaphore_nc(struct semaphore *sem, int timeout)
{
    return take_semaphore(sem, timeout);
}

static inline NO_THREAD_SAFETY_ANALYSIS
int give_semaphore_nc(struct semaphore *sem)
{
    return give_semaphore(sem);
}

// experimental memory allocation from shooting buffer (~160MB on 5D2)

typedef struct
{
    struct memSuite *ret;
    uint32_t size;
    uint32_t timed_out;
    struct semaphore *sem;
    int pool;               /* index in RSCMGR_POOLS, -1 = RSCMGR_TYPE_ID */
} alloc_msg_t;

static struct semaphore *alloc_sem = 0;
static struct semaphore *free_sem = 0;

/* for SRM - to know whether somebody called shoot_malloc_suite(0) or not */
static GUARDED_BY(mem_sem) struct memSuite *shoot_full_suite = 0;

int GetNumberOfChunks(struct memSuite *suite)
{
    CHECK_SUITE_SIGNATURE(suite);
    return suite->num_chunks;
}

int GetSizeOfMemorySuite(struct memSuite *suite)
{
    CHECK_SUITE_SIGNATURE(suite);
    return suite->size;
}

int GetSizeOfMemoryChunk(struct memChunk *chunk)
{
    CHECK_CHUNK_SIGNATURE(chunk);
    return chunk->size;
}

#ifdef CONFIG_RSCMGR_IBUS
#include "ibus.h"

/**
 * DIGIC X: some RscMgr pools (R8: 0xb and the big pool) are described by a
 * 64-bit IBus base instead of a CPU address. Canon's
 * GetMemoryAddressOfMemoryChunk asserts on their chunks, although they are
 * ordinary RAM that the fixed memory windows map.
 *
 * Same decision as the ROM getter (R8 0xe0849076):
 *   chunk type 1 / 2, no owner heap, or owner without IBus base
 *       -> the ROM getter (unchanged behaviour)
 *   owner with IBus base (the case the ROM asserts on)
 *       -> GetIBusAddressOfMemoryChunk + ibus_to_cpu
 * The result is uncached, like the ROM's (e.g. 0x462d6070 for the generic pool),
 * so callers (mlv_lite, silent, mem.c ...) see no difference.
 */
void *GetMemoryAddressOfMemoryChunk(struct memChunk *chunk)
{
    CHECK_CHUNK_SIGNATURE(chunk);

    uint32_t owner = (uint32_t)chunk->heap_ptr;     /* +0x14 */
    int type = chunk->mem_type;                     /* +0x24 */
    if (type == 1 || type == 2 || !owner ||
        (MEM(owner + 0x40) | MEM(owner + 0x44)) == 0)   /* owner's u64 IBus base (0xe01c4d96) */
    {
        return _GetMemoryAddressOfMemoryChunk(chunk);
    }

    uint32_t cpu = ibus_to_cpu(GetIBusAddressOfMemoryChunk(chunk));
    return cpu ? UNCACHEABLE(cpu) : NULL;           /* NULL: no CPU window (DRAM Ch3) */
}
#endif // CONFIG_RSCMGR_IBUS

static void freeCBR(unsigned int a)
{
    give_semaphore_nc(free_sem);
}

static void freeCBR_nowait(unsigned int a)
{
}

#ifdef CONFIG_RSCMGR_POOLS
/* multi-pool shoot_malloc_suite, see rscmgr_pools_malloc_suite() */
static GUARDED_BY(mem_sem) struct memSuite *rscmgr_joined = 0;
static REQUIRES(mem_sem) void rscmgr_pools_free_all();
static void rscmgr_pool_request_waiting(int pool);
static void rscmgr_pool_request_done(int pool);
#endif

REQUIRES(mem_sem)
void _shoot_free_suite(struct memSuite *hSuite)
{
    if (hSuite == shoot_full_suite)
    {
        shoot_full_suite = 0;
    }

#ifdef CONFIG_RSCMGR_POOLS
    if (hSuite != NULL && hSuite == rscmgr_joined)
    {
        rscmgr_pools_free_all();
        return;
    }
#endif

    if (hSuite != NULL)
    {
        // FreeMemoryResource is not null pointer safe on D678, crashes
        FreeMemoryResource(hSuite, freeCBR, 0);
        hSuite = NULL;
    }
    take_semaphore_nc(free_sem, 0);
}

static void allocCBR(unsigned int priv, struct memSuite *hSuite)
{
    alloc_msg_t *suite_info = (alloc_msg_t *)priv;
    
    /* in case we timed out last time, immediately free the newly allocated suite (its the one that timed out) */
    if(suite_info->timed_out)
    {
        if (hSuite != NULL)
        {
            // FreeMemoryResource is not null pointer safe on D678, crashes
            FreeMemoryResource(hSuite, freeCBR_nowait, 0);
            hSuite = NULL;
        }
#ifdef CONFIG_RSCMGR_POOLS
        if (suite_info->pool >= 0)
        {
            rscmgr_pool_request_done(suite_info->pool);
        }
#endif
        _free(suite_info);
        return;
    }
    
    suite_info->ret = hSuite;
    give_semaphore_nc(suite_info->sem);
}

unsigned int exmem_save_buffer(struct memSuite *hSuite, char *file)
{
    unsigned int written = 0;
    
    FILE *f = FIO_CreateFile(file);
    if (f)
    {
        struct memChunk *currentChunk;
        unsigned char *chunkAddress;
        unsigned int chunkAvail;
        
        currentChunk = GetFirstChunkFromSuite(hSuite);
        
        while(currentChunk)
        {
            chunkAvail = GetSizeOfMemoryChunk(currentChunk);
            chunkAddress = (unsigned char*)GetMemoryAddressOfMemoryChunk(currentChunk);
            
            FIO_WriteFile(f, chunkAddress, chunkAvail);
            written += chunkAvail;
            currentChunk = GetNextMemoryChunk(hSuite, currentChunk);
        }
        FIO_CloseFile(f);
    }
    
    return written;
}

unsigned int exmem_clear(struct memSuite *hSuite, char fill)
{
    unsigned int written = 0;
    
    struct memChunk *currentChunk;
    unsigned char *chunkAddress;
    unsigned int chunkAvail;
    
    currentChunk = GetFirstChunkFromSuite(hSuite);
    
    while(currentChunk)
    {
        chunkAvail = GetSizeOfMemoryChunk(currentChunk);
        chunkAddress = (unsigned char*)GetMemoryAddressOfMemoryChunk(currentChunk);
        
        memset(CACHEABLE(chunkAddress), fill, chunkAvail);
        written += chunkAvail;
        currentChunk = GetNextMemoryChunk(hSuite, currentChunk);
    }
    return written;
}

/* exmem_ibus_alloc: memory for bus masters, see exmem.h */
extern void *_AllocateMemory(size_t size);
extern void  _FreeMemory(void *ptr);

enum { IBUS_SRC_NONE, IBUS_SRC_ML, IBUS_SRC_MALLOC, IBUS_SRC_ALLOCMEM };

static void *ibus_src_alloc(int source, uint32_t size)
{
    switch (source)
    {
        case IBUS_SRC_ML:       return __mem_malloc(size, MEM_TEMPORARY | MEM_DMA, __FILE__, __LINE__);
        case IBUS_SRC_MALLOC:   return _malloc(size);
        case IBUS_SRC_ALLOCMEM: return _AllocateMemory(size);
    }
    return NULL;
}

static void ibus_src_free(int source, void *raw)
{
    switch (source)
    {
        case IBUS_SRC_ML:       __mem_free(raw);    break;
        case IBUS_SRC_MALLOC:   _free(raw);         break;
        case IBUS_SRC_ALLOCMEM: _FreeMemory(raw);   break;
    }
}

int exmem_ibus_alloc(struct ibus_buf *buf, uint32_t size, uint32_t align, int pid, int flags)
{
    static const int long_lived[] = { IBUS_SRC_MALLOC, IBUS_SRC_ALLOCMEM, IBUS_SRC_NONE };
    static const int temporary[]  = { IBUS_SRC_ML, IBUS_SRC_MALLOC, IBUS_SRC_ALLOCMEM, IBUS_SRC_NONE };
    const int *sources = (flags & IBUS_BUF_LONG_LIVED) ? long_lived : temporary;

    memset(buf, 0, sizeof(*buf));

    /* at least one cache line, so the uncached buffer shares no line with other data */
    if (align < 32)
    {
        align = 32;
    }
    if (!size || (align & (align - 1)))
    {
        return -1;
    }
    uint32_t rounded = (size + align - 1) & ~(align - 1);

    for (int i = 0; sources[i] != IBUS_SRC_NONE; i++)
    {
        void *raw = ibus_src_alloc(sources[i], rounded + align);
        if (!raw)
        {
            continue;
        }

        uint32_t aligned = ((uint32_t)raw + align - 1) & ~(align - 1);
        void *cpu = UNCACHEABLE(aligned);
        uint64_t ibus = 0;

#ifdef CONFIG_RSCMGR_IBUS
        ibus = cpu_to_ibus(cpu);
        if (!ibus || (pid >= 0 && memiarb_allowed(pid, IBUS_DRAM_CH(ibus)) == 0))
        {
            /* not RAM, or a DRAM channel this bus master may not use */
            ibus_src_free(sources[i], raw);
            continue;
        }
#else
        (void)pid;
#endif

        /* no dirty cache line may later overwrite what is written uncached */
        sync_caches();

        buf->cpu    = cpu;
        buf->ibus   = ibus;
        buf->size   = size;
        buf->raw    = raw;
        buf->source = sources[i];
        return 0;
    }

    return -1;
}

void exmem_ibus_free(struct ibus_buf *buf)
{
    if (buf && buf->raw)
    {
        ibus_src_free(buf->source, buf->raw);
    }
    if (buf)
    {
        memset(buf, 0, sizeof(*buf));
    }
}

#ifdef CONFIG_DRAM3_WORKMEM
#ifndef CONFIG_RSCMGR_IBUS
#error "CONFIG_DRAM3_WORKMEM needs CONFIG_RSCMGR_IBUS (ibus.h, memory windows)"
#endif
/*
 * DRAM Ch3 from RscMgr WorkMem
 * ============================
 *
 * WorkMem areas are fixed address ranges listed in the ROM area table
 * (R8 0xe0f31c98). WorkMem_Alloc(id) answers through a callback, at once
 * when the area is free; when Canon holds it the request is queued until
 * Canon frees it. So: ask for every area, wait a short time, and keep what
 * arrived. A request still queued after that is marked "late": its
 * callback gives the area back as soon as it gets it.
 */
extern void WorkMem_Alloc(uint32_t id, void (*cbr)(void *ctx, void *handle), void *ctx);
extern void WorkMem_Free(void *handle, void (*cbr)(void *ctx), void *ctx);

struct workmem_part                 /* 24 bytes */
{
    uint32_t cpu;                   /* 0 on DRAM Ch3 */
    uint32_t unk1;
    uint64_t ibus;
    uint32_t size;
    uint32_t unk2;
};

struct workmem_handle
{
    uint32_t id;
    uint32_t count;
    struct workmem_part part[5];
};

enum { AREA_IDLE, AREA_REQUESTED, AREA_HELD, AREA_LATE, AREA_FREEING };

static const uint32_t dram3_area_id[] = DRAM3_WORKMEM_AREAS;
#define DRAM3_AREAS COUNT(dram3_area_id)

static struct workmem_handle *dram3_handle[DRAM3_AREAS];
static volatile int dram3_state[DRAM3_AREAS];
static struct semaphore *dram3_sem = NULL;     /* wake-up only: gives may merge */
static struct dram3_mem dram3;

#define DRAM3_WAIT_MS 200                       /* free areas answer in < 10 ms */
#define DRAM3_FREE_MS 2000

/*
 * Wait until no area is in `state`, at most timeout_ms. Returns how many
 * still are. The callbacks give dram3_sem only to wake us up: gives can
 * merge, so never count them - look at the states instead.
 */
static int dram3_wait_while(int state, int timeout_ms)
{
    int deadline = get_ms_clock() + timeout_ms;
    while (1)
    {
        int pending = 0;
        for (int i = 0; i < DRAM3_AREAS; i++)
        {
            pending += (dram3_state[i] == state);
        }

        int left = deadline - get_ms_clock();
        if (!pending || left <= 0)
        {
            return pending;
        }
        take_semaphore(dram3_sem, MIN(left, 10));
    }
}

static void dram3_free_cbr(void *ctx)
{
    int i = (int)ctx;
    dram3_handle[i] = NULL;
    dram3_state[i] = AREA_IDLE;
    give_semaphore(dram3_sem);
}

static void dram3_alloc_cbr(void *ctx, void *handle)
{
    int i = (int)ctx;
    dram3_handle[i] = handle;

    uint32_t old = cli();
    int late = (dram3_state[i] == AREA_LATE);
    if (!late)
    {
        dram3_state[i] = AREA_HELD;
    }
    sei(old);

    if (late)
    {
        WorkMem_Free(handle, dram3_free_cbr, ctx);   /* we stopped waiting */
    }
    else
    {
        give_semaphore(dram3_sem);
    }
}

static int ranges_overlap(const struct ibus_range *a, uint64_t ibus, uint32_t size)
{
    return ibus < a->ibus + a->size && a->ibus < ibus + size;
}

/* add one area part to dram3.range[], kept sorted by size, largest first */
static void dram3_add_range(uint64_t ibus, uint32_t size)
{
    if (IBUS_DRAM_CH(ibus) != 3 || size == 0)
    {
        return;
    }
    for (int k = 0; k < dram3.count; k++)
    {
        if (ranges_overlap(&dram3.range[k], ibus, size))
        {
            return;                             /* two areas share memory: use it once */
        }
    }
    if (dram3.count == DRAM3_MAX_RANGES)
    {
        return;
    }

    int k = dram3.count++;
    while (k > 0 && dram3.range[k - 1].size < size)
    {
        dram3.range[k] = dram3.range[k - 1];
        k--;
    }
    dram3.range[k].ibus = ibus;
    dram3.range[k].size = size;
    dram3.total += size;
}

const struct dram3_mem *exmem_dram3_alloc(void)
{
    if (!dram3_sem)
    {
        dram3_sem = create_named_semaphore("dram3", 0);
    }
    if (dram3.count)
    {
        return NULL;                            /* already taken */
    }

    for (int i = 0; i < DRAM3_AREAS; i++)
    {
        if (dram3_state[i] == AREA_IDLE)
        {
            dram3_state[i] = AREA_REQUESTED;
            WorkMem_Alloc(dram3_area_id[i], dram3_alloc_cbr, (void *)i);
        }
    }

    /* free areas answer within a few ms; busy ones never do */
    dram3_wait_while(AREA_REQUESTED, DRAM3_WAIT_MS);

    uint32_t old = cli();
    for (int i = 0; i < DRAM3_AREAS; i++)
    {
        if (dram3_state[i] == AREA_REQUESTED)
        {
            dram3_state[i] = AREA_LATE;
        }
    }
    sei(old);

    memset(&dram3, 0, sizeof(dram3));
    for (int i = 0; i < DRAM3_AREAS; i++)
    {
        if (dram3_state[i] == AREA_LATE && dram3.busy_count < DRAM3_MAX_BUSY)
        {
            dram3.busy_id[dram3.busy_count++] = dram3_area_id[i];
            printf("[DRAM3] area %x busy\n", (unsigned int)dram3_area_id[i]);
        }

        struct workmem_handle *h = dram3_handle[i];
        if (dram3_state[i] != AREA_HELD || !h)
        {
            continue;
        }
        for (int p = 0; p < (int)h->count && p < COUNT(h->part); p++)
        {
            dram3_add_range(h->part[p].ibus, h->part[p].size);
        }
    }

    printf("[DRAM3] %d ranges, %d MB\n", dram3.count, (int)(dram3.total >> 20));
    if (!dram3.count)
    {
        exmem_dram3_free();
        return NULL;
    }

    return &dram3;
}

void exmem_dram3_free(void)
{
    for (int i = 0; i < DRAM3_AREAS; i++)
    {
        if (dram3_state[i] == AREA_HELD)
        {
            dram3_state[i] = AREA_FREEING;
            WorkMem_Free(dram3_handle[i], dram3_free_cbr, (void *)i);
        }
    }

    int left = dram3_wait_while(AREA_FREEING, DRAM3_FREE_MS);
    if (left)
    {
        printf("[DRAM3] %d areas not freed after %d ms\n", left, DRAM3_FREE_MS);
    }
    memset(&dram3, 0, sizeof(dram3));
}
#endif /* CONFIG_DRAM3_WORKMEM */

/**
 * Resource type ID, the 4th argument of AllocateMemoryResource() and
 * AllocateContinuousMemoryResource(): which RscMgr pool shoot_malloc uses.
 * Older models use 0x50; cameras where that ID is invalid set RSCMGR_TYPE_ID
 * in consts.h (R8: 0, the generic pool; 0x50 asserts there).
 */
#ifndef RSCMGR_TYPE_ID
#define RSCMGR_TYPE_ID 0x50
#endif

/* one RscMgr request; NULL if it doesn't fit (the request then stays queued
 * in RscMgr and allocCBR frees it when it completes later) */
static struct memSuite *rscmgr_alloc_suite(size_t size, int type, int continuous, int pool)
{
    alloc_msg_t *suite_info = _malloc(sizeof(alloc_msg_t));
    if (!suite_info)
    {
        return NULL;
    }

    suite_info->ret = NULL;
    suite_info->timed_out = 0;
    suite_info->size = size;
    suite_info->sem = alloc_sem;
    suite_info->pool = pool;

    if (continuous)
    {
        AllocateContinuousMemoryResource(size, allocCBR, (unsigned int)suite_info, type);
    }
    else
    {
        AllocateMemoryResource(size, allocCBR, (unsigned int)suite_info, type);
    }

    int r = take_semaphore_nc(suite_info->sem, 100);
    if (r)
    {
        // signal to allocCBR that it needs to free suite_info
#ifdef CONFIG_RSCMGR_POOLS
        if (pool >= 0)
        {
            rscmgr_pool_request_waiting(pool);
        }
#endif
        suite_info->timed_out = 1;
        return NULL;
    }

    struct memSuite *hSuite = suite_info->ret;
    _free(suite_info);

    ASSERT((int)size <= hSuite->size);

    return hSuite;
}

/* when size is set to zero, it will try to allocate the maximum possible block */
static struct memSuite *shoot_malloc_suite_int(size_t size)
{
    return rscmgr_alloc_suite(size, RSCMGR_TYPE_ID, 0, -1);
}

static size_t largest_chunk_size(struct memSuite *hSuite)
{
    int max_size = 0;
    struct memChunk *hChunk = GetFirstChunkFromSuite(hSuite);
    while(hChunk)
    {
        int size = GetSizeOfMemoryChunk(hChunk);
        max_size = MAX(max_size, size);
        hChunk = GetNextMemoryChunk(hSuite, hChunk);
    }
    return max_size;
}

static REQUIRES(mem_sem)
size_t shoot_malloc_autodetect()
{
    /* allocate some backup that will service the queued allocation request that fails during the loop */
    size_t backup_size = 4 * 1024 * 1024;
    size_t max_size = 0;
    struct memSuite *backup = shoot_malloc_suite_int(backup_size);

    if (backup == NULL)
        return max_size;

    for (int size_mb = 4; size_mb < 1024; size_mb += 4)
    {
        int tested_size = size_mb * 1024 * 1024;
        //qprintf("[shoot_malloc] trying %s\n", format_memory_size(tested_size));
        struct memSuite *testSuite = shoot_malloc_suite_int(tested_size);
        if (testSuite)
        {
            /* leave 1MB unallocated, just in case */
            max_size = tested_size + backup_size - 1024 * 1024;
            _shoot_free_suite(testSuite);
        }
        else
        {
            //qprintf("[shoot_malloc] memory full\n");
            break;
        }
    }
    /* now free the backup suite. this causes the queued allocation before to get finished. as we timed out, it will get freed immediately in exmem.c:allocCBR */
    _shoot_free_suite(backup);
    
    //qprintf("[shoot_malloc] autodetected size: %s\n", format_memory_size(max_size));
    return max_size;
}

static REQUIRES(mem_sem)
size_t shoot_malloc_autodetect_contig(uint32_t requested_size)
{
    /* allocate some backup that will service the queued allocation request that fails during the loop */
    size_t backup_size = 1024 * 1024;
    size_t max_contig_size = 0;
    struct memSuite *backup = shoot_malloc_suite_int(backup_size);

    if (backup == NULL)
        return max_contig_size;

    for (int size_mb = 1; size_mb < 1024; size_mb++)
    {
        int tested_size = size_mb * 1024 * 1024;
        //qprintf("[shoot_contig] trying %s\n", format_memory_size(tested_size));
        struct memSuite *testSuite = shoot_malloc_suite_int(tested_size);
        if (testSuite)
        {
            /* find largest chunk */
            max_contig_size = largest_chunk_size(testSuite);
 
            _shoot_free_suite(testSuite);

            if (requested_size && requested_size <= max_contig_size)
            {
                /* have we already got a contiguous block large enough? */
                break;
            }
        }
        else
        {
            //qprintf("[shoot_contig] memory full\n");
            break;
        }
    }
    /* now free the backup suite. this causes the queued allocation before to get finished. as we timed out, it will get freed immediately in exmem.c:allocCBR */
    _shoot_free_suite(backup);
    
    //qprintf("[shoot_contig] autodetected size: %s (requested %x)\n", format_memory_size(max_contig_size), requested_size);

    return max_contig_size;
}

#ifdef CONFIG_RSCMGR_POOLS
/**
 * Cameras with several RscMgr pools (RSCMGR_POOLS in consts.h, R8: big,
 * 0xb and 0x58): shoot_malloc_suite(0) takes the largest size from each
 * pool and joins them into one suite, the same way _srm_malloc_suite()
 * packs SRM buffers. Modules (mlv_lite, silent...) see one suite.
 *
 * The RSCMGR_TYPE_ID pool (R8: generic) is not in the list: it stays
 * available to _shoot_malloc, i.e. ML's own fio_malloc buffers.
 *
 * RscMgr queues a request that doesn't fit instead of failing it. Each
 * pool keeps a count of its requests still waiting (they complete and are
 * freed by allocCBR later); a pool with waiting requests is skipped, so a
 * blocked pool doesn't hold up the others. This also skips the R8 big pool
 * while RAW LV holds area 0x40.
 */
struct rscmgr_pool
{
    int type;               /* type ID for (Continuous)AllocateMemoryResource */
    int continuous;         /* 1 = AllocateContinuousMemoryResource */
};

static const struct rscmgr_pool rscmgr_pools[] = RSCMGR_POOLS;

static volatile int rscmgr_pending[COUNT(rscmgr_pools)];
static GUARDED_BY(mem_sem) struct memSuite *rscmgr_parts[COUNT(rscmgr_pools)];

static NO_THREAD_SAFETY_ANALYSIS void rscmgr_pool_request_waiting(int pool)
{
    rscmgr_pending[pool]++;
}

/* called from allocCBR (RscMgr task) when a timed out request completes */
static NO_THREAD_SAFETY_ANALYSIS void rscmgr_pool_request_done(int pool)
{
    rscmgr_pending[pool]--;
}

static struct memSuite *rscmgr_pool_alloc(int pool, size_t size)
{
    return rscmgr_alloc_suite(size, rscmgr_pools[pool].type, rscmgr_pools[pool].continuous, pool);
}

static void rscmgr_pool_free(struct memSuite *suite)
{
    if (suite)
    {
        FreeMemoryResource(suite, freeCBR, 0);
        take_semaphore_nc(free_sem, 0);
    }
}

/* largest size a pool gives, in 4 MB steps (backup trick, as shoot_malloc_autodetect) */
static size_t rscmgr_pool_autodetect(int pool)
{
    const size_t step = 4 * 1024 * 1024;

    if (rscmgr_pending[pool] > 0)
    {
        return 0;
    }

    struct memSuite *backup = rscmgr_pool_alloc(pool, step);
    if (!backup)
    {
        return 0;
    }

    size_t max_size = 0;
    for (int size_mb = 4; size_mb < 1024; size_mb += 4)
    {
        size_t tested_size = size_mb * 1024 * 1024;
        struct memSuite *test = rscmgr_pool_alloc(pool, tested_size);
        if (!test)
        {
            break;
        }
        /* leave 1MB unallocated, just in case */
        max_size = tested_size + step - 1024 * 1024;
        rscmgr_pool_free(test);
    }

    /* the queued request completes now, and allocCBR frees it */
    rscmgr_pool_free(backup);
    for (int i = 0; i < 50 && rscmgr_pending[pool] > 0; i++)
    {
        msleep(10);
    }

    return max_size;
}

/* add the chunks of `part` to `*joined` (created on the first chunk) */
static int rscmgr_join(struct memSuite **joined, struct memSuite *part)
{
    for (struct memChunk *chunk = GetFirstChunkFromSuite(part); chunk; chunk = GetNextMemoryChunk(part, chunk))
    {
        void *addr = GetMemoryAddressOfMemoryChunk(chunk);
        int size = GetSizeOfMemoryChunk(chunk);
        if (!addr)
        {
            return -1;
        }

        if (!*joined)
        {
            *joined = CreateMemorySuite(addr, size, 0);
            if (!*joined)
            {
                return -1;
            }
        }
        else
        {
            struct memChunk *copy = CreateMemoryChunk(addr, size, 0);
            if (!copy)
            {
                return -1;
            }
            AddMemoryChunk(*joined, copy);
        }
    }
    return 0;
}

static REQUIRES(mem_sem)
void rscmgr_pools_free_all()
{
    if (rscmgr_joined)
    {
        /* frees our suite / chunk structures only, not the memory */
        DeleteMemorySuite(rscmgr_joined);
        rscmgr_joined = 0;
    }
    for (int i = 0; i < COUNT(rscmgr_pools); i++)
    {
        rscmgr_pool_free(rscmgr_parts[i]);
        rscmgr_parts[i] = 0;
    }
}

/* everything the pools can give, as one suite; NULL if none */
static REQUIRES(mem_sem)
struct memSuite *rscmgr_pools_malloc_suite()
{
    if (rscmgr_joined)
    {
        /* one at a time, like the full shoot_malloc suite */
        return NULL;
    }

    /* list order matters: request the largest pool first */
    for (int i = 0; i < COUNT(rscmgr_pools); i++)
    {
        size_t size = rscmgr_pool_autodetect(i);
        if (size)
        {
            rscmgr_parts[i] = rscmgr_pool_alloc(i, size);
        }
        printf("[rscmgr] pool %d (type %x): %s\n", i, rscmgr_pools[i].type,
               rscmgr_parts[i] ? format_memory_size(rscmgr_parts[i]->size) : "nothing");
    }

    struct memSuite *joined = 0;
    for (int i = 0; i < COUNT(rscmgr_pools); i++)
    {
        if (rscmgr_parts[i] && rscmgr_join(&joined, rscmgr_parts[i]) < 0)
        {
            rscmgr_joined = joined;
            rscmgr_pools_free_all();
            return NULL;
        }
    }

    if (!joined)
    {
        rscmgr_pools_free_all();
        return NULL;
    }

    rscmgr_joined = joined;
    return joined;
}
#endif // CONFIG_RSCMGR_POOLS

REQUIRES(mem_sem)
struct memSuite *_shoot_malloc_suite(size_t size)
{
    //qprintf("_shoot_malloc_suite(%x)\n", size);

    if(size)
    {
        /* allocate exact memory size */
        return shoot_malloc_suite_int(size);
    }
    else
    {
        /* allocate as much as we can */
#ifdef CONFIG_RSCMGR_POOLS
        struct memSuite *joined = rscmgr_pools_malloc_suite();
        if (joined)
        {
            shoot_full_suite = joined;
            return joined;
        }
#endif
        size_t max_size = shoot_malloc_autodetect();
        struct memSuite * hSuite = shoot_malloc_suite_int(max_size);
        shoot_full_suite = hSuite;
        return hSuite;
    }
}

REQUIRES(mem_sem)
struct memSuite * _shoot_malloc_suite_contig(size_t size)
{
    //qprintf("_shoot_malloc_suite_contig(%x)\n", size);

    if (size == 0 || size > 1024*1024)
    {
        /* check whether we can allocate a block with the requested size */
        size_t max_size = shoot_malloc_autodetect_contig(size);

        if (size && size > max_size)
        {
            /* requested more than we can handle? give up */
            return 0;
        }
        if (size == 0)
        {
            /* allocate as much as we can */
            size = max_size;
        }
    }

    /* we now know for sure how much to allocate */
    //qprintf("_shoot_malloc_suite_contig(trying %x)\n", size);

    alloc_msg_t *suite_info = _malloc(sizeof(alloc_msg_t));
    
    suite_info->ret = NULL;
    suite_info->timed_out = 0;
    suite_info->size = size;
    suite_info->sem = alloc_sem;
    suite_info->pool = -1;
    
    AllocateContinuousMemoryResource(size, allocCBR, (unsigned int)suite_info, RSCMGR_TYPE_ID);

    int r = take_semaphore_nc(suite_info->sem, 100);
    if (r)
    {
        suite_info->timed_out = 1;
        return NULL;
    }
    
    struct memSuite * hSuite = suite_info->ret;
    _free(suite_info);
    
    ASSERT((int)size <= hSuite->size);
    return hSuite;
}

REQUIRES(mem_sem)
void* _shoot_malloc(size_t size)
{
    struct memSuite *theSuite = _shoot_malloc_suite_contig(size + 4);
    if (!theSuite)
        return 0;
    
    /* now we only have to tweak some things so it behaves like plain malloc */
    void* hChunk = (void*)GetFirstChunkFromSuite(theSuite);
    void* ptr = (void*)GetMemoryAddressOfMemoryChunk(hChunk);
    *(struct memSuite **)ptr = theSuite;
    //~ printf("shoot_malloc(%s) => %x hSuite=%x\n", format_memory_size(size), ptr+4, theSuite);
    return ptr + 4;
}

REQUIRES(mem_sem)
void _shoot_free(void *ptr)
{
    if (!ptr)
        return;
    if ((intptr_t)ptr & 3)
        return;
    struct memSuite *hSuite = *(struct memSuite **)(ptr - 4);
    //~ printf("shoot_free(%x) hSuite=%x\n", ptr, hSuite);
    if (hSuite != NULL)
        FreeMemoryResource(hSuite, freeCBR, 0);
    take_semaphore_nc(free_sem, 0);
}

/* just a dummy heuristic for now */
REQUIRES(mem_sem)
int _shoot_get_free_space()
{
    if (!alloc_sem)
    {
        return 0;
    }

    if (shoot_full_suite)
    {
        return 0;
    }

    /* fixme: report actual size (how?) */
    return (int)(31.5 * 1024 * 1024);
}

#if 0
void exmem_test()
{
    struct memSuite *hSuite = 0;
    struct memChunk *hChunk = 0;
    
    msleep(2000);
    AllocateMemoryResource(1024*1024*32, allocCBR, (unsigned int)&hSuite, RSCMGR_TYPE_ID);
    int r = take_semaphore_nc(alloc_sem, 100);
    if (r)
        return;
    
    if(!hSuite)
    {
        bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_BLACK), 0, 0, "Alloc Fail");
        return;
    }
    hChunk = GetFirstChunkFromSuite(hSuite);
    int num = 0;
    
    bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_BLACK), 0, 0, "C:%d S:0x%08X", GetNumberOfChunks(hSuite), GetSizeOfMemorySuite(hSuite) );
    while(hChunk)
    {
        if(num > 13)
        {
            num = 13;
        }
        bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_BLACK), 0, 30 + num * 20, 
            "[%d] A:0x%08X S:0x%08X R:0x%08X", num, GetMemoryAddressOfMemoryChunk(hChunk), GetSizeOfMemoryChunk(hChunk), GetRemainOfMemoryChunk(hChunk));
        hChunk = GetNextMemoryChunk(hSuite, hChunk);
        num++;
    } 
    bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_BLACK), 0, 30 + num++ * 20, "Done");

    FreeMemoryResource(hSuite, freeCBR, 0);
}
#endif

/* SRM job memory */

// SJE FIXME: disable SRM stuff if it's known to not work on a cam.
// This removes the need to find SRM_BUFFER_SIZE in order to build.
#ifndef CONFIG_MEMORY_SRM_NOT_WORKING

/* These buffers must be freed in the same order as allocated. */
/* To keep things simple, let's allocate the entire SRM memory on first call (all or nothing) */
static GUARDED_BY(mem_sem) int srm_allocated = 0;

/* There are a few fixed-size buffers; the exact size is a camera-specific constant
 * SRM_BUFFER_SIZE = RAW buffer size, usually around 30-40 MB
 * it's hardcoded in consts.h for speed, since autodetection is slow
 */
static GUARDED_BY(mem_sem) struct
{
    void *buffer;
    int used: 1;
    int use_after_free: 1;
} srm_buffers[SRM_MAX_BUF_COUNT_VIDEO_MODE] = {{0}};

/* used to know when allocation was done */
static struct semaphore *srm_alloc_sem = 0;

/* called from RscMgr task */
static REQUIRES(RscMgr)
void srm_malloc_cbr(void** dst_ptr, void* raw_buffer, uint32_t raw_buffer_size)
{
    //printf("srm_malloc_cbr(%x, %x, %x)\n", dst_ptr, raw_buffer, raw_buffer_size);

    /* we can't tell how much to allocate; the allocator tells us */
    /* the value is hardcoded in consts.h, for speed (probing is very slow) */
    ASSERT(SRM_BUFFER_SIZE == raw_buffer_size);
    
    /* return the newly allocated buffer in the output variable */
    *dst_ptr = raw_buffer;
    
    /* announce it's done */
    give_semaphore_nc(srm_alloc_sem);
}

/* after allocating this buffer, you can no longer take pictures (ERR70); will lock the shutter to prevent it */
static void srm_shutter_lock()
{
    /* block the shutter button to avoid ERR70 */
    /* (only touch the shutter-related bits, just in case) */
    gui_uilock(icu_uilock | UILOCK_SHUTTER);
}

static void srm_shutter_unlock()
{
    /* unlock the shutter button */
    /* (only touch the shutter-related bits, just in case) */
    gui_uilock(icu_uilock & ~UILOCK_SHUTTER);
}

static REQUIRES(mem_sem)
void srm_alloc_internal()
{
    printf("[SRM] alloc all buffers\n");

    if (srm_allocated || lens_info.job_state)
    {
        /* already allocated, or picture taking in progress */
        return;
    }

    srm_shutter_lock();
    msleep(50);

    if (srm_allocated || lens_info.job_state)
    {
        /* did you manage to press the shutter meanwhile? */
        /* you should go to a race contest :) */
        return;
    }

    /* clear previous state */
    memset(srm_buffers, 0, sizeof(srm_buffers));

    /* try to allocate as many buffers as we can */
    for (int i = 0; i < COUNT(srm_buffers); i++)
    {
        /* allocate a large contiguous buffer, normally used for RAW photo capture */
        SRM_AllocateMemoryResourceFor1stJob(srm_malloc_cbr, &srm_buffers[i].buffer);
        int err = take_semaphore_nc(srm_alloc_sem, 100);

        if (err)
        {
            /* the call will time out when there's no more RAM, and the request will be dropped */
            /* (unlike shoot_malloc, here it won't trigger the CBR after freeing something) */

            /* all SRM memory allocated => Canon code already locked the shutter for us */
            /* (we still need the lock active while allocating, to pass the race condition test) */
            srm_shutter_unlock();

            /* free the first buffer to avoid the BUSY screen and its side effects */
            /* fixme: how to allocate the entire SRM memory without this issue? */
            SRM_FreeMemoryResourceFor1stJob(srm_buffers[0].buffer, 0, 0);

            if (shoot_full_suite)
            {
                /* using this buffer appears to be safe as long as the Shoot memory is fully allocated
                 * that way, Canon code will not be able to start H.264 recording, take pictures
                 * or - hopefully - perform other actions that would require changing the memory layout
                 * or allocating large amounts of memory (to be checked) */
                printf("[SRM] use-after-free %x\n", srm_buffers[0].buffer);
                srm_buffers[0].use_after_free = 1;
            }
            else
            {
                /* discard this buffer - possibly unsafe */
                printf("[SRM] not using %x\n", srm_buffers[0].buffer);
                srm_buffers[0].buffer = 0;
            }

            break;
        }

        printf("[SRM] buffer %x\n", srm_buffers[i].buffer);
    }

    srm_allocated = 1;
}

static REQUIRES(mem_sem)
void srm_free_internal()
{
    /* any buffers still used? */
    for (int i = 0; i < COUNT(srm_buffers); i++)
    {
        if (srm_buffers[i].used)
        {
            return;
        }
    }

    printf("[SRM] free all buffers\n");

    /* free all SRM buffers */
    /* note: first buffer is use-after-free */
    for (int i = 0; i < COUNT(srm_buffers); i++)
    {
        if (srm_buffers[i].buffer)
        {
            ASSERT(!srm_buffers[i].used);

            if (!srm_buffers[i].use_after_free)
            {
                SRM_FreeMemoryResourceFor1stJob(srm_buffers[i].buffer, 0, 0);
            }

            srm_buffers[i].buffer = 0;
            srm_buffers[i].use_after_free = 0;
        }
    }

    srm_shutter_unlock();

    srm_allocated = 0;
}

REQUIRES(mem_sem)
struct memSuite * _srm_malloc_suite(int num_requested_buffers)
{
    printf("srm_malloc_suite(%d)...\n", num_requested_buffers);

    /* alloc from operating system, if needed */
    srm_alloc_internal();

    if (num_requested_buffers == 0)
    {
        /* if you request 0, this means allocate as much as you can */
        num_requested_buffers = COUNT(srm_buffers);
    }

    /* pack the buffers into a memory suite, so they can be used in the same way as with shoot_malloc_suite */
    struct memSuite *suite = 0;

    /* note: buffer 0 is use-after-free; try the others first */
    for (int i = COUNT(srm_buffers) - 1; i >= 0 && num_requested_buffers; i--)
    {
        if (srm_buffers[i].buffer && !srm_buffers[i].used)
        {
            if (!suite)
            {
                /* first buffer */
                suite = CreateMemorySuite(srm_buffers[i].buffer, SRM_BUFFER_SIZE, 0);
            }
            else
            {
                /* subsequent buffers */
                struct memChunk *chunk = CreateMemoryChunk(srm_buffers[i].buffer, SRM_BUFFER_SIZE, 0);
                ASSERT(chunk);
                AddMemoryChunk(suite, chunk);
            }

            /* mark as used */
            srm_buffers[i].used = 1;

            /* next */
            num_requested_buffers--;
        }
    }
    
    printf("srm_malloc_suite => %x\n", suite);
    return suite;
}

REQUIRES(mem_sem)
void _srm_free_suite(struct memSuite *suite)
{
    if (suite == NULL)
    {
        DryosDebugMsg(0, 15, "WARNING: suite was NULL");
        // SJE should we do srm_shutter_unlock() and clear srm_allocated,
        // as below?  Probably not, but I don't know how to handle this
        // error case.
        return;
    }
        
    printf("srm_free_suite(%x)\n", suite);

    struct memChunk *chunk = GetFirstChunkFromSuite(suite);

    while(chunk)
    {
        /* make sure we have a suite returned by srm_malloc_suite */
        uint32_t size = GetSizeOfMemoryChunk(chunk);
        ASSERT(size == SRM_BUFFER_SIZE);

        /* mark each chunk as free in our internal SRM buffer list */
        void* buf = GetMemoryAddressOfMemoryChunk(chunk);

        for (int i = COUNT(srm_buffers) - 1; i >= 0; i--)
        {
            if (srm_buffers[i].buffer == buf)
            {
                ASSERT(srm_buffers[i].used);
                srm_buffers[i].used = 0;
            }
        }
        
        chunk = GetNextMemoryChunk(suite, chunk);
    }
    
    /* after freeing the big buffers, this will free the memory suite and the chunks (the data structures) */
    DeleteMemorySuite(suite);

    /* free to operating system, if needed */
    srm_free_internal();
}

/* malloc-like wrapper for the SRM buffers */

/* similar to shoot_malloc, but limited to a single large buffer for now */
REQUIRES(mem_sem)
void* _srm_malloc(size_t size)
{
    /* alloc from operating system, if needed */
    srm_alloc_internal();

    /* find the first unused buffer */
    /* note: buffer 0 is use-after-free; try the others first */
    void *buffer = 0;
    for (int i = COUNT(srm_buffers) - 1; i >= 0; i--)
    {
        if (srm_buffers[i].buffer && !srm_buffers[i].used)
        {
            buffer = srm_buffers[i].buffer;
            srm_buffers[i].used = 1;
            break;
        }
    }
    
    /* here we can't request a certain size; we can just check whether we got enough, or not */
    /* note: size checking is done after allocating, in order to simplify the deallocation code */
    if (SRM_BUFFER_SIZE < size + 4)
    {
        /* not enough */
        _srm_free(buffer);
        return 0;
    }
    
    return buffer;
}

REQUIRES(mem_sem)
void _srm_free(void* ptr)
{
    if (!ptr)
        return;

    /* identify the buffer from its pointer, and mark it as unused */
    for (int i = COUNT(srm_buffers) - 1; i >= 0; i--)
    {
        if (srm_buffers[i].used && srm_buffers[i].buffer == ptr)
        {
            srm_buffers[i].used = 0;
        }
    }

    /* deallocate the suite from the system, if needed */
    srm_free_internal();
}

REQUIRES(mem_sem)
int _srm_get_max_region()
{
    if (!srm_allocated)
    {
        /* assume we have at least one buffer */
        return SRM_BUFFER_SIZE;
    }

    for (int i = COUNT(srm_buffers) - 1; i >= 0; i--)
    {
        if (srm_buffers[i].buffer && !srm_buffers[i].used)
        {
            return SRM_BUFFER_SIZE;
        }
    }

    return 0;
}

REQUIRES(mem_sem)
int _srm_get_free_space()
{
    if (!srm_allocated)
    {
        /* assume we have at least one buffer */
        return SRM_BUFFER_SIZE;
    }

    int free_space = 0;

    for (int i = COUNT(srm_buffers) - 1; i >= 0; i--)
    {
        if (srm_buffers[i].buffer && !srm_buffers[i].used)
        {
            free_space += SRM_BUFFER_SIZE;
        }
    }

    return free_space;
}
#endif // ~CONFIG_MEMORY_SRM_NOT_WORKING

static void exmem_init()
{
    alloc_sem = create_named_semaphore(NULL, SEM_CREATE_LOCKED);
    free_sem = create_named_semaphore(NULL, SEM_CREATE_LOCKED);
#ifndef CONFIG_MEMORY_SRM_NOT_WORKING
    srm_alloc_sem = create_named_semaphore(NULL, SEM_CREATE_LOCKED);
#endif
}

INIT_FUNC("exmem", exmem_init);


//#define CONFIG_RSCMGR_UNUSED_SPACE_TEST

#ifdef CONFIG_RSCMGR_UNUSED_SPACE_TEST
    #ifndef CONFIG_MARK_UNUSED_MEMORY_AT_STARTUP
    #error This requires CONFIG_MARK_UNUSED_MEMORY_AT_STARTUP
    #endif

#include "console.h"

static void test_unused_buffer(uint32_t start, uint32_t end)
{
    printf("Testing %08X - %08X ...\n", start, end);
    for (uint32_t a = start; a < end; a += 4)
    {
        if (MEM(a) != 0x124B1DE0)
        {
            /* find the memory range that appears used */
            /* ignore unused gaps < 1K */
            int count = 1024;
            uint32_t b = a;
            uint32_t c = a;
            while (count-- && b < end)
            {
                b += 4;
                if (MEM(b) != 0x124B1DE0)
                {
                    count = 1024;
                    c = b;
                }
            }
            console_show();
            printf("%x ... %x: used\n", a, c);
            a = c + 4;
        }
    }
}

static void rscmgr_test_unused_space()
{
    while(1)
    {
        if (srm_buffers[0].buffer)
        {
            ASSERT(srm_buffers[0].use_after_free);
            uint32_t start = (uint32_t) srm_buffers[0].buffer;
            uint32_t end = start + SRM_BUFFER_SIZE;
            test_unused_buffer(start, end);
        }

        msleep(1000);
    }
}

TASK_CREATE("rscmgr_test", rscmgr_test_unused_space, 0, 0x1f, 0);
#endif

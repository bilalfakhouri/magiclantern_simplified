/* extended memory, allocated from shoot memory buffer */

#ifndef _exmem_h_
#define _exmem_h_

struct memSuite
{
    char* signature; // MemSuite
    int size;
    int num_chunks;
    void *last_chunk; // points to last chunks chunk.prev
    void *first_chunk; // points to first chunks chunk.prev
    int mem_type; // not used during free, so mem types may vary within a suite
};

struct memChunk
{
    char* signature; // MemChunk
    void *prev; // points to previous chunks chunk.prev or to suite.last_chunk
    void *next; // points to next chunks chunk.prev or to suite.last_chunk
    int size;
    int remain;
    void *heap_ptr;
    void *memory_address;
    int ring_heap_handle;
    void (*free_cbr)(void *addr);
    int mem_type;
};

#define CHECK_SUITE_SIGNATURE(suite) ASSERT(streq((suite)->signature, "MemSuite"));
#define CHECK_CHUNK_SIGNATURE(chunk) ASSERT(streq((chunk)->signature, "MemChunk"));

/* these return a memory suite, which consists of one or more memory chunks */
/* it is up to user to iterate through the chunks */
struct memSuite *shoot_malloc_suite(size_t size);
void shoot_free_suite(struct memSuite * hSuite);

/* this returns a memory suite with a single contiguous block, but may fail because of memory fragmentation */
struct memSuite * shoot_malloc_suite_contig(size_t size);

/* this returns a memory suite with large (30-40 MB) constant-size blocks (normally used for RAW capture) */
/* this is on top of what you can get with shoot_malloc_suite */
/* num_requested_buffers can be 0 for autodetection */
struct memSuite * srm_malloc_suite(int num_requested_buffers);
void srm_free_suite(struct memSuite * suite);

/**
 * Memory for a bus master (EDMAC, Ximr...): aligned, uncached CPU address plus
 * its 64-bit IBus address (DIGIC X with CONFIG_RSCMGR_IBUS; 0 elsewhere).
 * On DIGIC X the buffer is checked against the MemiArbiter table for `pid`
 * (pid < 0: only check that it is on a DRAM channel), so the ROM never sees
 * an address it would assert on.
 *
 * IBUS_BUF_LONG_LIVED: kept for a long time (e.g. a display layer): taken
 * from the heap (malloc, then AllocateMemory), never from shoot memory,
 * which Canon expects to be empty for some operations (Err 70).
 * Without it, ML's allocator (fio_malloc path) is tried first.
 *
 * Returns 0 on success, -1 on failure (buf is zeroed).
 */
struct ibus_buf
{
    void     *cpu;          /* uncached CPU address, aligned */
    uint64_t  ibus;         /* IBus address of cpu, 0 if not DIGIC X */
    uint32_t  size;
    void     *raw;          /* exmem's bookkeeping: pointer to free */
    int       source;
};

#define IBUS_BUF_LONG_LIVED 1

int  exmem_ibus_alloc(struct ibus_buf *buf, uint32_t size, uint32_t align, int pid, int flags);
void exmem_ibus_free(struct ibus_buf *buf);

/**
 * DRAM Ch3 memory (DIGIC X, CONFIG_DRAM3_WORKMEM).
 *
 * The CPU has no address for DRAM Ch3; only bus masters reach it (EDMAC,
 * the card DMA). exmem_dram3_alloc() takes every RscMgr WorkMem area from
 * DRAM3_WORKMEM_AREAS (consts.h) that Canon is not using right now and
 * returns their parts as IBus ranges, largest first. Areas Canon holds are
 * skipped, never waited for.
 *
 * Use it with edmac_copy_ibus() / edmac_copy_rectangle_ibus() and
 * FIO_WriteFileIBus(). Check memiarb_allowed() for the EDMAC channel pids.
 * Give it back (exmem_dram3_free) before anything that changes Canon's
 * memory layout: mode switch, still capture, playback, entering menus.
 *
 * Only one user at a time. Returns NULL if nothing could be taken.
 */
struct ibus_range
{
    uint64_t ibus;
    uint32_t size;
};

#define DRAM3_MAX_RANGES 32

#define DRAM3_MAX_BUSY   16

struct dram3_mem
{
    int      count;
    uint32_t total;                             /* bytes */
    struct ibus_range range[DRAM3_MAX_RANGES];  /* largest first */
    int      busy_count;                        /* areas Canon held (not taken) */
    uint32_t busy_id[DRAM3_MAX_BUSY];           /* their WorkMem area ids */
    int      cpu_access;                        /* 1: exmem_dram3_read32 / write32 work */
};

const struct dram3_mem *exmem_dram3_alloc(void);
void exmem_dram3_free(void);

/**
 * CPU access to single words of DRAM Ch3 (any IBus address, 4-byte aligned)
 * while exmem_dram3_alloc() memory is held, through a dynamic window that
 * exmem keeps for itself. About 1 us per word: for headers and markers,
 * never for image data. Safe from any task (interrupts off for the access).
 * Only if dram3_mem.cpu_access; otherwise read32 returns 0, write32 does nothing.
 */
uint32_t exmem_dram3_read32(uint64_t ibus);
void exmem_dram3_write32(uint64_t ibus, uint32_t value);

/**
 * 1 if this camera can use DRAM Ch3 memory (CONFIG_DRAM3_WORKMEM).
 * For modules: declare it with WEAK_FUNC(ret_0), so it returns 0 on
 * cameras where the core doesn't have it.
 */
int exmem_dram3_available(void);

/**
 * IBus address of a CPU buffer, for modules (which can't use ibus.h).
 * 0 if not RAM, or the camera has no verified window table (CONFIG_RSCMGR_IBUS).
 */
uint64_t exmem_cpu_to_ibus(const void *cpu);

/* dump the contents of a memsuite */
unsigned int exmem_save_buffer(struct memSuite * hSuite, char *file);

/* fill the entire memsuite with some char */
unsigned int exmem_clear(struct memSuite * hSuite, char fill);

/* MemorySuite routines */
int AllocateMemoryResource(int size, void (*cbr)(unsigned int, struct memSuite *), unsigned int ctx, int type);
int AllocateContinuousMemoryResource(int size, void (*cbr)(unsigned int, struct memSuite *), unsigned int ctx, int type);
int FreeMemoryResource(struct memSuite *hSuite, void (*cbr)(unsigned int), unsigned int ctx);

int GetNumberOfChunks(struct memSuite * suite);
int GetSizeOfMemorySuite(struct memSuite * suite);
struct memChunk * GetFirstChunkFromSuite(struct memSuite * suite);
struct memChunk * GetNextMemoryChunk(struct memSuite * suite, struct memChunk * chunk);
int GetSizeOfMemoryChunk(struct memChunk * chunk);
void* GetMemoryAddressOfMemoryChunk(struct memChunk * chunk);

#ifdef CONFIG_RSCMGR_IBUS
/* ROM getters; GetMemoryAddressOfMemoryChunk() above is ML's wrapper (exmem.c) */
void* _GetMemoryAddressOfMemoryChunk(struct memChunk * chunk);
uint64_t GetIBusAddressOfMemoryChunk(struct memChunk * chunk);
#endif

struct memSuite * CreateMemorySuite(void* first_chunk_address, size_t first_chunk_size, uint32_t flags);
void DeleteMemorySuite(struct memSuite * suite);
struct memChunk * CreateMemoryChunk(void* address, size_t size, uint32_t flags);
int AddMemoryChunk(struct memSuite * suite, struct memChunk * chunk);

/* similar to shoot_malloc, for very large buffers (32-40 MB) */
/* usage caveat: you *must* free these as soon as you finish working with them,
 * because you can't take pictures as long as one of those is allocated */
void* _srm_malloc(size_t size);
void _srm_free(void* ptr);
int _srm_get_free_space();
int _srm_get_max_region();
struct memSuite * _srm_malloc_suite(int num_requested_buffers);
void _srm_free_suite(struct memSuite * suite);

void SRM_AllocateMemoryResourceFor1stJob(void (*callback)(void** dst_ptr, void* raw_buffer, uint32_t raw_buffer_size), void** dst_ptr);
void SRM_FreeMemoryResourceFor1stJob(void* raw_buffer, int unk1_zero, int unk2_zero);

#endif

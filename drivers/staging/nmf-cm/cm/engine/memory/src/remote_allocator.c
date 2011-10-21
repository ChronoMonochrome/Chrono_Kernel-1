/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Jean-Philippe FASSINO <jean-philippe.fassino@stericsson.com> for ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2.
 */
/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Jean-Philippe FASSINO <jean-philippe.fassino@stericsson.com> for ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2.
 */
/*
 * Include
 */
#include "../inc/remote_allocator.h"
#include "../inc/remote_allocator_utils.h"
#include "../inc/chunk_mgr.h"

#include <cm/engine/trace/inc/trace.h>
#include <cm/engine/trace/inc/xtitrace.h>

static t_cm_chunk* cm_MM_RA_getLastChunk(t_cm_allocator_desc* alloc);
static void cm_MM_RA_checkAllocator(t_cm_allocator_desc* alloc);
//static void cm_MM_RA_checkAlloc(t_cm_allocator_desc* alloc, t_uint32 size, t_uint32 align, t_uint32 min, t_uint32 max);

int bin_index(unsigned int sz) {
    /*
     * 32 bins of size       2
     * 16 bins of size      16
     *  8 bins of size     128
     *  4 bins of size    1024
     *  2 bins of size    8192
     *  1 bin  of size what's left
     *
     */
    return (((sz >> 6) ==    0) ?       (sz >>  1): // 0        -> 0 .. 31
            ((sz >> 6) <=    4) ?  28 + (sz >>  4): // 64       -> 32 .. 47
            ((sz >> 6) <=   20) ?  46 + (sz >>  7): // 320      -> 48 .. 55
            ((sz >> 6) <=   84) ?  55 + (sz >> 10): // 1344     -> 56 .. 59
            ((sz >> 6) <=  340) ?  59 + (sz >> 13): // 5440     -> 60 .. 61
            62);                                    // 21824..
}

static t_cm_allocator_desc* ListOfAllocators = NULL;

PUBLIC t_cm_allocator_desc* cm_MM_CreateAllocator(t_cm_size size, t_uint32 offset, const char* name)
{
    t_cm_allocator_desc *alloc;

    CM_ASSERT(fillChunkPool() == CM_OK);

    /* Alloc structure */
    alloc = (t_cm_allocator_desc*)OSAL_Alloc_Zero(sizeof(t_cm_allocator_desc));
    CM_ASSERT(alloc != NULL);

    // Add allocator in list
    alloc->next = ListOfAllocators;
    ListOfAllocators = alloc;

    /* Create first chunk */
    alloc->chunks = allocChunk();

    /* assign name */
    alloc->pAllocName = name;

    alloc->chunks->size    = size;
    alloc->chunks->offset  = offset;
    alloc->chunks->alloc   = alloc;
    alloc->free_mem_chunks[bin_index(alloc->chunks->size)] = alloc->chunks;

    alloc->size = size;

    //TODO, juraj, alloc impacts trace format
    cm_TRC_traceMemAlloc(TRACE_ALLOCATOR_COMMAND_CREATE, 0, size, name);

    return alloc;
}

PUBLIC t_cm_error cm_MM_DeleteAllocator(t_cm_allocator_desc *alloc)
{
    t_cm_chunk *chunk, *next_cm_chunk;

    cm_TRC_traceMemAlloc(TRACE_ALLOCATOR_COMMAND_DESTROY, 0, 0, alloc->pAllocName);

    /* Parse all chunks and free them */
    chunk = alloc->chunks;
    while(chunk != 0)
    {
        next_cm_chunk = chunk->next;
        unlinkChunk(alloc, chunk);
        freeChunk(chunk);

        chunk = next_cm_chunk;
    }

    // Remove allocator from the list
    if(ListOfAllocators == alloc)
        ListOfAllocators = alloc->next;
    else {
        t_cm_allocator_desc *prev = ListOfAllocators;
        while(prev->next != alloc)
            prev = prev->next;
        prev->next = alloc->next;
    }


    /* Free allocator descriptor */
    OSAL_Free(alloc);

    return CM_OK;
}

PUBLIC t_cm_error cm_MM_ResizeAllocator(t_cm_allocator_desc *alloc, t_cm_size size)
{
    t_cm_error error;

    /* sanity check */
    if (size == 0)
        return CM_INVALID_PARAMETER;

    if((error = fillChunkPool()) != CM_OK)
        return error;

    if (size > alloc->size) {
        /* ok, increase allocator */
        t_uint32 deltaSize = size - alloc->size;
        t_cm_chunk *last = cm_MM_RA_getLastChunk(alloc);

        if (last->status == MEM_FREE) {
            /* last chunk is a free one, just increase size */
            unlinkFreeMem(alloc, last);
            last->size += deltaSize;
            alloc->size += deltaSize;
            /* now list of free chunk is potentially no more ordered */
            updateFreeList(alloc, last);
        } else {
            /* last chunk is a used one, add new free chunk */
            last->size += deltaSize;
            splitChunk(alloc, last, last->offset + deltaSize, FREE_CHUNK_AFTER);
            alloc->size += deltaSize;
        }
    } else {
        /* reduce allocator */
        t_uint32 deltaSize = alloc->size - size;
        t_cm_chunk *last = cm_MM_RA_getLastChunk(alloc);

        /* check if resize is possible */
        if (last->status == MEM_USED)
            return CM_NO_MORE_MEMORY;
        if (last->size < deltaSize)
            return CM_NO_MORE_MEMORY;

        /* ok, rezise can be performed */
        if (last->size == deltaSize)        {
            t_cm_chunk *prev = last->prev;

            /* remove last free chunk */
            mergeChunk(alloc, prev, last);
            prev->size -= deltaSize;
        } else {
            unlinkFreeMem(alloc, last);
            /* reduce size of last free chunk */
            last->size -= deltaSize;
            /* now list of free chunk is potentially no more ordered */
            updateFreeList(alloc, last);
        }
    }

    alloc->size = size;

    if (cmIntensiveCheckState) {
        cm_MM_RA_checkAllocator(alloc);
    }

    return CM_OK;
}

t_cm_error cm_MM_getValidMemoryHandle(t_cm_memory_handle handle, t_memory_handle* validHandle)
{
#ifdef LINUX
    /* On linux, there is already a check within the linux part
     * => we don't need to check twice */
    *validHandle = (t_memory_handle)handle;
    return CM_OK;
#else
    t_cm_allocator_desc *alloc = ListOfAllocators;

    for(; alloc != NULL; alloc = alloc->next)
    {
        t_cm_chunk* chunk = alloc->chunks;

        /* Parse all chunks */
        for(; chunk != NULL; chunk = chunk->next)
        {
            if(chunk == (t_memory_handle)handle)
            {
                if(chunk->status == MEM_FREE)
                    return CM_MEMORY_HANDLE_FREED;

                *validHandle = (t_memory_handle)handle;

                return CM_OK;
            }
        }
    }

    return CM_UNKNOWN_MEMORY_HANDLE;
#endif
}

//TODO, juraj, add appartenance to allocHandle (of chunk) and degage setUserData
PUBLIC t_memory_handle cm_MM_Alloc(
        t_cm_allocator_desc* alloc,
        t_cm_size size,
        t_cm_memory_alignment memAlignment,
        t_uint32 seg_offset,
        t_uint32 seg_size,
        t_uint32 domainId)
{
    t_cm_chunk* chunk;
    t_uint32 aligned_offset;
    t_uint32 aligned_end;
    t_uint32 seg_end = seg_offset + seg_size;
    int i;

    /* Sanity check */
    if ( (size == 0) || (size > seg_size) )
        return INVALID_MEMORY_HANDLE;

    if(fillChunkPool() != CM_OK)
        return INVALID_MEMORY_HANDLE;

    /* Get first chunk available for the specific size */
    // Search a list with a free chunk
    for(i = bin_index(size); i < BINS; i++)
    {
        chunk = alloc->free_mem_chunks[i];
        while (chunk != 0)
        {
            /* Alignment of the lower boundary */
            aligned_offset = ALIGN_VALUE(MAX(chunk->offset, seg_offset), (memAlignment + 1));

            aligned_end = aligned_offset + size;

            if ((aligned_end <= seg_end)
                    && aligned_end <= (chunk->offset + chunk->size)
                    && aligned_offset >= seg_offset
                    && aligned_offset >= chunk->offset)
                goto found;

            chunk = chunk->next_free_mem;
        }
    }

    return INVALID_MEMORY_HANDLE;

found:

    /* Remove chunk from free list */
    unlinkFreeMem(alloc, chunk);

    //create an empty chunk before the allocated one
    if (chunk->offset < aligned_offset) {
        chunk = splitChunk(alloc, chunk, aligned_offset, FREE_CHUNK_BEFORE);
    }
    //create an empty chunk after the allocated one
    if (chunk->offset + chunk->size > aligned_end) {
        splitChunk(alloc, chunk, aligned_end, FREE_CHUNK_AFTER);
    }

    chunk->status = MEM_USED;
    chunk->prev_free_mem = 0;
    chunk->next_free_mem = 0;
    chunk->domainId = domainId;

    //TODO, juraj, alloc impacts trace format
    cm_TRC_traceMem(TRACE_ALLOC_COMMAND_ALLOC, 0, chunk->offset, chunk->size);

    if (cmIntensiveCheckState) {
        cm_MM_RA_checkAllocator(alloc);
    }

    chunk->alloc = alloc;
    return (t_memory_handle) chunk;
}

//caution - if successfull, the chunk offset will be aligned with seg_offset
//caution++ the offset of the allocated chunk changes implicitly
PUBLIC t_memory_handle cm_MM_Realloc(
                t_cm_allocator_desc* alloc,
                const t_cm_size size,
                const t_uint32 offset,
                const t_cm_memory_alignment memAlignment,
                const t_memory_handle handle)
{
    t_cm_chunk *chunk = (t_cm_chunk*)handle;

    /* check reallocation is related to this chunk! */
    CM_ASSERT(chunk->offset <= (offset + size));
    CM_ASSERT(offset <= (chunk->offset + chunk->size));
    CM_ASSERT(size);

    /* check if extend low */
    if (offset < chunk->offset) {
        /* note: it is enough to check only the previous chunk,
         *      because adjacent chunks of same status are merged
         */
        if ((chunk->prev == 0)
           ||(chunk->prev->status != MEM_FREE)
           ||(chunk->prev->offset > offset)) {
            return INVALID_MEMORY_HANDLE;
        }
    }

    /* check if extend high, note as above */
    if ( (offset + size) > (chunk->offset + chunk->size)) {
        if ((chunk->next == 0)
           ||(chunk->next->status != MEM_FREE)
           ||( (chunk->next->offset + chunk->next->size) < (offset + size))) {
            return INVALID_MEMORY_HANDLE;
        }
    }

    if(fillChunkPool() != CM_OK)
        return INVALID_MEMORY_HANDLE;


#if 0
    /* extend low
     *      all conditions should have been checked
     *      this must not fail
     */
    if (offset < chunk->offset) {
        t_cm_chunk *tmp = splitChunk(alloc, chunk->prev, offset, FREE_CHUNK_BEFORE); //tmp = chunk->prev
        CM_ASSERT(tmp);
        tmp->status = MEM_USED;
        tmp->prev->status = MEM_FREE;
        mergeChunk(alloc, tmp, chunk);
        if ((tmp->prev->prev != 0)
           && (tmp->prev->prev->status == MEM_FREE)) {
            mergeChunk(alloc, tmp->prev->prev, tmp->prev);
        }
        chunk = tmp;
    }

    /* extend high */
    if ( (offset + size) > (chunk->offset + chunk->size)) {
        t_cm_chunk *tmp = splitChunk(alloc, chunk->next, offset + size, FREE_CHUNK_AFTER); //tmp = chunk->next->next
        CM_ASSERT(tmp);
        tmp->status = MEM_USED;
        mergeChunk(alloc, chunk, tmp);
        if ((tmp->next->next != 0)
           && (tmp->next->next->status == MEM_FREE)) {
            mergeChunk(alloc, tmp->next, tmp->next->next);
        }
    }

    /* reduce top */
    if ((offset + size) < (chunk->offset + chunk->size)) {
        t_cm_chunk *tmp = splitChunk(alloc, chunk, offset + size, FREE_CHUNK_AFTER); //tmp = chunk, chunk = result
        CM_ASSERT(tmp);
        tmp->status = MEM_USED;
        tmp->next->status = MEM_FREE;
        if ((tmp->next->next != 0)
           && (tmp->next->next->status == MEM_FREE)) {
            mergeChunk(alloc, tmp->next, tmp->next->next);
        }
    }

    /* reduce bottom */
    if (offset > chunk->offset) {
        t_cm_chunk *tmp = splitChunk(alloc, chunk, offset, FREE_CHUNK_BEFORE); //tmp->next = chunk, tmp = result
        CM_ASSERT(tmp);
        tmp->status = MEM_USED;
        tmp->prev->status = MEM_FREE;
        if ((tmp->prev->prev != 0)
           &&(tmp->prev->prev->status == MEM_FREE)) {
            mergeChunk(alloc, tmp->prev->prev, tmp->prev);
        }
        chunk = tmp;
    }
#else
    /* extend low
     *      all conditions should have been checked
     *      this must not fail
     */
    if (offset < chunk->offset) {
        t_uint32 delta = chunk->prev->offset + chunk->prev->size - offset;
        CM_ASSERT(chunk->prev->status == MEM_FREE); //TODO, juraj, already checked
        unlinkFreeMem(alloc, chunk->prev);
        chunk->prev->size -= delta;
        chunk->offset -= delta;
        chunk->size += delta;
        updateFreeList(alloc, chunk->prev);
    }

    /* extend high */
    if ( (offset + size) > (chunk->offset + chunk->size)) {
        t_uint32 delta = size - chunk->size;
        CM_ASSERT(chunk->next->status == MEM_FREE); //TODO, juraj, already checked
        unlinkFreeMem(alloc, chunk->next);
        chunk->size += delta;
        chunk->next->offset += delta;
        chunk->next->size -= delta;
        updateFreeList(alloc, chunk->next);
    }

    /* reduce top */
    if ((offset + size) < (chunk->offset + chunk->size)) {
        if (chunk->next->status == MEM_FREE) {
            t_uint32 delta = chunk->size - size;
            unlinkFreeMem(alloc, chunk->next);
            chunk->size -= delta;
            chunk->next->offset -= delta;
            chunk->next->size += delta;
            updateFreeList(alloc, chunk->next);
        } else {
            t_cm_chunk *tmp = splitChunk(alloc, chunk, offset + size, FREE_CHUNK_AFTER); //tmp = chunk, chunk = result
            tmp->status = MEM_USED;
            tmp->next->status = MEM_FREE;
        }
    }

    /* reduce bottom */
    if (offset > chunk->offset) {
        if (chunk->prev->status == MEM_FREE) {
            t_uint32 delta = offset - chunk->offset;
            unlinkFreeMem(alloc, chunk->prev);
            chunk->prev->size += delta;
            chunk->offset = offset;
            chunk->size -= delta;
            updateFreeList(alloc, chunk->prev);
        } else {
            t_cm_chunk *tmp = splitChunk(alloc, chunk, offset, FREE_CHUNK_BEFORE); //tmp->next = chunk, tmp = result
            tmp->status = MEM_USED;
            tmp->prev->status = MEM_FREE;
        }
    }
#endif
    cm_MM_RA_checkAllocator(alloc);

    return (t_memory_handle)chunk;
}

PUBLIC void cm_MM_Free(t_cm_allocator_desc* alloc, t_memory_handle memHandle)
{
    t_cm_chunk* chunk = (t_cm_chunk*)memHandle;

    //TODO, juraj, alloc impacts trace format
    cm_TRC_traceMem(TRACE_ALLOC_COMMAND_FREE, 0,
            chunk->offset, chunk->size);

    /* Update chunk status */
    chunk->status = MEM_FREE;
    chunk->domainId = 0x0;

    /* Check if the previous chunk is free */
    if((chunk->prev != 0) && (chunk->prev->status == MEM_FREE)) {
        chunk = chunk->prev; //chunk, ie. chunk->next, will be freed
        mergeChunk(alloc, chunk, chunk->next);
    }

    /* Check if the next chunk is free */
    if((chunk->next != 0) && (chunk->next->status == MEM_FREE)) {
        mergeChunk(alloc, chunk, chunk->next);
    }

    unlinkFreeMem(alloc, chunk);
    updateFreeList(alloc, chunk);

    if (cmIntensiveCheckState) {
        cm_MM_RA_checkAllocator(alloc);
    }
}

PUBLIC t_cm_error cm_MM_GetAllocatorStatus(t_cm_allocator_desc* alloc, t_uint32 offset, t_uint32 size, t_cm_allocator_status *pStatus)
{
    t_cm_chunk* chunk = alloc->chunks;
    t_uint8 min_free_size_updated = FALSE;

    /* Init status */
    pStatus->global.used_block_number = 0;
    pStatus->global.free_block_number = 0;
    pStatus->global.maximum_free_size = 0;
    pStatus->global.minimum_free_size = 0xFFFFFFFF;
    pStatus->global.accumulate_free_memory = 0;
    pStatus->global.accumulate_used_memory = 0;
    pStatus->global.size = alloc->size;
    pStatus->domain.maximum_free_size = 0;
    pStatus->domain.minimum_free_size = 0xFFFFFFFF;
    pStatus->domain.accumulate_free_memory = 0;
    pStatus->domain.accumulate_used_memory = 0;
    pStatus->domain.size= size;

    //TODO, juraj, get allocator status for a domain
    /* Parse all chunks */
    while(chunk != 0)
    {

        /* Chunk is free */
        if (chunk->status == MEM_FREE) {
            pStatus->global.free_block_number++;
            pStatus->global.accumulate_free_memory += chunk->size;

            /* Check max size */
            if (chunk->size > pStatus->global.maximum_free_size)
            {
                pStatus->global.maximum_free_size = chunk->size;
            }

            /* Check min size */
            if (chunk->size < pStatus->global.minimum_free_size)
            {
                pStatus->global.minimum_free_size = chunk->size;
                min_free_size_updated = TRUE;
            }
        } else {/* Chunk used */
            pStatus->global.used_block_number++;
            pStatus->global.accumulate_used_memory += chunk->size;
        }

        chunk = chunk->next;
    }

    /* Put max free size to min free size */
    if (min_free_size_updated == FALSE) {
        pStatus->global.minimum_free_size = pStatus->global.maximum_free_size;
    }

    return CM_OK;
}

PUBLIC t_uint32 cm_MM_GetOffset(t_memory_handle memHandle)
{
    /* Provide offset */
    return ((t_cm_chunk*)memHandle)->offset;
}

PUBLIC t_uint32 cm_MM_GetSize(t_memory_handle memHandle)
{
    return ((t_cm_chunk*)memHandle)->size;
}

PUBLIC t_uint32 cm_MM_GetAllocatorSize(t_cm_allocator_desc* alloc)
{
    return alloc->size;
}

PUBLIC void cm_MM_SetMemoryHandleUserData(t_memory_handle memHandle, t_uint16 userData)
{
    ((t_cm_chunk*)memHandle)->userData = userData;
}

PUBLIC void cm_MM_GetMemoryHandleUserData(t_memory_handle memHandle, t_uint16 *pUserData, t_cm_allocator_desc **alloc)
{
    *pUserData = ((t_cm_chunk*)memHandle)->userData;
    if (alloc)
        *alloc = ((t_cm_chunk*)memHandle)->alloc;
}

/*
 * check free list is ordered
 * check all chunks are correctly linked
 * check adjacent chunks are not FREE
 */
static void cm_MM_RA_checkAllocator(t_cm_allocator_desc* alloc)
{
    t_cm_chunk *chunk = alloc->chunks;
    t_cm_chunk *first = chunk;
    t_cm_chunk *last = chunk;
    t_uint32 size = 0;
    int i;

    while(chunk != 0) {
        CM_ASSERT(chunk->alloc == alloc);

        if (chunk->next != 0) {
            CM_ASSERT(!((chunk->status == MEM_FREE) && (chunk->next->status == MEM_FREE))); //two free adjacent blocks
            CM_ASSERT(chunk->offset < chunk->next->offset); //offsets reverted
            last = chunk->next;
        }
        size += chunk->size;
        chunk = chunk->next;
    }

    CM_ASSERT(size == alloc->size);

    for(i = 0; i < BINS; i++)
    {
        chunk = alloc->free_mem_chunks[i];
        while(chunk != 0) {
            if (chunk->next_free_mem != 0) {
                CM_ASSERT(chunk->size <= chunk->next_free_mem->size); //free list not ordered
            }
            CM_ASSERT(!(chunk->prev == 0 && (chunk != first))); //chunk not linked properly
            CM_ASSERT(!(chunk->next == 0 && (chunk != last))); //chunk not linked property
            chunk = chunk->next_free_mem;
        }
    }
}

#if 0
static void cm_MM_RA_checkAlloc(t_cm_allocator_desc* alloc, t_uint32 size, t_uint32 align, t_uint32 min, t_uint32 max)
{
    t_cm_chunk *chunk = alloc->chunks;

    while(chunk != 0) {
        if (chunk->status == MEM_USED) {
            chunk = chunk->next;
            continue;
        }
        if (chunk->size < size) {
            chunk = chunk->next;
            continue;
        }

        if (min < chunk->offset) {
            t_uint32 aligned_offset = ALIGN_VALUE(chunk->offset, align + 1);
            t_uint32 aligned_end = aligned_offset + size;
            if ((aligned_offset + size <= chunk->offset + chunk->size)
                    && (chunk->offset + chunk->size <= aligned_end)){
                break;
            }
        }

        if (min >= chunk->offset) {
            t_uint32 aligned_offset = ALIGN_VALUE(min, align + 1);
            t_uint32 aligned_end = aligned_offset + size;
            if ((aligned_offset + size <= chunk->offset + chunk->size)
                    && (chunk->offset + chunk->size <= aligned_end)) {
                break;
            }
        }

        chunk = chunk->next;
    }

    CM_ASSERT(chunk == 0);
}
#endif

/***************************************************************************/
/*
 * cm_mm_ra_getLastChunk
 * param handle     : Handle of the allocator
 * return : last
 *
 * Free all chunk in the allocator
 * Free allocator descriptor
 *
 */
/***************************************************************************/
static t_cm_chunk* cm_MM_RA_getLastChunk(t_cm_allocator_desc* alloc)
{
    t_cm_chunk* pChunk = alloc->chunks;

    while(pChunk->next != 0) {pChunk = pChunk->next;}

    return pChunk;
}

PUBLIC void cm_MM_DumpMemory(t_cm_allocator_desc* alloc, t_uint32 start, t_uint32 end)
{
    t_cm_chunk *chunk = alloc->chunks;

    LOG_INTERNAL(0, "ALLOCATOR Dumping allocator \"%s\" [0x%08x:0x%08x]\n", alloc->pAllocName, start, end, 0, 0, 0);
    while(chunk != 0) {
        if (((chunk->offset < start) && (chunk->offset + chunk->size > start))
          || ((chunk->offset < end) && (chunk->offset + chunk->size > end))
          || ((chunk->offset > start) && (chunk->offset + chunk->size < end))
          || ((chunk->offset < start) && (chunk->offset + chunk->size > end)))
        {
            LOG_INTERNAL(0, "ALLOCATOR chunk 0x%08x -> 0x%08x: status:%s, domainId: 0x%x\n",
                    chunk->offset,
                    chunk->offset + chunk->size,
                    chunk->status?"FREE":"USED",
                    chunk->domainId, 0, 0);
        }
        chunk = chunk->next;
    }
}

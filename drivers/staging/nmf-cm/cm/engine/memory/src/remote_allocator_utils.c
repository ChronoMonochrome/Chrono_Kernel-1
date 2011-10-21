/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Jean-Philippe FASSINO <jean-philippe.fassino@stericsson.com> for ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2.
 */
#include <cm/engine/memory/inc/remote_allocator_utils.h>

/***************************************************************************/
/*
 * linkChunk
 * param prev     : Pointer on previous chunk where the chunk will be added
 * param add      : Pointer on chunk to add
 *
 * Add a chunk in the memory list
 *
 */
/***************************************************************************/
PUBLIC void linkChunk(t_cm_chunk* prev, t_cm_chunk* add)
{
    /* Link previous */
    add->prev = prev;
    add->next = prev->next;

    /* Link next */
    if (prev->next != 0)
    {
        prev->next->prev = add;
    }
    prev->next = add;
}

/***************************************************************************/
/*
 * unlinkChunk
 * param allocHandle : Allocator handle
 * param current     : Pointer on chunk to remove
 *
 * Remove a chunk in the memory list and update first pointer
 *
 */
/***************************************************************************/
PUBLIC void unlinkChunk(t_cm_allocator_desc* alloc ,t_cm_chunk* current)
{
    /* Unlink previous */
    if (current->prev !=0)
    {
        current->prev->next = current->next;
    }

    /* Unlink next */
    if (current->next !=0)
    {
        current->next->prev= current->prev;
    }

    /* Update first pointer */
    if (alloc ->chunks == current)
    {
    alloc ->chunks = current->next;
    }
}


/***************************************************************************/
/*
 * unlinkFreeMem() unlinks chunk from free memory double-linked list
 *   makes the previous and next chunk in the list point to each other..
 * param allocHandle : Allocator handle
 * param current     : Pointer on chunk to remove
 *
 * Remove a chunk in the free memory list and update pointer
 *
 */
/***************************************************************************/
PUBLIC void unlinkFreeMem(t_cm_allocator_desc* alloc ,t_cm_chunk* current)
{
    int bin = bin_index(current->size);

    /* unlink previous */
    if (current->prev_free_mem != 0)
    {
        current->prev_free_mem->next_free_mem = current->next_free_mem;
    }

    /* Unlink next */
    if (current->next_free_mem !=0 )
    {
        current->next_free_mem->prev_free_mem = current->prev_free_mem;
    }

    /* update first free pointer */
    if (alloc->free_mem_chunks[bin] == current)
    {
        alloc->free_mem_chunks[bin] = current->next_free_mem;
    }

    current->prev_free_mem = 0;
    current->next_free_mem = 0;
}

/***************************************************************************/
/*
 * linkFreeMemBefore
 * param add      : Pointer on chunk to add
 * param next     : Pointer on next chunk where the chunk will be added before
 *
 * Add a chunk in the free memory list
 *
 */
/***************************************************************************/
PUBLIC void linkFreeMemBefore(t_cm_chunk* add, t_cm_chunk* next)
{
    /* Link next */
    add->prev_free_mem = next->prev_free_mem;
    add->next_free_mem = next;

    /* Link previous */
    if (next->prev_free_mem != 0)
    {
        next->prev_free_mem->next_free_mem = add;
    }
    next->prev_free_mem = add;
}

/***************************************************************************/
/*
 * linkFreeMemAfter
 * param add      : Pointer on chunk to add
 * param prev     : Pointer on previous chunk where the chunk will be added after
 *
 * Add a chunk in the free memory list
 *
 */
/***************************************************************************/
PUBLIC void linkFreeMemAfter(t_cm_chunk* prev,t_cm_chunk* add)
{
    /* Link previous */
    add->prev_free_mem = prev;
    add->next_free_mem = prev->next_free_mem;

    /* Link next */
    if (prev->next_free_mem != 0)
    {
        prev->next_free_mem->prev_free_mem = add;
    }
    prev->next_free_mem = add;
}


/***************************************************************************/
/*
 * updateFreeList
 * param allocHandle : Allocator handle
 * param offset      : Pointer on chunk
 *
 * Update free memory list, ordered by size
 *
 */
/***************************************************************************/
PUBLIC void updateFreeList(t_cm_allocator_desc* alloc , t_cm_chunk* chunk)
{
    t_cm_chunk* free_chunk;
    int bin = bin_index(chunk->size);

    /* check case with no more free block */
    if (alloc->free_mem_chunks[bin] == 0)
    {
        alloc->free_mem_chunks[bin] = chunk;
        return ;
    }

    /* order list */
    free_chunk = alloc->free_mem_chunks[bin];
    while ((free_chunk->next_free_mem != 0) && (chunk->size > free_chunk->size))
    {
        free_chunk = free_chunk->next_free_mem;
    }

    /* Add after free chunk if smaller -> we are the last */
    if(free_chunk->size <= chunk->size)
    {
        linkFreeMemAfter(free_chunk,chunk);
    }
    else // This mean that we are smaller
    {
        linkFreeMemBefore(chunk,free_chunk);

        /* Update first free chunk */
        if (alloc->free_mem_chunks[bin] == free_chunk)
        {
            alloc->free_mem_chunks[bin] = chunk;
        }
    }
}


/***************************************************************************/
/*
 * mergeChunk
 * param allocHandle  : Allocator handle
 * param merged_chunk : Pointer on merged chunk
 * param destroy      : Pointer on chunk to destroy
 *
 * Link and destroy merged chunks
 *
 */
/***************************************************************************/
PUBLIC void mergeChunk(t_cm_allocator_desc* alloc,t_cm_chunk *merged_chunk, t_cm_chunk *destroy)
{
    /* Assign offset to the merged */
    /* assume chunks ordered!
    if (merged_chunk->offset > destroy->offset) {
        merged_chunk->offset = destroy->offset;
    }
    */

    /* Remove chunk */
    unlinkChunk(alloc, destroy);
    unlinkFreeMem(alloc, destroy);

    if (merged_chunk->status == MEM_FREE)
        unlinkFreeMem(alloc, merged_chunk);

    /* Update size */
    merged_chunk->size += destroy->size;

    if (merged_chunk->status == MEM_FREE)
        updateFreeList(alloc, merged_chunk);

    freeChunk(destroy);
}

/***************************************************************************/
/*
 * splitChunk
 * param allocHandle : Allocator handle
 * param chunk       : Current chunk (modified in place)
 * param offset      : Offset address of the start memory
 * return            : New chunk handle or 0 if an error occurs
 *
 * Create new chunk before/after the current chunk with the size
 */
/***************************************************************************/
PUBLIC t_cm_chunk* splitChunk(t_cm_allocator_desc* alloc ,t_cm_chunk *chunk,
        t_uint32 offset, t_mem_split_position position)
{
    t_cm_chunk *free;
    t_cm_chunk *returned;

    t_cm_chunk* new_chunk = allocChunk();

    if (position == FREE_CHUNK_AFTER) {
        returned = chunk;
        free = new_chunk;
    } else { //FREE_CHUNK_BEFORE
        returned = new_chunk;
        free = chunk;
    }

    new_chunk->offset = offset;
    new_chunk->size   = chunk->offset + chunk->size - offset;
    new_chunk->alloc  = alloc;
    chunk->size = offset - chunk->offset;

    linkChunk(chunk, new_chunk);
    unlinkFreeMem(alloc, free);
    updateFreeList(alloc, free);

    return returned;
}

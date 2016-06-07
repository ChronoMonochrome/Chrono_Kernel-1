/*
 * Copyright (C) 2010-2012 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "mali_osk.h"
#include "mali_osk_list.h"
#include "ump_osk.h"
#include "ump_uk_types.h"
#include "ump_kernel_interface.h"
#include "ump_kernel_common.h"



/* ---------------- UMP kernel space API functions follows ---------------- */


#ifndef USING_HWMEM
UMP_KERNEL_API_EXPORT ump_secure_id ump_dd_secure_id_get(ump_dd_handle memh)
{
	ump_dd_mem * mem = (ump_dd_mem *)memh;

	DEBUG_ASSERT_POINTER(mem);

	DBG_MSG(5, ("Returning secure ID. ID: %u\n", mem->secure_id));

	return mem->secure_id;
}



UMP_KERNEL_API_EXPORT ump_dd_handle ump_dd_handle_create_from_secure_id(ump_secure_id secure_id)
{
	ump_dd_mem * mem;

	_mali_osk_lock_wait(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);

	DBG_MSG(5, ("Getting handle from secure ID. ID: %u\n", secure_id));
	if (0 != ump_descriptor_mapping_get(device.secure_id_map, (int)secure_id, (void**)&mem))
	{
		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
		DBG_MSG(1, ("Secure ID not found. ID: %u\n", secure_id));
		return UMP_DD_HANDLE_INVALID;
	}

	ump_dd_reference_add(mem);

	_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);

	return (ump_dd_handle)mem;
}



UMP_KERNEL_API_EXPORT unsigned long ump_dd_phys_block_count_get(ump_dd_handle memh)
{
	ump_dd_mem * mem = (ump_dd_mem*) memh;

	DEBUG_ASSERT_POINTER(mem);

	return mem->nr_blocks;
}



UMP_KERNEL_API_EXPORT ump_dd_status_code ump_dd_phys_blocks_get(ump_dd_handle memh, ump_dd_physical_block * blocks, unsigned long num_blocks)
{
	ump_dd_mem * mem = (ump_dd_mem *)memh;

	DEBUG_ASSERT_POINTER(mem);

	if (blocks == NULL)
	{
		DBG_MSG(1, ("NULL parameter in ump_dd_phys_blocks_get()\n"));
		return UMP_DD_INVALID;
	}

	if (mem->nr_blocks != num_blocks)
	{
		DBG_MSG(1, ("Specified number of blocks do not match actual number of blocks\n"));
		return UMP_DD_INVALID;
	}

	DBG_MSG(5, ("Returning physical block information. ID: %u\n", mem->secure_id));

	_mali_osk_memcpy(blocks, mem->block_array, sizeof(ump_dd_physical_block) * mem->nr_blocks);

	return UMP_DD_SUCCESS;
}



UMP_KERNEL_API_EXPORT ump_dd_status_code ump_dd_phys_block_get(ump_dd_handle memh, unsigned long index, ump_dd_physical_block * block)
{
	ump_dd_mem * mem = (ump_dd_mem *)memh;

	DEBUG_ASSERT_POINTER(mem);

	if (block == NULL)
	{
		DBG_MSG(1, ("NULL parameter in ump_dd_phys_block_get()\n"));
		return UMP_DD_INVALID;
	}

	if (index >= mem->nr_blocks)
	{
		DBG_MSG(5, ("Invalid index specified in ump_dd_phys_block_get()\n"));
		return UMP_DD_INVALID;
	}

	DBG_MSG(5, ("Returning physical block information. ID: %u, index: %lu\n", mem->secure_id, index));

	*block = mem->block_array[index];

	return UMP_DD_SUCCESS;
}



UMP_KERNEL_API_EXPORT unsigned long ump_dd_size_get(ump_dd_handle memh)
{
	ump_dd_mem * mem = (ump_dd_mem*)memh;

	DEBUG_ASSERT_POINTER(mem);

	DBG_MSG(5, ("Returning size. ID: %u, size: %lu\n", mem->secure_id, mem->size_bytes));

	return mem->size_bytes;
}



UMP_KERNEL_API_EXPORT void ump_dd_reference_add(ump_dd_handle memh)
{
	ump_dd_mem * mem = (ump_dd_mem*)memh;
	int new_ref;

	DEBUG_ASSERT_POINTER(mem);

	new_ref = _ump_osk_atomic_inc_and_read(&mem->ref_count);

	DBG_MSG(5, ("Memory reference incremented. ID: %u, new value: %d\n", mem->secure_id, new_ref));
}



UMP_KERNEL_API_EXPORT void ump_dd_reference_release(ump_dd_handle memh)
{
	int new_ref;
	ump_dd_mem * mem = (ump_dd_mem*)memh;

	DEBUG_ASSERT_POINTER(mem);

	/* We must hold this mutex while doing the atomic_dec_and_read, to protect
	that elements in the ump_descriptor_mapping table is always valid.  If they
	are not, userspace may accidently map in this secure_ids right before its freed
	giving a mapped backdoor into unallocated memory.*/
	_mali_osk_lock_wait(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);

	new_ref = _ump_osk_atomic_dec_and_read(&mem->ref_count);

	DBG_MSG(5, ("Memory reference decremented. ID: %u, new value: %d\n", mem->secure_id, new_ref));

	if (0 == new_ref)
	{
		DBG_MSG(3, ("Final release of memory. ID: %u\n", mem->secure_id));

		ump_descriptor_mapping_free(device.secure_id_map, (int)mem->secure_id);

		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
		mem->release_func(mem->ctx, mem);
		_mali_osk_free(mem);
	}
	else
	{
		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
	}
}
#else

/*
 * Copyright (C) ST-Ericsson SA 2011
 * Author: Magnus Wendt <magnus.wendt@stericsson.com> for
 * ST-Ericsson.
 * License terms: GNU General Public License (GPL), version 2.
 */

#include "ump_kernel_types.h"
#include "mali_kernel_common.h"

#include <linux/hwmem.h>
#include <linux/err.h>
#include <linux/slab.h>


/* The UMP kernel API for hwmem has been mapped so that
 * ump_dd_handle == hwmem_alloc
 * ump_secure_id == hwmem global name
 *
 * The current implementation is limited to contiguous memory
 */

ump_secure_id ump_dd_secure_id_get(ump_dd_handle memh)
{
	int hwmem_name = hwmem_get_name((struct hwmem_alloc *) memh);

	if (unlikely(hwmem_name < 0)) {
		MALI_DEBUG_PRINT(1, ("%s: Invalid Alloc 0x%x\n",__func__, memh));
		return UMP_INVALID_SECURE_ID;
	}

	return (ump_secure_id)hwmem_name;
}



ump_dd_handle ump_dd_handle_create_from_secure_id(ump_secure_id secure_id)
{
	struct hwmem_alloc *alloc;
	enum hwmem_mem_type mem_type;
	enum hwmem_access access;

	alloc = hwmem_resolve_by_name((int) secure_id);

	if (IS_ERR(alloc)) {
		MALI_DEBUG_PRINT(1, ("%s: Invalid UMP id %d\n",__func__, secure_id));
		return UMP_DD_HANDLE_INVALID;
	}

	hwmem_get_info(alloc, NULL, &mem_type, &access);

	if (unlikely((access & (HWMEM_ACCESS_READ | HWMEM_ACCESS_WRITE | HWMEM_ACCESS_IMPORT)) !=
	                       (HWMEM_ACCESS_READ | HWMEM_ACCESS_WRITE | HWMEM_ACCESS_IMPORT))) {
		MALI_DEBUG_PRINT(1, ("%s: Access denied on UMP id %d, (access==%d)\n",
			__func__, secure_id, access));
		hwmem_release(alloc);
		return UMP_DD_HANDLE_INVALID;
	}

	return (ump_dd_handle)alloc;
}



unsigned long ump_dd_phys_block_count_get(ump_dd_handle memh)
{
	size_t hwmem_mem_chunk_length;
	int hwmem_result = 0;
	struct hwmem_alloc *alloc = (struct hwmem_alloc *)memh;

	/* Call hwmem_pin with mem_chunks set to NULL to get hwmem_mem_chunk_length */
	hwmem_result = hwmem_pin(alloc, NULL, &hwmem_mem_chunk_length);

	return hwmem_mem_chunk_length;
}



ump_dd_status_code ump_dd_phys_blocks_get(ump_dd_handle memh,
                                          ump_dd_physical_block * blocks,
                                          unsigned long num_blocks)
{
	struct hwmem_mem_chunk *hwmem_mem_chunks;
	size_t hwmem_mem_chunk_length = num_blocks;

	int hwmem_result;
	int i;

	struct hwmem_alloc *alloc = (struct hwmem_alloc *)memh;

	hwmem_mem_chunks = (struct hwmem_mem_chunk *)kmalloc(sizeof(struct hwmem_mem_chunk)*num_blocks, GFP_KERNEL);

	if (unlikely(blocks == NULL)) {
		MALI_DEBUG_PRINT(1, ("%s: blocks == NULL\n",__func__));
		return UMP_DD_INVALID;
	}

	MALI_DEBUG_PRINT(5, ("Returning physical block information. Alloc: 0x%x num_blocks=%d\n", memh, num_blocks));

	/* It might not look natural to pin here, but it matches the usage by the mali kernel module */
	hwmem_result = hwmem_pin(alloc, hwmem_mem_chunks, &hwmem_mem_chunk_length);

	if (unlikely(hwmem_result < 0)) {
		MALI_DEBUG_PRINT(1, ("%s: Pin failed. Alloc: 0x%x\n",__func__, memh));
		kfree(hwmem_mem_chunks);
		return UMP_DD_INVALID;
	}

	/* Scattered: Currently every page is one mem chunk. It's probably more
	   efficient to create bigger mem chunks if possible when allocated pages
	   are next to each other in memory */
	for(i = 0; i < hwmem_mem_chunk_length; i++) {
		blocks[i].addr = hwmem_mem_chunks[i].paddr;
		blocks[i].size = hwmem_mem_chunks[i].size;
	}
	kfree(hwmem_mem_chunks);

	hwmem_set_domain(alloc, HWMEM_ACCESS_READ | HWMEM_ACCESS_WRITE,
		HWMEM_DOMAIN_SYNC, NULL);

	return UMP_DD_SUCCESS;
}



ump_dd_status_code ump_dd_phys_block_get(ump_dd_handle memh,
                                         unsigned long index,
                                         ump_dd_physical_block * block)
{
	if (unlikely(0 != index))	{
		MALI_DEBUG_PRINT(1, ("%s: index == %d (!= 0)\n",__func__, index));
		return UMP_DD_INVALID;
	}
	return ump_dd_phys_blocks_get(memh, block, 1);
}



unsigned long ump_dd_size_get(ump_dd_handle memh)
{
	struct hwmem_alloc *alloc = (struct hwmem_alloc *)memh;
	int size;

	hwmem_get_info(alloc, &size, NULL, NULL);

	return size;
}



void ump_dd_reference_add(ump_dd_handle memh)
{
	/* This is never called from tha mali kernel driver */
}



void ump_dd_reference_release(ump_dd_handle memh)
{
	struct hwmem_alloc *alloc = (struct hwmem_alloc *)memh;

	hwmem_unpin(alloc);
	hwmem_release(alloc);

	return;
}
#endif

/* --------------- Handling of user space requests follows --------------- */


_mali_osk_errcode_t _ump_uku_get_api_version( _ump_uk_api_version_s *args )
{
	ump_session_data * session_data;

	DEBUG_ASSERT_POINTER( args );
	DEBUG_ASSERT_POINTER( args->ctx );

	session_data = (ump_session_data *)args->ctx;

	/* check compatability */
	if (args->version == UMP_IOCTL_API_VERSION)
	{
		DBG_MSG(3, ("API version set to newest %d (compatible)\n", GET_VERSION(args->version)));
		args->compatible = 1;
		session_data->api_version = args->version;
	}
	else if (args->version == MAKE_VERSION_ID(1))
	{
		DBG_MSG(2, ("API version set to depricated: %d (compatible)\n", GET_VERSION(args->version)));
		args->compatible = 1;
		session_data->api_version = args->version;
	}
	else
	{
		DBG_MSG(2, ("API version set to %d (incompatible with client version %d)\n", GET_VERSION(UMP_IOCTL_API_VERSION), GET_VERSION(args->version)));
		args->compatible = 0;
		args->version = UMP_IOCTL_API_VERSION; /* report our version */
	}

	return _MALI_OSK_ERR_OK;
}


_mali_osk_errcode_t _ump_ukk_release( _ump_uk_release_s *release_info )
{
	ump_session_memory_list_element * session_memory_element;
	ump_session_memory_list_element * tmp;
	ump_session_data * session_data;
	_mali_osk_errcode_t ret = _MALI_OSK_ERR_INVALID_FUNC;
	int secure_id;

	DEBUG_ASSERT_POINTER( release_info );
	DEBUG_ASSERT_POINTER( release_info->ctx );

	/* Retreive the session data */
	session_data = (ump_session_data*)release_info->ctx;

	/* If there are many items in the memory session list we
	 * could be de-referencing this pointer a lot so keep a local copy
	 */
	secure_id = release_info->secure_id;

	DBG_MSG(4, ("Releasing memory with IOCTL, ID: %u\n", secure_id));

	/* Iterate through the memory list looking for the requested secure ID */
	_mali_osk_lock_wait(session_data->lock, _MALI_OSK_LOCKMODE_RW);
	_MALI_OSK_LIST_FOREACHENTRY(session_memory_element, tmp, &session_data->list_head_session_memory_list, ump_session_memory_list_element, list)
	{
		if ( session_memory_element->mem->secure_id == secure_id)
		{
			ump_dd_mem *release_mem;

			release_mem = session_memory_element->mem;
			_mali_osk_list_del(&session_memory_element->list);
			ump_dd_reference_release(release_mem);
			_mali_osk_free(session_memory_element);

			ret = _MALI_OSK_ERR_OK;
			break;
		}
	}

	_mali_osk_lock_signal(session_data->lock, _MALI_OSK_LOCKMODE_RW);
 	DBG_MSG_IF(1, _MALI_OSK_ERR_OK != ret, ("UMP memory with ID %u does not belong to this session.\n", secure_id));

	DBG_MSG(4, ("_ump_ukk_release() returning 0x%x\n", ret));
	return ret;
}

_mali_osk_errcode_t _ump_ukk_size_get( _ump_uk_size_get_s *user_interaction )
{
	ump_dd_mem * mem;
	_mali_osk_errcode_t ret = _MALI_OSK_ERR_FAULT;

	DEBUG_ASSERT_POINTER( user_interaction );

	/* We lock the mappings so things don't get removed while we are looking for the memory */
	_mali_osk_lock_wait(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
	if (0 == ump_descriptor_mapping_get(device.secure_id_map, (int)user_interaction->secure_id, (void**)&mem))
	{
		user_interaction->size = mem->size_bytes;
		DBG_MSG(4, ("Returning size. ID: %u, size: %lu ", (ump_secure_id)user_interaction->secure_id, (unsigned long)user_interaction->size));
		ret = _MALI_OSK_ERR_OK;
	}
	else
	{
		 user_interaction->size = 0;
		DBG_MSG(1, ("Failed to look up mapping in ump_ioctl_size_get(). ID: %u\n", (ump_secure_id)user_interaction->secure_id));
	}

	_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
	return ret;
}



void _ump_ukk_msync( _ump_uk_msync_s *args )
{
	ump_dd_mem * mem = NULL;
	void *virtual = NULL;
	u32 size = 0;
	u32 offset = 0;

	_mali_osk_lock_wait(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
	ump_descriptor_mapping_get(device.secure_id_map, (int)args->secure_id, (void**)&mem);

	if (NULL == mem)
	{
		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
		DBG_MSG(1, ("Failed to look up mapping in _ump_ukk_msync(). ID: %u\n", (ump_secure_id)args->secure_id));
		return;
	}
	/* Ensure the memory doesn't dissapear when we are flushing it. */
	ump_dd_reference_add(mem);
	_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);

	/* Returns the cache settings back to Userspace */
	args->is_cached=mem->is_cached;

	/* If this flag is the only one set, we should not do the actual flush, only the readout */
	if ( _UMP_UK_MSYNC_READOUT_CACHE_ENABLED==args->op )
	{
		DBG_MSG(3, ("_ump_ukk_msync READOUT  ID: %u Enabled: %d\n", (ump_secure_id)args->secure_id, mem->is_cached));
		goto msync_release_and_return;
	}

	/* Nothing to do if the memory is not caches */
	if ( 0==mem->is_cached )
	{
		DBG_MSG(3, ("_ump_ukk_msync IGNORING ID: %u Enabled: %d  OP: %d\n", (ump_secure_id)args->secure_id, mem->is_cached, args->op));
		goto msync_release_and_return;
	}
	DBG_MSG(3, ("UMP[%02u] _ump_ukk_msync  Flush  OP: %d Address: 0x%08x Mapping: 0x%08x\n",
	            (ump_secure_id)args->secure_id, args->op, args->address, args->mapping));

	if ( args->address )
	{
		virtual = (void *)((u32)args->address);
		offset = (u32)((args->address) - (args->mapping));
	} else {
		/* Flush entire mapping when no address is specified. */
		virtual = args->mapping;
	}
	if ( args->size )
	{
		size = args->size;
	} else {
		/* Flush entire mapping when no size is specified. */
		size = mem->size_bytes - offset;
	}

	if ( (offset + size) > mem->size_bytes )
	{
		DBG_MSG(1, ("Trying to flush more than the entire UMP allocation: offset: %u + size: %u > %u\n", offset, size, mem->size_bytes));
		goto msync_release_and_return;
	}

	/* The actual cache flush - Implemented for each OS*/
	_ump_osk_msync( mem, virtual, offset, size, args->op, NULL);

msync_release_and_return:
	ump_dd_reference_release(mem);
	return;
}

void _ump_ukk_cache_operations_control(_ump_uk_cache_operations_control_s* args)
{
	ump_session_data * session_data;
	ump_uk_cache_op_control op;

	DEBUG_ASSERT_POINTER( args );
	DEBUG_ASSERT_POINTER( args->ctx );

	op = args->op;
	session_data = (ump_session_data *)args->ctx;

	_mali_osk_lock_wait(session_data->lock, _MALI_OSK_LOCKMODE_RW);
	if ( op== _UMP_UK_CACHE_OP_START )
	{
		session_data->cache_operations_ongoing++;
		DBG_MSG(4, ("Cache ops start\n" ));
		if ( session_data->cache_operations_ongoing != 1 )
		{
			DBG_MSG(2, ("UMP: Number of simultanious cache control ops: %d\n", session_data->cache_operations_ongoing) );
		}
	}
	else if ( op== _UMP_UK_CACHE_OP_FINISH )
	{
		DBG_MSG(4, ("Cache ops finish\n"));
		session_data->cache_operations_ongoing--;
		#if 0
		if ( session_data->has_pending_level1_cache_flush)
		{
			/* This function will set has_pending_level1_cache_flush=0 */
			_ump_osk_msync( NULL, NULL, 0, 0, _UMP_UK_MSYNC_FLUSH_L1, session_data);
		}
		#endif

		/* to be on the safe side: always flush l1 cache when cache operations are done */
		_ump_osk_msync( NULL, NULL, 0, 0, _UMP_UK_MSYNC_FLUSH_L1, session_data);
		DBG_MSG(4, ("Cache ops finish end\n" ));
	}
	else
	{
		DBG_MSG(1, ("Illegal call to %s at line %d\n", __FUNCTION__, __LINE__));
	}
	_mali_osk_lock_signal(session_data->lock, _MALI_OSK_LOCKMODE_RW);

}

void _ump_ukk_switch_hw_usage(_ump_uk_switch_hw_usage_s *args )
{
	ump_dd_mem * mem = NULL;
	ump_uk_user old_user;
	ump_uk_msync_op cache_op = _UMP_UK_MSYNC_CLEAN_AND_INVALIDATE;
	ump_session_data *session_data;

	DEBUG_ASSERT_POINTER( args );
	DEBUG_ASSERT_POINTER( args->ctx );

	session_data = (ump_session_data *)args->ctx;

	_mali_osk_lock_wait(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
	ump_descriptor_mapping_get(device.secure_id_map, (int)args->secure_id, (void**)&mem);

	if (NULL == mem)
	{
		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
		DBG_MSG(1, ("Failed to look up mapping in _ump_ukk_switch_hw_usage(). ID: %u\n", (ump_secure_id)args->secure_id));
		return;
	}

	old_user = mem->hw_device;
	mem->hw_device = args->new_user;

	DBG_MSG(3, ("UMP[%02u] Switch usage  Start  New: %s  Prev: %s.\n", (ump_secure_id)args->secure_id, args->new_user?"MALI":"CPU",old_user?"MALI":"CPU"));

	if ( ! mem->is_cached )
	{
		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
		DBG_MSG(3, ("UMP[%02u] Changing owner of uncached memory. Cache flushing not needed.\n", (ump_secure_id)args->secure_id));
		return;
	}

	if ( old_user == args->new_user)
	{
		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
		DBG_MSG(4, ("UMP[%02u] Setting the new_user equal to previous for. Cache flushing not needed.\n", (ump_secure_id)args->secure_id));
		return;
	}
	if (
		 /* Previous AND new is both different from CPU */
		 (old_user != _UMP_UK_USED_BY_CPU) && (args->new_user != _UMP_UK_USED_BY_CPU  )
	   )
	{
		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
		DBG_MSG(4, ("UMP[%02u] Previous and new user is not CPU. Cache flushing not needed.\n", (ump_secure_id)args->secure_id));
		return;
	}

	if ( (old_user != _UMP_UK_USED_BY_CPU ) && (args->new_user==_UMP_UK_USED_BY_CPU) )
	{
		cache_op =_UMP_UK_MSYNC_INVALIDATE;
		DBG_MSG(4, ("UMP[%02u] Cache invalidation needed\n", (ump_secure_id)args->secure_id));
#ifdef UMP_SKIP_INVALIDATION
#error
		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
		DBG_MSG(4, ("UMP[%02u] Performing Cache invalidation SKIPPED\n", (ump_secure_id)args->secure_id));
		return;
#endif
	}
	/* Ensure the memory doesn't dissapear when we are flushing it. */
	ump_dd_reference_add(mem);
	_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);

	/* Take lock to protect: session->cache_operations_ongoing and session->has_pending_level1_cache_flush */
	_mali_osk_lock_wait(session_data->lock, _MALI_OSK_LOCKMODE_RW);
	/* Actual cache flush */
	_ump_osk_msync( mem, NULL, 0, mem->size_bytes, cache_op, session_data);
	_mali_osk_lock_signal(session_data->lock, _MALI_OSK_LOCKMODE_RW);

	ump_dd_reference_release(mem);
	DBG_MSG(4, ("UMP[%02u] Switch usage  Finish\n", (ump_secure_id)args->secure_id));
	return;
}

void _ump_ukk_lock(_ump_uk_lock_s *args )
{
	ump_dd_mem * mem = NULL;

	_mali_osk_lock_wait(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
	ump_descriptor_mapping_get(device.secure_id_map, (int)args->secure_id, (void**)&mem);

	if (NULL == mem)
	{
		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
		DBG_MSG(1, ("UMP[%02u] Failed to look up mapping in _ump_ukk_lock(). ID: %u\n", (ump_secure_id)args->secure_id));
		return;
	}
	ump_dd_reference_add(mem);
	_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);

	DBG_MSG(1, ("UMP[%02u] Lock. New lock flag: %d. Old Lock flag:\n", (u32)args->secure_id, (u32)args->lock_usage, (u32) mem->lock_usage ));

	mem->lock_usage = (ump_lock_usage) args->lock_usage;

	/** TODO: TAKE LOCK HERE */

	ump_dd_reference_release(mem);
}

void _ump_ukk_unlock(_ump_uk_unlock_s *args )
{
	ump_dd_mem * mem = NULL;

	_mali_osk_lock_wait(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
	ump_descriptor_mapping_get(device.secure_id_map, (int)args->secure_id, (void**)&mem);

	if (NULL == mem)
	{
		_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);
		DBG_MSG(1, ("Failed to look up mapping in _ump_ukk_unlock(). ID: %u\n", (ump_secure_id)args->secure_id));
		return;
	}
	ump_dd_reference_add(mem);
	_mali_osk_lock_signal(device.secure_id_map_lock, _MALI_OSK_LOCKMODE_RW);

	DBG_MSG(1, ("UMP[%02u] Unlocking. Old Lock flag:\n", (u32)args->secure_id, (u32) mem->lock_usage ));

	mem->lock_usage = (ump_lock_usage) UMP_NOT_LOCKED;

	/** TODO: RELEASE LOCK HERE */

	ump_dd_reference_release(mem);
}

/*
 * Copyright (C) ST-Ericsson AB 2010
 *
 * ST-Ericsson HW memory driver
 *
 * Author: Marcus Lorentzon <marcus.xm.lorentzon@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */

#ifndef _HWMEM_H_
#define _HWMEM_H_

#if !defined(__KERNEL__) && !defined(_KERNEL)
#include <stdint.h>
#include <sys/types.h>
#else
#include <linux/types.h>
#include <linux/mm_types.h>
#endif

#define HWMEM_DEFAULT_DEVICE_NAME "hwmem"

/**
 * @brief Flags defining behavior of allocation
 */
enum hwmem_alloc_flags {
	/**
	 * @brief Buffer will not be cached and not buffered
	 */
	HWMEM_ALLOC_UNCACHED             = (0 << 0),
	/**
	 * @brief Buffer will be buffered, but not cached
	 */
	HWMEM_ALLOC_BUFFERED             = (1 << 0),
	/**
	 * @brief Buffer will be cached and buffered, use cache hints to be
	 * more specific
	 */
	HWMEM_ALLOC_CACHED               = (3 << 0),
	/**
	 * @brief Buffer should be cached write-back in both level 1 and 2 cache
	 */
	HWMEM_ALLOC_CACHE_HINT_WB        = (1 << 2),
	/**
	 * @brief Buffer should be cached write-through in both level 1 and
	 * 2 cache
	 */
	HWMEM_ALLOC_CACHE_HINT_WT        = (2 << 2),
	/**
	 * @brief Buffer should be cached write-back in level 1 cache
	 */
	HWMEM_ALLOC_CACHE_HINT_WB_INNER  = (3 << 2),
	/**
	 * @brief Buffer should be cached write-through in level 1 cache
	 */
	HWMEM_ALLOC_CACHE_HINT_WT_INNER  = (4 << 2),
	HWMEM_ALLOC_CACHE_HINT_MASK      = 0x1C,
};

/**
 * @brief Flags defining buffer access mode.
 */
enum hwmem_access {
	/**
	 * @brief Buffer will be read from.
	 */
	HWMEM_ACCESS_READ  = (1 << 0),
	/**
	 * @brief Buffer will be written to.
	 */
	HWMEM_ACCESS_WRITE = (1 << 1),
	/**
	 * @brief Buffer will be imported.
	 */
	HWMEM_ACCESS_IMPORT = (1 << 2),
};

/**
 * @brief Flags defining memory type.
 */
enum hwmem_mem_type {
	/**
	 * @brief Scattered system memory. Currently not supported!
	 */
	HWMEM_MEM_SCATTERED_SYS  = (1 << 0),
	/**
	 * @brief Contiguous system memory.
	 */
	HWMEM_MEM_CONTIGUOUS_SYS = (1 << 1),
};

/**
 * @brief Values defining memory domain.
 */
enum hwmem_domain {
	/**
	 * @brief This value specifies the neutral memory domain. Setting this
	 * domain will syncronize all supported memory domains (currently CPU).
	 */
	HWMEM_DOMAIN_SYNC = 0,
	/**
	 * @brief This value specifies the CPU memory domain.
	 */
	HWMEM_DOMAIN_CPU  = 1,
};

/**
 * @brief Structure defining a region of a memory buffer.
 *
 * A buffer is defined to contain a number of equally sized blocks. Each block
 * has a part of it included in the region [<start>-<end>). That is
 * <end>-<start> bytes. Each block is <size> bytes long. Total number of bytes
 * in the region is (<end> - <start>) * <count>. First byte of the region is
 * <offset> + <start> bytes into the buffer.
 *
 * Here's an example of a region in a graphics buffer (X = buffer, R = region):
 *
 * XXXXXXXXXXXXXXXXXXXX \
 * XXXXXXXXXXXXXXXXXXXX |-- offset = 60
 * XXXXXXXXXXXXXXXXXXXX /
 * XXRRRRRRRRXXXXXXXXXX \
 * XXRRRRRRRRXXXXXXXXXX |-- count = 4
 * XXRRRRRRRRXXXXXXXXXX |
 * XXRRRRRRRRXXXXXXXXXX /
 * XXXXXXXXXXXXXXXXXXXX
 * --| start = 2
 * ----------| end = 10
 * --------------------| size = 20
 */
struct hwmem_region {
	/**
	 * @brief The first block's offset from beginning of buffer.
	 */
	uint32_t offset;
	/**
	 * @brief The number of blocks included in this region.
	 */
	uint32_t count;
	/**
	 * @brief The index of the first byte included in this block.
	 */
	uint32_t start;
	/**
	 * @brief The index of the last byte included in this block plus one.
	 */
	uint32_t end;
	/**
	 * @brief The size in bytes of each block.
	 */
	uint32_t size;
};

/* User space API */

/**
 * @brief Alloc request data.
 */
struct hwmem_alloc_request {
	/**
	 * @brief [in] Size of requested allocation in bytes. Size will be
	 * aligned to PAGE_SIZE bytes.
	 */
	uint32_t size;
	/**
	 * @brief [in] Flags describing requested allocation options.
	 */
	uint32_t flags; /* enum hwmem_alloc_flags */
	/**
	 * @brief [in] Default access rights for buffer.
	 */
	uint32_t default_access; /* enum hwmem_access */
	/**
	 * @brief [in] Memory type of the buffer.
	 */
	uint32_t mem_type; /* enum hwmem_mem_type */
};

/**
 * @brief Set domain request data.
 */
struct hwmem_set_domain_request {
	/**
	 * @brief [in] Identifier of buffer to be prepared. If 0 is specified
	 * the buffer associated with the current file instance will be used.
	 */
	int32_t id;
	/**
	 * @brief [in] Value specifying the new memory domain.
	 */
	uint32_t domain; /* enum hwmem_domain */
	/**
	 * @brief [in] Flags specifying access mode of the operation.
	 *
	 * One of HWMEM_ACCESS_READ and HWMEM_ACCESS_WRITE is required.
	 * For details, @see enum hwmem_access.
	 */
	uint32_t access; /* enum hwmem_access */
	/**
	 * @brief [in] The region of bytes to be prepared.
	 *
	 * For details, @see struct hwmem_region.
	 */
	struct hwmem_region region;
};

/**
 * @brief Pin request data.
 */
struct hwmem_pin_request {
	/**
	 * @brief [in] Identifier of buffer to be pinned. If 0 is specified,
	 * the buffer associated with the current file instance will be used.
	 */
	int32_t id;
	/**
	 * @brief [out] Physical address of first word in buffer.
	 */
	uint32_t phys_addr;
	/**
	 * @brief [in] Pointer to buffer for physical addresses of pinned
	 * scattered buffer. Buffer must be (buffer_size / page_size) *
	 * sizeof(uint32_t) bytes.
	 * This field can be NULL for physically contiguos buffers.
	 */
	uint32_t *scattered_addrs;
};

/**
 * @brief Set access rights request data.
 */
struct hwmem_set_access_request {
	/**
	 * @brief [in] Identifier of buffer to be pinned. If 0 is specified,
	 * the buffer associated with the current file instance will be used.
	 */
	int32_t id;
	/**
	 * @param access Access value indicating what is allowed.
	 */
	uint32_t access; /* enum hwmem_access */
	/**
	 * @param pid Process ID to set rights for.
	 */
	pid_t pid;
};

/**
 * @brief Get info request data.
 */
struct hwmem_get_info_request {
	/**
	 * @brief [in] Identifier of buffer to get info about. If 0 is specified,
	 * the buffer associated with the current file instance will be used.
	 */
	int32_t id;
	/**
	 * @brief [out] Size in bytes of buffer.
	 */
	uint32_t size;
	/**
	 * @brief [out] Memory type of buffer.
	 */
	uint32_t mem_type; /* enum hwmem_mem_type */
	/**
	 * @brief [out] Access rights for buffer.
	 */
	uint32_t access; /* enum hwmem_access */
};

/**
 * @brief Allocates <size> number of bytes and returns a buffer identifier.
 *
 * Input is a pointer to a hwmem_alloc_request struct.
 *
 * @return A buffer identifier on success, or a negative error code.
 */
#define HWMEM_ALLOC_IOC _IOW('W', 1, struct hwmem_alloc_request)

/**
 * @brief Allocates <size> number of bytes and associates the created buffer
 * with the current file instance.
 *
 * If the current file instance is already associated with a buffer the call
 * will fail. Buffers referenced through files instances shall not be released
 * with HWMEM_RELEASE_IOC, instead the file instance shall be closed.
 *
 * Input is a pointer to a hwmem_alloc_request struct.
 *
 * @return Zero on success, or a negative error code.
 */
#define HWMEM_ALLOC_FD_IOC _IOW('W', 2, struct hwmem_alloc_request)

/**
 * @brief Releases buffer.
 *
 * Buffers are reference counted and will not be destroyed until the last
 * reference is released. Bufferes allocated with ALLOC_FD_IOC not allowed.
 *
 * Input is the buffer identifier.
 *
 * @return Zero on success, or a negative error code.
 */
#define HWMEM_RELEASE_IOC _IO('W', 3)

/**
 * @brief Set the buffer's memory domain and prepares it for access.
 *
 * Input is a pointer to a hwmem_set_domain_request struct.
 *
 * @return Zero on success, or a negative error code.
 */
#define HWMEM_SET_DOMAIN_IOC _IOR('W', 4, struct hwmem_set_domain_request)

/**
 * @brief Pins the buffer and returns the physical address of the buffer.
 *
 * @return Zero on success, or a negative error code.
 */
#define HWMEM_PIN_IOC _IOWR('W', 5, struct hwmem_pin_request)

/**
 * @brief Unpins the buffer.
 *
 * @return Zero on success, or a negative error code.
 */
#define HWMEM_UNPIN_IOC _IO('W', 6)

/**
 * @brief Set access rights for buffer.
 *
 * @return Zero on success, or a negative error code.
 */
#define HWMEM_SET_ACCESS_IOC _IOW('W', 7, struct hwmem_set_access_request)

/**
 * @brief Get buffer information.
 *
 * Input is the buffer identifier. If 0 is specified the buffer associated
 * with the current file instance will be used.
 *
 * @return Zero on success, or a negative error code.
 */
#define HWMEM_GET_INFO_IOC _IOWR('W', 8, struct hwmem_get_info_request)

/**
 * @brief Export the buffer identifier for use in another process.
 *
 * The global name will not increase the buffers reference count and will
 * therefore not keep the buffer alive.
 *
 * Input is the buffer identifier. If 0 is specified the buffer associated with
 * the current file instance will be exported.
 *
 * @return A global buffer name on success, or a negative error code.
 */
#define HWMEM_EXPORT_IOC _IO('W', 9)

/**
 * @brief Import a buffer to allow local access to the buffer.
 *
 * Input is the buffer's global name.
 *
 * @return The imported buffer's identifier on success, or a negative error code.
 */
#define HWMEM_IMPORT_IOC _IO('W', 10)

/**
 * @brief Import a buffer to allow local access to the buffer using fd.
 *
 * Input is the buffer's global name.
 *
 * @return Zero on success, or a negative error code.
 */
#define HWMEM_IMPORT_FD_IOC _IO('W', 11)

#ifdef __KERNEL__

/* Kernel API */

struct hwmem_alloc;

/**
 * @brief Allocates <size> number of bytes.
 *
 * @param size Number of bytes to allocate. All allocations are page aligned.
 * @param flags Allocation options.
 * @param def_access Default buffer access rights.
 * @param mem_type Memory type.
 *
 * @return Pointer to allocation, or a negative error code.
 */
struct hwmem_alloc *hwmem_alloc(u32 size, enum hwmem_alloc_flags flags,
		enum hwmem_access def_access, enum hwmem_mem_type mem_type);

/**
 * @brief Release a previously allocated buffer.
 * When last reference is released, the buffer will be freed.
 *
 * @param alloc Buffer to be released.
 */
void hwmem_release(struct hwmem_alloc *alloc);

/**
 * @brief Set the buffer domain and prepare it for access.
 *
 * @param alloc Buffer to be prepared.
 * @param access Flags defining memory access mode of the call.
 * @param domain Value specifying the memory domain.
 * @param region Structure defining the minimum area of the buffer to be
 * prepared.
 *
 * @return Zero on success, or a negative error code.
 */
int hwmem_set_domain(struct hwmem_alloc *alloc, enum hwmem_access access,
		enum hwmem_domain domain, struct hwmem_region *region);

/**
 * @brief Pins the buffer.
 *
 * @param alloc Buffer to be pinned.
 * @param phys_addr Reference to variable to receive physical address.
 * @param scattered_phys_addrs Pointer to buffer to receive physical addresses
 * of all pages in the scattered buffer. Can be NULL if buffer is contigous.
 * Buffer size must be (buffer_size / page_size) * sizeof(uint32_t) bytes.
 */
int hwmem_pin(struct hwmem_alloc *alloc, uint32_t *phys_addr,
					uint32_t *scattered_phys_addrs);

/**
 * @brief Unpins the buffer.
 *
 * @param alloc Buffer to be unpinned.
 */
void hwmem_unpin(struct hwmem_alloc *alloc);

/**
 * @brief Map the buffer to user space.
 *
 * @param alloc Buffer to be unpinned.
 */
int hwmem_mmap(struct hwmem_alloc *alloc, struct vm_area_struct *vma);

/**
 * @brief Map the buffer for use in the kernel.
 *
 * This function implicitly pins the buffer.
 *
 * @param alloc Buffer to be mapped.
 *
 * @return Pointer to buffer, or a negative error code.
 */
void *hwmem_kmap(struct hwmem_alloc *alloc);

/**
 * @brief Un-map a buffer previously mapped with hwmem_kmap.
 *
 * This function implicitly unpins the buffer.
 *
 * @param alloc Buffer to be un-mapped.
 */
void hwmem_kunmap(struct hwmem_alloc *alloc);

/**
 * @brief Set access rights for buffer.
 *
 * @param alloc Buffer to set rights for.
 * @param access Access value indicating what is allowed.
 * @param pid Process ID to set rights for.
 */
int hwmem_set_access(struct hwmem_alloc *alloc, enum hwmem_access access,
								pid_t pid);

/**
 * @brief Get buffer information.
 *
 * @param alloc Buffer to get information about.
 * @param size Pointer to size output variable.
 * @param size Pointer to memory type output variable.
 * @param size Pointer to access rights output variable.
 */
void hwmem_get_info(struct hwmem_alloc *alloc, uint32_t *size,
	enum hwmem_mem_type *mem_type, enum hwmem_access *access);

/**
 * @brief Allocate a global buffer name.
 * Generated buffer name is valid in all processes. Consecutive calls will get
 * the same name for the same buffer.
 *
 * @param alloc Buffer to be made public.
 *
 * @return Positive global name on success, or a negative error code.
 */
int hwmem_get_name(struct hwmem_alloc *alloc);

/**
 * @brief Import the global buffer name to allow local access to the buffer.
 * This call will add a buffer reference. Resulting buffer should be
 * released with a call to hwmem_release.
 *
 * @param name A valid global buffer name.
 *
 * @return Pointer to allocation, or a negative error code.
 */
struct hwmem_alloc *hwmem_resolve_by_name(s32 name);

/* Internal */

struct hwmem_platform_data {
	/* Starting physical address of memory region */
	unsigned long start;
	/* Size of memory region */
	unsigned long size;
};

#endif

#endif /* _HWMEM_H_ */

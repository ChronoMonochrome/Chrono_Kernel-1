/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * ST-Ericsson B2R2 internal definitions
 *
 * Author: Robert Fekete <robert.fekete@stericsson.com>
 * Author: Paul Wannback
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */

#ifndef _LINUX_DRIVERS_VIDEO_B2R2_INTERNAL_H_
#define _LINUX_DRIVERS_VIDEO_B2R2_INTERNAL_H_


#include <video/b2r2_blt.h>

#include "b2r2_core.h"
#include "b2r2_global.h"

#include "b2r2_hw.h"

/* The maximum possible number of temporary buffers needed */
#define MAX_TMP_BUFS_NEEDED 2

/* Size of the color look-up table */
#define CLUT_SIZE 1024

/**
 * b2r2_blt_device() - Returns the device associated with B2R2 BLT.
 *                     Mainly for debugging with dev_... functions.
 *
 * Returns the device pointer or NULL
 */
struct device *b2r2_blt_device(void);

/**
 * struct b2r2_blt_instance - Represents the B2R2 instance (one per open)
 *
 * @lock: Lock to protect the instance
 *
 * @report_list: Ready requests that should be reported,
 * @report_list_waitq: Wait queue for report list
 * @no_of_active_requests: Number of requests added but not reported
 *                         in callback.
 * @synching: true if any client is waiting for b2r2_blt_synch(0)
 * @synch_done_waitq: Wait queue to handle synching on request_id 0
 */
struct b2r2_blt_instance {
	struct mutex lock;

	/* Requests to be reported */
	struct list_head report_list;
	wait_queue_head_t report_list_waitq;

	/* Below for synching */
	u32 no_of_active_requests;
	bool synching;
	wait_queue_head_t synch_done_waitq;
};

/**
 * struct b2r2_node - Represents a B2R2 node with reqister values, executed
 *                    by B2R2. Should be allocated non-cached.
 *
 * @next: Next node
 * @physical_address: Physical address to be given to B2R2
 *                    (physical address of "node" member below)
 * @node: The B2R2 node with register settings. This is the data
 *        that B2R2 will use.
 *
 */
struct b2r2_node {
	struct b2r2_node *next;
	u32 physical_address;

	int src_tmp_index;
	int dst_tmp_index;

	int src_index;

	/* B2R2 regs comes here */
	struct b2r2_link_list node;
};

/**
 * struct b2r2_resolved_buf - Contains calculated information about
 *                            image buffers.
 *
 * @physical_address: Physical address of the buffer
 * @virtual_address: Virtual address of the buffer
 * @is_pmem: true if buffer is from pmem
 * @hwmem_session: Hwmem session
 * @hwmem_alloc: Hwmem alloc
 * @filep: File pointer of mapped file (like pmem device, frame buffer device)
 * @file_physical_start: Physical address of file start
 * @file_virtual_start: Virtual address of file start
 * @file_len: File len
 *
 */
struct b2r2_resolved_buf {
	u32                   physical_address;
	void                 *virtual_address;
	bool                  is_pmem;
	struct hwmem_alloc   *hwmem_alloc;
	/* Data for validation below */
	struct file          *filep;
	u32                   file_physical_start;
	u32                   file_virtual_start;
	u32                   file_len;
};


/**
 * b2r2_work_buf - specification for a temporary work buffer
 *
 * @size      - the size of the buffer (set by b2r2_node_split)
 * @phys_addr - the physical address of the buffer (set by b2r2_blt_main)
 */
struct b2r2_work_buf {
	u32 size;
	u32 phys_addr;
	void *virt_addr;
	u32 mem_handle;
};


/**
 * b2r2_op_type - the type of B2R2 operation to configure
 */
enum b2r2_op_type {
	B2R2_DIRECT_COPY,
	B2R2_DIRECT_FILL,
	B2R2_COPY,
	B2R2_FILL,
	B2R2_SCALE,
	B2R2_ROTATE,
	B2R2_SCALE_AND_ROTATE,
	B2R2_FLIP,
};

/**
 * b2r2_fmt_type - the type of buffer for a given format
 */
enum b2r2_fmt_type {
	B2R2_FMT_TYPE_RASTER,
	B2R2_FMT_TYPE_SEMI_PLANAR,
	B2R2_FMT_TYPE_PLANAR,
};

/**
 * b2r2_fmt_conv - the type of format conversion to do
 */
enum b2r2_fmt_conv {
	B2R2_FMT_CONV_NONE,
	B2R2_FMT_CONV_RGB_TO_YUV,
	B2R2_FMT_CONV_YUV_TO_RGB,
	B2R2_FMT_CONV_YUV_TO_YUV,
	B2R2_FMT_CONV_RGB_TO_BGR,
	B2R2_FMT_CONV_BGR_TO_RGB,
	B2R2_FMT_CONV_YUV_TO_BGR,
	B2R2_FMT_CONV_BGR_TO_YUV,
};

/**
 * b2r2_node_split_buf - information about a source or destination buffer
 *
 * @addr            - the physical base address
 * @chroma_addr     - the physical address of the chroma plane
 * @chroma_cr_addr  - the physical address of the Cr chroma plane
 * @fmt             - the buffer format
 * @fmt_type        - the buffer format type
 * @rect            - the rectangle of the buffer to use
 * @color           - the color value to use is case of a fill operation
 * @pitch           - the pixmap byte pitch
 * @height          - the pixmap height
 * @alpha_range     - the alpha range of the buffer (0-128 or 0-255)
 * @hso             - the horizontal scan order
 * @vso             - the vertical scan order
 * @endian          - the endianess of the buffer
 * @plane_selection - the plane to write if buffer is planar or semi-planar
 */
struct b2r2_node_split_buf {
	u32 addr;
	u32 chroma_addr;
	u32 chroma_cr_addr;

	enum b2r2_blt_fmt fmt;
	enum b2r2_fmt_type type;

	struct b2r2_blt_rect rect;
	struct b2r2_blt_rect win;

	s32 dx;
	s32 dy;

	u32 color;
	u16 pitch;
	u16 width;
	u16 height;

	enum b2r2_ty alpha_range;
	enum b2r2_ty hso;
	enum b2r2_ty vso;
	enum b2r2_ty endian;
	enum b2r2_tty dither;

	/* Plane selection (used when writing to a multibuffer format) */
	enum b2r2_tty plane_selection;

	/* Chroma plane selection (used when writing planar formats) */
	enum b2r2_tty chroma_selection;

	int tmp_buf_index;
};

/**
 * b2r2_node_split_job - an instance of a node split job
 *
 * @type          - the type of operation
 * @ivmx          - the ivmx matrix to use for color conversion
 * @blend         - determines if blending is enabled
 * @clip          - determines if destination clipping is enabled
 * @swap_fg_bg    - determines if FG and BG should be swapped when blending
 * @flags         - the flags passed in the blt request
 * @flag_param    - parameter required by certain flags,
 *                  e.g. color for source color keying.
 * @transform     - the transforms passed in the blt request
 * @global_alpha  - the global alpha
 * @clip_rect     - the clipping rectangle to use
 * @horiz_rescale - determmines if horizontal rescaling is enabled
 * @horiz_sf      - the horizontal scale factor
 * @vert_rescale  - determines if vertical rescale is enabled
 * @vert_sf       - the vertical scale factor
 * @src           - the incoming source buffer
 * @dst           - the outgoing destination buffer
 * @work_bufs     - work buffer specifications
 * @tmp_bufs      - temporary buffers
 * @buf_count     - the number of temporary buffers used for the job
 * @node_count    - the number of nodes used for the job
 * @max_buf_size  - the maximum size of temporary buffers
 * @nbr_rows      - the number of tile rows in the blit operation
 * @nbr_cols      - the number of time columns in the blit operation
 */
struct b2r2_node_split_job {
	enum b2r2_op_type type;

	const u32 *ivmx;

	bool blend;
	bool clip;
	bool rotation;

	bool swap_fg_bg;

	u32 flags;
	u32 flag_param;
	u32 transform;
	u32 global_alpha;

	struct b2r2_blt_rect clip_rect;

	bool h_rescale;
	u16 h_rsf;

	bool v_rescale;
	u16 v_rsf;

	struct b2r2_node_split_buf src;
	struct b2r2_node_split_buf dst;

	struct b2r2_work_buf work_bufs[MAX_TMP_BUFS_NEEDED];
	struct b2r2_node_split_buf tmp_bufs[MAX_TMP_BUFS_NEEDED];

	u32 buf_count;
	u32 node_count;
	u32 max_buf_size;
};

/**
 * struct b2r2_blt_request - Represents one B2R2 blit request
 *
 * @instance: Back pointer to the instance structure
 * @list: List item to keep track of requests per instance
 * @user_req: The request received from userspace
 * @job: The administration structure for the B2R2 job,
 *       consisting of one or more nodes
 * @node_split_job: The administration structure for the B2R2 node split job
 * @first_node: Pointer to the first B2R2 node
 * @request_id: Request id for this job
 * @node_split_handle: Handle of the node split
 * @src_resolved: Calculated info about the source buffer
 * @src_mask_resolved: Calculated info about the source mask buffer
 * @dst_resolved: Calculated info about the destination buffer
 * @profile: True if the blit shall be profiled, false otherwise
 */
struct b2r2_blt_request {
	struct b2r2_blt_instance   *instance;
	struct list_head           list;
	struct b2r2_blt_req        user_req;
	struct b2r2_core_job       job;
	struct b2r2_node_split_job node_split_job;
	struct b2r2_node           *first_node;
	int                        request_id;

	/* Resolved buffer addresses */
	struct b2r2_resolved_buf src_resolved;
	struct b2r2_resolved_buf src_mask_resolved;
	struct b2r2_resolved_buf dst_resolved;

	/* TBD: Info about SRAM usage & needs */
	struct b2r2_work_buf *bufs;
	u32 buf_count;

	/* color look-up table */
	void *clut;
	u32 clut_phys_addr;

	/* Profiling stuff */
	bool profile;

	s32 nsec_active_in_cpu;

	u32 start_time_nsec;
	s32 total_time_nsec;
};

/* FIXME: The functions below should be removed when we are
   switching to the new Robert Lind allocator */

/**
 * b2r2_blt_alloc_nodes() - Allocate nodes
 *
 * @node_count: Number of nodes to allocate
 *
 * Return:
 *   Returns a pointer to the first node in the node list.
 */
struct b2r2_node *b2r2_blt_alloc_nodes(int node_count);

/**
 * b2r2_blt_free_nodes() - Release nodes previously allocated via
 *                         b2r2_generate_nodes
 *
 * @first_node: First node in linked list of nodes
 */
void b2r2_blt_free_nodes(struct b2r2_node *first_node);

/**
 * b2r2_blt_module_init() - Initialize the B2R2 blt module
 */
int b2r2_blt_module_init(void);

/**
 * b2r2_blt_module_exit() - Un-initialize the B2R2 blt module
 */
void b2r2_blt_module_exit(void);

#endif

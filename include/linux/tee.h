/*
 * Trusted Execution Environment (TEE) interface for TrustZone enabled ARM CPUs.
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Shujuan Chen <shujuan.chen@stericsson.com>
 * Author: Martin Hovang <martin.xm.hovang@stericsson.com
 * Author: Joakim Bech <joakim.xx.bech@stericsson.com>
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef TEE_H
#define TEE_H

/* tee_cmd id values */
#define TEED_OPEN_SESSION    0x00000000U
#define TEED_CLOSE_SESSION   0x00000001U
#define TEED_INVOKE          0x00000002U

/* tee_retval id values */
#define TEED_SUCCESS                0x00000000U
#define TEED_ERROR_GENERIC          0xFFFF0000U
#define TEED_ERROR_ACCESS_DENIED    0xFFFF0001U
#define TEED_ERROR_CANCEL           0xFFFF0002U
#define TEED_ERROR_ACCESS_CONFLICT  0xFFFF0003U
#define TEED_ERROR_EXCESS_DATA      0xFFFF0004U
#define TEED_ERROR_BAD_FORMAT       0xFFFF0005U
#define TEED_ERROR_BAD_PARAMETERS   0xFFFF0006U
#define TEED_ERROR_BAD_STATE        0xFFFF0007U
#define TEED_ERROR_ITEM_NOT_FOUND   0xFFFF0008U
#define TEED_ERROR_NOT_IMPLEMENTED  0xFFFF0009U
#define TEED_ERROR_NOT_SUPPORTED    0xFFFF000AU
#define TEED_ERROR_NO_DATA          0xFFFF000BU
#define TEED_ERROR_OUT_OF_MEMORY    0xFFFF000CU
#define TEED_ERROR_BUSY             0xFFFF000DU
#define TEED_ERROR_COMMUNICATION    0xFFFF000EU
#define TEED_ERROR_SECURITY         0xFFFF000FU
#define TEED_ERROR_SHORT_BUFFER     0xFFFF0010U

/* TEE origin codes */
#define TEED_ORIGIN_DRIVER          0x00000002U
#define TEED_ORIGIN_TEE             0x00000003U
#define TEED_ORIGIN_TEE_APPLICATION 0x00000004U

#define TEE_UUID_CLOCK_SIZE 8

#define TEEC_CONFIG_PAYLOAD_REF_COUNT 4

/**
 * struct tee_uuid - Structure that represent an uuid.
 * @timeLow: The low field of the time stamp.
 * @timeMid: The middle field of the time stamp.
 * @timeHiAndVersion: The high field of the timestamp multiplexed
 *                    with the version number.
 * @clockSeqAndNode: The clock sequence and the node.
 *
 * This structure have different naming (camel case) to comply with Global
 * Platforms TEE Client API spec. This type is defined in RFC4122.
 */
struct tee_uuid {
	uint32_t timeLow;
	uint16_t timeMid;
	uint16_t timeHiAndVersion;
	uint8_t clockSeqAndNode[TEE_UUID_CLOCK_SIZE];
};

/**
 * struct tee_sharedmemory - Shared memory block for TEE.
 * @buffer: The in/out data to TEE.
 * @size: The size of the data.
 * @flags: Variable telling whether it is a in, out or in/out parameter.
 */
struct tee_sharedmemory {
	void *buffer;
	size_t size;
	uint32_t flags;
};

/**
 * struct tee_operation - Payload for sessions or invoke operation.
 * @shm: Array containing the shared memory buffers.
 * @flags: Tells which if memory buffers that are in use.
 */
struct tee_operation {
	struct tee_sharedmemory shm[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	uint32_t flags;
};

/**
 * struct tee_session - The session of an open tee device.
 * @state: The current state in the linux kernel.
 * @err: Error code (as in Global Platform TEE Client API spec)
 * @origin: Origin for the error code (also from spec).
 * @id: Implementation defined type, 0 if not used.
 * @ta: The trusted application.
 * @uuid: The uuid for the trusted application.
 * @cmd: The command to be executed in the trusted application.
 * @driver_cmd: The command type in the driver. This is used from a client (user
 *              space to tell the Linux kernel whether it's a open-,
 *              close-session or if it is an invoke command.
 * @ta_size: The size of the trusted application.
 * @op: The payload for the trusted application.
 * @sync: Mutex to handle multiple use of clients.
 *
 * This structure is mainly used in the Linux kernel as a session context for
 * ongoing operations. Other than that it is also used in the communication with
 * the user space.
 */
struct tee_session {
	uint32_t state;
	uint32_t err;
	uint32_t origin;
	uint32_t id;
	void *ta;
	struct tee_uuid *uuid;
	unsigned int cmd;
	unsigned int driver_cmd;
	unsigned int ta_size;
	struct tee_operation *op;
	struct mutex *sync;
};

/**
 * struct tee_read - Contains the error message and the origin.
 * @err: Error code (as in Global Platform TEE Client API spec)
 * @origin: Origin for the error code (also from spec).
 *
 * This is used by user space when a user space application wants to get more
 * information about an error.
 */
struct tee_read {
	unsigned int err; /* return value */
	unsigned int origin; /* error origin */
};

/**
 * Function that handles the function calls to trusted applications.
 * @param ts: The session of a operation to be executed.
 * @param sec_cmd: The type of command to be executed, open-, close-session,
 *                 invoke command.
 */
int call_sec_world(struct tee_session *ts, int sec_cmd);

#endif

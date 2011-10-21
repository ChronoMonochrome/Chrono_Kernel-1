/*
 * Copyright (C) ST-Ericsson SA 2010. All rights reserved.
 * This code is ST-Ericsson proprietary and confidential.
 * Any use of the code for whatever purpose is subject to
 * specific written permission of ST-Ericsson SA.
 */
 
#ifndef HOST_EE_TYPE_H
#define HOST_EE_TYPE_H

/*!
 * \brief Notify callback method type.
 *
 * \ingroup HOSTEE
 */
typedef void (*t_nmf_notify)(void *contextHandler);

/*!
 * \brief Definition of the command ID type
 */
typedef t_uint32 t_ee_cmd_id;

/*!
 * \brief Definition of the command ID
 */
typedef enum {
    EE_CMD_TRACE_ON,                //!< Enable tracing and force network resetting and dumping
    EE_CMD_TRACE_OFF                //!< Disable tracing
} t_ee_cmd_idDescription;


#endif

/*
 * Copyright (C) ST-Ericsson SA 2010. All rights reserved.
 * This code is ST-Ericsson proprietary and confidential.
 * Any use of the code for whatever purpose is subject to
 * specific written permission of ST-Ericsson SA.
 */
 
/*!
 * \defgroup HOSTEE_MODULE Host Execution Engine
 */

/*!
 * \defgroup HOSTEE Host Execution Engine API
 * \ingroup HOSTEE_MODULE
 */

#ifndef HOST_EE_H
#define HOST_EE_H


#include <inc/typedef.h>

#include <nmf/inc/channel_type.h>
#include <nmf/inc/service_type.h>

#include <ee/api/ee_type.h>

/*!
 * \brief Get the version of the NMF Host EE at runtime
 *
 * This method should be used to query the version number  of  the
 * NMF Component Manager engine at runtime. This is useful when using
 * to check if version of the engine linked with application correspond
 * to engine used for development.
 *
 * Such code can be used to check compatibility: \code
    t_uint32 nmfversion;

    // Print NMF version
    EE_GetVersion(&nmfversion);
    LOG("NMF Version %d-%d-%d\n",
            VERSION_MAJOR(nmfversion),
            VERSION_MINOR(nmfversion),
            VERSION_PATCH(nmfversion));
    if(NMF_VERSION != nmfversion) {
        LOG("Error: Incompatible API version %d != %d\n", NMF_VERSION, nmfversion);
        EXIT();
    }
 * \endcode
 *
 * \param[out] version Internal hardcoded version (use \ref VERSION_MAJOR, \ref  VERSION_MINOR, \ref VERSION_PATCH  macros to decode it).
 *
 * \ingroup HOSTEE
 */
PUBLIC IMPORT_SHARED void EE_GetVersion(t_uint32 *version);

/*!
 * \brief Set the mode of the Host EE.
 *
 * According the (\ref t_ee_cmd_id) value, this routine allows to modify dynamically the behavior of the Host EE.
 *
 * \param[in] aCmdID Command ID.
 * \param[in] aParam Parameter of command ID if required.
 *
 * \ingroup HOSTEE
 */
PUBLIC IMPORT_SHARED t_nmf_error EE_SetMode(t_ee_cmd_id aCmdID, t_sint32 aParam);

/*!
 * \brief Create a channel for communication between host ee and user.
 *
 * The purpose of the function is to:
 * - create a channel or get a channel, regarding the flag parameter
 *
 * \param[in]  flags   Whether the caller want to create a new channel (CHANNEL_PRIVATE)
 *                     or use the shared one (CHANNEL_PRIVATE) (it will be created
 *                     if it does not yet exist)
 * \param[out] channel Channel number.
 *
 * \exception NMF_NO_MORE_MEMORY Not enough memory to create the callback Channel.
 * \exception NMF_INVALID_PARAMETER The specified flags is invalid.
 *
 * \ingroup HOSTEE
 */
PUBLIC IMPORT_SHARED t_nmf_error EE_CreateChannel(t_nmf_channel_flag flags, t_nmf_channel *channel);

/*!
 * \brief Flush a channel to allow user to safely close it.
 *
 * The purpose of the function is to allow safe call of EE_CloseChannel() later on. Calling
 * EE_FlushChannel() will allow a blocking call to EE_GetMessage() to exit with an error
 * NMF_FLUSH_MESSAGE. After EE_GetMessage() has exit with such a value user must no more
 * call EE_GetMessage() and can safely call EE_CloseChannel() that will destroy channel.
 * In case of the share channel EE_FlushChannel() will return false for isFlushMessageGenerated if
 * it's internal reference counter is not zero, in that case no NMF_FLUSH_MESSAGE error is return 
 * by EE_GetMessage() and user can immediatly call EE_CloseChannel().
 * In case user know that no usage of channel is done when he want to destroy channel, call to this api
 * is optionnal and user can safely call EE_CloseChannel().
 *
 * \param[in]  channel                  Channel number
 * \param[out] isFlushMessageGenerated  Allow user to know if it must wait for NMF_FLUSH_MESSAGE return
 *                                      of EE_GetMessage() before calling EE_CloseChannel()
 *
 * \exception NMF_INVALID_PARAMETER The specified flags is invalid.
 *
 * \ingroup HOSTEE
 */
PUBLIC IMPORT_SHARED t_nmf_error EE_FlushChannel(t_nmf_channel channel, t_bool *isFlushMessageGenerated);

/*!
 * \brief Unregister channel
 *
 * The purpose of the function is to:
 * - destroy a channel from the user to the Host ee.
 *
 * The user must call EE_UserDone() as many time as EE_UserInit().
 * At the last EE_UserDone() call, the channel is closed and definitely destroyed
 * All service callback must be unregistered first.
 *
 * \param[in] channel    Channel number:
 *
 * \ingroup HOSTEE
 */
PUBLIC IMPORT_SHARED t_nmf_error EE_CloseChannel(t_nmf_channel channel);

/*!
 * \brief Register a service callback to this channel.
 *
 * \param[in] channel           The channel on which the callback must be registered.
 * \param[in] handler           The given callback.
 * \param[in] contextHandler    The context associated with this callback.
 *
 * \exception NMF_NO_MORE_MEMORY Not enough memory to associate service with the Channel.
 *
 * \ingroup HOSTEE
 */
PUBLIC IMPORT_SHARED t_nmf_error EE_RegisterService(t_nmf_channel channel, t_nmf_serviceCallback handler, void *contextHandler);

/*!
 * \brief Unregister a service callback from this channel.
 *
 * \param[in] channel           The channel on which the callback must be registered.
 * \param[in] handler           The given callback.
 * \param[in] contextHandler    The context associated with this callback.
 *
 * \exception NMF_INVALID_PARAMETER The channel or the callback doesn't exist.
 *
 * \ingroup HOSTEE
 */
PUBLIC IMPORT_SHARED t_nmf_error EE_UnregisterService(t_nmf_channel channel, t_nmf_serviceCallback handler, void *contextHandler);

/*!
 * \brief Unregister a notify callback for this channel.
 *
 * This method will register a callback that will be call each time a message has
 * been push in queue of the channel.
 * To unregister your callback just register a null notify.
 *
 * \param[in] channel           The channel on which the callback must be registered.
 * \param[in] notify            The given callback.
 * \param[in] contextHandler    The context associated with this callback.
 *
 * \ingroup HOSTEE
 */
PUBLIC IMPORT_SHARED t_nmf_error EE_RegisterNotify(t_nmf_channel channel, t_nmf_notify notify, void *contextHandler);

/*!
 * \brief Get received message from specified callback channel.
 *
 * This method can be used to retrieve callback message from Host ee. Returned message could then
 * be dispatch through EE_ExecuteMessage.
 *
 * \param[in]   channel         The channel from which the message must be retrieved
 * \param[out]  clientContext   client context.
 * \param[out]  message         Reference on buffer to be unmarshalled. The buffer is allocated internally.
 * \param[in]   blockable       Indicate if the call could blocked or not.
 *
 * \exception NMF_NO_MESSAGE            No waited message.
 * \exception NMF_INVALID_PARAMETER     At least one input parameters is invalid
 *
 * \ingroup HOSTEE
 */
PUBLIC IMPORT_SHARED t_nmf_error EE_GetMessage(t_nmf_channel channel, void **clientContext, char **message, t_bool blockable);

/*!
 * \brief Execute a message. User callback will be execute.
 *
 * This method allow the message retrieved through EE_GetMessage to the right user callback.
 *
 * \param[in]   itfref          Interface reference.
 * \param[in]   message         Reference on buffer to be unmarshalled.
 *
 * \ingroup HOSTEE
 */
PUBLIC IMPORT_SHARED void EE_ExecuteMessage(void *itfref, char *message);

#endif

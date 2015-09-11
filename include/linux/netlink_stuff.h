#ifndef _LINUX_NETLINKSTUFF_H
#define _LINUX_NETLINKSTUFF_H

#include <linux/skbuff.h>
#include <net/netlink.h>
#include <linux/netlink.h>

#define NETLINK_CB(skb)         (*(struct netlink_skb_parms*)&((skb)->cb))
#define NETLINK_CREDS(skb)     (&NETLINK_CB((skb)).creds)

/**
 * nlmsg_multicast - multicast a netlink message
 * @sk: netlink socket to spread messages to
 * @skb: netlink message as socket buffer
 * @pid: own netlink pid to avoid sending to yourself
 * @group: multicast group id
 * @flags: allocation flags
 */
static inline int nlmsg_multicast(struct sock *sk, struct sk_buff *skb,
                                  u32 pid, unsigned int group, gfp_t flags)
{
        int err;

        NETLINK_CB(skb).dst_group = group;

        err = netlink_broadcast(sk, skb, pid, group, flags);
        if (err > 0)
                err = 0;

        return err;
}

/**
 * nlmsg_put_answer - Add a new callback based netlink message to an skb
 * @skb: socket buffer to store message in
 * @cb: netlink callback
 * @type: message type
 * @payload: length of message payload
 * @flags: message flags
 *
 * Returns NULL if the tailroom of the skb is insufficient to store
 * the message header and payload.
 */
static inline struct nlmsghdr *nlmsg_put_answer(struct sk_buff *skb,
                                                struct netlink_callback *cb,
                                                int type, int payload,
                                                int flags)
{
        return nlmsg_put(skb, NETLINK_CB(cb->skb).pid, cb->nlh->nlmsg_seq,
                         type, payload, flags);
}



#endif

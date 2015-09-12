#ifndef _NET_GENETLINKSTUFF_H
#define _NET_GENETLINKSTUFF_H

#include <net/genetlink.h>

/**
 * genlmsg_multicast_netns - multicast a netlink message to a specific netns
 * @net: the net namespace
 * @skb: netlink message as socket buffer
 * @pid: own netlink pid to avoid sending to yourself
 * @group: multicast group id
 * @flags: allocation flags
 */
static inline int genlmsg_multicast_netns(struct net *net, struct sk_buff *skb,
                                          u32 pid, unsigned int group, gfp_t flags)
{
        return nlmsg_multicast(net->genl_sock, skb, pid, group, flags);
}

/*
 * genlmsg_multicast - multicast a netlink message to the default netns
 * @skb: netlink message as socket buffer
 * @pid: own netlink pid to avoid sending to yourself
 * @group: multicast group id
 * @flags: allocation flags
 */
static inline int genlmsg_multicast(struct sk_buff *skb, u32 pid,
                                    unsigned int group, gfp_t flags)
{
        return genlmsg_multicast_netns(&init_net, skb, pid, group, flags);
}

#endif

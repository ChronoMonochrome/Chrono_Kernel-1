#ifndef __BLUETOOTH_STUFF_H
#define __BLUETOOTH_STUFF_H

#include <net/bluetooth/bluetooth_mgmt.h>

/* Skb helpers */
struct bt_skb_cb {
        __u8 pkt_type;
        __u8 incoming;
        __u16 expect;
        __u8 tx_seq;
        __u8 retries;
        __u8 sar;
        unsigned short channel;
        __u8 force_active;
};
#define bt_cb(skb) ((struct bt_skb_cb *)((skb)->cb))

static inline struct sk_buff *bt_skb_alloc(unsigned int len, gfp_t how)
{
        struct sk_buff *skb;

        if ((skb = alloc_skb(len + BT_SKB_RESERVE, how))) {
                skb_reserve(skb, BT_SKB_RESERVE);
                bt_cb(skb)->incoming  = 0;
        }
        return skb;
}

static inline struct sk_buff *bt_skb_send_alloc(struct sock *sk,
                                        unsigned long len, int nb, int *err)
{
        struct sk_buff *skb;

        release_sock(sk);
        if ((skb = sock_alloc_send_skb(sk, len + BT_SKB_RESERVE, nb, err))) {
                skb_reserve(skb, BT_SKB_RESERVE);
                bt_cb(skb)->incoming  = 0;
        }
        lock_sock(sk);

        if (!skb && *err)
                return NULL;

        *err = sock_error(sk);
        if (*err)
                goto out;

        if (sk->sk_shutdown) {
                *err = -ECONNRESET;
                goto out;
        }

        return skb;

out:
        kfree_skb(skb);
        return NULL;
}


#endif

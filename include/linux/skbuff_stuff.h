#ifndef _LINUX_NAPISTUFF_H
#define _LINUX_NAPISTUFF_H

#include <linux/skbuff.h>

struct napi_gro_cb {
        /* Virtual address of skb_shinfo(skb)->frags[0].page + offset. */
        void *frag0;

        /* Length of frag0. */
        unsigned int frag0_len;

        /* This indicates where we are processing relative to skb->data. */
        int data_offset;

        /* This is non-zero if the packet may be of the same flow. */
        int same_flow;

        /* This is non-zero if the packet cannot be merged with the new skb. */
        int flush;

        /* Number of segments aggregated. */
        int count;

        /* Free the skb? */
        int free;
};

#define NAPI_GRO_CB(skb) ((struct napi_gro_cb *)(skb)->cb)

static inline unsigned int skb_gro_offset(const struct sk_buff *skb)
{
        return NAPI_GRO_CB(skb)->data_offset;
}

static inline unsigned int skb_gro_len(const struct sk_buff *skb)
{
        return skb->len - NAPI_GRO_CB(skb)->data_offset;
}

static inline void skb_gro_pull(struct sk_buff *skb, unsigned int len)
{
        NAPI_GRO_CB(skb)->data_offset += len;
}

static inline void *skb_gro_header_fast(struct sk_buff *skb,
                                        unsigned int offset)
{
        return NAPI_GRO_CB(skb)->frag0 + offset;
}

static inline int skb_gro_header_hard(struct sk_buff *skb, unsigned int hlen)
{
        return NAPI_GRO_CB(skb)->frag0_len < hlen;
}

static inline void *skb_gro_header_slow(struct sk_buff *skb, unsigned int hlen,
                                        unsigned int offset)
{
        if (!pskb_may_pull(skb, hlen))
                return NULL;

        NAPI_GRO_CB(skb)->frag0 = NULL;
        NAPI_GRO_CB(skb)->frag0_len = 0;
        return skb->data + offset;
}

static inline void *skb_gro_mac_header(struct sk_buff *skb)
{
        return NAPI_GRO_CB(skb)->frag0 ?: skb_mac_header(skb);
}

static inline void *skb_gro_network_header(struct sk_buff *skb)
{
        return (NAPI_GRO_CB(skb)->frag0 ?: skb->data) +
               skb_network_offset(skb);
}

#endif

#ifndef _TCP_STUFF_H
#define _TCP_STUFF_H

#include <net/tcp.h>

#define TCP_SKB_CB(__skb)       ((struct tcp_skb_cb *)&((__skb)->cb[0]))

static inline void tcp_minshall_update(struct tcp_sock *tp, unsigned int mss,
                                       const struct sk_buff *skb)
{
        if (skb->len < mss)
                tp->snd_sml = TCP_SKB_CB(skb)->end_seq;
}

extern int tcp_is_cwnd_limited(const struct sock *sk, u32 in_flight);

                                       const struct sk_buff *skb)
{
        if (skb->len < mss)
                tp->snd_sml = TCP_SKB_CB(skb)->end_seq;
}

static inline void tcp_minshall_update(struct tcp_sock *tp, unsigned int mss,
                                       const struct sk_buff *skb)
{
        if (skb->len < mss)
                tp->snd_sml = TCP_SKB_CB(skb)->end_seq;
}

static inline void tcp_openreq_init(struct request_sock *req,
                                    struct tcp_options_received *rx_opt,
                                    struct sk_buff *skb)
{
        struct inet_request_sock *ireq = inet_rsk(req);

        req->rcv_wnd = 0;               /* So that tcp_send_synack() knows! */
        req->cookie_ts = 0;
        tcp_rsk(req)->rcv_isn = TCP_SKB_CB(skb)->seq;
        req->mss = rx_opt->mss_clamp;
        req->ts_recent = rx_opt->saw_tstamp ? rx_opt->rcv_tsval : 0;
        ireq->tstamp_ok = rx_opt->tstamp_ok;
        ireq->sack_ok = rx_opt->sack_ok;
        ireq->snd_wscale = rx_opt->snd_wscale;
        ireq->wscale_ok = rx_opt->wscale_ok;
        ireq->acked = 0;
        ireq->ecn_ok = 0;
        ireq->rmt_port = tcp_hdr(skb)->source;
        ireq->loc_port = tcp_hdr(skb)->dest;
}

/* Start sequence of the highest skb with SACKed bit, valid only if
 * sacked > 0 or when the caller has ensured validity by itself.
 */
static inline u32 tcp_highest_sack_seq(struct tcp_sock *tp)
{
        if (!tp->sacked_out)
                return tp->snd_una;

        if (tp->highest_sack == NULL)
                return tp->snd_nxt;

        return TCP_SKB_CB(tp->highest_sack)->seq;
}

static inline void tcp_openreq_init(struct request_sock *req,
                                    struct tcp_options_received *rx_opt,
                                    struct sk_buff *skb)
{
        struct inet_request_sock *ireq = inet_rsk(req);

        req->rcv_wnd = 0;               /* So that tcp_send_synack() knows! */
        req->cookie_ts = 0;
        tcp_rsk(req)->rcv_isn = TCP_SKB_CB(skb)->seq;
        req->mss = rx_opt->mss_clamp;
        req->ts_recent = rx_opt->saw_tstamp ? rx_opt->rcv_tsval : 0;
        ireq->tstamp_ok = rx_opt->tstamp_ok;
        ireq->sack_ok = rx_opt->sack_ok;
        ireq->snd_wscale = rx_opt->snd_wscale;
        ireq->wscale_ok = rx_opt->wscale_ok;
        ireq->acked = 0;
        ireq->ecn_ok = 0;
        ireq->rmt_port = tcp_hdr(skb)->source;
        ireq->loc_port = tcp_hdr(skb)->dest;
}

/* Start sequence of the highest skb with SACKed bit, valid only if
 * sacked > 0 or when the caller has ensured validity by itself.
 */
static inline u32 tcp_highest_sack_seq(struct tcp_sock *tp)
{
        if (!tp->sacked_out)
                return tp->snd_una;

        if (tp->highest_sack == NULL)
                return tp->snd_nxt;

        return TCP_SKB_CB(tp->highest_sack)->seq;
}

#endif

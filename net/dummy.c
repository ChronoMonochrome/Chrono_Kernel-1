#if defined(CONFIG_IPV6_MODULE)
#include <linux/in6.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/notifier.h>
#include <net/udp.h>


struct sock *udp6_lib_lookup(struct net *net, const struct in6_addr *saddr, __be16 sport,
                                    const struct in6_addr *daddr, __be16 dport,
                                    int dif)
{
	return NULL;
}


int ipv6_find_hdr(const struct sk_buff *skb, unsigned int *offset,
                         int target, unsigned short *fragoff)
{
	return 0;
}

struct rt6_info          *rt6_lookup(struct net *net,
                                            const struct in6_addr *daddr,
                                            const struct in6_addr *saddr,
                                            int oif, int flags)
{
	return NULL;
}


void nf_defrag_ipv6_enable(void)
{
}

int register_inet6addr_notifier(struct notifier_block *nb)
{
	return 0;
}

const struct in6_addr in6addr_any;
#endif

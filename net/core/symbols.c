#include <linux/export.h>
#include <net/tcp.h>

struct sock;
struct xt_action_param;
struct sk_buff;
#include <linux/netfilter/xt_socket.h>

struct xt_match;
#include <linux/netfilter/x_tables.h>

MODEXPORT_SYMBOL(tcp_nuke_addr);
MODEXPORT_SYMBOL(xt_socket_put_sk);
MODEXPORT_SYMBOL(xt_socket_get4_sk);
MODEXPORT_SYMBOL(xt_socket_get6_sk);
MODEXPORT_SYMBOL(xt_register_match);

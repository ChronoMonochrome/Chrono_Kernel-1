#
# Makefile for the Linux networking core.
#

#CFLAGS_sock.o += -fno-strict-aliasing
#CFLAGS_skbuff.o += -fno-strict-aliasing
#CFLAGS_dev.o += -fno-strict-aliasing

KBUILD_CFLAGS += -fno-strict-aliasing

obj-y := sock.o request_sock.o skbuff.o iovec.o datagram.o stream.o scm.o \
	 gen_stats.o gen_estimator.o net_namespace.o secure_seq.o

obj-$(CONFIG_SYSCTL) += sysctl_net_core.o

obj-y		     += dev.o ethtool.o dev_addr_lists.o dst.o netevent.o \
			neighbour.o rtnetlink.o utils.o link_watch.o filter.o

obj-$(CONFIG_XFRM) += flow.o
obj-y += net-sysfs.o
obj-$(CONFIG_NET_PKTGEN) += pktgen.o
obj-$(CONFIG_NETPOLL) += netpoll.o
obj-$(CONFIG_NET_DMA) += user_dma.o
obj-$(CONFIG_FIB_RULES) += fib_rules.o
obj-$(CONFIG_TRACEPOINTS) += net-traces.o
obj-$(CONFIG_NET_DROP_MONITOR) += drop_monitor.o
obj-$(CONFIG_NETWORK_PHY_TIMESTAMPING) += timestamping.o

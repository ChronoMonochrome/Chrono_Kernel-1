/* header for portlist.c */
#ifndef _PORTLIST_H_
#define _PORTLIST_H_

#ifdef CONFIG_SVNET_WHITELIST
static int process_whilte_list(void);
#endif
#define AP_STATE_SLEEP 0x01
#define AP_STATE_WAKEUP 0x02
extern int tx_ap_state(unsigned char apstate);
#endif

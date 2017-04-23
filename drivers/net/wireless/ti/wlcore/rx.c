/*
 * This file is part of wl1271
 *
 * Copyright (C) 2009 Nokia Corporation
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/gfp.h>

<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
#include "wl12xx.h"
=======
#include "wlcore.h"
#include "debug.h"
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c
#include "acx.h"
#include "rx.h"
#include "io.h"
#include "hw_ops.h"

<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
static u8 wl1271_rx_get_mem_block(struct wl1271_fw_common_status *status,
				  u32 drv_rx_counter)
{
	return le32_to_cpu(status->rx_pkt_descs[drv_rx_counter]) &
		RX_MEM_BLOCK_MASK;
}

static u32 wl1271_rx_get_buf_size(struct wl1271_fw_common_status *status,
				 u32 drv_rx_counter)
=======
/*
 * TODO: this is here just for now, it must be removed when the data
 * operations are in place.
 */
#include "../wl12xx/reg.h"

static u32 wlcore_rx_get_buf_size(struct wl1271 *wl,
				  u32 rx_pkt_desc)
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c
{
	if (wl->quirks & WLCORE_QUIRK_RX_BLOCKSIZE_ALIGN)
		return (rx_pkt_desc & ALIGNED_RX_BUF_SIZE_MASK) >>
		       ALIGNED_RX_BUF_SIZE_SHIFT;

	return (rx_pkt_desc & RX_BUF_SIZE_MASK) >> RX_BUF_SIZE_SHIFT_DIV;
}

<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
=======
static u32 wlcore_rx_get_align_buf_size(struct wl1271 *wl, u32 pkt_len)
{
	if (wl->quirks & WLCORE_QUIRK_RX_BLOCKSIZE_ALIGN)
		return ALIGN(pkt_len, WL12XX_BUS_BLOCK_SIZE);

	return pkt_len;
}

>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c
static void wl1271_rx_status(struct wl1271 *wl,
			     struct wl1271_rx_descriptor *desc,
			     struct ieee80211_rx_status *status,
			     u8 beacon)
{
	memset(status, 0, sizeof(struct ieee80211_rx_status));

	if ((desc->flags & WL1271_RX_DESC_BAND_MASK) == WL1271_RX_DESC_BAND_BG)
		status->band = IEEE80211_BAND_2GHZ;
	else
		status->band = IEEE80211_BAND_5GHZ;

	status->rate_idx = wlcore_rate_to_idx(wl, desc->rate, status->band);

#ifdef CONFIG_WL12XX_HT
	/* 11n support */
	if (desc->rate <= wl->hw_min_ht_rate)
		status->flag |= RX_FLAG_HT;
#endif

	status->signal = desc->rssi;

	/*
	 * FIXME: In wl1251, the SNR should be divided by two.  In wl1271 we
	 * need to divide by two for now, but TI has been discussing about
	 * changing it.  This needs to be rechecked.
	 */
	wl->noise = desc->rssi - (desc->snr >> 1);

	status->freq = ieee80211_channel_to_frequency(desc->channel,
						      status->band);

	if (desc->flags & WL1271_RX_DESC_ENCRYPT_MASK) {
		u8 desc_err_code = desc->status & WL1271_RX_DESC_STATUS_MASK;

		status->flag |= RX_FLAG_IV_STRIPPED | RX_FLAG_MMIC_STRIPPED |
				RX_FLAG_DECRYPTED;

		if (unlikely(desc_err_code == WL1271_RX_DESC_MIC_FAIL)) {
			status->flag |= RX_FLAG_MMIC_ERROR;
			wl1271_warning("Michael MIC error");
		}
	}
}

<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
static int wl1271_rx_handle_data(struct wl1271 *wl, u8 *data, u32 length)
=======
static int wl1271_rx_handle_data(struct wl1271 *wl, u8 *data, u32 length,
				 enum wl_rx_buf_align rx_align, u8 *hlid)
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c
{
	struct wl1271_rx_descriptor *desc;
	struct sk_buff *skb;
	struct ieee80211_hdr *hdr;
	u8 *buf;
	u8 beacon = 0;
<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
=======
	u8 is_data = 0;
	u8 reserved = 0;
	u16 seq_num;
	u32 pkt_data_len;
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c

	/*
	 * In PLT mode we seem to get frames and mac80211 warns about them,
	 * workaround this by not retrieving them at all.
	 */
	if (unlikely(wl->state == WL1271_STATE_PLT))
		return -EINVAL;

	pkt_data_len = wlcore_hw_get_rx_packet_len(wl, data, length);
	if (!pkt_data_len) {
		wl1271_error("Invalid packet arrived from HW. length %d",
			     length);
		return -EINVAL;
	}

	if (rx_align == WLCORE_RX_BUF_UNALIGNED)
		reserved = NET_IP_ALIGN;

	/* the data read starts with the descriptor */
	desc = (struct wl1271_rx_descriptor *) data;

	switch (desc->status & WL1271_RX_DESC_STATUS_MASK) {
	/* discard corrupted packets */
	case WL1271_RX_DESC_DRIVER_RX_Q_FAIL:
	case WL1271_RX_DESC_DECRYPT_FAIL:
		wl1271_warning("corrupted packet in RX with status: 0x%x",
			       desc->status & WL1271_RX_DESC_STATUS_MASK);
		return -EINVAL;
	case WL1271_RX_DESC_SUCCESS:
	case WL1271_RX_DESC_MIC_FAIL:
		break;
	default:
		wl1271_error("invalid RX descriptor status: 0x%x",
			     desc->status & WL1271_RX_DESC_STATUS_MASK);
		return -EINVAL;
	}

<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
	skb = __dev_alloc_skb(length, GFP_KERNEL);
=======
	/* skb length not including rx descriptor */
	skb = __dev_alloc_skb(pkt_data_len + reserved, GFP_KERNEL);
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c
	if (!skb) {
		wl1271_error("Couldn't allocate RX frame");
		return -ENOMEM;
	}

<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
	buf = skb_put(skb, length);
	memcpy(buf, data, length);

	/* now we pull the descriptor out of the buffer */
	skb_pull(skb, sizeof(*desc));
=======
	/* reserve the unaligned payload(if any) */
	skb_reserve(skb, reserved);

	buf = skb_put(skb, pkt_data_len);

	/*
	 * Copy packets from aggregation buffer to the skbs without rx
	 * descriptor and with packet payload aligned care. In case of unaligned
	 * packets copy the packets in offset of 2 bytes guarantee IP header
	 * payload aligned to 4 bytes.
	 */
	memcpy(buf, data + sizeof(*desc), pkt_data_len);
	if (rx_align == WLCORE_RX_BUF_PADDED)
		skb_pull(skb, NET_IP_ALIGN);

	*hlid = desc->hlid;
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c

	hdr = (struct ieee80211_hdr *)skb->data;
	if (ieee80211_is_beacon(hdr->frame_control))
		beacon = 1;

	wl1271_rx_status(wl, desc, IEEE80211_SKB_RXCB(skb), beacon);

	wl1271_debug(DEBUG_RX, "rx skb 0x%p: %d B %s", skb,
		     skb->len - desc->pad_len,
		     beacon ? "beacon" : "");

	skb_queue_tail(&wl->deferred_rx_queue, skb);
	ieee80211_queue_work(wl->hw, &wl->netstack_work);

	return 0;
}

<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
void wl1271_rx(struct wl1271 *wl, struct wl1271_fw_common_status *status)
{
	struct wl1271_acx_mem_map *wl_mem_map = wl->target_mem_map;
=======
void wl12xx_rx(struct wl1271 *wl, struct wl_fw_status *status)
{
	unsigned long active_hlids[BITS_TO_LONGS(WL12XX_MAX_LINKS)] = {0};
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c
	u32 buf_size;
	u32 fw_rx_counter  = status->fw_rx_counter & NUM_RX_PKT_DESC_MOD_MASK;
	u32 drv_rx_counter = wl->rx_counter & NUM_RX_PKT_DESC_MOD_MASK;
	u32 rx_counter;
<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
	u32 mem_block;
	u32 pkt_length;
	u32 pkt_offset;
=======
	u32 pkt_len, align_pkt_len;
	u32 pkt_offset, des;
	u8 hlid;
	enum wl_rx_buf_align rx_align;
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c

	while (drv_rx_counter != fw_rx_counter) {
		buf_size = 0;
		rx_counter = drv_rx_counter;
		while (rx_counter != fw_rx_counter) {
<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
			pkt_length = wl1271_rx_get_buf_size(status, rx_counter);
			if (buf_size + pkt_length > WL1271_AGGR_BUFFER_SIZE)
=======
			des = le32_to_cpu(status->rx_pkt_descs[rx_counter]);
			pkt_len = wlcore_rx_get_buf_size(wl, des);
			align_pkt_len = wlcore_rx_get_align_buf_size(wl,
								     pkt_len);
			if (buf_size + align_pkt_len > WL1271_AGGR_BUFFER_SIZE)
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c
				break;
			buf_size += align_pkt_len;
			rx_counter++;
			rx_counter &= NUM_RX_PKT_DESC_MOD_MASK;
		}

		if (buf_size == 0) {
			wl1271_warning("received empty data");
			break;
		}

<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
		if (wl->chip.id != CHIP_ID_1283_PG20) {
			/*
			 * Choose the block we want to read
			 * For aggregated packets, only the first memory block
			 * should be retrieved. The FW takes care of the rest.
			 */
			mem_block = wl1271_rx_get_mem_block(status,
							    drv_rx_counter);

			wl->rx_mem_pool_addr.addr = (mem_block << 8) +
			   le32_to_cpu(wl_mem_map->packet_memory_pool_start);

			wl->rx_mem_pool_addr.addr_extra =
				wl->rx_mem_pool_addr.addr + 4;

			wl1271_write(wl, WL1271_SLV_REG_DATA,
				     &wl->rx_mem_pool_addr,
				     sizeof(wl->rx_mem_pool_addr), false);
		}

=======
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c
		/* Read all available packets at once */
		des = le32_to_cpu(status->rx_pkt_descs[drv_rx_counter]);
		wlcore_hw_prepare_read(wl, des, buf_size);
		wlcore_read_data(wl, REG_SLV_MEM_DATA, wl->aggr_buf,
				 buf_size, true);

		/* Split data into separate packets */
		pkt_offset = 0;
		while (pkt_offset < buf_size) {
<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
			pkt_length = wl1271_rx_get_buf_size(status,
					drv_rx_counter);
=======
			des = le32_to_cpu(status->rx_pkt_descs[drv_rx_counter]);
			pkt_len = wlcore_rx_get_buf_size(wl, des);
			rx_align = wlcore_hw_get_rx_buf_align(wl, des);

>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c
			/*
			 * the handle data call can only fail in memory-outage
			 * conditions, in that case the received frame will just
			 * be dropped.
			 */
<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
			wl1271_rx_handle_data(wl,
					      wl->aggr_buf + pkt_offset,
					      pkt_length);
=======
			if (wl1271_rx_handle_data(wl,
						  wl->aggr_buf + pkt_offset,
						  pkt_len, rx_align,
						  &hlid) == 1) {
				if (hlid < WL12XX_MAX_LINKS)
					__set_bit(hlid, active_hlids);
				else
					WARN(1,
					     "hlid exceeded WL12XX_MAX_LINKS "
					     "(%d)\n", hlid);
			}

>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c
			wl->rx_counter++;
			drv_rx_counter++;
			drv_rx_counter &= NUM_RX_PKT_DESC_MOD_MASK;
			pkt_offset += wlcore_rx_get_align_buf_size(wl, pkt_len);
		}
	}

	/*
	 * Write the driver's packet counter to the FW. This is only required
	 * for older hardware revisions
	 */
<<<<<<< HEAD:drivers/net/wireless/wl12xx/rx.c
	if (wl->quirks & WL12XX_QUIRK_END_OF_TRANSACTION)
		wl1271_write32(wl, RX_DRIVER_COUNTER_ADDRESS, wl->rx_counter);
}
=======
	if (wl->quirks & WLCORE_QUIRK_END_OF_TRANSACTION)
		wl1271_write32(wl, WL12XX_REG_RX_DRIVER_COUNTER,
			       wl->rx_counter);
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/rx.c

void wl1271_set_default_filters(struct wl1271 *wl)
{
	if (wl->bss_type == BSS_TYPE_AP_BSS) {
		wl->rx_config = WL1271_DEFAULT_AP_RX_CONFIG;
		wl->rx_filter = WL1271_DEFAULT_AP_RX_FILTER;
	} else {
		wl->rx_config = WL1271_DEFAULT_STA_RX_CONFIG;
		wl->rx_filter = WL1271_DEFAULT_STA_RX_FILTER;
	}
}

#ifdef CONFIG_PM
int wl1271_rx_filter_enable(struct wl1271 *wl,
			    int index, bool enable,
			    struct wl12xx_rx_filter *filter)
{
	int ret;

	if (wl->rx_filter_enabled[index] == enable) {
		wl1271_warning("Request to enable an already "
			     "enabled rx filter %d", index);
		return 0;
	}

	ret = wl1271_acx_set_rx_filter(wl, index, enable, filter);

	if (ret) {
		wl1271_error("Failed to %s rx data filter %d (err=%d)",
			     enable ? "enable" : "disable", index, ret);
		return ret;
	}

	wl->rx_filter_enabled[index] = enable;

	return 0;
}

void wl1271_rx_filter_clear_all(struct wl1271 *wl)
{
	int i;

	for (i = 0; i < WL1271_MAX_RX_FILTERS; i++) {
		if (!wl->rx_filter_enabled[i])
			continue;
		wl1271_rx_filter_enable(wl, i, 0, NULL);
	}
}
#endif /* CONFIG_PM */

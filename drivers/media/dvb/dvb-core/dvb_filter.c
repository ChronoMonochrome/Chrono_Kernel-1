#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include "dvb_filter.h"

#if 0
static unsigned int bitrates[3][16] =
{{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
 {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
 {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}};
#endif

static u32 freq[4] = {480, 441, 320, 0};

static unsigned int ac3_bitrates[32] =
    {32,40,48,56,64,80,96,112,128,160,192,224,256,320,384,448,512,576,640,
     0,0,0,0,0,0,0,0,0,0,0,0,0};

static u32 ac3_frames[3][32] =
    {{64,80,96,112,128,160,192,224,256,320,384,448,512,640,768,896,1024,
      1152,1280,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {69,87,104,121,139,174,208,243,278,348,417,487,557,696,835,975,1114,
      1253,1393,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {96,120,144,168,192,240,288,336,384,480,576,672,768,960,1152,1344,
      1536,1728,1920,0,0,0,0,0,0,0,0,0,0,0,0,0}};



#if 0
static void setup_ts2pes(ipack *pa, ipack *pv, u16 *pida, u16 *pidv,
		  void (*pes_write)(u8 *buf, int count, void *data),
		  void *priv)
{
	dvb_filter_ipack_init(pa, IPACKS, pes_write);
	dvb_filter_ipack_init(pv, IPACKS, pes_write);
	pa->pid = pida;
	pv->pid = pidv;
	pa->data = priv;
	pv->data = priv;
}
#endif

#if 0
static void ts_to_pes(ipack *p, u8 *buf) // don't need count (=188)
{
	u8 off = 0;

	if (!buf || !p ){
#ifdef CONFIG_DEBUG_PRINTK
		printk("NULL POINTER IDIOT\n");
#else
		;
#endif
		return;
	}
	if (buf[1]&PAY_START) {
		if (p->plength == MMAX_PLENGTH-6 && p->found>6){
			p->plength = p->found-6;
			p->found = 0;
			send_ipack(p);
			dvb_filter_ipack_reset(p);
		}
	}
	if (buf[3] & ADAPT_FIELD) {  // adaptation field?
		off = buf[4] + 1;
		if (off+4 > 187) return;
	}
	dvb_filter_instant_repack(buf+4+off, TS_SIZE-4-off, p);
}
#endif

#if 0
/* needs 5 byte input, returns picture coding type*/
static int read_picture_header(u8 *headr, struct mpg_picture *pic, int field, int pr)
{
	u8 pct;

#ifdef CONFIG_DEBUG_PRINTK
	if (pr) printk( "Pic header: ");
#else
	if (pr) ;
#endif
	pic->temporal_reference[field] = (( headr[0] << 2 ) |
					  (headr[1] & 0x03) )& 0x03ff;
#ifdef CONFIG_DEBUG_PRINTK
	if (pr) printk( " temp ref: 0x%04x", pic->temporal_reference[field]);
#else
	if (pr) ;
#endif

	pct = ( headr[1] >> 2 ) & 0x07;
	pic->picture_coding_type[field] = pct;
	if (pr) {
		switch(pct){
			case I_FRAME:
#ifdef CONFIG_DEBUG_PRINTK
				printk( "  I-FRAME");
#else
				;
#endif
				break;
			case B_FRAME:
#ifdef CONFIG_DEBUG_PRINTK
				printk( "  B-FRAME");
#else
				;
#endif
				break;
			case P_FRAME:
#ifdef CONFIG_DEBUG_PRINTK
				printk( "  P-FRAME");
#else
				;
#endif
				break;
		}
	}


	pic->vinfo.vbv_delay  = (( headr[1] >> 5 ) | ( headr[2] << 3) |
				 ( (headr[3] & 0x1F) << 11) ) & 0xffff;

#ifdef CONFIG_DEBUG_PRINTK
	if (pr) printk( " vbv delay: 0x%04x", pic->vinfo.vbv_delay);
#else
	if (pr) ;
#endif

	pic->picture_header_parameter = ( headr[3] & 0xe0 ) |
		((headr[4] & 0x80) >> 3);

	if ( pct == B_FRAME ){
		pic->picture_header_parameter |= ( headr[4] >> 3 ) & 0x0f;
	}
#ifdef CONFIG_DEBUG_PRINTK
	if (pr) printk( " pic head param: 0x%x",
			pic->picture_header_parameter);
#else
	if (pr) ;
#endif

	return pct;
}
#endif

#if 0
/* needs 4 byte input */
static int read_gop_header(u8 *headr, struct mpg_picture *pic, int pr)
{
#ifdef CONFIG_DEBUG_PRINTK
	if (pr) printk("GOP header: ");
#else
	if (pr) ;
#endif

	pic->time_code  = (( headr[0] << 17 ) | ( headr[1] << 9) |
			   ( headr[2] << 1 ) | (headr[3] &0x01)) & 0x1ffffff;

#ifdef CONFIG_DEBUG_PRINTK
	if (pr) printk(" time: %d:%d.%d ", (headr[0]>>2)& 0x1F,
		       ((headr[0]<<4)& 0x30)| ((headr[1]>>4)& 0x0F),
		       ((headr[1]<<3)& 0x38)| ((headr[2]>>5)& 0x0F));
#else
	if (pr) ;
#endif

	if ( ( headr[3] & 0x40 ) != 0 ){
		pic->closed_gop = 1;
	} else {
		pic->closed_gop = 0;
	}
#ifdef CONFIG_DEBUG_PRINTK
	if (pr) printk("closed: %d", pic->closed_gop);
#else
	if (pr) ;
#endif

	if ( ( headr[3] & 0x20 ) != 0 ){
		pic->broken_link = 1;
	} else {
		pic->broken_link = 0;
	}
#ifdef CONFIG_DEBUG_PRINTK
	if (pr) printk(" broken: %d\n", pic->broken_link);
#else
	if (pr) ;
#endif

	return 0;
}
#endif

#if 0
/* needs 8 byte input */
static int read_sequence_header(u8 *headr, struct dvb_video_info *vi, int pr)
{
	int sw;
	int form = -1;

#ifdef CONFIG_DEBUG_PRINTK
	if (pr) printk("Reading sequence header\n");
#else
	if (pr) ;
#endif

	vi->horizontal_size	= ((headr[1] &0xF0) >> 4) | (headr[0] << 4);
	vi->vertical_size	= ((headr[1] &0x0F) << 8) | (headr[2]);

	sw = (int)((headr[3]&0xF0) >> 4) ;

	switch( sw ){
	case 1:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("Videostream: ASPECT: 1:1");
#else
			;
#endif
		vi->aspect_ratio = 100;
		break;
	case 2:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("Videostream: ASPECT: 4:3");
#else
			;
#endif
		vi->aspect_ratio = 133;
		break;
	case 3:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("Videostream: ASPECT: 16:9");
#else
			;
#endif
		vi->aspect_ratio = 177;
		break;
	case 4:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("Videostream: ASPECT: 2.21:1");
#else
			;
#endif
		vi->aspect_ratio = 221;
		break;

	case 5 ... 15:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("Videostream: ASPECT: reserved");
#else
			;
#endif
		vi->aspect_ratio = 0;
		break;

	default:
		vi->aspect_ratio = 0;
		return -1;
	}

	if (pr)
#ifdef CONFIG_DEBUG_PRINTK
		printk("  Size = %dx%d",vi->horizontal_size,vi->vertical_size);
#else
		;
#endif

	sw = (int)(headr[3]&0x0F);

	switch ( sw ) {
	case 1:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("  FRate: 23.976 fps");
#else
			;
#endif
		vi->framerate = 23976;
		form = -1;
		break;
	case 2:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("  FRate: 24 fps");
#else
			;
#endif
		vi->framerate = 24000;
		form = -1;
		break;
	case 3:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("  FRate: 25 fps");
#else
			;
#endif
		vi->framerate = 25000;
		form = VIDEO_MODE_PAL;
		break;
	case 4:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("  FRate: 29.97 fps");
#else
			;
#endif
		vi->framerate = 29970;
		form = VIDEO_MODE_NTSC;
		break;
	case 5:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("  FRate: 30 fps");
#else
			;
#endif
		vi->framerate = 30000;
		form = VIDEO_MODE_NTSC;
		break;
	case 6:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("  FRate: 50 fps");
#else
			;
#endif
		vi->framerate = 50000;
		form = VIDEO_MODE_PAL;
		break;
	case 7:
		if (pr)
#ifdef CONFIG_DEBUG_PRINTK
			printk("  FRate: 60 fps");
#else
			;
#endif
		vi->framerate = 60000;
		form = VIDEO_MODE_NTSC;
		break;
	}

	vi->bit_rate = (headr[4] << 10) | (headr[5] << 2) | (headr[6] & 0x03);

	vi->vbv_buffer_size
		= (( headr[6] & 0xF8) >> 3 ) | (( headr[7] & 0x1F )<< 5);

	if (pr){
#ifdef CONFIG_DEBUG_PRINTK
		printk("  BRate: %d Mbit/s",4*(vi->bit_rate)/10000);
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
		printk("  vbvbuffer %d",16*1024*(vi->vbv_buffer_size));
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
		printk("\n");
#else
		;
#endif
	}

	vi->video_format = form;

	return 0;
}
#endif


#if 0
static int get_vinfo(u8 *mbuf, int count, struct dvb_video_info *vi, int pr)
{
	u8 *headr;
	int found = 0;
	int c = 0;

	while (found < 4 && c+4 < count){
		u8 *b;

		b = mbuf+c;
		if ( b[0] == 0x00 && b[1] == 0x00 && b[2] == 0x01
		     && b[3] == 0xb3) found = 4;
		else {
			c++;
		}
	}

	if (! found) return -1;
	c += 4;
	if (c+12 >= count) return -1;
	headr = mbuf+c;
	if (read_sequence_header(headr, vi, pr) < 0) return -1;
	vi->off = c-4;
	return 0;
}
#endif


#if 0
static int get_ainfo(u8 *mbuf, int count, struct dvb_audio_info *ai, int pr)
{
	u8 *headr;
	int found = 0;
	int c = 0;
	int fr = 0;

	while (found < 2 && c < count){
		u8 b[2];
		memcpy( b, mbuf+c, 2);

		if ( b[0] == 0xff && (b[1] & 0xf8) == 0xf8)
			found = 2;
		else {
			c++;
		}
	}

	if (!found) return -1;

	if (c+3 >= count) return -1;
	headr = mbuf+c;

	ai->layer = (headr[1] & 0x06) >> 1;

	if (pr)
#ifdef CONFIG_DEBUG_PRINTK
		printk("Audiostream: Layer: %d", 4-ai->layer);
#else
		;
#endif


	ai->bit_rate = bitrates[(3-ai->layer)][(headr[2] >> 4 )]*1000;

	if (pr){
		if (ai->bit_rate == 0)
#ifdef CONFIG_DEBUG_PRINTK
			printk("  Bit rate: free");
#else
			;
#endif
		else if (ai->bit_rate == 0xf)
#ifdef CONFIG_DEBUG_PRINTK
			printk("  BRate: reserved");
#else
			;
#endif
		else
#ifdef CONFIG_DEBUG_PRINTK
			printk("  BRate: %d kb/s", ai->bit_rate/1000);
#else
			;
#endif
	}

	fr = (headr[2] & 0x0c ) >> 2;
	ai->frequency = freq[fr]*100;
	if (pr){
		if (ai->frequency == 3)
#ifdef CONFIG_DEBUG_PRINTK
			printk("  Freq: reserved\n");
#else
			;
#endif
		else
#ifdef CONFIG_DEBUG_PRINTK
			printk("  Freq: %d kHz\n",ai->frequency);
#else
			;
#endif

	}
	ai->off = c;
	return 0;
}
#endif


int dvb_filter_get_ac3info(u8 *mbuf, int count, struct dvb_audio_info *ai, int pr)
{
	u8 *headr;
	int found = 0;
	int c = 0;
	u8 frame = 0;
	int fr = 0;

	while ( !found  && c < count){
		u8 *b = mbuf+c;

		if ( b[0] == 0x0b &&  b[1] == 0x77 )
			found = 1;
		else {
			c++;
		}
	}

	if (!found) return -1;
	if (pr)
#ifdef CONFIG_DEBUG_PRINTK
		printk("Audiostream: AC3");
#else
		;
#endif

	ai->off = c;
	if (c+5 >= count) return -1;

	ai->layer = 0;  // 0 for AC3
	headr = mbuf+c+2;

	frame = (headr[2]&0x3f);
	ai->bit_rate = ac3_bitrates[frame >> 1]*1000;

	if (pr)
#ifdef CONFIG_DEBUG_PRINTK
		printk("  BRate: %d kb/s", (int) ai->bit_rate/1000);
#else
		;
#endif

	ai->frequency = (headr[2] & 0xc0 ) >> 6;
	fr = (headr[2] & 0xc0 ) >> 6;
	ai->frequency = freq[fr]*100;
	if (pr) printk ("  Freq: %d Hz\n", (int) ai->frequency);


	ai->framesize = ac3_frames[fr][frame >> 1];
	if ((frame & 1) &&  (fr == 1)) ai->framesize++;
	ai->framesize = ai->framesize << 1;
	if (pr) printk ("  Framesize %d\n",(int) ai->framesize);


	return 0;
}
EXPORT_SYMBOL(dvb_filter_get_ac3info);


#if 0
static u8 *skip_pes_header(u8 **bufp)
{
	u8 *inbuf = *bufp;
	u8 *buf = inbuf;
	u8 *pts = NULL;
	int skip = 0;

	static const int mpeg1_skip_table[16] = {
		1, 0xffff,      5,     10, 0xffff, 0xffff, 0xffff, 0xffff,
		0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff
	};


	if ((inbuf[6] & 0xc0) == 0x80){ /* mpeg2 */
		if (buf[7] & PTS_ONLY)
			pts = buf+9;
		else pts = NULL;
		buf = inbuf + 9 + inbuf[8];
	} else {        /* mpeg1 */
		for (buf = inbuf + 6; *buf == 0xff; buf++)
			if (buf == inbuf + 6 + 16) {
				break;
			}
		if ((*buf & 0xc0) == 0x40)
			buf += 2;
		skip = mpeg1_skip_table [*buf >> 4];
		if (skip == 5 || skip == 10) pts = buf;
		else pts = NULL;

		buf += mpeg1_skip_table [*buf >> 4];
	}

	*bufp = buf;
	return pts;
}
#endif

#if 0
static void initialize_quant_matrix( u32 *matrix )
{
	int i;

	matrix[0]  = 0x08101013;
	matrix[1]  = 0x10131616;
	matrix[2]  = 0x16161616;
	matrix[3]  = 0x1a181a1b;
	matrix[4]  = 0x1b1b1a1a;
	matrix[5]  = 0x1a1a1b1b;
	matrix[6]  = 0x1b1d1d1d;
	matrix[7]  = 0x2222221d;
	matrix[8]  = 0x1d1d1b1b;
	matrix[9]  = 0x1d1d2020;
	matrix[10] = 0x22222526;
	matrix[11] = 0x25232322;
	matrix[12] = 0x23262628;
	matrix[13] = 0x28283030;
	matrix[14] = 0x2e2e3838;
	matrix[15] = 0x3a454553;

	for ( i = 16 ; i < 32 ; i++ )
		matrix[i] = 0x10101010;
}
#endif

#if 0
static void initialize_mpg_picture(struct mpg_picture *pic)
{
	int i;

	/* set MPEG1 */
	pic->mpeg1_flag = 1;
	pic->profile_and_level = 0x4A ;        /* MP@LL */
	pic->progressive_sequence = 1;
	pic->low_delay = 0;

	pic->sequence_display_extension_flag = 0;
	for ( i = 0 ; i < 4 ; i++ ){
		pic->frame_centre_horizontal_offset[i] = 0;
		pic->frame_centre_vertical_offset[i] = 0;
	}
	pic->last_frame_centre_horizontal_offset = 0;
	pic->last_frame_centre_vertical_offset = 0;

	pic->picture_display_extension_flag[0] = 0;
	pic->picture_display_extension_flag[1] = 0;
	pic->sequence_header_flag = 0;
	pic->gop_flag = 0;
	pic->sequence_end_flag = 0;
}
#endif

#if 0
static void mpg_set_picture_parameter( int32_t field_type, struct mpg_picture *pic )
{
	int16_t last_h_offset;
	int16_t last_v_offset;

	int16_t *p_h_offset;
	int16_t *p_v_offset;

	if ( pic->mpeg1_flag ){
		pic->picture_structure[field_type] = VIDEO_FRAME_PICTURE;
		pic->top_field_first = 0;
		pic->repeat_first_field = 0;
		pic->progressive_frame = 1;
		pic->picture_coding_parameter = 0x000010;
	}

	/* Reset flag */
	pic->picture_display_extension_flag[field_type] = 0;

	last_h_offset = pic->last_frame_centre_horizontal_offset;
	last_v_offset = pic->last_frame_centre_vertical_offset;
	if ( field_type == FIRST_FIELD ){
		p_h_offset = pic->frame_centre_horizontal_offset;
		p_v_offset = pic->frame_centre_vertical_offset;
		*p_h_offset = last_h_offset;
		*(p_h_offset + 1) = last_h_offset;
		*(p_h_offset + 2) = last_h_offset;
		*p_v_offset = last_v_offset;
		*(p_v_offset + 1) = last_v_offset;
		*(p_v_offset + 2) = last_v_offset;
	} else {
		pic->frame_centre_horizontal_offset[3] = last_h_offset;
		pic->frame_centre_vertical_offset[3] = last_v_offset;
	}
}
#endif

#if 0
static void init_mpg_picture( struct mpg_picture *pic, int chan, int32_t field_type)
{
	pic->picture_header = 0;
	pic->sequence_header_data
		= ( INIT_HORIZONTAL_SIZE << 20 )
			| ( INIT_VERTICAL_SIZE << 8 )
			| ( INIT_ASPECT_RATIO << 4 )
			| ( INIT_FRAME_RATE );
	pic->mpeg1_flag = 0;
	pic->vinfo.horizontal_size
		= INIT_DISP_HORIZONTAL_SIZE;
	pic->vinfo.vertical_size
		= INIT_DISP_VERTICAL_SIZE;
	pic->picture_display_extension_flag[field_type]
		= 0;
	pic->pts_flag[field_type] = 0;

	pic->sequence_gop_header = 0;
	pic->picture_header = 0;
	pic->sequence_header_flag = 0;
	pic->gop_flag = 0;
	pic->sequence_end_flag = 0;
	pic->sequence_display_extension_flag = 0;
	pic->last_frame_centre_horizontal_offset = 0;
	pic->last_frame_centre_vertical_offset = 0;
	pic->channel = chan;
}
#endif

void dvb_filter_pes2ts_init(struct dvb_filter_pes2ts *p2ts, unsigned short pid,
			    dvb_filter_pes2ts_cb_t *cb, void *priv)
{
	unsigned char *buf=p2ts->buf;

	buf[0]=0x47;
	buf[1]=(pid>>8);
	buf[2]=pid&0xff;
	p2ts->cc=0;
	p2ts->cb=cb;
	p2ts->priv=priv;
}
EXPORT_SYMBOL(dvb_filter_pes2ts_init);

int dvb_filter_pes2ts(struct dvb_filter_pes2ts *p2ts, unsigned char *pes,
		      int len, int payload_start)
{
	unsigned char *buf=p2ts->buf;
	int ret=0, rest;

	//len=6+((pes[4]<<8)|pes[5]);

	if (payload_start)
		buf[1]|=0x40;
	else
		buf[1]&=~0x40;
	while (len>=184) {
		buf[3]=0x10|((p2ts->cc++)&0x0f);
		memcpy(buf+4, pes, 184);
		if ((ret=p2ts->cb(p2ts->priv, buf)))
			return ret;
		len-=184; pes+=184;
		buf[1]&=~0x40;
	}
	if (!len)
		return 0;
	buf[3]=0x30|((p2ts->cc++)&0x0f);
	rest=183-len;
	if (rest) {
		buf[5]=0x00;
		if (rest-1)
			memset(buf+6, 0xff, rest-1);
	}
	buf[4]=rest;
	memcpy(buf+5+rest, pes, len);
	return p2ts->cb(p2ts->priv, buf);
}
EXPORT_SYMBOL(dvb_filter_pes2ts);

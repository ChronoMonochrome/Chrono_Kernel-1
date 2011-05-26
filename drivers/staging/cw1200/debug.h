#ifndef CW1200_DEBUG_H_INCLUDED
#define CW1200_DEBUG_H_INCLUDED

struct cw200_common;

#ifdef CONFIG_CW1200_DEBUGFS

int cw1200_debug_init(struct cw1200_common *priv);
void cw1200_debug_release(struct cw1200_common *priv);

#else /* CONFIG_CW1200_DEBUGFS */

static inline int cw1200_debug_init(struct cw1200_common *priv)
{
	return 0;
}

static inline void cw1200_debug_release(struct cw1200_common *priv)
{
}

#endif /* CONFIG_CW1200_DEBUGFS */

#endif /* CW1200_DEBUG_H_INCLUDED */

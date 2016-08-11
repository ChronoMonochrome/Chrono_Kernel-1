#ifndef MACH_TYPES_H
#define MACH_TYPES_H
#include <generated/mach-types.h>
extern int board_type;

#define RUN_ON_CODINA_ONLY					\
		 						\
		if (likely(board_type == MACH_TYPE_CODINA) ||	\
unlikely(board_type == 0 && strstr(CONFIG_CMDLINE, "codina"))){ \
			if (unlikely(board_type == 0))		\
				board_type = MACH_TYPE_CODINA;	\

#define RUN_ON_JANICE_ONLY					\
		 						\
		if (likely(board_type == MACH_TYPE_JANICE) ||	\
unlikely(board_type == 0 && strstr(CONFIG_CMDLINE, "janice"))){ \
			if (unlikely(board_type == 0))		\
				board_type = MACH_TYPE_JANICE;	\

#define ELSE } else
#define END_RUN }
#endif

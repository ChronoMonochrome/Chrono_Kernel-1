#ifndef MACH_TYPES_H
#define MACH_TYPES_H
#include <generated/mach-types.h>
extern int board_type;

#define RUN_ON_CODINA_ONLY if (strstr(CONFIG_CMDLINE, "codina")) {
#define RUN_ON_JANICE_ONLY if (strstr(CONFIG_CMDLINE, "janice")) {
#define END_RUN }
#endif

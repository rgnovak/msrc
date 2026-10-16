#ifndef COCKPIT_TASK_H
#define COCKPIT_TASK_H

#include "common.h"

/* FreeRTOS task that owns both cockpit panels (PFD on CS1, moving map
 * on CS2). Started from main.c alongside srxl2_task. */
void cockpit_task(void *parameters);

#endif /* COCKPIT_TASK_H */

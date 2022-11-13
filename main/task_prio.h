#ifndef MAIN_TASK_PRIO_H_
#define MAIN_TASK_PRIO_H_

#include "freertos/FreeRTOS.h"

#define TP_READER 1
#define TP_RTCD 1
#define TP_UPLOAD 1
#define TP_TAMPER 1
#define TP_MAIN 2
#define TP_UI (configMAX_PRIORITIES - 2)

#endif /* MAIN_TASK_PRIO_H_ */

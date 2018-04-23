#ifndef MAB_REDIS_LOG_H
#define MAB_REDIS_LOG_H

#define log_error(fmt, ...) fprintf(stderr, "[ERROR] "fmt"\n", ##__VA_ARGS__)

#ifdef DEV
#define log_dev(fmt, ...)   printf("[DEV  ] "fmt"\n", ##__VA_ARGS__)
#else
#define log_dev(fmt, ...)   do{}while(0)
#endif

#endif

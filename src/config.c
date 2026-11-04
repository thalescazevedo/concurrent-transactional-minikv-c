/*
 * config.c - Funções auxiliares: msleep, now_ms e account_key.
 *
 * ATENÇÃO: Este arquivo NÃO deve ser alterado!
 */

#include <time.h>
#include <errno.h>
#include "config.h"

config_t config;

int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0) {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

void account_key(unsigned int i, char *buf)
{
    snprintf(buf, KEY_LEN, "conta-%04u", i);
}

/*
 * wal.c - O log (write-ahead log) com group commit.
 *
 * Implemente todas as funções deste arquivo.
 *
 * VOCÊ DEVE ALTERAR ESTE ARQUIVO.
 */

#include <stdlib.h>
#include <string.h>
#include "wal.h"

void wal_init(wal_t *self, disk_t *disk)
{
    self->disk = disk;

    /* TODO: Adicionar código aqui se necessário! */

    plog("[wal] Initialized\n");
}

void wal_finalize(wal_t *self)
{
    /* TODO: Adicionar código aqui se necessário! */

    plog("[wal] Finalized\n");
}

long wal_append(wal_t *self, const char *from, const char *to, int amount)
{
    /* TODO: Implementar! O código abaixo deve ser modificado! */
    return 0;
}

void wal_wait_durable(wal_t *self, long lsn)
{
    /* TODO: Implementar! */
}

void * wal_writer_run(void *arg)
{
    wal_t *self = (wal_t *) arg;

    plog("[wal] Log writer is now running!\n");

    /* TODO: Implementar! Enquanto o log não for encerrado, retire todos os
       registros disponíveis no buffer e grave-os com disk_write_batch(). */
    (void) self;

    plog("[wal] Log writer has finished!\n");
    return NULL;
}

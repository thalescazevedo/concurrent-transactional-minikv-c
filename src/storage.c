/*
 * storage.c - Armazenamento do MiniKV: tabela hash em memória e disco (log).
 *
 * ATENÇÃO: Este arquivo NÃO deve ser alterado!
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sched.h>
#include "storage.h"

/*============================================================================*
 * Tabela hash                                                                *
 *============================================================================*/

/**
 * @brief Às vezes cede o processador no meio de uma leitura ou escrita.
 * Não muda o resultado de um programa correto, mas torna condições de
 * corrida muito mais fáceis de observar.
 */
static void race_window(void)
{
    static _Thread_local unsigned int state = 0;

    if (state == 0)
        state = (unsigned int) (uintptr_t) &state | 1;
    state = state * 1103515245u + 12345u;
    if (((state >> 16) & 7) == 0)
        sched_yield();
}

unsigned long table_hash(const char *key)
{
    unsigned long hash = 14695981039346656037ul;
    for (; *key; key++) {
        hash ^= (unsigned char) *key;
        hash *= 1099511628211ul;
    }
    return hash;
}

static entry_t *table_find(table_t *self, const char *key)
{
    entry_t *e = self->buckets[table_hash(key) % self->n_buckets];
    while (e != NULL && strcmp(e->key, key) != 0)
        e = e->next;
    if (e == NULL) {
        fprintf(stderr, "[table] Key not found: %s\n", key);
        exit(1);
    }
    return e;
}

void table_init(table_t *self, unsigned int capacity)
{
    self->capacity = capacity;
    self->size = 0;
    self->n_buckets = 2 * capacity + 1;
    self->buckets = calloc(self->n_buckets, sizeof(entry_t *));
    self->entries = calloc(capacity, sizeof(entry_t));
}

void table_finalize(table_t *self)
{
    free(self->buckets);
    free(self->entries);
}

void table_insert(table_t *self, const char *key, int value)
{
    if (self->size == self->capacity) {
        fprintf(stderr, "[table] Table is full\n");
        exit(1);
    }
    entry_t *e = &self->entries[self->size++];
    unsigned long b = table_hash(key) % self->n_buckets;
    strncpy(e->key, key, KEY_LEN - 1);
    e->value = value;
    e->next = self->buckets[b];
    self->buckets[b] = e;
}

int table_get(table_t *self, const char *key)
{
    entry_t *e = table_find(self, key);
    race_window();
    return e->value;
}

void table_add(table_t *self, const char *key, int delta)
{
    entry_t *e = table_find(self, key);
    int value = e->value;
    race_window();
    e->value = value + delta;
}

unsigned int table_size(table_t *self)
{
    return self->size;
}

int table_value_at(table_t *self, unsigned int i)
{
    race_window();
    return self->entries[i].value;
}

/*============================================================================*
 * Disco                                                                      *
 *============================================================================*/

void disk_init(disk_t *self)
{
    self->file = fopen(WAL_FILE, "w+b");
    if (self->file == NULL) {
        perror("[disk] fopen");
        exit(1);
    }
    atomic_init(&self->durable_lsn, -1);
    atomic_init(&self->writing, 0);
    atomic_init(&self->concurrent_writes, 0);
    self->last_lsn = -1;
    self->batches = 0;
    self->records = 0;
    self->empty_batches = 0;
    self->order_errors = 0;
    plog("[disk] Initialized\n");
}

void disk_finalize(disk_t *self)
{
    fclose(self->file);
    plog("[disk] Finalized\n");
}

void disk_write_batch(disk_t *self, const log_record_t *records, unsigned int n)
{
    /* Detecta chamadas concorrentes. */
    if (atomic_exchange(&self->writing, 1)) {
        atomic_fetch_add(&self->concurrent_writes, 1);
        return;
    }

    if (n == 0) {
        self->empty_batches++;
        atomic_store(&self->writing, 0);
        return;
    }

    /* Confere a ordem dos LSNs. */
    for (unsigned int i = 0; i < n; i++) {
        if (records[i].lsn != self->last_lsn + 1)
            self->order_errors++;
        self->last_lsn = records[i].lsn;
    }

    /* Grava o lote e simula o fsync. */
    fwrite(records, sizeof(log_record_t), n, self->file);
    fflush(self->file);
    msleep(config.fsync_time);

    self->batches++;
    self->records += n;
    plog("[disk] Batch of %u records written (last LSN %ld)\n", n, self->last_lsn);

    atomic_store(&self->durable_lsn, self->last_lsn);
    atomic_store(&self->writing, 0);
}

long disk_durable_lsn(disk_t *self)
{
    return atomic_load(&self->durable_lsn);
}

unsigned long disk_read_all(log_record_t **out)
{
    FILE *f = fopen(WAL_FILE, "rb");
    unsigned long n = 0, cap = 1024;
    *out = malloc(cap * sizeof(log_record_t));

    if (f == NULL)
        return 0;
    while (fread(&(*out)[n], sizeof(log_record_t), 1, f) == 1) {
        if (++n == cap) {
            cap *= 2;
            *out = realloc(*out, cap * sizeof(log_record_t));
        }
    }
    fclose(f);
    return n;
}

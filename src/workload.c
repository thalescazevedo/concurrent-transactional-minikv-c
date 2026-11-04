/*
 * workload.c - Carga de trabalho e verificação do MiniKV: clientes, thread de
 * snapshot e estatísticas.
 *
 * ATENÇÃO: Este arquivo NÃO deve ser alterado!
 */

#include <stdlib.h>
#include "workload.h"
#include "server.h"

/*============================================================================*
 * Clientes                                                                   *
 *============================================================================*/

void client_init(client_t *self, db_t *db, unsigned int id, unsigned int seed)
{
    self->id = id;
    self->db = db;
    self->seed = seed;
    self->gets = 0;
    self->transfers_ok = 0;
    self->transfers_refused = 0;
    self->not_durable = 0;
}

void * client_run(void *arg)
{
    client_t *self = (client_t *) arg;
    char from[KEY_LEN], to[KEY_LEN];

    plog("[client] Client %u is now running!\n", self->id);

    for (unsigned int op = 0; op < config.ops; op++) {
        if ((unsigned int) rand_r(&self->seed) % 100 < TRANSFER_RATE) {
            /* Transferência entre duas contas distintas. */
            unsigned int a = rand_r(&self->seed) % config.accounts;
            unsigned int b = rand_r(&self->seed) % (config.accounts - 1);
            if (b >= a)
                b++;
            int amount = 1 + rand_r(&self->seed) % MAX_AMOUNT;
            account_key(a, from);
            account_key(b, to);

            long lsn = db_transfer(self->db, from, to, amount);
            if (lsn < 0) {
                self->transfers_refused++;
            } else {
                self->transfers_ok++;
                /* A transferência foi confirmada: ela já deve estar em disco. */
                if (disk_durable_lsn(self->db->wal->disk) < lsn)
                    self->not_durable++;
            }
            plog("[client] Client %u: %s -> %s (%d): LSN %ld\n", self->id, from, to, amount, lsn);
        } else {
            /* Consulta de saldo. */
            account_key(rand_r(&self->seed) % config.accounts, from);
            int balance = db_get(self->db, from);
            plog("[client] Client %u: %s = %d\n", self->id, from, balance);
            (void) balance;
            self->gets++;
        }
    }

    plog("[client] Client %u has finished!\n", self->id);
    return NULL;
}

/*============================================================================*
 * Snapshot                                                                   *
 *============================================================================*/

void snapshot_init(snapshot_t *self, db_t *db)
{
    self->db = db;
    atomic_init(&self->stop, 0);
    self->balances = malloc(config.accounts * sizeof(int));
    self->taken = 0;
    self->inconsistent = 0;
    self->total_wait_ms = 0;
    self->max_wait_ms = 0;
}

void snapshot_finalize(snapshot_t *self)
{
    free(self->balances);
}

void snapshot_stop(snapshot_t *self)
{
    atomic_store(&self->stop, 1);
}

void * snapshot_run(void *arg)
{
    snapshot_t *self = (snapshot_t *) arg;
    long expected = (long) config.accounts * INITIAL_BALANCE;

    plog("[snapshot] Snapshot thread is now running!\n");

    while (1) {
        msleep(config.snapshot_interval);
        if (atomic_load(&self->stop))
            break;

        double wait = db_snapshot(self->db, self->balances);

        long sum = 0;
        for (unsigned int i = 0; i < config.accounts; i++)
            sum += self->balances[i];

        self->taken++;
        self->total_wait_ms += wait;
        if (wait > self->max_wait_ms)
            self->max_wait_ms = wait;
        if (sum != expected) {
            self->inconsistent++;
            plog("[snapshot] Inconsistent snapshot: sum = %ld (expected %ld)\n", sum, expected);
        } else {
            plog("[snapshot] Snapshot %lu ok (waited %.1f ms)\n", self->taken, wait);
        }
    }

    plog("[snapshot] Snapshot thread has finished!\n");
    return NULL;
}

/*============================================================================*
 * Estatísticas                                                               *
 *============================================================================*/

static int key_index(const char *key)
{
    unsigned int i;
    if (sscanf(key, "conta-%u", &i) != 1 || i >= config.accounts)
        return -1;
    return (int) i;
}

int stats_print(server_t *server, disk_t *disk, double elapsed_ms)
{
    int errors = 0;
    long expected = (long) config.accounts * INITIAL_BALANCE;
    table_t *table = &server->db->table;

    /* Clientes. */
    unsigned long gets = 0, ok = 0, refused = 0, not_durable = 0;
    for (unsigned int i = 0; i < server->n_clients; i++) {
        gets += server->clients[i].gets;
        ok += server->clients[i].transfers_ok;
        refused += server->clients[i].transfers_refused;
        not_durable += server->clients[i].not_durable;
    }
    unsigned long ops = gets + ok + refused;

    printf(BAR);
    printf("Clients:\n");
    printf("  Operations         : %lu (%.0f ops/s)\n", ops, ops / (elapsed_ms / 1000.0));
    printf("  Balance queries    : %lu\n", gets);
    printf("  Transfers accepted : %lu\n", ok);
    printf("  Transfers refused  : %lu\n", refused);
    printf("  Confirmed before durable: %lu", not_durable);
    if (not_durable) { printf(" [NOT DURABLE]"); errors++; }
    printf("\n\n");

    /* Saldos em memória. */
    long sum = 0;
    unsigned int negative = 0;
    for (unsigned int i = 0; i < table_size(table); i++) {
        sum += table->entries[i].value;
        if (table->entries[i].value < 0)
            negative++;
    }
    printf("Balances in memory:\n");
    printf("  Sum                : %ld (expected %ld)", sum, expected);
    if (sum != expected) { printf(" [SUM MISMATCH]"); errors++; }
    printf("\n");
    printf("  Negative balances  : %u", negative);
    if (negative) { printf(" [NEGATIVE BALANCE]"); errors++; }
    printf("\n\n");

    /* Disco. */
    printf("Log writer:\n");
    printf("  fsyncs             : %lu\n", disk->batches);
    printf("  Records written    : %lu\n", disk->records);
    printf("  Average batch size : %.2f\n", disk->batches ? (double) disk->records / disk->batches : 0.0);
    printf("  Empty batches      : %lu", disk->empty_batches);
    if (disk->empty_batches) printf(" [BUSY WAITING?]");
    printf("\n");
    printf("  Out-of-order LSNs  : %lu", disk->order_errors);
    if (disk->order_errors) { printf(" [LOG ORDER]"); errors++; }
    printf("\n");
    unsigned long concurrent = atomic_load(&disk->concurrent_writes);
    printf("  Concurrent writes  : %lu", concurrent);
    if (concurrent) { printf(" [CONCURRENT WRITE]"); errors++; }
    printf("\n\n");

    /* Reconstrução a partir do log. */
    log_record_t *records;
    unsigned long n = disk_read_all(&records);
    long *replay = malloc(config.accounts * sizeof(long));
    unsigned long replay_negative = 0, bad_records = 0;
    for (unsigned int i = 0; i < config.accounts; i++)
        replay[i] = INITIAL_BALANCE;
    for (unsigned long r = 0; r < n; r++) {
        int from = key_index(records[r].from), to = key_index(records[r].to);
        if (from < 0 || to < 0 || records[r].lsn != (long) r) {
            bad_records++;
            continue;
        }
        if (replay[from] < records[r].amount)
            replay_negative++;
        replay[from] -= records[r].amount;
        replay[to] += records[r].amount;
    }
    unsigned long mismatches = 0;
    for (unsigned int i = 0; i < table_size(table); i++)
        if (replay[i] != table->entries[i].value)
            mismatches++;

    printf("Replay of %s:\n", WAL_FILE);
    printf("  Records            : %lu (expected %lu)", n, ok);
    if (n != ok) { printf(" [LOST TRANSFER]"); errors++; }
    printf("\n");
    printf("  Invalid records    : %lu", bad_records);
    if (bad_records) { printf(" [LOG ORDER]"); errors++; }
    printf("\n");
    printf("  Overdrafts         : %lu", replay_negative);
    if (replay_negative) { printf(" [NEGATIVE BALANCE]"); errors++; }
    printf("\n");
    printf("  Accounts differing : %lu", mismatches);
    if (mismatches) { printf(" [REPLAY MISMATCH]"); errors++; }
    printf("\n\n");
    free(replay);
    free(records);

    /* Snapshots. */
    snapshot_t *snap = &server->snapshot;
    printf("Snapshots:\n");
    printf("  Taken              : %lu\n", snap->taken);
    printf("  Inconsistent       : %lu", snap->inconsistent);
    if (snap->inconsistent) { printf(" [INCONSISTENT SNAPSHOT]"); errors++; }
    printf("\n");
    printf("  Average wait       : %.2f ms\n", snap->taken ? snap->total_wait_ms / snap->taken : 0.0);
    printf("  Maximum wait       : %.2f ms\n", snap->max_wait_ms);
    printf(BAR);

    printf(errors ? "%d error(s) detected!\n" : "No errors detected.\n", errors);
    return errors;
}

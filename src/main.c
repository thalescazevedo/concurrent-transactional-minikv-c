/*
 * main.c - Programa principal: lê os argumentos, inicializa tudo, cria os
 * clientes, encerra o servidor e imprime a verificação.
 *
 * ATENÇÃO: Este arquivo NÃO deve ser alterado!
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "storage.h"
#include "wal.h"
#include "db.h"
#include "server.h"
#include "workload.h"

config_t parse(int argc, char **argv)
{
    int c;
    config_t config = { 16, 500, 64, 8, 8, 5, 50 };

    while ((c = getopt(argc, argv, "c:n:a:s:b:f:p:h")) != -1) {
        switch (c) {
            case 'c':
                config.clients = atoi(optarg);
                break;
            case 'n':
                config.ops = atoi(optarg);
                break;
            case 'a':
                config.accounts = atoi(optarg);
                break;
            case 's':
                config.partitions = atoi(optarg);
                break;
            case 'b':
                config.buffer_size = atoi(optarg);
                break;
            case 'f':
                config.fsync_time = atoi(optarg);
                break;
            case 'p':
                config.snapshot_interval = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [OPTIONS]\n\nOptions:\n", argv[0]);
                printf("  -c  Number of clients (default 16).\n");
                printf("  -n  Operations per client (default 500).\n");
                printf("  -a  Number of accounts (default 64, minimum 2).\n");
                printf("  -s  Number of partitions (default 8).\n");
                printf("  -b  Log buffer capacity, in records (default 8).\n");
                printf("  -f  fsync time (default 5 ms).\n");
                printf("  -p  Interval between snapshots (default 50 ms).\n");
                printf("  -h  Prints this help.\n");
            default:
                exit(1);
        }
    }

    if (config.clients < 1 || config.accounts < 2 || config.partitions < 1 ||
        config.buffer_size < 1 || config.snapshot_interval < 1) {
        fprintf(stderr, "Invalid arguments (see -h).\n");
        exit(1);
    }

    return config;
}

/* --------------------------------------------------------- *
 *                                                           *
 *     ATENÇÃO: A FUNÇÃO MAIN NÃO PODERÁ SER ALTERADA!       *
 *                                                           *
 * --------------------------------------------------------- */

int main(int argc, char **argv)
{
    disk_t disk;
    wal_t wal;
    db_t db;
    server_t server;

    /* Faz o parsing dos parâmetros de entrada. */
    config = parse(argc, argv);

    /* Imprime as informações da execução. */
    printf(BAR);
    printf("Clients                 : %u\n", config.clients);
    printf("Operations per client   : %u\n", config.ops);
    printf("Accounts                : %u\n", config.accounts);
    printf("Partitions              : %u\n", config.partitions);
    printf("Log buffer capacity     : %u records\n", config.buffer_size);
    printf("fsync time              : %u ms\n", config.fsync_time);
    printf("Snapshot interval       : %u ms\n", config.snapshot_interval);
    printf(BAR);
    printf("\n");

    /* Inicializa o gerador de números aleatórios. */
    srand(time(NULL));

    /* Inicializa o disco, o log, o banco e o servidor. */
    disk_init(&disk);
    wal_init(&wal, &disk);
    db_init(&db, &wal);
    server_init(&server, &db, &wal);

    /* Inicia a thread de log e a thread de snapshot. */
    server_start(&server);

    /* Cria os clientes e espera todos terminarem. */
    double start = now_ms();
    for (unsigned int i = 0; i < config.clients; i++)
        server_spawn_client(&server);
    server_join_clients(&server);
    double elapsed = now_ms() - start;

    /* Encerra o servidor. */
    server_shutdown(&server);

    /* Verifica a execução e imprime as estatísticas. */
    int errors = stats_print(&server, &disk, elapsed);

    /* Finaliza tudo. */
    server_finalize(&server);
    db_finalize(&db);
    wal_finalize(&wal);
    disk_finalize(&disk);

    return errors ? 1 : 0;
}

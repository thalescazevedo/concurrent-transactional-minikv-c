/*
 * workload.h - Carga de trabalho e verificação do MiniKV.
 *
 *   - Clientes (client_*): threads que fazem consultas e transferências.
 *   - Snapshot (snapshot_*): thread que periodicamente copia todos os saldos
 *     com db_snapshot() e confere se a soma está correta.
 *   - Estatísticas (stats_*): verificação de corretude ao final da execução.
 *
 * ATENÇÃO: Este arquivo NÃO deve ser alterado!
 */

#ifndef __WORKLOAD_H__
#define __WORKLOAD_H__

    #include <pthread.h>
    #include <stdatomic.h>
    #include "db.h"

    struct server;

    /*============================================================================*
     * Clientes                                                                   *
     *============================================================================*/

    /**
     * @brief Um cliente do banco.
     */
    typedef struct client
    {
        unsigned int id;   /* Identificador do cliente.         */
        db_t *db;          /* Referência para o banco.          */
        pthread_t thread;  /* A thread do cliente.              */
        unsigned int seed; /* Semente para rand_r().            */

        unsigned long gets;              /* Consultas realizadas.                    */
        unsigned long transfers_ok;      /* Transferências aceitas.                  */
        unsigned long transfers_refused; /* Transferências recusadas (sem saldo).    */
        unsigned long not_durable;       /* Aceitas antes de estarem em disco.       */
    } client_t;

    /**
     * @brief Inicializa um cliente.
     */
    extern void client_init(client_t *self, db_t *db, unsigned int id, unsigned int seed);

    /**
     * @brief Função executada pela thread cliente: realiza config.ops operações
     * aleatórias (consultas e transferências).
     *
     * @param arg O argumento de entrada da thread, que deverá ser um client_t.
     */
    extern void * client_run(void *arg);

    /*============================================================================*
     * Snapshot                                                                   *
     *============================================================================*/

    /**
     * @brief A thread de snapshot e suas estatísticas.
     */
    typedef struct snapshot
    {
        db_t *db;         /* Referência para o banco.       */
        pthread_t thread; /* A thread de snapshot.          */
        atomic_int stop;  /* Pedido de término.             */
        int *balances;    /* Vetor que recebe os saldos.    */

        unsigned long taken;        /* Snapshots tirados.                 */
        unsigned long inconsistent; /* Snapshots com soma incorreta.      */
        double total_wait_ms;       /* Soma dos tempos de espera.         */
        double max_wait_ms;         /* Maior tempo de espera.             */
    } snapshot_t;

    /**
     * @brief Inicializa o snapshot.
     */
    extern void snapshot_init(snapshot_t *self, db_t *db);

    /**
     * @brief Finaliza o snapshot.
     */
    extern void snapshot_finalize(snapshot_t *self);

    /**
     * @brief Função executada pela thread de snapshot: a cada
     * config.snapshot_interval ms chama db_snapshot(), até snapshot_stop().
     *
     * @param arg O argumento de entrada da thread, que deverá ser um snapshot_t.
     */
    extern void * snapshot_run(void *arg);

    /**
     * @brief Pede o término da thread de snapshot. A thread termina em até
     * config.snapshot_interval ms (mais a duração de um snapshot). Não espera
     * a thread terminar: use pthread_join() para isso.
     */
    extern void snapshot_stop(snapshot_t *self);

    /*============================================================================*
     * Estatísticas                                                               *
     *============================================================================*/

    /**
     * @brief Verifica a corretude da execução e imprime as estatísticas.
     *
     * @param server O servidor (já encerrado com server_shutdown()).
     * @param disk O disco.
     * @param elapsed_ms Duração da fase dos clientes, em ms.
     *
     * @returns Número de erros encontrados.
     */
    extern int stats_print(struct server *server, disk_t *disk, double elapsed_ms);

#endif /*__WORKLOAD_H__*/

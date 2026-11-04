/*
 * server.h - O servidor: cria, espera e encerra as threads (clientes, thread
 * de log e thread de snapshot).
 *
 * Adicione atributos às structs e assinaturas de novas funções onde indicado.
 */

#ifndef __SERVER_H__
#define __SERVER_H__

    #include <pthread.h>
    #include "db.h"
    #include "wal.h"
    #include "workload.h"

    /**
     * @brief O servidor: cria e encerra as threads do banco.
     */
    typedef struct server
    {
        db_t *db;              /* Referência para o banco.          */
        wal_t *wal;            /* Referência para o log.            */
        client_t *clients;     /* Clientes.                         */
        unsigned int n_clients;/* Número de clientes criados.       */
        snapshot_t snapshot;   /* A thread de snapshot.             */
        pthread_t log_writer;  /* A thread de log.                  */

        /* TODO: Adicione aqui os atributos que achar necessários para implementar o
        comportamento do servidor. Esses atributos deverão ser usados pelas funções
        do servidor. */
    } server_t;

    /*============================================================================*
     * Funções utilizadas em arquivos que incluem esse .h                         *
     *============================================================================*/

    /**
     * @brief Inicializa o servidor.
     *
     * @param self O servidor.
     * @param db O banco.
     * @param wal O log.
     */
    extern void server_init(server_t *self, db_t *db, wal_t *wal);

    /**
     * @brief Finaliza o servidor.
     *
     * @param self O servidor.
     */
    extern void server_finalize(server_t *self);

    /**
     * @brief Cria e inicia a thread de log e a thread de snapshot.
     *
     * @param self O servidor.
     */
    extern void server_start(server_t *self);

    /**
     * @brief Cria uma thread cliente e a executa.
     *
     * @param self O servidor.
     */
    extern void server_spawn_client(server_t *self);

    /**
     * @brief Espera todos os clientes terminarem. (JÁ PRONTA)
     *
     * @param self O servidor.
     */
    extern void server_join_clients(server_t *self);

    /**
     * @brief Encerra o servidor depois que todos os clientes terminaram: garante
     * que todos os registros pendentes foram gravados em disco, encerra a thread
     * de log e a thread de snapshot e espera essas threads terminarem.
     *
     * @param self O servidor.
     */
    extern void server_shutdown(server_t *self);

    /*============================================================================*
     * ATENÇÃO: Insira aqui funções que você quiser adicionar à interface para    *
     * serem usadas em arquivos que incluem esse header.                          *
     *============================================================================*/

#endif /*__SERVER_H__*/

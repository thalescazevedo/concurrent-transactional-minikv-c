/*
 * db.h - O banco: consultas, transferências e snapshots sobre a tabela de
 * contas, divididas em partições.
 *
 * Adicione atributos às structs e assinaturas de novas funções onde indicado.
 */

#ifndef __DB_H__
#define __DB_H__

    #include <pthread.h>
    #include <semaphore.h>
    #include "wal.h"

    /**
     * @brief O banco de dados.
     */
    typedef struct db
    {
        table_t table; /* Tabela hash com as contas (sem sincronização). */
        wal_t *wal;    /* Referência para o log.                          */

        /* TODO: Adicione aqui os atributos que achar necessários para implementar o
        comportamento do banco. Esses atributos deverão ser usados pelas funções
        do banco. */
    } db_t;

    /*============================================================================*
     * Funções utilizadas em arquivos que incluem esse .h                         *
     *============================================================================*/

    /**
     * @brief Inicializa o banco com config.accounts contas, cada uma com saldo
     * INITIAL_BALANCE, divididas em config.partitions partições.
     *
     * @param self O banco.
     * @param wal O log.
     */
    extern void db_init(db_t *self, wal_t *wal);

    /**
     * @brief Finaliza o banco.
     *
     * @param self O banco.
     */
    extern void db_finalize(db_t *self);

    /**
     * @brief Retorna a partição da conta key, no intervalo [0, config.partitions).
     */
    extern unsigned int db_partition_of(db_t *self, const char *key);

    /**
     * @brief Consulta o saldo de uma conta. (JÁ PRONTA)
     *
     * @returns O saldo da conta key.
     */
    extern int db_get(db_t *self, const char *key);

    /**
     * @brief Transfere amount da conta from para a conta to. (JÁ PRONTA)
     *
     * @returns O LSN do registro de log da transferência, ou -1 se a transferência
     * foi recusada por falta de saldo.
     */
    extern long db_transfer(db_t *self, const char *from, const char *to, int amount);

    /**
     * @brief Copia os saldos de todas as contas para balances. (JÁ PRONTA)
     *
     * @param balances Vetor com config.accounts posições.
     *
     * @returns Quanto tempo (em ms) o snapshot esperou em db_snapshot_begin().
     */
    extern double db_snapshot(db_t *self, int *balances);

    /**
     * @brief Protege uma consulta à conta key. Várias consultas a uma mesma
     * partição podem acontecer ao mesmo tempo, mas nunca durante uma
     * transferência que modifica aquela partição.
     */
    extern void db_acquire_read(db_t *self, const char *key);

    /**
     * @brief Libera a proteção adquirida com db_acquire_read().
     */
    extern void db_release_read(db_t *self, const char *key);

    /**
     * @brief Protege as contas from e to para uma transferência. As duas contas
     * podem estar em partições diferentes ou na mesma partição.
     */
    extern void db_acquire_pair(db_t *self, const char *from, const char *to);

    /**
     * @brief Libera a proteção adquirida com db_acquire_pair().
     */
    extern void db_release_pair(db_t *self, const char *from, const char *to);

    /**
     * @brief Inicia um snapshot: ao retornar, nenhuma transferência pode estar
     * em andamento nem começar até db_snapshot_end().
     */
    extern void db_snapshot_begin(db_t *self);

    /**
     * @brief Termina um snapshot, permitindo que as transferências continuem.
     */
    extern void db_snapshot_end(db_t *self);

    /*============================================================================*
     * ATENÇÃO: Insira aqui funções que você quiser adicionar à interface para    *
     * serem usadas em arquivos que incluem esse header.                          *
     *============================================================================*/

#endif /*__DB_H__*/

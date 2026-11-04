/*
 * storage.h - Armazenamento do MiniKV.
 *
 *   - Tabela hash (table_*): guarda os saldos das contas em memória. NÃO possui
 *     nenhuma sincronização.
 *   - Disco (disk_*): o arquivo de log (WAL_FILE), onde a thread de log grava os
 *     registros em lotes, com um fsync simulado por lote.
 *
 * ATENÇÃO: Este arquivo NÃO deve ser alterado!
 */

#ifndef __STORAGE_H__
#define __STORAGE_H__

    #include <stdatomic.h>
    #include "config.h"

    /*============================================================================*
     * Tabela hash                                                                *
     *                                                                            *
     * Todas as entradas são inseridas na inicialização (antes de existir         *
     * qualquer thread) e nenhuma entrada é inserida ou removida depois disso:    *
     * apenas os valores mudam.                                                   *
     *============================================================================*/

    /**
     * @brief Uma entrada da tabela hash.
     */
    typedef struct entry
    {
        char key[KEY_LEN];  /* Chave.                         */
        int value;          /* Valor (saldo da conta).        */
        struct entry *next; /* Próxima entrada do mesmo bucket. */
    } entry_t;

    /**
     * @brief Tabela hash com encadeamento.
     */
    typedef struct table
    {
        entry_t **buckets;      /* Listas de entradas por bucket.             */
        unsigned int n_buckets; /* Número de buckets.                         */
        entry_t *entries;       /* Entradas, na ordem de inserção.            */
        unsigned int size;      /* Número de entradas inseridas.              */
        unsigned int capacity;  /* Número máximo de entradas.                 */
    } table_t;

    /*============================================================================*
     * Funções utilizadas em arquivos que incluem esse .h                         *
     *============================================================================*/

    /**
     * @brief Inicializa uma tabela vazia com espaço para capacity entradas.
     */
    extern void table_init(table_t *self, unsigned int capacity);

    /**
     * @brief Finaliza a tabela.
     */
    extern void table_finalize(table_t *self);

    /**
     * @brief Insere uma nova entrada. Só deve ser usada na inicialização.
     */
    extern void table_insert(table_t *self, const char *key, int value);

    /**
     * @brief Retorna o valor associado a key.
     */
    extern int table_get(table_t *self, const char *key);

    /**
     * @brief Soma delta ao valor associado a key.
     */
    extern void table_add(table_t *self, const char *key, int delta);

    /**
     * @brief Retorna o número de entradas da tabela.
     */
    extern unsigned int table_size(table_t *self);

    /**
     * @brief Retorna o valor da i-ésima entrada (na ordem de inserção).
     */
    extern int table_value_at(table_t *self, unsigned int i);

    /**
     * @brief Função de hash (FNV-1a) usada pela tabela e pelas partições.
     */
    extern unsigned long table_hash(const char *key);

    /*============================================================================*
     * Disco                                                                      *
     *============================================================================*/

    /**
     * @brief Um registro de log: descreve uma transferência aceita.
     */
    typedef struct log_record
    {
        long lsn;           /* Log sequence number: 0, 1, 2, ... */
        char from[KEY_LEN]; /* Conta de origem.                  */
        char to[KEY_LEN];   /* Conta de destino.                 */
        int amount;         /* Valor transferido.                */
    } log_record_t;

    /**
     * @brief O "disco": o arquivo de log e as estatísticas de escrita.
     */
    typedef struct disk
    {
        FILE *file;                  /* Arquivo de log.                                  */
        atomic_long durable_lsn;     /* Maior LSN já gravado (-1 se nenhum).             */
        atomic_int writing;          /* Há uma escrita em andamento?                     */
        long last_lsn;               /* LSN do último registro gravado.                  */
        unsigned long batches;       /* Número de lotes gravados (= número de fsyncs).   */
        unsigned long records;       /* Número de registros gravados.                    */
        unsigned long empty_batches; /* Chamadas com lote vazio.                         */
        unsigned long order_errors;  /* Registros fora de ordem, repetidos ou com lacuna. */
        atomic_ulong concurrent_writes; /* Chamadas concorrentes a disk_write_batch().   */
    } disk_t;

    /*============================================================================*
     * Funções utilizadas em arquivos que incluem esse .h                         *
     *============================================================================*/

    /**
     * @brief Inicializa o disco, criando um arquivo de log vazio (WAL_FILE).
     */
    extern void disk_init(disk_t *self);

    /**
     * @brief Finaliza o disco, fechando o arquivo de log.
     */
    extern void disk_finalize(disk_t *self);

    /**
     * @brief Grava um lote de registros no arquivo de log e faz um fsync, que
     * demora config.fsync_time ms. Ao retornar, todos os registros do lote
     * estão em disco.
     *
     * A função deve ser chamada por uma thread de cada vez. Os registros devem
     * estar em ordem crescente de LSN, continuando a partir do último registro
     * gravado, sem lacunas nem repetições.
     *
     * @param records Vetor com os registros.
     * @param n Número de registros no vetor.
     */
    extern void disk_write_batch(disk_t *self, const log_record_t *records, unsigned int n);

    /**
     * @brief Retorna o maior LSN já gravado em disco (-1 se nenhum).
     */
    extern long disk_durable_lsn(disk_t *self);

    /**
     * @brief Lê todos os registros do arquivo de log. Usada na verificação.
     *
     * @param out Recebe um vetor alocado com malloc() (deve ser liberado com free()).
     *
     * @returns Número de registros lidos.
     */
    extern unsigned long disk_read_all(log_record_t **out);

#endif /*__STORAGE_H__*/

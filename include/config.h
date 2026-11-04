/*
 * config.h - Configuração da execução (argumentos de linha de comando),
 * constantes e funções auxiliares (msleep, now_ms, account_key, plog).
 *
 * ATENÇÃO: Este arquivo NÃO deve ser alterado!
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

    #include <stdio.h>

    /*============================================================================*
     * Constantes                                                                 *
     *============================================================================*/

    #define INITIAL_BALANCE 1000      /* Saldo inicial de cada conta.                   */
    #define TRANSFER_RATE   50        /* Percentual de operações que são transferências. */
    #define MAX_AMOUNT      200       /* Valor máximo de uma transferência.             */
    #define KEY_LEN         16        /* Tamanho máximo de uma chave (com o '\0').      */
    #define WAL_FILE        "wal.log" /* Arquivo do log.                                */

    /*============================================================================*
     * Configuração da execução                                                   *
     *============================================================================*/

    typedef struct config {
        unsigned int clients;           /* Número de threads cliente (-c).            */
        unsigned int ops;               /* Operações por cliente (-n).                */
        unsigned int accounts;          /* Número de contas (-a).                     */
        unsigned int partitions;        /* Número de partições (-s).                  */
        unsigned int buffer_size;       /* Capacidade do buffer de log (-b).          */
        unsigned int fsync_time;        /* Tempo de um fsync, em ms (-f).             */
        unsigned int snapshot_interval; /* Intervalo entre snapshots, em ms (-p).     */
    } config_t;

    extern config_t config;

    /*============================================================================*
     * Macros                                                                     *
     *============================================================================*/

    /**
     * @brief Imprime um log em modo debug.
     */
    #ifndef NDEBUG
        #define plog(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)
    #else
        #define plog(...)
    #endif

    /**
     * @brief Separador.
     */
    #define BAR "=====================================================================\n"

    /*============================================================================*
     * Funções                                                                    *
     *============================================================================*/

    /**
     * @brief Coloca a thread corrente para dormir.
     *
     * @param msec Tempo em milissegundos.
     *
     * @returns 0 em caso de sucesso e -1 em caso de erro.
     */
    extern int msleep(long msec);

    /**
     * @brief Retorna o tempo corrente (relógio monotônico) em milissegundos.
     */
    extern double now_ms(void);

    /**
     * @brief Escreve em buf a chave da conta de índice i (ex.: "conta-0042").
     *
     * @param i Índice da conta, no intervalo [0, config.accounts).
     * @param buf Buffer com pelo menos KEY_LEN bytes.
     */
    extern void account_key(unsigned int i, char *buf);

#endif /*__CONFIG_H__*/

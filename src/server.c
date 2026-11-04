/*
 * server.c - O servidor: cria, espera e encerra as threads.
 *
 * Implemente server_shutdown e complete as demais funções se necessário.
 *
 * VOCÊ DEVE ALTERAR ESTE ARQUIVO.
 */

#include <stdlib.h>
#include "server.h"

void server_init(server_t *self, db_t *db, wal_t *wal)
{
    self->db = db;
    self->wal = wal;
    self->clients = malloc(config.clients * sizeof(client_t));
    self->n_clients = 0;
    snapshot_init(&self->snapshot, db);

    /* TODO: Adicionar código aqui se necessário! */

    plog("[server] Initialized\n");
}

void server_finalize(server_t *self)
{
    /* TODO: Adicionar código aqui se necessário! */

    snapshot_finalize(&self->snapshot);
    free(self->clients);
    plog("[server] Finalized\n");
}

void server_start(server_t *self)
{
    /* TODO: Adicionar código aqui se necessário! */

    /* Cria a thread de log e a thread de snapshot. */
    pthread_create(&self->log_writer, NULL, wal_writer_run, (void *) self->wal);
    pthread_create(&self->snapshot.thread, NULL, snapshot_run, (void *) &self->snapshot);

    plog("[server] Started\n");
}

void server_spawn_client(server_t *self)
{
    unsigned int id = self->n_clients;

    /* Inicializa o cliente. */
    client_init(&self->clients[id], self->db, id, (unsigned int) rand());

    /* TODO: Adicionar código aqui se necessário! */

    /* Cria a thread cliente. */
    pthread_create(&self->clients[id].thread, NULL, client_run, (void *) &self->clients[id]);
    self->n_clients++;

    plog("[server] Client %u created!\n", id);
}

void server_shutdown(server_t *self)
{
    /* TODO: Garantir que todos os registros pendentes foram gravados em disco
       e encerrar a thread de log, mesmo que ela esteja bloqueada esperando
       registros. */

    /* Encerra a thread de snapshot. */
    snapshot_stop(&self->snapshot);

    /* Espera a thread de log e a thread de snapshot terminarem. */
    pthread_join(self->log_writer, NULL);
    pthread_join(self->snapshot.thread, NULL);

    plog("[server] Shut down\n");
}

/* --------------------------------------------------------- */
/* ATENÇÃO: Não será necessário modificar as funções abaixo! */
/* --------------------------------------------------------- */

void server_join_clients(server_t *self)
{
    for (unsigned int i = 0; i < self->n_clients; i++)
        pthread_join(self->clients[i].thread, NULL);
}

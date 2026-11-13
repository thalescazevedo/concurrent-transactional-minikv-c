/*
 * db.c - O banco: consultas, transferências e snapshots.
 *
 * Implemente as funções de sincronização (db_acquire_*, db_release_*,
 * db_snapshot_begin/end). As operações db_get, db_transfer e db_snapshot, no
 * final do arquivo, já estão prontas e NÃO devem ser alteradas.
 *
 * VOCÊ DEVE ALTERAR ESTE ARQUIVO.
 */

#include <stdlib.h>
#include "db.h"

/* Edit - Thales: 
    Aloquei espaço na memória e iniciei um rwlock para cada partição.
    Declarei previamente no Header, defini aqui.
*/

void db_init(db_t *self, wal_t *wal)
{
    /* Cria as contas (NÃO ALTERE!). */
    char key[KEY_LEN];
    self->wal = wal;
    table_init(&self->table, config.accounts);
    for (unsigned int i = 0; i < config.accounts; i++) {
        account_key(i, key);
        table_insert(&self->table, key, INITIAL_BALANCE);
    }

    /* TODO: Adicionar código aqui se necessário! */

    /* Part 1 */
    self->rwlocks = malloc(config.partitions * sizeof(pthread_rwlock_t));
    
    for (unsigned int i = 0; i < config.partitions; i++) {
        pthread_rwlock_init(&self->rwlocks[i], NULL);
    }

    /* Part 3*/

    pthread_rwlock_init(&self->snaplock, NULL);

    plog("[db] Initialized\n");
}

/* Edit - Thales: 
    Destrui todos os rwlock - um para cada partição
*/

void db_finalize(db_t *self)
{
    /* TODO: Adicionar código aqui se necessário! */

    /* Part 1 */
    for (int i = 0; i < config.partitions; i++) {
        pthread_rwlock_destroy(&self->rwlocks[i]);
    }
    free(self->rwlocks);
    
    /* Part 3 */

    pthread_rwlock_destroy(&self->snaplock);

    table_finalize(&self->table);
    plog("[db] Finalized\n");
}

/*
================================================================
                        Parte 1 - Thales                 
================================================================
*/


// Serve para travar a partição do Banco de Dados que vai ser acessada
void db_acquire_read(db_t *self, const char *key)
{
    unsigned int partition = db_partition_of(self,key);
    pthread_rwlock_rdlock(&self->rwlocks[partition]);
}

// Serve para liberar a partição do Banco de Dados que vai ser acessada
void db_release_read(db_t *self, const char *key)
{
    unsigned int partition = db_partition_of(self,key);
    pthread_rwlock_unlock(&self->rwlocks[partition]);
}

void db_acquire_pair(db_t *self, const char *from, const char *to)
{
    /* ======= Trecho da parte 3 =======*/
    pthread_rwlock_rdlock(&self->snaplock);
    /* =================================*/
    unsigned int p_from = db_partition_of(self, from);
    unsigned int p_to   = db_partition_of(self, to);

    unsigned int p1 = (p_from < p_to) ? p_from : p_to;
    unsigned int p2 = (p_from < p_to) ? p_to   : p_from;
    
    // Tranco a partição de menor valor primeiro
    /* Isso evita o caso em que uma thread tranca uma, outra tranca outra
    e nenhuma das duas consegue finalizar sua operação */
    pthread_rwlock_wrlock(&self->rwlocks[p1]);
    
    // Tranco a partição 2 APENAS SE não for a mesma que já está trancada
    /* Isso evita o caso em que se tenta trancar uma partição que já estava trancada,
    o que prenderia infinitamente o programa aqui, já que para ela ser destrancada, teria que
    necessariamente sair daqui.
    */
    if (p1 != p2){
        pthread_rwlock_wrlock(&self->rwlocks[p2]);
    }
}

void db_release_pair(db_t *self, const char *from, const char *to)
{

    unsigned int p_from = db_partition_of(self, from);
    unsigned int p_to   = db_partition_of(self, to);

    unsigned int p1 = (p_from < p_to) ? p_from: p_to;
    unsigned int p2 = (p_from < p_to) ? p_to  : p_from;
    
    // Destranco a partição de menor valor primeiro
    /* É necessário manter essa lógica pois pode ser que a partition_2 não esteja trancada,
    e é impossível destrancar algo que não estava trancado */
    pthread_rwlock_unlock(&self->rwlocks[p1]);
    
    if (p1 != p2){
        pthread_rwlock_unlock(&self->rwlocks[p2]);
    }

    /* ======= Trecho da parte 3 =======*/
    pthread_rwlock_unlock(&self->snaplock);
    /* =================================*/
}

/*
================================================================
                        Parte 3                 
================================================================
*/

void db_snapshot_begin(db_t *self)
{
    pthread_rwlock_wrlock(&self->snaplock);
}

void db_snapshot_end(db_t *self)
{
    pthread_rwlock_unlock(&self->snaplock);
}

/*
================================================================
*/

/* --------------------------------------------------------- */
/* ATENÇÃO: Não será necessário modificar as funções abaixo! */
/* --------------------------------------------------------- */

unsigned int db_partition_of(db_t *self, const char *key)
{
    return table_hash(key) % config.partitions;
}

int db_get(db_t *self, const char *key)
{
    db_acquire_read(self, key);
    int balance = table_get(&self->table, key);
    db_release_read(self, key);
    return balance;
}

long db_transfer(db_t *self, const char *from, const char *to, int amount)
{
    long lsn = -1;

    /* Trava as duas contas e, se houver saldo, transfere e registra no log. */
    db_acquire_pair(self, from, to);
    if (table_get(&self->table, from) >= amount) {
        table_add(&self->table, from, -amount);
        table_add(&self->table, to, amount);
        lsn = wal_append(self->wal, from, to, amount);
    }
    db_release_pair(self, from, to);

    /* Só confirma ao cliente depois que o registro estiver em disco. */
    if (lsn >= 0)
        wal_wait_durable(self->wal, lsn);

    return lsn;
}

double db_snapshot(db_t *self, int *balances)
{
    double start = now_ms();
    db_snapshot_begin(self);
    double waited = now_ms() - start;

    for (unsigned int i = 0; i < table_size(&self->table); i++)
        balances[i] = table_value_at(&self->table, i);

    db_snapshot_end(self);
    return waited;
}

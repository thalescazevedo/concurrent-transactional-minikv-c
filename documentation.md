# Part 1

## Thales

A primeira vez que fiz, comecei usando mutex. Após reler o enunciado, fiz uma busca exploratória acerca de RWLOCKS e então optei por utilizá-los. Para seguir a premissa do enunciado também de evitar declarar variáveis globais, declarei-o no escopo da struct db, no header. Depois, em sequência:

### em db_init:

1. Aloquei dinamicamente memória para o array de RWLOCKS (um rwlock para cada partição);
2. Inicializei os RWLOCKS.

### em db_finalize:

1. Destruí todos os RWLOCKS;
2. Liberei a memória alocada.

### nos métodos read:

1. Detectei a partição;
2. Usei o método rdlock - ele tranca apenas para leitura

- Isso é o principal fator que torna o RWLOCK melhor do que Mutex

### nos métodos write:

1. Detecto as duas partições;
2. Tranco as partições
3. Primeira observação: Para evitar Deadlock, tranco a partição de menor valor primeiro:
   - Isso evita o caso em que uma thread tranca uma, outra tranca outra e nenhuma das duas consegue finalizar sua operação
4. Segunda observação: Para evitar Deadlock, tranco a segunda partição apenas se ela for diferente da primeira:
   - Isso evita o caso em que se tenta trancar uma partição que já estava trancada, o que prenderia infinitamente o programa aqui, já que para ela ser destrancada, teria que necessariamente sair daqui.
5. No método de desbloqueio, destranco a partição de menor valor primeiro e só destranco a segunda se ela foi trancada anteriormente.

# Part 3

## Thales

Para essa parte, o ideal é novamente usar RWLOCK para não trancar o banco inteiro ao fazer snapshots (além de não trancar para cada operação) - operações de leitura devem continuar sendo permitidas.

1. Declarar no header, na struct db "pthread_rwlock_t snaplock;";
2. Adicionar inicialização e destruição em db_init e db_finalize;
3. Implementar wrlock em db_snapshot_begin(...) para inibir novas solicitações de escritas durante o snapshot - age como um mutex trancando todo mundo, mas com a vantagem de que quando liberado, várias ações voltam ao mesmo tempo, e não só uma por vez como no mutex;
4. Implementar rdlock em db_acquire_pair(...) para servir como semáforo para o wrlock: ele vai "suspender" temporariamente a operação até que o wrlock seja liberado. O wrlock depende completamente dele pra funcionar, senão, seria apenas algo bloqueando nada.
5. Liberar os wrlock e rdlock nos respectivos métodos.

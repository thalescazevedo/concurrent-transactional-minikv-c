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

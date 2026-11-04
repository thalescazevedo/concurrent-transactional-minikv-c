# MiniKV: Banco Chave-Valor Transacional e Concorrente

[![C11](https://img.shields.io/badge/Language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![POSIX Threads](https://img.shields.io/badge/API-POSIX_Threads_(pthreads)-orange.svg)](https://en.wikipedia.org/wiki/Pthreads)
[![UFSC](https://img.shields.io/badge/Institution-UFSC-green.svg)](https://ufsc.br/)

O **MiniKV** é um projeto de banco de dados chave-valor transacional em memória desenvolvido em linguagem C com suporte a concorrência via threads POSIX (`pthreads`). O projeto simula o funcionamento interno de sistemas de gerência de banco de dados modernos (como PostgreSQL, MySQL e Redis), enfrentando e solucionando desafios clássicos de concorrência: acesso paralelo de múltiplos clientes com granularidade ajustável, garantia de durabilidade com escrita em lote (*group commit*) e extração de cópias consistentes da memória (*snapshots*) sem causar inanição (*starvation*) ou condições de corrida.

Este projeto foi desenvolvido como trabalho prático na disciplina de **Programação Concorrente** do curso de graduação da **Universidade Federal de Santa Catarina (UFSC)**.

---

## 📌 Sumário
- [MiniKV: Banco Chave-Valor Transacional e Concorrente](#minikv-banco-chave-valor-transacional-e-concorrente)
  - [📌 Sumário](#-sumário)
  - [💡 Visão Geral e Objetivos](#-visão-geral-e-objetivos)
    - [Desafios Concorrentes Abordados](#desafios-concorrentes-abordados)
  - [🏛️ Arquitetura do Sistema](#️-arquitetura-do-sistema)
    - [1. Controle de Concorrência e Particionamento](#1-controle-de-concorrência-e-particionamento)
    - [2. Durabilidade com Write-Ahead Log (WAL) e Group Commit](#2-durabilidade-com-write-ahead-log-wal-e-group-commit)
    - [3. Snapshots Consistentes e Prevenção de Starvation](#3-snapshots-consistentes-e-prevenção-de-starvation)
    - [4. Gerenciamento e Encerramento Gracioso](#4-gerenciamento-e-encerramento-gracioso)
  - [📂 Estrutura do Repositório](#-estrutura-do-repositório)
    - [Divisão entre Código Base e Implementação](#divisão-entre-código-base-e-implementação)
  - [🛠️ Compilação e Execução](#️-compilação-e-execução)
    - [Pré-requisitos](#pré-requisitos)
    - [Instruções de Compilação](#instruções-de-compilação)
    - [Parâmetros de Execução](#parâmetros-de-execução)
    - [Exemplos de Uso](#exemplos-de-uso)
  - [🧪 Verificação de Corretude e Diagnóstico](#-verificação-de-corretude-e-diagnóstico)
  - [📜 Créditos e Direitos Autorais](#-créditos-e-direitos-autorais)
    - [Autoria do Código Base e Especificação](#autoria-do-código-base-e-especificação)
    - [Autoria das Soluções de Sincronização](#autoria-das-soluções-de-sincronização)

---

## 💡 Visão Geral e Objetivos

O MiniKV armazena saldos de contas bancárias (identificadas por chaves textuais como `"conta-0042"`), mantendo como principal invariante a **conservação da soma total dos saldos**: qualquer transferência entre contas apenas move valores de um saldo para outro; portanto, a soma de todos os saldos em qualquer instante consistente deve se manter idêntica ao valor inicial.

O sistema é submetido a uma carga de trabalho simultânea composta por:
- **$C$ Threads Clientes:** executam sequências aleatórias de consultas de saldo (`get`) e transferências monetárias (`transfer`);
- **1 Thread de Log (*Log Writer*):** responsável por descarregar registros de operações pendentes em disco;
- **1 Thread de Snapshot:** responsável por capturar periodicamente o estado completo de todas as contas em memória.

### Desafios Concorrentes Abordados
1. **Isolamento e Atomicidade:** Permitir leitura e escrita concorrentes de forma granular sem corrupção de memória ou leituras parciais de transferências.
2. **Prevenção de Deadlocks:** Evitar impasses ao adquirir travas sobre múltiplos recursos simultaneamente (duas contas envolvidas em uma transferência).
3. **Durabilidade e Eficiência de E/S:** Agrupar operações em lote no buffer de log (*group commit*) para amortizar o custo de sincronização em disco (`fsync`), liberando os clientes de forma sincronizada apenas após seus dados estarem gravados.
4. **Coordenação com Snapshots:** Capturar uma visão global consistente do banco sem travar indefinidamente as operações regulares (*starvation* de leitores/escritores ou da própria thread de snapshot).

---

## 🏛️ Arquitetura do Sistema

```
                      +-----------------------------+
                      |       CLIENT THREADS        |
                      |  (Transfer / Balance Get)   |
                      +--------------+--------------+
                                     |
              +----------------------+----------------------+
              |                                             |
              v                                             v
+-----------------------------+               +-----------------------------+
|    BANCO PARTICIONADO       |               |     WRITE-AHEAD LOG (WAL)   |
|   (Tabela Hash em Memória)  |               |       (Group Commit)        |
+-----------------------------+               +-----------------------------+
| Partição 0: [Mutex/RWLock]  |               |  Ring Buffer / Log Queue    |
| Partição 1: [Mutex/RWLock]  |  LSN append   |  (Capacidade: B registros)  |
| ...                         +-------------->+-----------------------------+
| Partição S-1: [Mutex/RWLock]|               |              |              |
+--------------+--------------+               +--------------+--------------+
               ^                                             |
               |                                             v Batch Flush
+--------------+--------------+               +-----------------------------+
|      SNAPSHOT THREAD        |               |       LOG WRITER THREAD     |
| (Cópia Atômica Global sem   |               |     (disk_write_batch)      |
|         Starvation)         |               +--------------+--------------+
+-----------------------------+                              |
                                                             v fsync
                                              +-----------------------------+
                                              |       DISCO (wal.log)       |
                                              +-----------------------------+
```

### 1. Controle de Concorrência e Particionamento
- A tabela hash em memória é dividida logicamente em **$S$ partições**. A partição de cada conta é determinada por `hash(chave) % S`.
- **Granularidade da Sincronização:** Em vez de uma trava global única que tornaria o banco um gargalo sequencial, cada partição possui seus próprios mecanismos de sincronização (leitores/escritores).
  - Operações de consulta (`db_get`) requerem acesso compartilhado de leitura à partição correspondente.
  - Transferências (`db_transfer`) exigem bloqueio exclusivo de escrita nas partições das contas envolvidas.
- **Prevenção de Deadlock:** Ao transferir entre contas pertencentes a partições distintas, as travas devem ser adquiridas seguindo uma ordem estrita e bem definida (por exemplo, ordenação pelo identificador da partição) para evitar condições de espera circular.

### 2. Durabilidade com Write-Ahead Log (WAL) e Group Commit
- Para garantir que nenhuma operação confirmada seja perdida em caso de falha, cada transferência bem-sucedida gera uma entrada no WAL com um número sequencial único denominado **LSN** (*Log Sequence Number*).
- **Group Commit:** Em vez de executar uma chamada lenta de `fsync` para cada transferência, os clientes depositam suas operações em um buffer compartilhado com capacidade para até $B$ registros (`wal_append`).
- A thread dedicativa de log (`wal_writer_run`) retira os lotes acumulados e grava todos os registros pendentes de uma só vez via `disk_write_batch`.
- Os clientes permanecem em espera bloqueante coordenada (`wal_wait_durable`) até que o LSN de sua respectiva operação seja consolidado em disco, garantindo durabilidade sem espera ocupada (*busy waiting*).

### 3. Snapshots Consistentes e Prevenção de Starvation
- A cada intervalo de $P$ milissegundos, a thread de snapshot captura uma imagem completa de todos os saldos em memória.
- O snapshot deve refletir um instante de tempo logicamente válido: a soma total de todos os saldos no snapshot deve bater exatamente com o total inicial, sem que nenhuma transferência apareça computada pela metade.
- **Equilíbrio de Vazão:** O mecanismo de barreira/pausa dos snapshots impede que clientes contínuos causem starvation na rotina de snapshot, e vice-versa, retomando o fluxo normal do banco logo após a cópia dos dados.

### 4. Gerenciamento e Encerramento Gracioso
- Ao término das operações de todos os clientes, o servidor realiza um encerramento ordenado (`server_shutdown`):
  1. Esvazia e sincroniza qualquer registro de log remanescente no buffer;
  2. Notifica e finaliza de forma segura a thread de log;
  3. Sinaliza o encerramento da thread de snapshot (`snapshot_stop`);
  4. Aguarda a finalização (`pthread_join`) de todas as threads operárias e desaloca os recursos.

---

## 📂 Estrutura do Repositório

O projeto é organizado nos seguintes diretórios e arquivos:

```plaintext
.
├── Makefile                # Regras de compilação, debug e empacotamento
├── compile_flags.txt       # Configuração para analisadores estáticos (clangd)
├── ENUNCIADO.pdf           # Especificação acadêmica detalhada do trabalho
├── include/
│   ├── config.h            # Tipos e estruturas de configuração global
│   ├── db.h                # Definição da interface e estrutura do banco (db_t)
│   ├── server.h            # Definição do ciclo de vida do servidor (server_t)
│   ├── storage.h           # Tabela hash e operações de escrita em disco
│   ├── wal.h               # Definição do WAL e buffer de log (wal_t)
│   └── workload.h          # Gerador de carga de clientes e verificação
└── src/
    ├── config.c            # Funções de temporização e formatação de chaves
    ├── db.c                # Implementação das operações e travas do banco
    ├── main.c              # Ponto de entrada (CLI, orquestração e testes)
    ├── server.c            # Inicialização e encerramento de threads do servidor
    ├── storage.c           # Implementação do armazenamento e injeção de race conditions
    ├── wal.c               # Implementação do buffer de log e thread de escrita
    └── workload.c          # Simulação de clientes, snapshots e checagem de invariantes
```

### Divisão entre Código Base e Implementação

| Componente | Arquivo(s) | Descrição | Escopo |
| :--- | :--- | :--- | :--- |
| **Sincronização do Banco** | `src/db.c`, `include/db.h` | Aquisição/liberação de leitura/escrita e isolamento de snapshots | *Implementação do Aluno* |
| **Log de Durabilidade** | `src/wal.c`, `include/wal.h` | Estrutura de buffer, group commit e sincronização com disco | *Implementação do Aluno* |
| **Ciclo do Servidor** | `src/server.c`, `include/server.h` | Inicialização, despacho e desligamento seguro de threads | *Implementação do Aluno* |
| **Infraestrutura e Carga** | `src/storage.c`, `src/workload.c` | Tabela hash, simulação de fsync, clientes e verificadores | **Código Base (Professores)** |
| **Inicializador e Configuração** | `src/main.c`, `src/config.c` | Leitura de flags CLI e setup da execução | **Código Base (Professores)** |

> [!NOTE]
> O arquivo `src/storage.c` inclui mecanismos deliberados de alternância de contexto do processador (`race_window`) durante operações de leitura e escrita para expor com facilidade quaisquer falhas de sincronização ou condições de corrida na solução proposta.

---

## 🛠️ Compilação e Execução

### Pré-requisitos
- Compilador C compatível com C11 (ex.: `gcc` ou `clang`);
- Biblioteca de threads POSIX (`pthread`);
- Utilitário GNU `make`;
- Opcional: ferramentas de análise dinâmica (`valgrind`, `ThreadSanitizer`).

### Instruções de Compilação

- **Compilação padrão (modo Release otimizado):**
  ```bash
  make
  ```

- **Compilação para Debug (com símbolos `-g` e logs detalhados via `plog`):**
  ```bash
  make clean && make RELEASE=false
  ```

- **Compilação com ThreadSanitizer (TSan) para detecção de corridas:**
  ```bash
  make clean && make ADDONS=-fsanitize=thread
  ```

- **Limpeza de artefatos gerados:**
  ```bash
  make clean
  ```

### Parâmetros de Execução

O binário `./program` aceita as seguintes opções de linha de comando:

| Flag | Descrição | Valor Padrão |
| :---: | :--- | :---: |
| `-c` | Número de threads clientes concorrentes | `16` |
| `-n` | Número de operações executadas por cada cliente | `500` |
| `-a` | Número total de contas no banco (mínimo 2) | `64` |
| `-s` | Número de partições da tabela hash | `8` |
| `-b` | Capacidade máxima do buffer de log (em registros) | `8` |
| `-f` | Tempo de simulação da sincronização em disco (`fsync`), em ms | `5` |
| `-p` | Intervalo de tempo entre coletas de snapshot, em ms | `50` |
| `-h` | Exibe a ajuda com o resumo das opções | - |

### Exemplos de Uso

1. **Cenário padrão:**
   ```bash
   ./program
   ```

2. **Cenário de alta concorrência e alta contenção:**
   ```bash
   ./program -c 32 -n 1000 -a 16 -s 2 -b 16 -f 2 -p 25
   ```

3. **Cenário com partição única (equivalente a trava global):**
   ```bash
   ./program -s 1
   ```

4. **Auditoria com Valgrind / Helgrind:**
   ```bash
   valgrind --tool=helgrind ./program -c 4 -n 100 -a 16 -s 4
   ```

---

## 🧪 Verificação de Corretude e Diagnóstico

Ao término da execução, o módulo de validação executa uma auditoria completa e exibe métricas detalhadas sobre o estado do sistema:

- **Consistência em Memória:**
  - `[SUM MISMATCH]`: Detecta se a soma total dos saldos ao final divergiu do total inicial.
  - `[NEGATIVE BALANCE]`: Alerta se alguma conta atingiu saldo negativo.
- **Garantias de Durabilidade:**
  - `[NOT DURABLE]`: Registra se alguma operação foi confirmada a um cliente antes da efetiva gravação em disco.
  - `[LOG ORDER]`: Identifica registros gravados fora da ordem de LSN.
  - `[CONCURRENT WRITE]`: Detecta chamadas concorrentes indevidas a rotinas de gravação atômica em disco.
  - `[BUSY WAITING?]`: Detecta se a thread de gravação realizou chamadas de lote vazio.
- **Reconstrução do Estado a partir do Log (Replay Test):**
  - O sistema relê o arquivo `wal.log` do início ao fim e reaplica todas as transferências gravadas.
  - Compara o estado reconstruído com o estado da memória (`[REPLAY MISMATCH]`, `[LOST TRANSFER]`).
- **Validação de Snapshots:**
  - `[INCONSISTENT SNAPSHOT]`: Verifica se algum snapshot foi tirado durante o estado intermediário de uma transferência.
  - Exibe tempos médio e máximo de espera para aferição de *starvation*.
- **Vazão:**
  - Apresenta o total consolidado de operações por segundo (ops/s).

---

## 📜 Créditos e Direitos Autorais

Este repositório contém material acadêmico desenvolvido no âmbito do curso de **Bacharelado em Ciências da Computação** da **Universidade Federal de Santa Catarina (UFSC)** para a disciplina de **Programação Concorrente**.

### Autoria do Código Base e Especificação
- **Prof. Dr. Eduardo Camilo Inácio** (Departamento de Informática e Estatística — INE/UFSC)
- **Prof. Dr. Márcio Castro** (Departamento de Informática e Estatística — INE/UFSC)

Os professores supracitados são os idealizadores, arquitetos e autores exclusivos do enunciado, da especificação do problema e do código-base da infraestrutura do MiniKV (incluindo o gerador de cargas `workload.c`, o simulador de armazenamento `storage.c`, os cabeçalhos de definição de interfaces e o orquestrador de testes `main.c`).

### Autoria das Soluções de Sincronização
As implementações dos mecanismos de controle de concorrência, sincronização de transações, políticas de travamento, gerenciamento do *Write-Ahead Log* e coordenação do ciclo de vida do servidor (arquivos `src/db.c`, `src/wal.c`, `src/server.c` e definições correlatas em `include/`) constituem a resolução do trabalho acadêmico desenvolvida por Thales Campos de Azevedo e Pedro Zumstein Bilac. O presente README foi elaborado com auxílio de Inteligência Artificial.

Este projeto é disponibilizado no GitHub para fins estritamente **educacionais, acadêmicos e de portfólio pessoal**, respeitando a integridade acadêmica e os direitos autorais de seus autores originais.

# Relatório T2

## Instruções

**OBS:** Nossa definição de round é uma execução inteira do loop principal do simulador, ou seja, um acesso de página para cada processo, totalizando quatro acessos.

1. Ajustar parâmetros no [types.h](types.h)

2. Compilar: `make`

3. Gerar listas de acesso: `./pagelist_gen <num rodadas> <% localidade>`

4. Executar simulação: `./vmem_sim <num rodadas> <algoritmo> [<k>]`
- Opções de algoritmo: NRU, 2ndC, LRU, WS

## Arquitetura e artefatos

### pagelist_gen

Fizemos um gerador de listas de acesso à páginas que suporta um parâmetro adicional de localidade. O parâmetro de localidade define uma chance em porcentagem do acesso seguinte ser uma página "local" (igual, +1 ou -1 do último acesso), ao invés de aleatório, simulando o comportamento de localidade de um processo.

Em nossos testes, comparamos o acesso aleatório (0%) pedido e também um alto grau de localidade (80%).

O nome dos arquivos de output pode ser alterado em types.h.

### procs_sim

Nosso programa que simula quatro processos foi criado conforme especificado. Utilizamos quatro pipes, um para cada processo, para enviar os pedidos de leitura e escrita ao processo vmem_sim, que é nosso simulador. A sincronização para manter a ordem de execução em round-robin foi feita com semáforos.

O procs_sim é executado automaticamente pelo vmem_sim com um fork, transmitindo parâmetros através de variáveis de ambiente.

### types

Tipos e definições de configuração utilizados no projeto.

### util

Estruturas de dados e funções auxiliares que costumamos reutilizar entre trabalhos. Nesse caso, apenas funções de print e as estruturas e funções de acesso para Queue e Set.

### vmem_helpers

Getters e setters para reduzir a complexidade do código principal, como acabamos utilizando arrays separadas para os quatro processos (`table_P1[entries]`, `table_P2[entries]`...) em vez de uma única array de arrays desde o início.

### vmem_sim

Os algoritmos NRU e 2ndC

## Resultados da simulação

TODO

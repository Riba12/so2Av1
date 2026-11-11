# Benchmark das três versões

Gerado em 07/11/2026 13:03. Máquina: **11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz**, com **4 núcleos físicos e 8 lógicos**.
Cada configuração rodou **5 vezes**; as tabelas mostram a média.

> **Como ler**
>
> - **Por ativo** é o tempo da parte dividida entre os trabalhadores (estatísticas, indicadores e
>   previsão de cada ação). É ela que deve cair com mais threads/processos. O resto do programa (ler os
>   CSVs, preparar, juntar no final) é igual em todas as versões e não se divide.
> - **Speedup** = tempo do sequencial ÷ tempo da versão, no mesmo container. `2,00x` = levou metade do tempo.
> - **Eficiência** = speedup ÷ N. 100% seria cada trabalhador extra render um sequencial inteiro.
> - **Desvio padrão** pequeno perto da média = medição estável. Se for grande, algo atrapalhou (outro
>   programa, a máquina esquentando...).
> - Os containers limitam o **tempo** de CPU (`--cpus`): no `cpu2`, mesmo com 8 trabalhadores, a soma
>   não passa de 2 CPUs.

## Resumo: speedup em cada container

| Versão | fora |
|---|---:|
| sequencial | 1,00x |
| threads 2 | 1,74x |
| threads 4 | 2,76x |
| threads 8 | 3,46x |
| threads 16 | 3,57x |
| processos 2 | 1,71x |
| processos 4 | 2,65x |
| processos 8 | 3,51x |
| processos 16 | 3,43x |

- **fora**: o mais rápido foi **threads 16**: 2,84 s por ativo, contra 10,13 s do sequencial (3,57x).

## Fora do container (sem limite de CPU)

| Versão | N | Por ativo (média) | Desvio padrão | Menor – maior | Speedup | Eficiência | Tempo total (média) |
|---|---:|---:|---:|---:|---:|---:|---:|
| sequencial | 1 | 10,128 s | 0,294 s | 9,639 – 10,409 s | 1,00x | 100% | 13,86 s |
| threads | 2 | 5,815 s | 0,114 s | 5,669 – 5,971 s | 1,74x | 87% | 10,97 s |
| threads | 4 | 3,669 s | 0,179 s | 3,475 – 3,947 s | 2,76x | 69% | 8,48 s |
| threads | 8 | 2,925 s | 0,402 s | 2,502 – 3,569 s | 3,46x | 43% | 6,71 s |
| threads | 16 | 2,839 s | 0,115 s | 2,739 – 3,034 s | 3,57x | 22% | 8,13 s |
| processos | 2 | 5,932 s | 0,283 s | 5,566 – 6,200 s | 1,71x | 85% | 11,25 s |
| processos | 4 | 3,819 s | 0,141 s | 3,634 – 3,946 s | 2,65x | 66% | 7,36 s |
| processos | 8 | 2,883 s | 0,106 s | 2,715 – 2,979 s | 3,51x | 44% | 8,13 s |
| processos | 16 | 2,949 s | 0,265 s | 2,641 – 3,369 s | 3,43x | 21% | 8,24 s |

## Como foi medido

- Script `analise/benchmark.sh`, rodado direto no Linux (Ubuntu no WSL2), **fora de qualquer container**
  e sem limite de CPU. Os programas e os dados ficam no disco do próprio Linux (não na pasta do Windows).
- Os dados são copiados para `/tmp` antes de medir, igual à medição nos containers, para as duas serem comparáveis.
- As repetições ficam no laço de fora: cada rodada passa por todas as configurações. Se a máquina
  esquentar no meio, o efeito se espalha por todas, em vez de cair numa só.
- Os tempos de cada execução estão em `bruto_<container>.csv`, nesta mesma pasta.

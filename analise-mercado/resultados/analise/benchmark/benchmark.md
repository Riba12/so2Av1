# Benchmark das três versões

Gerado em 07/11/2026 12:32. Máquina: **11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz**, com **4 núcleos físicos e 8 lógicos**.
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

| Versão | cpu1 | cpu2 | cpu4 | cpu8 |
|---|---:|---:|---:|---:|
| sequencial | 1,00x | 1,00x | 1,00x | 1,00x |
| threads 2 | 0,95x | 1,74x | 1,76x | 1,77x |
| threads 4 | 0,49x | 1,15x | 2,71x | 2,72x |
| threads 8 | 0,43x | 0,64x | 1,88x | 3,33x |
| threads 16 | – | – | – | 3,53x |
| processos 2 | 0,97x | 1,75x | 1,74x | 1,74x |
| processos 4 | 0,48x | 1,16x | 2,67x | 2,68x |
| processos 8 | 0,43x | 0,64x | 1,83x | 3,35x |
| processos 16 | – | – | – | 3,43x |

- **cpu1**: o mais rápido foi **processos 2**: 9,19 s por ativo, contra 8,92 s do sequencial (0,97x).
- **cpu2**: o mais rápido foi **processos 2**: 5,01 s por ativo, contra 8,76 s do sequencial (1,75x).
- **cpu4**: o mais rápido foi **threads 4**: 3,47 s por ativo, contra 9,43 s do sequencial (2,71x).
- **cpu8**: o mais rápido foi **threads 16**: 2,74 s por ativo, contra 9,69 s do sequencial (3,53x).

## Container cpu1

| Versão | N | Por ativo (média) | Desvio padrão | Menor – maior | Speedup | Eficiência | Tempo total (média) |
|---|---:|---:|---:|---:|---:|---:|---:|
| sequencial | 1 | 8,916 s | 0,257 s | 8,673 – 9,243 s | 1,00x | 100% | 13,81 s |
| threads | 2 | 9,360 s | 0,187 s | 9,121 – 9,622 s | 0,95x | 48% | 14,07 s |
| threads | 4 | 18,357 s | 2,086 s | 15,121 – 20,216 s | 0,49x | 12% | 23,24 s |
| threads | 8 | 20,963 s | 3,574 s | 16,105 – 23,854 s | 0,43x | 5% | 25,63 s |
| processos | 2 | 9,185 s | 0,182 s | 9,019 – 9,417 s | 0,97x | 49% | 13,76 s |
| processos | 4 | 18,557 s | 2,520 s | 14,961 – 21,518 s | 0,48x | 12% | 23,47 s |
| processos | 8 | 20,651 s | 3,015 s | 16,223 – 24,649 s | 0,43x | 5% | 25,44 s |

## Container cpu2

| Versão | N | Por ativo (média) | Desvio padrão | Menor – maior | Speedup | Eficiência | Tempo total (média) |
|---|---:|---:|---:|---:|---:|---:|---:|
| sequencial | 1 | 8,761 s | 0,179 s | 8,579 – 9,008 s | 1,00x | 100% | 13,51 s |
| threads | 2 | 5,021 s | 0,092 s | 4,932 – 5,154 s | 1,74x | 87% | 9,84 s |
| threads | 4 | 7,589 s | 0,416 s | 7,077 – 8,232 s | 1,15x | 29% | 12,42 s |
| threads | 8 | 13,708 s | 1,251 s | 12,475 – 15,610 s | 0,64x | 8% | 18,61 s |
| processos | 2 | 5,011 s | 0,144 s | 4,806 – 5,178 s | 1,75x | 87% | 9,68 s |
| processos | 4 | 7,571 s | 0,260 s | 7,261 – 7,895 s | 1,16x | 29% | 12,34 s |
| processos | 8 | 13,765 s | 1,202 s | 11,995 – 15,092 s | 0,64x | 8% | 18,51 s |

## Container cpu4

| Versão | N | Por ativo (média) | Desvio padrão | Menor – maior | Speedup | Eficiência | Tempo total (média) |
|---|---:|---:|---:|---:|---:|---:|---:|
| sequencial | 1 | 9,427 s | 0,079 s | 9,341 – 9,532 s | 1,00x | 100% | 14,30 s |
| threads | 2 | 5,353 s | 0,047 s | 5,316 – 5,431 s | 1,76x | 88% | 10,25 s |
| threads | 4 | 3,475 s | 0,117 s | 3,323 – 3,583 s | 2,71x | 68% | 8,45 s |
| threads | 8 | 5,024 s | 0,112 s | 4,937 – 5,186 s | 1,88x | 23% | 9,91 s |
| processos | 2 | 5,430 s | 0,171 s | 5,221 – 5,608 s | 1,74x | 87% | 10,41 s |
| processos | 4 | 3,526 s | 0,074 s | 3,412 – 3,596 s | 2,67x | 67% | 8,41 s |
| processos | 8 | 5,149 s | 0,179 s | 4,890 – 5,353 s | 1,83x | 23% | 10,22 s |

## Container cpu8

| Versão | N | Por ativo (média) | Desvio padrão | Menor – maior | Speedup | Eficiência | Tempo total (média) |
|---|---:|---:|---:|---:|---:|---:|---:|
| sequencial | 1 | 9,691 s | 0,322 s | 9,319 – 10,095 s | 1,00x | 100% | 14,91 s |
| threads | 2 | 5,479 s | 0,141 s | 5,275 – 5,646 s | 1,77x | 88% | 10,70 s |
| threads | 4 | 3,560 s | 0,071 s | 3,443 – 3,626 s | 2,72x | 68% | 8,77 s |
| threads | 8 | 2,909 s | 0,268 s | 2,614 – 3,244 s | 3,33x | 42% | 8,30 s |
| threads | 16 | 2,745 s | 0,294 s | 2,558 – 3,265 s | 3,53x | 22% | 8,13 s |
| processos | 2 | 5,576 s | 0,078 s | 5,497 – 5,670 s | 1,74x | 87% | 10,75 s |
| processos | 4 | 3,613 s | 0,090 s | 3,498 – 3,741 s | 2,68x | 67% | 8,84 s |
| processos | 8 | 2,890 s | 0,195 s | 2,711 – 3,221 s | 3,35x | 42% | 8,09 s |
| processos | 16 | 2,828 s | 0,166 s | 2,672 – 3,028 s | 3,43x | 21% | 7,94 s |

## Como foi medido

- Script `analise/benchmark.sh`, um container por vez (`docker compose run --rm -T <container> ...`).
- Os dados são copiados para dentro do container antes de medir: ler da pasta do Windows é lento e
  instável. Isso deixa o **tempo total** mais curto que numa execução normal, mas não muda o **por ativo**.
- As repetições ficam no laço de fora: cada rodada passa por todas as configurações. Se a máquina
  esquentar no meio, o efeito se espalha por todas, em vez de cair numa só.
- Os tempos de cada execução estão em `bruto_<container>.csv`, nesta mesma pasta.

---

## Conclusões (análise da rodada de 07/11/2026: janela de 500, Ridge 1, 5 repetições)

> Esta seção foi escrita à mão a partir das tabelas acima. Se o `benchmark_relatorio.sh` rodar de novo, o
> arquivo é refeito e ela some (guarde uma cópia).

### 1. Com CPU sobrando, paralelizar funciona

| Container | Melhor configuração | Speedup | Ideal (nº de CPUs) |
|---|---|---:|---:|
| cpu2 | 2 trabalhadores | 1,75x | 2x |
| cpu4 | 4 trabalhadores | 2,71x | 4x |
| cpu8 | 8 a 16 trabalhadores | 3,33x a 3,53x | 8x |

- **Até 4 o ganho é forte**: cada trabalhador ganha um núcleo físico.
- **De 4 para 8, o ganho é pequeno** (2,72x → 3,33x): a máquina tem 4 núcleos físicos, e os 4 "extras" são
  Hyper-Threading, que divide o mesmo núcleo entre duas threads. Rende uns 20 a 30% a mais, não o dobro.
- **De 8 para 16, quase nada** (3,33x → 3,53x): não existe CPU nova. O pouco que melhora vem de deixar os
  núcleos ocupados enquanto alguma thread espera memória.
- O speedup nunca chega ao ideal: parte do trabalho disputa memória e cache, e o último trabalhador a terminar
  segura todos.

### 2. Mais trabalhadores do que CPUs **piora**, e muito

| Container | N = CPUs | N = 2× CPUs | N = 4× CPUs ou mais |
|---|---:|---:|---:|
| cpu1 | 0,95x (N=2) | 0,49x (N=4) | 0,43x (N=8) |
| cpu2 | 1,75x | 1,15x | 0,64x |
| cpu4 | 2,71x | 1,88x | – |

No `cpu1`, 4 trabalhadores levam **o dobro do tempo** do sequencial. Não é só "dividir a mesma CPU": o limite
`--cpus` é uma **cota de tempo** a cada 100 ms. Com muitas threads, a cota acaba logo no começo de cada período
e o container inteiro fica **parado até o próximo** (*throttling*). Somam-se a isso as trocas de contexto e o
cache, que fica sendo disputado. O desvio padrão alto nesses casos (2 a 3,5 s) mostra o quanto isso é irregular.

**Regra prática:** usar N igual ao número de CPUs disponíveis. Passar disso só ajuda quando há núcleos lógicos
sobrando (o caso do `cpu8`).

### 3. Threads e processos empatam

Em todas as configurações a diferença entre os dois ficou abaixo de 3%, dentro do desvio padrão. O custo
extra dos processos (`fork`, criar a memória compartilhada e copiar os resultados no final) é de milissegundos,
contra segundos de trabalho por ativo. Como nenhum dos dois precisa de semáforo ou mutex (cada um escreve só
nas suas posições), eles fazem exatamente o mesmo trabalho.

### 4. O tempo total ganha menos que o "por ativo" (Lei de Amdahl)

No `cpu8`, o "por ativo" cai de 9,69 s para 2,75 s (3,5x), mas o **tempo total** só cai de 14,91 s para
7,94 s (1,9x). A leitura dos CSVs e a preparação (cerca de 5 s) continuam sequenciais, e com o "por ativo"
rápido elas viram a maior parte do tempo. O próximo ganho grande viria de **paralelizar a leitura** (um arquivo
por ano em cada thread).

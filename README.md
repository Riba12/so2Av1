# so2-financeiro: análise do mercado de ações em paralelo

Trabalho de Sistemas Operacionais II (FAETERJ). O programa lê 11 anos de cotações da B3 (2015 a 2025) e, para
cada uma das 391 ações que passam pelos filtros, calcula estatísticas, indicadores técnicos (média móvel,
Bollinger, IFR, MACD, estocástico, ATR, OBV…) e uma previsão de alta ou queda para o dia seguinte, por regressão
linear, conferida dia a dia contra o que aconteceu. No fim, cruza as ações entre si: índice de mercado, beta e
correlação.

A mesma análise existe em três versões, em C++ no estilo das aulas (`pthread_create`, `fork`, `shmget`,
`printf`). As três dão **exatamente o mesmo resultado** (CSVs idênticos byte a byte):

| Versão | Como divide o trabalho | Como os resultados voltam |
|---|---|---|
| `sequencial` | uma ação depois da outra | já estão na memória do programa |
| `threads N` | N threads: a principal + N−1 criadas com `pthread_create` | cada thread escreve na sua posição de um vetor global |
| `processos N` | N processos: o pai + N−1 filhos criados com `fork` | memória compartilhada (`shmget`/`shmat`) |

O número de trabalhadores é passado na execução. Cada um analisa as ações `id, id+N, id+2N…` (divisão
intercalada). Como nenhum trabalhador escreve onde outro escreve, não há região crítica: a única sincronização é
esperar todos terminarem (`pthread_join` / `wait`) antes de juntar os resultados.

## Passo a passo em uma máquina nova

Só é preciso o **git** e o **Docker** (no Windows, o Docker Desktop aberto). O container já traz g++ e make.

```bash
git clone https://github.com/Riba12/so2Av1.git
cd so2Av1
docker compose build            # cria a imagem (só na primeira vez)
docker compose run --rm cpu4    # abre um terminal no container, com limite de 4 CPUs
```

Dentro do container (ele já abre na pasta `analise-mercado/`):

```bash
make -C analise                 # compila: gera analise/build/sequencial, threads e processos
sequencial                      # roda a versão sequencial
```

O clone tem cerca de 170 MB, porque os dados de entrada (`dados/processados/`) vêm junto. Não é preciso baixar
nem converter nada.

## Executar

```bash
sequencial              # versão sequencial
threads 4               # versão com 4 threads
processos 4             # versão com 4 processos
threads 4 PETR4         # qualquer versão aceita o código de uma ação, para detalhar só ela
```

Os comandos funcionam sem caminho porque o container coloca `analise/build/` no `PATH`. Fora do container, use
`./analise/build/sequencial`, sempre de dentro de `analise-mercado/`.

Cada execução mostra o resumo no terminal e grava em `resultados/analise/`:

- `relatorio_ativos.csv`, `distribuicao_retornos.csv`, `retornos_descartados.csv`: os números de cada ação;
- `relatorios/relatorio_<versao>_<n>_<data>.md`: um relatório explicado, para ler.

Esses arquivos são recriados a cada execução e não vão para o git.

Conferir que as três versões dão o mesmo resultado:

```bash
sequencial > /dev/null && cp resultados/analise/relatorio_ativos.csv /tmp/seq.csv
threads 8 > /dev/null && cmp /tmp/seq.csv resultados/analise/relatorio_ativos.csv && echo identico
processos 8 > /dev/null && cmp /tmp/seq.csv resultados/analise/relatorio_ativos.csv && echo identico
ipcs -m                 # depois de "processos", não deve sobrar memória compartilhada
```

## Experimentos

O `docker-compose.yml` tem um serviço para cada limite de CPU (`cpu1`, `cpu2`, `cpu4`, `cpu8`) e um sem
limite (`livre`).

| O que | Como rodar | Resultado em |
|---|---|---|
| Dentro do container, com 1, 2, 4 e 8 CPUs | `docker compose run --rm -T cpu2 bash analise/benchmark.sh 8 5` (um por container) | `resultados/analise/benchmark/benchmark.md` |
| Fora do container (Linux do WSL2, no Windows) | `bash analise/benchmark.sh 16 5`, depois de `analise/copiar_para_wsl.sh` | `resultados/analise/benchmark_fora/benchmark.md` |
| Gráficos | abrir `resultados/analise/graficos/graficos.html` | `resultados/analise/graficos/*.png` |

Cada configuração rodou 5 vezes; as tabelas trazem média, desvio padrão, speedup e eficiência. O passo a passo
completo, incluindo a instalação do WSL2, está no [COMANDOS.md](COMANDOS.md).

Principais resultados (tempo da parte dividida, máquina com 4 núcleos físicos e 8 lógicos):

| | Sequencial | 2 | 4 | 8 | 16 |
|---|---:|---:|---:|---:|---:|
| threads, fora do container | 10,1 s | 5,8 s (1,7x) | 3,7 s (2,8x) | 2,9 s (3,5x) | 2,8 s (3,6x) |
| processos, fora do container | 10,1 s | 5,9 s (1,7x) | 3,8 s (2,7x) | 2,9 s (3,5x) | 2,9 s (3,4x) |
| threads, container com 1 CPU | 8,9 s | 9,4 s (0,95x) | 18,4 s (0,49x) | 21,0 s (0,43x) | – |

- O ganho é forte até 4 trabalhadores (um por núcleo físico), pequeno até 8 (Hyper-Threading) e quase nulo depois.
- Mais trabalhadores do que CPUs **piora**: com 1 CPU, 4 threads levam o dobro do sequencial.
- Threads e processos empatam: o custo do `fork` e da memória compartilhada é pequeno perto do trabalho.
- Com o mesmo número de CPUs, dentro e fora do container dão o mesmo tempo; o que pesa é o limite `--cpus`.
- O tempo total ganha menos que a parte dividida: a leitura dos CSVs (~3,7 s) continua sequencial. Pela Lei de
  Amdahl, o programa inteiro fica no máximo 2,9x mais rápido.

A análise completa está na seção "Conclusões" de `resultados/analise/benchmark/benchmark.md`.

## Organização

```
Dockerfile, docker-compose.yml     ambiente (containers com 1, 2, 4 e 8 CPUs e um sem limite)
COMANDOS.md                        comandos do projeto, passo a passo
analise-mercado/
  analise/
    comum.h                        constantes (filtros, janelas, Ridge...) e structs
    comum.cpp                      leitura, preparação, indicadores, previsão e etapas entre ações
    relatorio.cpp                  saída no terminal e CSVs
    relatorio_md.cpp               relatório explicado em Markdown
    sequencial.cpp                 versão sequencial
    threads.cpp                    versão com threads
    processos.cpp                  versão com processos + memória compartilhada
    Makefile                       compila as três versões
    benchmark.sh                   mede as três versões e gera o benchmark.md
    benchmark_relatorio.sh         monta o benchmark.md a partir dos tempos brutos
    copiar_para_wsl.sh             copia código e dados para o Linux do WSL (medição fora do container)
  dados/processados/               entrada: um CSV por ano (cotações COTAHIST da B3)
  dados/eventos/ajustes.csv        entrada: desdobramentos e grupamentos (B3 e CVM), para corrigir os preços
  resultados/analise/
    benchmark/                     tempos dentro dos containers
    benchmark_fora/                tempos fora do container
    graficos/                      gráficos dos benchmarks
```

Os dados de entrada já vêm prontos no repositório: as cotações oficiais da B3 (arquivos COTAHIST) convertidas
para CSV, e os eventos corporativos (desdobramentos, grupamentos, bonificações) cruzados entre B3 e CVM.

*Trabalho acadêmico, não é recomendação de investimento.*

# Comandos do projeto (SO2)

## Estrutura das pastas

```
so2Av1/
├── Dockerfile, docker-compose.yml   ambiente (containers com 1, 2, 4 e 8 CPUs, e um sem limite)
├── README.md                        visão geral e resultados
├── COMANDOS.md                      este arquivo
└── analise-mercado/
    ├── analise/                     código: comum + sequencial, threads e processos; compila em analise/build/
    ├── dados/
    │   ├── processados/             entrada: um CSV por ano (COTAHIST da B3)
    │   └── eventos/                 entrada: ajustes.csv (desdobramentos, grupamentos, bonificações)
    └── resultados/analise/          saídas da análise, benchmarks e gráficos
```

## Primeira vez em uma máquina nova

```powershell
git clone https://github.com/Riba12/so2Av1.git
cd so2Av1
docker compose build
```

O `docker compose build` só precisa rodar de novo se o `Dockerfile` mudar.

## Entrar no container

### Fluxo de trabalho (containers ficam ligados)

Sobe os containers (1, 2, 4 e 8 CPUs, e o sem limite) em background, uma vez só:

```powershell
docker compose up -d
```

Entra em qualquer um deles quantas vezes quiser (edições nos `.cpp` já aparecem dentro, só precisa recompilar):

```powershell
docker compose exec cpu1 bash   # limite de 1 CPU
docker compose exec cpu2 bash   # limite de 2 CPUs
docker compose exec cpu4 bash   # limite de 4 CPUs
docker compose exec cpu8 bash   # limite de 8 CPUs
docker compose exec livre bash  # sem limite
```

O `bash` no fim é o programa que roda dentro do container: um terminal. Para rodar um comando direto, sem abrir
o terminal, troque o `bash` pelo comando:

```powershell
docker compose exec cpu4 ./analise/build/threads 4
docker compose exec cpu4 bash -c "make -C analise && sequencial"
```

Os containers já abrem em `/app/analise-mercado`. Só derruba no final da sessão:

```powershell
docker compose down
```

### Alternativa: container temporário (some sozinho ao sair)

```powershell
docker compose run --rm cpu1   # limite de 1 CPU
docker compose run --rm cpu2   # limite de 2 CPUs
docker compose run --rm cpu4   # limite de 4 CPUs
docker compose run --rm cpu8   # limite de 8 CPUs
docker compose run --rm livre  # sem limite
```

Equivalente sem o compose, com `docker run` direto (como pede o enunciado):

```powershell
docker run --rm -it --cpus="1" -v "${PWD}:/app" -w /app/analise-mercado so2-projeto-av1 bash
docker run --rm -it --cpus="2" -v "${PWD}:/app" -w /app/analise-mercado so2-projeto-av1 bash
docker run --rm -it --cpus="4" -v "${PWD}:/app" -w /app/analise-mercado so2-projeto-av1 bash
docker run --rm -it --cpus="8" -v "${PWD}:/app" -w /app/analise-mercado so2-projeto-av1 bash
docker run --rm -it -v "${PWD}:/app" -w /app/analise-mercado so2-projeto-av1 bash   # sem limite
```

## Compilar e executar

Dentro do container (que já abre em `/app/analise-mercado`):

```
analise/comum.h, comum.cpp      constantes, structs, leitura, preparação, indicadores, previsão, análise
analise/relatorio.cpp           saída no terminal + CSVs
analise/relatorio_md.cpp        relatório explicado em Markdown
analise/sequencial.cpp          um ativo depois do outro
analise/threads.cpp             N threads: a principal + N-1 criadas, todas analisam ativos
analise/processos.cpp           N processos: o pai + N-1 filhos (fork), resultados por memória compartilhada
```

```bash
make -C analise         # compila o que mudou (gera analise/build/)
make -C analise clean   # apaga analise/build/
sequencial              # lê dados/processados/ + ajustes.csv, analisa e mostra o resumo
sequencial PETR4        # resumo + o detalhe de uma ação específica
threads 4               # mesma análise com 4 trabalhadores: a thread principal + 3 criadas
threads 4 PETR4         # 4 threads + o detalhe da PETR4
processos 4             # mesma análise com 4 trabalhadores: o pai + 3 filhos (fork + shmget)
ipcs -m                 # lista segmentos de memória compartilhada (depois de "processos" não deve sobrar nenhum)
```

Os comandos funcionam sem caminho porque o Dockerfile coloca `analise-mercado/analise/build` no `PATH`. Fora
do container é `./analise/build/sequencial`. Os programas procuram `dados/` a partir da pasta atual, então rode
sempre de dentro de `analise-mercado/`.

As versões paralelas têm que dar **exatamente** o mesmo resultado do sequencial. Para conferir:

```bash
sequencial > /dev/null && cp resultados/analise/relatorio_ativos.csv /tmp/seq.csv
threads 8 > /dev/null && cmp /tmp/seq.csv resultados/analise/relatorio_ativos.csv && echo identico
processos 8 > /dev/null && cmp /tmp/seq.csv resultados/analise/relatorio_ativos.csv && echo identico
```

Saídas da análise (além do terminal), em `resultados/analise/`. São recriadas a cada execução e não vão para o git:

- `relatorios/relatorio_<versao>_<n>_AAAA-MM-DD_HH-MM-SS.md`: relatório completo e explicado da execução
  (resumo em frases, tabelas com "como ler", glossário, tempos). Um arquivo novo a cada execução.
- `relatorio_ativos.csv`: uma linha por ação, com estatísticas, indicadores do último pregão e previsão
  (é o arquivo para comparar as três versões número a número).
- `distribuicao_retornos.csv`: histograma dos retornos diários (faixas de 1%).
- `retornos_descartados.csv`: cada retorno que ficou de fora e o motivo.

Atenção: o programa só lê os arquivos `COTAHIST_A<ano>.csv` de `dados/processados/`, de 2015 a 2025
(`PRIMEIRO_ANO`/`ULTIMO_ANO` em `analise/comum.h`). Ano que não existe é pulado, e qualquer outro arquivo na
pasta é ignorado.

## O que é calculado

Todas as contas ficam em `analise/comum.cpp`:

- **Estatísticas por ação:** preços, retorno médio, volatilidade, volume, maiores altas e quedas, histograma.
- **Indicadores:** MM20, Bandas de Bollinger, volatilidade 20d, IFR 14, MACD 12/26/9, momento 10d, estocástico
  %K 14, ATR 14, OBV (parâmetros clássicos de mercado). Um retorno descartado reinicia os indicadores.
- **Previsão:** regressão linear com 9 indicadores e **Ridge** (`RIDGE = 1`), que segura os pesos para o modelo
  não decorar ruído. Para cada dia, treina só com os 500 pregões anteriores (`JANELA_TREINO`) e confere com o que
  aconteceu. O acerto é comparado com um chute simples (repetir a direção mais comum na mesma janela) e com os
  chutes fixos "sempre ALTA" e "sempre QUEDA".
- **Entre ações (no fim, depois do join/wait):** índice próprio de mercado (média dos retornos, pesos iguais),
  beta de cada ação e correlação entre pares de empresas diferentes.

### Filtros e ajustes (constantes em `analise/comum.h`)

- Só ações de empresas (`tipo` ON, PN, PNA, PNB, PNC, UNT).
- Ativos negociados em pelo menos 95% dos pregões do período em que existiram (`PRESENCA_MINIMA`) e com no
  mínimo 20 dias (`MINIMO_DIAS_NEGOCIADOS`).
- Ativos sem nenhuma fonte oficial de eventos (sem CNPJ na CVM e sem cadastro na B3) são excluídos.
- Desdobramentos, grupamentos e bonificações: preços anteriores corrigidos pelo fator oficial (`ajustes.csv`).
- Cisões: o retorno do dia é descartado.
- Piso de preço (`PRECO_MINIMO`): retorno descartado quando o fechamento anterior negociado é menor que R$ 1,00.
- Todo retorno descartado é listado, com o motivo, em `resultados/analise/retornos_descartados.csv`.
- Retornos são de **preço** (sem reinvestimento de dividendos).

### Formato dos CSVs de entrada

Um CSV por ano (`COTAHIST_A2025.csv`), só com o mercado à vista:

`data,ativo,abertura,maxima,minima,fechamento,volume,volume_financeiro,negocios,tipo,empresa`

- `volume` = quantidade de ações negociadas; `volume_financeiro` = total em R$.
- `tipo` = espécie do papel: `ON`/`PN`/`PNA`/`PNB`/`PNC`/`UNT` = ações; o resto (fundos, BDRs, direitos) é filtrado.
- `empresa` = nome resumido da empresa na B3 (ex.: `PETROBRAS`).
- Preços já vêm divididos pelo fator de cotação, ou seja, sempre por ação.

## Teste de desempenho (benchmark)

Roda cada versão várias vezes em cada container e gera `resultados/analise/benchmark/benchmark.md` (médias,
desvio padrão, speedup e eficiência), mais os tempos de cada execução em `bruto_<container>.csv`. Do Windows,
na pasta do projeto, um container por vez:

```powershell
docker compose run --rm -T cpu1 bash analise/benchmark.sh 8 5    # N até 8, 5 repetições
docker compose run --rm -T cpu2 bash analise/benchmark.sh 8 5
docker compose run --rm -T cpu4 bash analise/benchmark.sh 8 5
docker compose run --rm -T cpu8 bash analise/benchmark.sh 16 5   # N até 16
```

- Em cada container roda o sequencial e threads/processos com N = 2, 4, 8 (e 16, se o máximo for 16).
- Cada container regrava só o próprio `bruto_<container>.csv` e refaz o `benchmark.md` com tudo que já foi
  medido. Dá para rodar um container de novo sem perder os outros.
- Só para refazer o `.md` a partir dos CSVs: `bash analise/benchmark_relatorio.sh`.
- O que comparar é o tempo **por ativo**, a parte dividida entre os trabalhadores. Os dados são copiados para
  dentro do container antes de medir, porque ler a pasta do Windows é lento e instável.
- Na máquina usada (i5-1135G7: 4 núcleos físicos, 8 lógicos) o `cpu8` equivale ao `livre`.

## Rodar FORA do container (Ubuntu no WSL2)

O enunciado pede tempos medidos fora do container. No Windows, isso é feito no Ubuntu do WSL2: um Linux de
verdade, onde `fork`, `shmget` e `pthread` funcionam sem mudar o código. O Windows nativo não tem `fork` nem
`shmget`.

Instalação (uma vez só, no PowerShell):

```powershell
wsl --install -d Ubuntu-24.04 --no-launch
wsl -d Ubuntu-24.04 -u root -- bash -c "apt-get update && apt-get install -y build-essential"
```

Copiar o código e os dados para dentro do Linux e compilar (repita depois de mudar o código). Os arquivos ficam
em `~/so2/analise-mercado`, no disco do Linux, porque ler `/mnt/c/...` é lento. Na pasta do projeto:

```powershell
wsl -d Ubuntu-24.04 -u root -- bash analise-mercado/analise/copiar_para_wsl.sh
```

Rodar:

```powershell
wsl -d Ubuntu-24.04 -u root --cd /root/so2/analise-mercado     # abre o terminal do Linux já na pasta
```

```bash
./analise/build/sequencial
./analise/build/threads 4
./analise/build/processos 4
bash analise/benchmark.sh 16 5     # grava em resultados/analise/benchmark_fora/ (não mexe no dos containers)
```

Para trazer o resultado de volta para a pasta do projeto no Windows (troque pelo caminho onde você clonou):

```bash
cp -r resultados/analise/benchmark_fora /mnt/c/<caminho>/so2Av1/analise-mercado/resultados/analise/
```

O `benchmark.sh` percebe sozinho que está fora de um container (não existe o arquivo `/.dockerenv`).

## Gráficos

`resultados/analise/graficos/graficos.html` mostra os três gráficos no navegador, com os números dos
benchmarks. Os PNGs da mesma pasta são imagens desses gráficos, para colar no relatório/slides.

Os números estão escritos no começo do `<script>` de `graficos.html` (ele não lê os `bruto_*.csv`). Se os tempos
mudarem, edite lá. Para refazer um PNG, abra `graficos.html?g=1` (ou `2`, `3`) no navegador: mostra só aquele
gráfico, no tema claro, pronto para um print.

## Gerenciar containers e imagens

```powershell
docker ps                    # containers rodando
docker ps -a                 # todos os containers, incluindo parados
docker images                # imagens locais
docker rm <nome_ou_id>       # remove um container parado
docker rmi <nome_ou_id>      # remove uma imagem
```

## Notas

- `--cpus="N"` limita o **tempo de CPU** (cgroup): o container pode usar qualquer núcleo da máquina, mas a soma
  do tempo usado não passa do equivalente a N núcleos. Não muda o que `nproc` mostra.
- O número de trabalhadores (argumento de `threads`/`processos`) e o limite de CPU (`--cpus`) são
  **independentes**: dá para rodar 8 processos num container de 1 CPU. É isso que o benchmark mede.
- O container usa o horário de Brasília (`TZ: America/Sao_Paulo` no compose; o padrão seria UTC). É isso que dá
  a hora certa no nome dos relatórios. Depois de mudar o compose, rode `docker compose up -d` para recriar os
  containers.
- O `.gitattributes` grava todo texto com fim de linha LF, mesmo editando no Windows: os scripts `.sh` rodam no
  Linux do container e quebrariam com CRLF.
- PowerShell 5.1 não aceita `&&` entre comandos no terminal; dentro de `bash -c "..."` funciona normalmente.

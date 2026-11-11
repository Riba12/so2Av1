#!/bin/bash
# Mede o tempo das 3 versoes (sequencial, threads, processos) DENTRO DESTE container e grava os tempos brutos em
# resultados/analise/benchmark/bruto_<container>.csv. No fim, gera de novo o benchmark.md com tudo que ja
# foi medido (todos os containers).
#
# Uso (no container, de dentro de analise-mercado/):
#   bash analise/benchmark.sh [N maximo] [repeticoes]
#   bash analise/benchmark.sh 8 5      -> sequencial + threads/processos com 2, 4 e 8, 5 vezes cada
#   bash analise/benchmark.sh 16 5     -> idem, com 16 tambem
#
# Do Windows, a matriz inteira (um container por vez):
#   docker compose run --rm -T cpu1 bash analise/benchmark.sh 8 5
#   docker compose run --rm -T cpu2 bash analise/benchmark.sh 8 5
#   docker compose run --rm -T cpu4 bash analise/benchmark.sh 8 5
#   docker compose run --rm -T cpu8 bash analise/benchmark.sh 16 5
#
# Fora do container (ex.: no Ubuntu do WSL), de dentro de analise-mercado/:
#   bash analise/benchmark.sh 16 5     -> grava em resultados/analise/benchmark_fora/

set -e

MAX_N=${1:-16}
REPETICOES=${2:-5}
LISTA_N="2 4 8 16"

# pasta analise-mercado/ (a de cima deste script): funciona no container (/app/...) e fora dele
RAIZ=$(cd "$(dirname "$0")/.." && pwd)
cd $RAIZ

# Dentro de um container Docker existe o arquivo /.dockerenv. Fora dele, os resultados vao para outra pasta
# (benchmark_fora/), sem mexer no que foi medido dentro dos containers.
if [ -f /.dockerenv ]; then
    # Quantas CPUs este container pode usar: o limite do --cpus fica no cgroup, em "cota periodo"
    # (ex.: "200000 100000" = 2 CPUs; "max 100000" = sem limite)
    read COTA PERIODO < /sys/fs/cgroup/cpu.max
    if [ "$COTA" = "max" ]; then
        CONTAINER="livre"
    else
        CONTAINER="cpu$((COTA / PERIODO))"
    fi
    PASTA_SAIDA=resultados/analise/benchmark
else
    CONTAINER="fora"
    PASTA_SAIDA=resultados/analise/benchmark_fora
fi

make -s -C analise
BIN=$RAIZ/analise/build

mkdir -p $PASTA_SAIDA
BRUTO=$PASTA_SAIDA/bruto_$CONTAINER.csv
echo "container,versao,n,repeticao,leitura,preparacao,por_ativo,entre_ativos,total" > $BRUTO

# Copia os dados para dentro do container: ler direto da pasta do Windows e lento e varia muito,
# o que atrapalharia a medicao. Os programas rodam nesta pasta (e gravam as saidas aqui, descartadas).
TMP=/tmp/benchmark
rm -rf $TMP
mkdir -p $TMP/resultados/analise/relatorios
cp -r dados $TMP/
cd $TMP

# A lista de execucoes: o sequencial + threads e processos com cada N ate o maximo
CONFIGS="sequencial:1"
for n in $LISTA_N; do
    if [ $n -le $MAX_N ]; then CONFIGS="$CONFIGS threads:$n"; fi
done
for n in $LISTA_N; do
    if [ $n -le $MAX_N ]; then CONFIGS="$CONFIGS processos:$n"; fi
done
TOTAL=$(( $(echo $CONFIGS | wc -w) * REPETICOES ))

echo "Container: $CONTAINER | repeticoes: $REPETICOES | execucoes: $TOTAL"
echo "Configuracoes: $CONFIGS"

# A repeticao fica no laco de FORA: cada rodada passa por todas as configuracoes. Assim, se a maquina
# esquentar ou alguma coisa rodar no fundo, o efeito se espalha por todas, em vez de cair numa so.
feitas=0
for rep in $(seq 1 $REPETICOES); do
    for config in $CONFIGS; do
        versao=${config%:*}
        n=${config#*:}

        inicio=$(date +%s.%N)
        if [ "$versao" = "sequencial" ]; then
            saida=$($BIN/sequencial)
        else
            saida=$($BIN/$versao $n)
        fi
        fim=$(date +%s.%N)

        # tira os tempos do bloco "Tempos:" que o programa imprime no fim
        leitura=$(echo "$saida" | awk '/^  leitura:/ {print $2}')
        preparacao=$(echo "$saida" | awk '/^  preparacao:/ {print $2}')
        porAtivo=$(echo "$saida" | awk -F': ' '/^    por ativo \(/ {split($2, p, " "); print p[1]}')
        entreAtivos=$(echo "$saida" | awk -F': ' '/^    entre ativos \(/ {split($2, p, " "); print p[1]}')
        total=$(awk -v a=$inicio -v b=$fim 'BEGIN {printf "%.3f", b - a}')

        echo "$CONTAINER,$versao,$n,$rep,$leitura,$preparacao,$porAtivo,$entreAtivos,$total" >> $RAIZ/$BRUTO

        feitas=$((feitas + 1))
        printf "[%3d/%d] rep %d  %-10s N=%-2s  por ativo: %6s s  total: %6s s\n" \
            $feitas $TOTAL $rep $versao $n $porAtivo $total
    done
done

cd $RAIZ
rm -rf $TMP
bash analise/benchmark_relatorio.sh $PASTA_SAIDA

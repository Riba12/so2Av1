#!/bin/bash
# Junta todos os resultados/analise/benchmark/bruto_*.csv e gera resultados/analise/benchmark/benchmark.md
# (medias, desvio padrao, speedup e eficiencia). O benchmark.sh chama este script no fim; tambem pode
# rodar sozinho:   bash analise/benchmark_relatorio.sh

#   bash analise/benchmark_relatorio.sh resultados/analise/benchmark_fora   (os medidos fora do container)

cd "$(dirname "$0")/.."
PASTA=${1:-resultados/analise/benchmark}
SAIDA=$PASTA/benchmark.md

if ! ls $PASTA/bruto_*.csv > /dev/null 2>&1; then
    echo "Nenhum bruto_*.csv em $PASTA: rode o benchmark.sh antes."
    exit 1
fi

MAQUINA=$(lscpu | sed -n 's/^Model name: *//p')
NUCLEOS=$(lscpu | sed -n 's/^Core(s) per socket: *//p')
LOGICOS=$(nproc --all)
DATA=$(date '+%d/%m/%Y %H:%M')

awk -F, -v maquina="$MAQUINA" -v nucleos="$NUCLEOS" -v logicos="$LOGICOS" -v data="$DATA" '
# numero no formato brasileiro: virgula decimal
function br(x, casas,    s) { s = sprintf("%." casas "f", x); sub(/\./, ",", s); return s }
function nomeConfig(v, n) { return (v == "sequencial") ? "sequencial" : v " " n }

FNR == 1 { next }   # cabecalho de cada arquivo
{
    c = $1; v = $2; n = $3; t = $7 + 0
    k = c SUBSEP v SUBSEP n
    qtd[k]++; soma[k] += t; soma2[k] += t * t; somaTotal[k] += $9
    if (!(k in menor) || t < menor[k]) menor[k] = t
    if (!(k in maior) || t > maior[k]) maior[k] = t
    temContainer[c] = 1
    if ($4 + 0 > maxRep) maxRep = $4 + 0
}

END {
    # ordem fixa das linhas e colunas (awk nao ordena sozinho)
    nc = split("cpu1 cpu2 cpu4 cpu8 livre fora", ordemC, " ")
    ncfg = split("sequencial:1 threads:2 threads:4 threads:8 threads:16 processos:2 processos:4 processos:8 processos:16", ordemCfg, " ")

    for (k in qtd) {
        media[k] = soma[k] / qtd[k]
        desvio[k] = 0
        if (qtd[k] > 1) {
            var = (soma2[k] - qtd[k] * media[k] * media[k]) / (qtd[k] - 1)
            if (var > 0) desvio[k] = sqrt(var)
        }
    }

    print "# Benchmark das três versões"
    print ""
    print "Gerado em " data ". Máquina: **" maquina "**, com **" nucleos " núcleos físicos e " logicos " lógicos**."
    print "Cada configuração rodou **" maxRep " vezes**; as tabelas mostram a média."
    print ""
    print "> **Como ler**"
    print ">"
    print "> - **Por ativo** é o tempo da parte dividida entre os trabalhadores (estatísticas, indicadores e"
    print ">   previsão de cada ação). É ela que deve cair com mais threads/processos. O resto do programa (ler os"
    print ">   CSVs, preparar, juntar no final) é igual em todas as versões e não se divide."
    print "> - **Speedup** = tempo do sequencial ÷ tempo da versão, no mesmo container. `2,00x` = levou metade do tempo."
    print "> - **Eficiência** = speedup ÷ N. 100% seria cada trabalhador extra render um sequencial inteiro."
    print "> - **Desvio padrão** pequeno perto da média = medição estável. Se for grande, algo atrapalhou (outro"
    print ">   programa, a máquina esquentando...)."
    print "> - Os containers limitam o **tempo** de CPU (`--cpus`): no `cpu2`, mesmo com 8 trabalhadores, a soma"
    print ">   não passa de 2 CPUs."
    print ""

    # ---- resumo: speedup de cada configuracao em cada container
    print "## Resumo: speedup em cada container"
    print ""
    linha = "| Versão |"; sep = "|---|"
    for (i = 1; i <= nc; i++) if (ordemC[i] in temContainer) { linha = linha " " ordemC[i] " |"; sep = sep "---:|" }
    print linha; print sep
    for (j = 1; j <= ncfg; j++) {
        split(ordemCfg[j], p, ":")
        linha = "| " nomeConfig(p[1], p[2]) " |"; algum = 0
        for (i = 1; i <= nc; i++) {
            c = ordemC[i]
            if (!(c in temContainer)) continue
            k = c SUBSEP p[1] SUBSEP p[2]; ks = c SUBSEP "sequencial" SUBSEP 1
            if ((k in media) && (ks in media) && media[k] > 0) { linha = linha " " br(media[ks] / media[k], 2) "x |"; algum = 1 }
            else linha = linha " – |"
        }
        if (algum) print linha
    }
    print ""

    # ---- o melhor de cada container
    for (i = 1; i <= nc; i++) {
        c = ordemC[i]
        if (!(c in temContainer)) continue
        ks = c SUBSEP "sequencial" SUBSEP 1
        melhor = ""; melhorT = 0
        for (j = 2; j <= ncfg; j++) {
            split(ordemCfg[j], p, ":"); k = c SUBSEP p[1] SUBSEP p[2]
            if ((k in media) && (melhor == "" || media[k] < melhorT)) { melhor = nomeConfig(p[1], p[2]); melhorT = media[k] }
        }
        if (melhor != "" && (ks in media))
            print "- **" c "**: o mais rápido foi **" melhor "**: " br(melhorT, 2) " s por ativo, contra " br(media[ks], 2) " s do sequencial (" br(media[ks] / melhorT, 2) "x)."
    }
    print ""

    # ---- detalhe por container
    for (i = 1; i <= nc; i++) {
        c = ordemC[i]
        if (!(c in temContainer)) continue
        ks = c SUBSEP "sequencial" SUBSEP 1
        if (c == "fora") print "## Fora do container (sem limite de CPU)"
        else print "## Container " c
        print ""
        print "| Versão | N | Por ativo (média) | Desvio padrão | Menor – maior | Speedup | Eficiência | Tempo total (média) |"
        print "|---|---:|---:|---:|---:|---:|---:|---:|"
        for (j = 1; j <= ncfg; j++) {
            split(ordemCfg[j], p, ":"); k = c SUBSEP p[1] SUBSEP p[2]
            if (!(k in media)) continue
            sp = "–"; ef = "–"
            if ((ks in media) && media[k] > 0) { s = media[ks] / media[k]; sp = br(s, 2) "x"; ef = br(100 * s / p[2], 0) "%" }
            print "| " p[1] " | " p[2] " | " br(media[k], 3) " s | " br(desvio[k], 3) " s | " br(menor[k], 3) " – " br(maior[k], 3) " s | " sp " | " ef " | " br(somaTotal[k] / qtd[k], 2) " s |"
        }
        print ""
    }

    print "## Como foi medido"
    print ""
    if ("fora" in temContainer) {
        print "- Script `analise/benchmark.sh`, rodado direto no Linux (Ubuntu no WSL2), **fora de qualquer container**"
        print "  e sem limite de CPU. Os programas e os dados ficam no disco do próprio Linux (não na pasta do Windows)."
        print "- Os dados são copiados para `/tmp` antes de medir, igual à medição nos containers, para as duas serem comparáveis."
    } else {
        print "- Script `analise/benchmark.sh`, um container por vez (`docker compose run --rm -T <container> ...`)."
        print "- Os dados são copiados para dentro do container antes de medir: ler da pasta do Windows é lento e"
        print "  instável. Isso deixa o **tempo total** mais curto que numa execução normal, mas não muda o **por ativo**."
    }
    print "- As repetições ficam no laço de fora: cada rodada passa por todas as configurações. Se a máquina"
    print "  esquentar no meio, o efeito se espalha por todas, em vez de cair numa só."
    print "- Os tempos de cada execução estão em `bruto_<container>.csv`, nesta mesma pasta."
}
' $PASTA/bruto_*.csv > $SAIDA

echo "Benchmark gravado em $SAIDA"

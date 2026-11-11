// relatorio_md.cpp - relatorio da execucao em Markdown, feito para ser lido por quem NAO conhece o projeto.
//
// Diferente do terminal, aqui cada secao comeca explicando o que os numeros querem dizer, os indicadores
// vem com uma frase de interpretacao e os numeros saem no formato brasileiro (R$ 37,22 / +1,23% / 4.123.456).

#include "comum.h"

#include <sys/stat.h>   // mkdir

#include <cmath>
#include <cstdio>
#include <ctime>
#include <map>

// =============================================================================================
// Formatacao no padrao brasileiro

// num(1234.5678, 2) = "1.234,57"  (ponto separa os milhares, virgula separa os decimais)
static string num(double valor, int casas) {
    char texto[64];
    snprintf(texto, sizeof(texto), "%.*f", casas, valor);   // "1234.57"
    string original = texto;

    // separa o sinal, a parte inteira e os decimais
    string sinal = "";
    if (original[0] == '-') {
        sinal = "-";
        original = original.substr(1);
    }
    string inteira = original, decimais = "";
    int ponto = -1;   // posicao do '.' no texto (-1 = nao tem)
    for (int i = 0; i < (int)original.size(); i++) {
        if (original[i] == '.') ponto = i;
    }
    if (ponto >= 0) {
        inteira = original.substr(0, ponto);
        decimais = "," + original.substr(ponto + 1);
    }

    // coloca um ponto a cada 3 digitos, de tras para frente
    string comPontos = "";
    int contador = 0;
    for (int i = (int)inteira.size() - 1; i >= 0; i--) {
        comPontos = inteira[i] + comPontos;
        contador++;
        if (contador % 3 == 0 && i > 0) comPontos = "." + comPontos;
    }
    return sinal + comPontos + decimais;
}

// comSinal(1.5, 2) = "+1,50" (alta ou queda: sempre com o sinal)
static string comSinal(double valor, int casas) {
    if (valor >= 0) return "+" + num(valor, casas);
    return num(valor, casas);
}

// pct(0.0123) = "1,23%"
static string pct(double valor, int casas = 2) {
    return num(valor * 100, casas) + "%";
}

// variacao(0.0123) = "+1,23%"
static string variacao(double valor, int casas = 2) {
    return comSinal(valor * 100, casas) + "%";
}

// reais(37.2) = "R$ 37,20"
static string reais(double valor) {
    return "R$ " + num(valor, 2);
}

// inteiro(4123456) = "4.123.456"
static string inteiro(long long valor) {
    return num(valor, 0);
}

// dinheiro(3730300000) = "R$ 3.730,3 bilhões" / dinheiro(52000) = "R$ 52 mil"
static string dinheiro(double valor) {
    if (valor >= 1e9) return "R$ " + num(valor / 1e9, 1) + " bilhões";
    if (valor >= 1e6) return "R$ " + num(valor / 1e6, 1) + " milhões";
    if (valor >= 1e3) return "R$ " + num(valor / 1e3, 0) + " mil";
    return "R$ " + num(valor, 2);
}

// "2025-12-30" -> "30/12/2025"
static string dataBr(const string& data) {
    if (data.size() != 10) return data;
    return data.substr(8, 2) + "/" + data.substr(5, 2) + "/" + data.substr(0, 4);
}

// Barra de tamanho proporcional: barra(50, 100, 20) = 10 blocos
static string barra(double valor, double maximo, int largura) {
    int blocos = 0;
    if (maximo > 0) blocos = (int)(valor / maximo * largura + 0.5);
    if (valor > 0 && blocos == 0) blocos = 1;   // um valor pequeno ainda aparece
    string texto = "";
    for (int i = 0; i < blocos; i++) texto += "█";
    return texto;
}

// "+1,23 pp (± 0,90)": diferenca entre o modelo e outro jeito de prever, e quanto dela pode ser acaso
static string textoComparacao(double m, double erro) {
    return comSinal(m * 100, 2) + " pp (± " + num(erro * 100, 2) + ")";
}

// A diferenca so diz algo se for maior que o "±" (o que o acaso explica). E ela pode ser real para os dois lados:
// o modelo pode ser melhor OU pior de verdade.
static string vereditoComparacao(const AvaliacaoPrevisao& p, double m, double erro, const string& outro) {
    if (p.previsoes < 2 || fabs(m) <= erro) return "empate (dentro do acaso)";
    if (m > 0) return "**melhor** que " + outro;
    return "**pior** que " + outro;
}

static string textoMargem(const AvaliacaoPrevisao& p) {
    return textoComparacao(margem(p), erroDaMargem(p));
}

static string veredito(const AvaliacaoPrevisao& p) {
    return vereditoComparacao(p, margem(p), erroDaMargem(p), "o chute");
}

// Chutes fixos: apostar todo dia na mesma direcao
static string vereditoSempreAlta(const AvaliacaoPrevisao& p) {
    return vereditoComparacao(p, taxaDeAcerto(p) - taxaSempreAlta(p), erroSobreSempreAlta(p), "\"sempre ALTA\"");
}

static string vereditoSempreQueda(const AvaliacaoPrevisao& p) {
    return vereditoComparacao(p, taxaDeAcerto(p) - taxaSempreQueda(p), erroSobreSempreQueda(p), "\"sempre QUEDA\"");
}

// O mesmo veredito, em forma de frase: "o modelo [é melhor que X / é pior que X / empata com X]"
static string fraseComparacao(const AvaliacaoPrevisao& p, double m, double erro, const string& outro) {
    if (p.previsoes < 2 || fabs(m) <= erro) return "empata com " + outro + " (diferença dentro do acaso)";
    if (m > 0) return "é **melhor** que " + outro;
    return "é **pior** que " + outro;
}

static string fraseChutesFixos(const AvaliacaoPrevisao& p) {
    return "o modelo " +
           fraseComparacao(p, taxaDeAcerto(p) - taxaSempreAlta(p), erroSobreSempreAlta(p), "\"sempre ALTA\"") + " e " +
           fraseComparacao(p, taxaDeAcerto(p) - taxaSempreQueda(p), erroSobreSempreQueda(p), "\"sempre QUEDA\"");
}

static string textoProximoPregao(const AvaliacaoPrevisao& p) {
    if (!p.temProxima) return "indisponível";
    if (p.retornoEstimado > 0) return "ALTA (" + variacao(p.retornoEstimado, 3) + ")";
    return "QUEDA (" + variacao(p.retornoEstimado, 3) + ")";
}

static string explicarMotivo(const string& motivo) {
    if (motivo == "CISAO") return "dia de cisão: o preço mudou porque a empresa se dividiu, não pelo mercado";
    if (motivo == "GAP_SEM_EVENTO") return "salto grande no preço sem nenhum evento oficial que o explique";
    if (motivo == MOTIVO_PRECO_MINIMO) return "o preço do dia anterior estava abaixo de R$ 1,00 (centavos distorcem o retorno)";
    return "";
}

// =============================================================================================
// Frases que explicam cada indicador do ultimo pregao

static string lerMediaMovel(const Indicadores& d, double fechamento) {
    if (fechamento > d.mm20) return "fechou **acima** da média dos últimos 20 dias: tendência recente de alta";
    return "fechou **abaixo** da média dos últimos 20 dias: tendência recente de queda";
}

static string lerBollinger(const Indicadores& d, double fechamento) {
    if (fechamento > d.bollingerSuperior) return "fechou **acima** da faixa: subiu mais do que o normal";
    if (fechamento < d.bollingerInferior) return "fechou **abaixo** da faixa: caiu mais do que o normal";
    return "fechou dentro da faixa normal de oscilação";
}

static string lerIfr(double ifr) {
    if (ifr >= 70) return "acima de 70: subiu rápido demais nos últimos dias (\"sobrecomprada\")";
    if (ifr <= 30) return "abaixo de 30: caiu rápido demais nos últimos dias (\"sobrevendida\")";
    return "entre 30 e 70: zona neutra";
}

static string lerEstocastico(double k) {
    if (k >= 80) return "fechou perto da **máxima** dos últimos 14 dias";
    if (k >= 50) return "fechou na metade de cima da faixa de preço dos últimos 14 dias";
    if (k > 20) return "fechou na metade de baixo da faixa de preço dos últimos 14 dias";
    return "fechou perto da **mínima** dos últimos 14 dias";
}

static string lerMacd(const Indicadores& d) {
    if (d.macd > d.macdSinal) return "MACD acima da linha de sinal: a força está do lado da **alta**";
    return "MACD abaixo da linha de sinal: a força está do lado da **queda**";
}

static string lerMomento(double momento) {
    if (momento > 0) return "o preço **subiu** " + pct(momento) + " em 10 dias";
    if (momento < 0) return "o preço **caiu** " + pct(-momento) + " em 10 dias";
    return "o preço ficou igual ao de 10 dias atrás";
}

static string lerObv(double obv) {
    if (obv > 0) return "nos últimos 10 dias, o volume foi maior nos dias de **alta**";
    if (obv < 0) return "nos últimos 10 dias, o volume foi maior nos dias de **queda**";
    return "volume equilibrado entre dias de alta e de queda";
}

static string lerBeta(const ResultadoAtivo& r) {
    if (!r.temBeta) return "sem dias suficientes em comum com o mercado";
    if (r.beta < 0) return "negativo: costuma andar ao contrário do mercado";
    if (r.beta >= 0.9 && r.beta <= 1.1) return "perto de 1: costuma oscilar parecido com o mercado";
    if (r.beta > 1) return "acima de 1: costuma oscilar **mais** que o mercado";
    return "abaixo de 1: costuma oscilar **menos** que o mercado";
}

// =============================================================================================
// Partes do relatorio

static void escreverResumoRapido(FILE* md, const DadosPreparados& dados, const ResultadoGeral& geral,
                                 const Tempos& tempos, const string& versao, int trabalhadores) {
    const vector<ResultadoAtivo>& ativos = geral.ativos;
    const AvaliacaoPrevisao& p = geral.previsao;

    fprintf(md, "## Resumo rápido\n\n");
    fprintf(md, "- **%s ações** analisadas, de %s a %s (%s dias de pregão).\n", inteiro(ativos.size()).c_str(),
            dataBr(geral.menorData).c_str(), dataBr(geral.maiorData).c_str(), inteiro(dados.pregoes.size()).c_str());

    string direcao = geral.mercado.retornoAcumulado >= 0 ? "subiu" : "caiu";
    fprintf(md, "- O mercado (média de todas as ações analisadas) **%s %s** no período.\n", direcao.c_str(),
            pct(fabs(geral.mercado.retornoAcumulado)).c_str());

    if (!geral.maioresAltas.empty()) {
        const Registro& dia = dados.registros[geral.maioresAltas[0].indice];
        fprintf(md, "- Maior alta em um único dia: **%s** (%s), **%s** em %s.\n", dia.ativo.c_str(),
                dia.empresa.c_str(), variacao(geral.maioresAltas[0].retorno).c_str(), dataBr(dia.data).c_str());
    }
    if (!geral.maioresQuedas.empty()) {
        const Registro& dia = dados.registros[geral.maioresQuedas[0].indice];
        fprintf(md, "- Maior queda em um único dia: **%s** (%s), **%s** em %s.\n", dia.ativo.c_str(),
                dia.empresa.c_str(), variacao(geral.maioresQuedas[0].retorno).c_str(), dataBr(dia.data).c_str());
    }

    vector<int> negociadas = maisNegociadas(ativos);
    if (!negociadas.empty()) {
        const ResultadoAtivo& r = ativos[negociadas[0]];
        fprintf(md, "- Ação mais negociada: **%s** (%s), %s no período.\n", nomeAtivo(dados, r).c_str(),
                nomeEmpresa(dados, r).c_str(), dinheiro(r.volumeFinanceiroTotal).c_str());
    }

    fprintf(md, "- Previsão: o modelo acertou a direção (alta ou queda) em **%s** dos dias. Um chute simples "
                "(\"vai repetir o que mais aconteceu nos últimos %d pregões\") acertaria **%s**. ",
            pct(taxaDeAcerto(p)).c_str(), JANELA_TREINO, pct(taxaBaseline(p)).c_str());
    string diferenca = comSinal(margem(p) * 100, 2) + " pp";
    if (!margemSignificativa(p)) {
        fprintf(md, "A diferença (%s) está dentro do que o acaso explica: **na prática, o modelo empata com o "
                    "chute simples**.\n", diferenca.c_str());
    } else if (margem(p) > 0) {
        fprintf(md, "O modelo é **melhor** que o chute, e a diferença (%s) é grande demais para ser só sorte.\n",
                diferenca.c_str());
    } else {
        fprintf(md, "O modelo é **pior** que o chute (%s), e a diferença é grande demais para ser só azar: "
                    "**na prática, ele não ajuda a prever**.\n", diferenca.c_str());
    }
    fprintf(md, "- Contra os chutes fixos (apostar todo dia na mesma direção): \"sempre ALTA\" acertaria **%s** e "
                "\"sempre QUEDA\", **%s**: %s.\n",
            pct(taxaSempreAlta(p)).c_str(), pct(taxaSempreQueda(p)).c_str(), fraseChutesFixos(p).c_str());
    fprintf(md, "- Para o pregão seguinte a %s, o modelo prevê **%s ações em ALTA** e **%s em QUEDA**.\n",
            dataBr(dados.pregoes.back()).c_str(), inteiro(geral.proximasAlta).c_str(),
            inteiro(geral.proximasQueda).c_str());

    double total = tempos.leitura + tempos.preparacao + tempos.porAtivo + tempos.entreAtivos;
    if (versao == "sequencial") {
        fprintf(md, "- Tempo total: **%s s**, dos quais %s s analisando as ações uma por uma.\n\n",
                num(total, 2).c_str(), num(tempos.porAtivo, 2).c_str());
    } else {
        fprintf(md, "- Tempo total: **%s s**, dos quais %s s na parte dividida entre os %d trabalhadores (%s).\n\n",
                num(total, 2).c_str(), num(tempos.porAtivo, 2).c_str(), trabalhadores, versao.c_str());
    }
}

static void escreverDados(FILE* md, const DadosPreparados& dados, const ResultadoGeral& geral) {
    fprintf(md, "## 1. De onde vieram os dados\n\n");
    fprintf(md, "> Os preços diários vêm dos arquivos oficiais da B3 (COTAHIST). Nem todo papel entra na análise: "
                "ficam só as **ações** de empresas, negociadas quase todo dia (para os cálculos não terem buracos) "
                "e com fonte oficial de eventos como desdobramentos (para corrigir os preços antigos).\n\n");

    fprintf(md, "| Etapa | Quantidade |\n|---|---:|\n");
    fprintf(md, "| Cotações lidas (todos os papéis: ações, fundos, BDRs...) | %s |\n", inteiro(dados.registrosLidos).c_str());
    fprintf(md, "| Cotações de ações | %s |\n", inteiro(dados.registros.size()).c_str());
    fprintf(md, "| Ações diferentes nos dados | %s |\n", inteiro(dados.ativosAcoes).c_str());
    fprintf(md, "| ... negociadas em pelo menos %s dos dias | %s |\n", pct(PRESENCA_MINIMA, 0).c_str(),
            inteiro(dados.ativosLiquidos).c_str());
    fprintf(md, "| ... e com fonte oficial de eventos: **analisadas** | **%s** |\n", inteiro(dados.blocos.size()).c_str());
    fprintf(md, "| Eventos corporativos aplicados (desdobramentos, grupamentos...) | %s |\n",
            inteiro(dados.ajustesAplicados).c_str());
    fprintf(md, "| Retornos diários usados nos cálculos | %s |\n\n", inteiro(geral.totalRetornos).c_str());

    // contagem dos descartes por motivo
    map<string, int> porMotivo;
    for (int b = 0; b < (int)dados.blocos.size(); b++) {
        for (int i = dados.blocos[b].inicio + 1; i < dados.blocos[b].fim; i++) {
            if (dados.registros[i].motivoDescarte != "") porMotivo[dados.registros[i].motivoDescarte]++;
        }
    }
    fprintf(md, "### Dias que ficaram de fora\n\n");
    fprintf(md, "> Alguns dias têm uma variação de preço que não foi causada pelo mercado. Eles são descartados "
                "para não distorcer as médias, e os indicadores daquela ação recomeçam do zero depois deles.\n\n");
    fprintf(md, "| Motivo | Dias | O que significa |\n|---|---:|---|\n");
    for (map<string, int>::iterator it = porMotivo.begin(); it != porMotivo.end(); it++) {
        fprintf(md, "| %s | %s | %s |\n", it->first.c_str(), inteiro(it->second).c_str(),
                explicarMotivo(it->first).c_str());
    }
    fprintf(md, "\nLista completa, dia a dia: `%s`.\n\n", ARQUIVO_DESCARTADOS.c_str());
}

static void escreverMercado(FILE* md, const ResultadoGeral& geral) {
    fprintf(md, "## 2. Como o mercado se comportou\n\n");
    fprintf(md, "> Os arquivos da B3 não trazem o Ibovespa, então o programa monta um **índice próprio**: em cada dia, "
                "a média da variação de todas as ações analisadas (todas com o mesmo peso).\n\n");
    fprintf(md, "| | |\n|---|---:|\n");
    fprintf(md, "| Variação acumulada no período | **%s** |\n", variacao(geral.mercado.retornoAcumulado).c_str());
    fprintf(md, "| Oscilação típica em um dia (volatilidade) | %s |\n", pct(geral.mercado.volatilidade).c_str());
    fprintf(md, "| Dias com pregão no índice | %s |\n\n", inteiro(geral.mercado.dias).c_str());

    // histograma em barras
    long long total = 0, maior = 0;
    for (int f = 0; f < NUM_FAIXAS; f++) {
        total += geral.histograma[f];
        if (geral.histograma[f] > maior) maior = geral.histograma[f];
    }
    if (total == 0) return;

    // faixas "-1% a 0%" e "0% a 1%" (ver faixaDoRetorno): os dias "calmos"
    long long calmos = geral.histograma[FAIXAS_POR_LADO] + geral.histograma[FAIXAS_POR_LADO + 1];
    fprintf(md, "### Como as variações diárias se distribuem\n\n");
    fprintf(md, "> Cada linha conta quantas vezes uma ação variou dentro daquela faixa em um dia. Em **%s** das "
                "vezes a variação ficou entre -1%% e +1%%: a maior parte dos dias é calma, e os dias extremos "
                "(as duas pontas) são raros.\n\n", pct((double)calmos / total, 1).c_str());
    fprintf(md, "| Variação no dia | Vezes | %% | |\n|---|---:|---:|---|\n");
    for (int f = 0; f < NUM_FAIXAS; f++) {
        fprintf(md, "| %s | %s | %s | %s |\n", rotuloDaFaixa(f).c_str(), inteiro(geral.histograma[f]).c_str(),
                pct((double)geral.histograma[f] / total, 1).c_str(),
                barra(geral.histograma[f], maior, 30).c_str());
    }
    fprintf(md, "\n");
}

static void tabelaExtremos(FILE* md, const DadosPreparados& dados, const vector<Extremo>& extremos) {
    fprintf(md, "| # | Ação | Empresa | Dia | Variação |\n|---:|---|---|---|---:|\n");
    for (int k = 0; k < (int)extremos.size(); k++) {
        const Registro& dia = dados.registros[extremos[k].indice];
        fprintf(md, "| %d | %s | %s | %s | %s |\n", k + 1, dia.ativo.c_str(), dia.empresa.c_str(),
                dataBr(dia.data).c_str(), variacao(extremos[k].retorno).c_str());
    }
    fprintf(md, "\n");
}

static void escreverExtremos(FILE* md, const DadosPreparados& dados, const ResultadoGeral& geral) {
    fprintf(md, "## 3. Maiores altas e quedas em um único dia\n\n");
    fprintf(md, "> Saltos desse tamanho quase sempre vêm de uma notícia da própria empresa (resultado, venda, "
                "recuperação judicial...). Os preços já estão corrigidos por desdobramentos e grupamentos, então "
                "esses saltos são movimentos reais de mercado.\n\n");
    fprintf(md, "**Maiores altas**\n\n");
    tabelaExtremos(md, dados, geral.maioresAltas);
    fprintf(md, "**Maiores quedas**\n\n");
    tabelaExtremos(md, dados, geral.maioresQuedas);
}

static void escreverRankings(FILE* md, const DadosPreparados& dados, const ResultadoGeral& geral) {
    const vector<ResultadoAtivo>& ativos = geral.ativos;
    vector<int> ordem;

    fprintf(md, "## 4. Ações mais negociadas\n\n");
    fprintf(md, "> Soma de todo o dinheiro que trocou de mãos com a ação no período.\n\n");
    fprintf(md, "| # | Ação | Empresa | Total negociado |\n|---:|---|---|---:|\n");
    ordem = maisNegociadas(ativos);
    for (int k = 0; k < (int)ordem.size(); k++) {
        const ResultadoAtivo& r = ativos[ordem[k]];
        fprintf(md, "| %d | %s | %s | %s |\n", k + 1, nomeAtivo(dados, r).c_str(), nomeEmpresa(dados, r).c_str(),
                dinheiro(r.volumeFinanceiroTotal).c_str());
    }

    fprintf(md, "\n## 5. Volatilidade: quanto o preço balança\n\n");
    fprintf(md, "> A volatilidade é a variação **típica** de um dia (o desvio padrão dos retornos diários). "
                "Uma ação com 3%% de volatilidade costuma subir ou cair uns 3%% num dia comum. Quanto maior, "
                "mais arriscada.\n\n");
    for (int lado = 0; lado < 2; lado++) {
        bool maisVolateis = (lado == 0);
        fprintf(md, "**%s**\n\n", maisVolateis ? "Mais voláteis (mais arriscadas)" : "Menos voláteis (mais calmas)");
        fprintf(md, "| # | Ação | Empresa | Volatilidade diária |\n|---:|---|---|---:|\n");
        ordem = rankingVolatilidade(ativos, maisVolateis);
        for (int k = 0; k < (int)ordem.size(); k++) {
            const ResultadoAtivo& r = ativos[ordem[k]];
            fprintf(md, "| %d | %s | %s | %s |\n", k + 1, nomeAtivo(dados, r).c_str(), nomeEmpresa(dados, r).c_str(),
                    pct(r.volatilidade).c_str());
        }
        fprintf(md, "\n");
    }

    fprintf(md, "## 6. Ações que andam juntas (correlação)\n\n");
    fprintf(md, "> A correlação vai de -1 a 1. Perto de **1**: quando uma sobe, a outra quase sempre sobe junto. "
                "Perto de **0**: não têm relação. Ficam de fora pares da mesma empresa (como PETR3 e PETR4), que "
                "andam juntos por definição. Só entram pares com pelo menos %d dias em comum.\n\n",
            MINIMO_DIAS_COMUNS);
    fprintf(md, "| # | Par | Empresas | Correlação | Dias em comum |\n|---:|---|---|---:|---:|\n");
    for (int k = 0; k < (int)geral.correlacoes.size(); k++) {
        const ParCorrelacao& par = geral.correlacoes[k];
        const ResultadoAtivo& a = ativos[par.a];
        const ResultadoAtivo& b = ativos[par.b];
        fprintf(md, "| %d | %s × %s | %s × %s | %s | %s |\n", k + 1, nomeAtivo(dados, a).c_str(),
                nomeAtivo(dados, b).c_str(), nomeEmpresa(dados, a).c_str(), nomeEmpresa(dados, b).c_str(),
                num(par.correlacao, 3).c_str(), inteiro(par.diasComuns).c_str());
    }
    fprintf(md, "\n");
}

static void tabelaMargem(FILE* md, const DadosPreparados& dados, const ResultadoGeral& geral, const vector<int>& ordem) {
    fprintf(md, "| # | Ação | Empresa | Modelo | Chute simples | Margem | Veredito | Dias | Próximo pregão |\n");
    fprintf(md, "|---:|---|---|---:|---:|---:|---|---:|---|\n");
    for (int k = 0; k < (int)ordem.size(); k++) {
        const ResultadoAtivo& r = geral.ativos[ordem[k]];
        const AvaliacaoPrevisao& p = r.previsao;
        fprintf(md, "| %d | %s | %s | %s | %s | %s | %s | %s | %s |\n", k + 1, nomeAtivo(dados, r).c_str(),
                nomeEmpresa(dados, r).c_str(), pct(taxaDeAcerto(p)).c_str(), pct(taxaBaseline(p)).c_str(),
                textoMargem(p).c_str(), veredito(p).c_str(), inteiro(p.previsoes).c_str(),
                textoProximoPregao(p).c_str());
    }
    fprintf(md, "\n");
}

static void escreverPrevisao(FILE* md, const DadosPreparados& dados, const ResultadoGeral& geral) {
    const AvaliacaoPrevisao& p = geral.previsao;

    fprintf(md, "## 7. Previsão: o modelo acerta?\n\n");
    fprintf(md, "> **Como funciona:** para cada ação e cada dia, o programa olha os %d indicadores daquele dia "
                "(média móvel, IFR, MACD...) e estima se o preço vai subir ou cair no dia seguinte. Ele aprende "
                "uma fórmula (regressão linear) usando **só os %d dias anteriores**, prevê o dia seguinte e "
                "confere com o que aconteceu de verdade. Nenhuma informação do futuro é usada.\n>\n",
            NUM_VARIAVEIS - 1, JANELA_TREINO);
    if (RIDGE > 0) {
        fprintf(md, "> **Freio (Ridge = %s):** com tantos indicadores parecidos e poucos dias de treino, a fórmula "
                    "tende a \"decorar\" o ruído daquele período e errar no seguinte. O Ridge segura o peso de cada "
                    "indicador perto de zero, e só deixa crescer quem realmente ajuda.\n>\n",
                num(RIDGE, 2).c_str());
    }
    fprintf(md, "> **Por que comparar com um chute simples:** acertar 55%% parece bom, mas se a ação subiu em 55%% "
                "dos dias, quem chuta \"sempre ALTA\" também acerta 55%%. Então o modelo é comparado com o chute "
                "\"vai repetir a direção que mais aconteceu nos últimos %d dias\". A **margem** é a diferença entre "
                "os dois, em pontos percentuais (pp). O ± diz quanto essa diferença pode ser só sorte: se a margem "
                "passar do ±, a diferença é real — para melhor **ou para pior**. Se não passar, é empate.\n\n",
            JANELA_TREINO);

    fprintf(md, "| Todas as ações juntas | |\n|---|---:|\n");
    fprintf(md, "| Dias previstos e conferidos | %s |\n", inteiro(p.previsoes).c_str());
    fprintf(md, "| Acerto do modelo | **%s** |\n", pct(taxaDeAcerto(p)).c_str());
    fprintf(md, "| Acerto do chute simples | **%s** |\n", pct(taxaBaseline(p)).c_str());
    fprintf(md, "| Margem do modelo | **%s** |\n", textoMargem(p).c_str());
    fprintf(md, "| Veredito | %s |\n", veredito(p).c_str());
    fprintf(md, "| Fora da conta: dias em que o preço não mudou | %s |\n", inteiro(p.semMovimento).c_str());
    fprintf(md, "| Fora da conta: janelas em que a fórmula não teve solução | %s |\n\n", inteiro(p.semSolucao).c_str());

    fprintf(md, "> **Chutes fixos:** a régua mais simples de todas é apostar **todo dia na mesma direção**. Quem "
                "chuta \"sempre ALTA\" acerta exatamente os dias que subiram; \"sempre QUEDA\", os que caíram. "
                "Um modelo que não passa nem disso não aprendeu nada. O chute simples acima é mais esperto que "
                "estes: ele troca de lado quando a tendência muda.\n\n");
    fprintf(md, "| Chute fixo | Acerto | Modelo contra ele | Veredito |\n|---|---:|---:|---|\n");
    fprintf(md, "| Sempre ALTA | %s | %s | %s |\n", pct(taxaSempreAlta(p)).c_str(),
            textoComparacao(taxaDeAcerto(p) - taxaSempreAlta(p), erroSobreSempreAlta(p)).c_str(),
            vereditoSempreAlta(p).c_str());
    fprintf(md, "| Sempre QUEDA | %s | %s | %s |\n\n", pct(taxaSempreQueda(p)).c_str(),
            textoComparacao(taxaDeAcerto(p) - taxaSempreQueda(p), erroSobreSempreQueda(p)).c_str(),
            vereditoSempreQueda(p).c_str());

    fprintf(md, "**Ações em que o modelo foi melhor que o chute** (só ações com %d+ dias previstos)\n\n", JANELA_TREINO);
    tabelaMargem(md, dados, geral, rankingMargem(geral.ativos, true));
    fprintf(md, "**Ações em que o modelo foi pior que o chute**\n\n");
    tabelaMargem(md, dados, geral, rankingMargem(geral.ativos, false));
}

static void secaoAtivo(FILE* md, const DadosPreparados& dados, const ResultadoAtivo& r) {
    const Indicadores& d = r.ultimo;
    const AvaliacaoPrevisao& p = r.previsao;

    fprintf(md, "### %s — %s\n\n", nomeAtivo(dados, r).c_str(), nomeEmpresa(dados, r).c_str());

    // previsao primeiro: e o que a pessoa procura
    if (p.temProxima) {
        fprintf(md, "**Previsão para o próximo pregão: %s** (variação estimada de %s).",
                p.retornoEstimado > 0 ? "ALTA" : "QUEDA", variacao(p.retornoEstimado, 3).c_str());
    } else if (!r.negociadaNoUltimoPregao) {
        fprintf(md, "**Previsão para o próximo pregão: indisponível** (a ação não foi negociada no último dia dos "
                    "dados; o último dia dela foi %s).", dataBr(ultimaData(dados, r)).c_str());
    } else {
        fprintf(md, "**Previsão para o próximo pregão: indisponível** (poucos dados no fim do período).");
    }
    if (p.previsoes > 0) {
        fprintf(md, " No passado, o modelo acertou %s de %s dias nesta ação; o chute simples acertaria %s. "
                    "Margem %s: %s. Chutes fixos: \"sempre ALTA\" acertaria %s e \"sempre QUEDA\", %s: %s.\n\n",
                pct(taxaDeAcerto(p)).c_str(), inteiro(p.previsoes).c_str(), pct(taxaBaseline(p)).c_str(),
                textoMargem(p).c_str(), veredito(p).c_str(), pct(taxaSempreAlta(p)).c_str(),
                pct(taxaSempreQueda(p)).c_str(), fraseChutesFixos(p).c_str());
    } else {
        fprintf(md, " Ainda não há histórico de acerto (precisa de %d dias de treino).\n\n", JANELA_TREINO);
    }

    fprintf(md, "| No período | Valor | Como ler |\n|---|---:|---|\n");
    fprintf(md, "| Período | %s a %s | %s dias de pregão |\n", dataBr(primeiraData(dados, r)).c_str(),
            dataBr(ultimaData(dados, r)).c_str(), inteiro(r.pregoes).c_str());
    fprintf(md, "| Preço médio | %s | mínimo %s, máximo %s (corrigidos por desdobramentos) |\n",
            reais(r.precoMedio).c_str(), reais(r.precoMinimo).c_str(), reais(r.precoMaximo).c_str());
    fprintf(md, "| Variação média por dia | %s | |\n", variacao(r.retornoMedio, 3).c_str());
    fprintf(md, "| Volatilidade | %s | num dia comum, sobe ou cai uns %s |\n", pct(r.volatilidade).c_str(),
            pct(r.volatilidade, 1).c_str());
    string beta = "-";
    if (r.temBeta) beta = num(r.beta, 2);
    fprintf(md, "| Beta | %s | %s |\n", beta.c_str(), lerBeta(r).c_str());
    fprintf(md, "| Volume médio | %s por dia | dinheiro negociado num dia típico |\n\n",
            dinheiro(r.volumeFinanceiroMedio).c_str());

    fprintf(md, "| No último pregão (%s) | Valor | Como ler |\n|---|---:|---|\n", dataBr(ultimaData(dados, r)).c_str());
    fprintf(md, "| Fechamento | %s | |\n", reais(r.fechamentoUltimo).c_str());
    if (!d.valido) {
        fprintf(md, "| Indicadores | indisponíveis | o último trecho sem dias descartados tem menos de %d pregões |\n\n",
                AQUECIMENTO + 1);
        return;
    }
    fprintf(md, "| Média móvel (%d dias) | %s | %s |\n", JANELA_MEDIA, reais(d.mm20).c_str(),
            lerMediaMovel(d, r.fechamentoUltimo).c_str());
    fprintf(md, "| Bandas de Bollinger | %s a %s | %s |\n", reais(d.bollingerInferior).c_str(),
            reais(d.bollingerSuperior).c_str(), lerBollinger(d, r.fechamentoUltimo).c_str());
    fprintf(md, "| Volatilidade (%d dias) | %s | variação típica de um dia nas últimas semanas |\n",
            JANELA_VOLATILIDADE, pct(d.volatilidade20).c_str());
    fprintf(md, "| IFR (%d) | %s | %s |\n", JANELA_IFR, num(d.ifr, 1).c_str(), lerIfr(d.ifr).c_str());
    fprintf(md, "| Estocástico %%K (%d) | %s | %s |\n", JANELA_ESTOCASTICO, num(d.estocasticoK, 1).c_str(),
            lerEstocastico(d.estocasticoK).c_str());
    fprintf(md, "| MACD (sinal) | %s (%s) | %s |\n", num(d.macd, 3).c_str(), num(d.macdSinal, 3).c_str(),
            lerMacd(d).c_str());
    fprintf(md, "| Momento (%d dias) | %s | %s |\n", JANELA_MOMENTO, variacao(d.momento).c_str(),
            lerMomento(d.momento).c_str());
    fprintf(md, "| ATR (%d) | %s | o preço costuma andar uns %s por dia (%s do preço) |\n", JANELA_ATR,
            reais(d.atr).c_str(), reais(d.atr).c_str(), pct(d.atr / r.fechamentoUltimo, 1).c_str());
    fprintf(md, "| OBV (%d dias) | %s | %s |\n\n", JANELA_OBV, variacao(d.obvVariacao, 1).c_str(),
            lerObv(d.obvVariacao).c_str());
}

static void escreverDestaques(FILE* md, const DadosPreparados& dados, const ResultadoGeral& geral,
                              const string& ativoPedido) {
    fprintf(md, "## 8. Ações em destaque\n\n");

    if (ativoPedido != "") {
        fprintf(md, "> A ação pedida na hora de rodar o programa.\n\n");
        int posicao = procurarAtivo(dados, geral, ativoPedido);
        if (posicao >= 0) {
            secaoAtivo(md, dados, geral.ativos[posicao]);
        } else {
            fprintf(md, "A ação **%s** não foi encontrada entre as analisadas: ou não existe nos dados, ou não é "
                        "ação, ou saiu nos filtros (pouco negociada / sem fonte oficial de eventos).\n\n",
                    ativoPedido.c_str());
        }
        return;
    }

    fprintf(md, "> As %d ações com a **maior alta** e as %d com a **maior queda** previstas para o próximo pregão "
                "(entre as que têm pelo menos %d dias de histórico de acerto). Para ver uma ação específica, "
                "rode o programa com o código dela, por exemplo `./analise/build/sequencial PETR4`.\n>\n"
                "> Lembre: cada previsão vem com o histórico de acerto da ação. Se o veredito não for "
                "\"melhor que o chute\", desconfie da previsão.\n\n",
            ATIVOS_EM_DESTAQUE, ATIVOS_EM_DESTAQUE, JANELA_TREINO);

    vector<int> altas = rankingPrevisaoProxima(geral.ativos, true, ATIVOS_EM_DESTAQUE);
    fprintf(md, "---\n\n**Maiores altas previstas**\n\n");
    for (int k = 0; k < (int)altas.size(); k++) secaoAtivo(md, dados, geral.ativos[altas[k]]);

    vector<int> quedas = rankingPrevisaoProxima(geral.ativos, false, ATIVOS_EM_DESTAQUE);
    fprintf(md, "---\n\n**Maiores quedas previstas**\n\n");
    for (int k = 0; k < (int)quedas.size(); k++) secaoAtivo(md, dados, geral.ativos[quedas[k]]);
}

static void escreverTempos(FILE* md, const string& versao, int trabalhadores, const Tempos& tempos) {
    double total = tempos.leitura + tempos.preparacao + tempos.porAtivo + tempos.entreAtivos;

    fprintf(md, "## 9. Tempo de execução\n\n");
    if (versao == "sequencial") {
        fprintf(md, "> Versão **sequencial**: um ativo depois do outro, num fluxo só.\n\n");
    } else {
        fprintf(md, "> Versão **%s** com **%d trabalhadores**. Só a etapa \"por ativo\" é dividida entre eles: "
                    "cada um analisa uma parte das ações ao mesmo tempo. As outras etapas rodam num fluxo só. "
                    "Para medir o ganho, compare este tempo com o da versão sequencial e com outros números de "
                    "trabalhadores.\n\n", versao.c_str(), trabalhadores);
    }
    fprintf(md, "| Etapa | Segundos | %% do total | |\n|---|---:|---:|---|\n");
    fprintf(md, "| Leitura dos arquivos | %s | %s | %s |\n", num(tempos.leitura, 3).c_str(),
            pct(tempos.leitura / total, 1).c_str(), barra(tempos.leitura, total, 20).c_str());
    fprintf(md, "| Preparação (filtros, ordenação, eventos) | %s | %s | %s |\n", num(tempos.preparacao, 3).c_str(),
            pct(tempos.preparacao / total, 1).c_str(), barra(tempos.preparacao, total, 20).c_str());
    fprintf(md, "| **Por ativo** (indicadores, previsão...) — dividida | **%s** | %s | %s |\n",
            num(tempos.porAtivo, 3).c_str(), pct(tempos.porAtivo / total, 1).c_str(),
            barra(tempos.porAtivo, total, 20).c_str());
    fprintf(md, "| Entre ativos (mercado, beta, correlação) | %s | %s | %s |\n", num(tempos.entreAtivos, 3).c_str(),
            pct(tempos.entreAtivos / total, 1).c_str(), barra(tempos.entreAtivos, total, 20).c_str());
    fprintf(md, "| **Total** | **%s** | 100%% | |\n\n", num(total, 3).c_str());
}

static void escreverRegrasEGlossario(FILE* md) {
    fprintf(md, "## 10. Regras aplicadas\n\n");
    fprintf(md, "- Só ações de empresas (tipos ON, PN, PNA, PNB, PNC e UNT). Fundos, BDRs e direitos ficam de fora.\n");
    fprintf(md, "- A ação precisa ter sido negociada em pelo menos %s dos pregões do período em que existiu, com "
                "no mínimo %d dias.\n", pct(PRESENCA_MINIMA, 0).c_str(), MINIMO_DIAS_NEGOCIADOS);
    fprintf(md, "- Ações sem nenhuma fonte oficial de eventos (CVM ou B3) ficam de fora.\n");
    fprintf(md, "- Desdobramentos, grupamentos e bonificações: os preços antigos são corrigidos pelo fator oficial, "
                "para não parecer que a ação despencou ou disparou.\n");
    fprintf(md, "- Os retornos são só de **preço** (dividendos não entram).\n");
    fprintf(md, "- Dias descartados: cisões, saltos sem evento oficial e preço anterior abaixo de %s.\n\n",
            reais(PRECO_MINIMO).c_str());

    fprintf(md, "## Glossário\n\n");
    fprintf(md, "| Termo | O que é |\n|---|---|\n");
    fprintf(md, "| Pregão | Um dia de negociação na bolsa. |\n");
    fprintf(md, "| Retorno / variação diária | Quanto o preço de fechamento mudou em relação ao dia anterior, em %%. |\n");
    fprintf(md, "| Volatilidade | Variação típica de um dia (desvio padrão dos retornos). Mede o risco. |\n");
    fprintf(md, "| Beta | Quanto a ação costuma se mover quando o mercado se move 1%%. Beta 1,5: o mercado sobe 1%%, "
                "a ação tende a subir 1,5%%. |\n");
    fprintf(md, "| Correlação | De -1 a 1: o quanto duas ações sobem e caem juntas. |\n");
    fprintf(md, "| Média móvel (MM20) | Média dos fechamentos dos últimos 20 dias. Suaviza o sobe e desce. |\n");
    fprintf(md, "| Bandas de Bollinger | Faixa de 2 desvios padrão em volta da MM20: onde o preço costuma ficar. |\n");
    fprintf(md, "| IFR (RSI) | De 0 a 100: compara as altas com as quedas dos últimos 14 dias. |\n");
    fprintf(md, "| Estocástico %%K | De 0 a 100: onde o fechamento está entre a mínima e a máxima dos últimos 14 dias. |\n");
    fprintf(md, "| MACD | Diferença entre uma média rápida (12 dias) e uma lenta (26). A linha de sinal é a média "
                "de 9 dias do próprio MACD. |\n");
    fprintf(md, "| Momento | Variação do preço em 10 dias. |\n");
    fprintf(md, "| ATR | Amplitude média de um dia (máxima menos mínima, contando saltos), em R$. |\n");
    fprintf(md, "| OBV | Compara o volume dos dias de alta com o dos dias de queda. |\n");
    fprintf(md, "| Chute fixo | Apostar todo dia na mesma direção (\"sempre ALTA\" ou \"sempre QUEDA\"). É a régua "
                "mínima: um modelo que não passa dela não aprendeu nada. |\n");
    fprintf(md, "| Chute simples (baseline) | Prever que amanhã vai repetir a direção mais comum dos últimos %d dias. |\n",
            JANELA_TREINO);
    fprintf(md, "| Margem | Acerto do modelo menos acerto do chute simples, em pontos percentuais (pp). |\n");
    fprintf(md, "| pp (ponto percentual) | Diferença entre duas porcentagens: de 50%% para 52%% são +2 pp. |\n\n");

    fprintf(md, "## Arquivos desta execução\n\n");
    fprintf(md, "- `%s`: todas as ações, uma por linha, com todos os números (abre no Excel)\n",
            ARQUIVO_RELATORIO_ATIVOS.c_str());
    fprintf(md, "- `%s`: quantas variações diárias caíram em cada faixa\n", ARQUIVO_DISTRIBUICAO.c_str());
    fprintf(md, "- `%s`: cada dia descartado e o motivo\n", ARQUIVO_DESCARTADOS.c_str());
}

// =============================================================================================

string gravarRelatorioMarkdown(const string& versao, int trabalhadores, const DadosPreparados& dados,
                               const ResultadoGeral& geral, const Tempos& tempos, const string& ativoPedido) {
    // data e hora de agora (um instante so, para o titulo e o nome do arquivo baterem)
    time_t agora = time(NULL);
    tm* momento = localtime(&agora);
    char dataHora[40], paraNome[40];
    strftime(dataHora, sizeof(dataHora), "%d/%m/%Y às %H:%M:%S", momento);
    strftime(paraNome, sizeof(paraNome), "%Y-%m-%d_%H-%M-%S", momento);

    // resultados/analise/relatorios/relatorio_threads_4_2026-11-06_10-30-00.md
    string caminho = PASTA_RELATORIOS + "/relatorio_" + versao + "_";
    if (versao != "sequencial") caminho += to_string(trabalhadores) + "_";
    caminho += string(paraNome) + ".md";

    mkdir("resultados", 0755);   // se ja existir, nao faz nada
    mkdir(PASTA_RESULTADOS.c_str(), 0755);
    mkdir(PASTA_RELATORIOS.c_str(), 0755);
    FILE* md = fopen(caminho.c_str(), "w");
    if (md == NULL) {
        printf("Erro: nao foi possivel criar %s\n", caminho.c_str());
        return "";
    }

    fprintf(md, "# Relatório da análise de mercado\n\n");
    fprintf(md, "Versão **%s**", versao.c_str());
    if (versao != "sequencial") fprintf(md, " com **%d trabalhadores**", trabalhadores);
    fprintf(md, " · gerado em %s\n\n", dataHora);
    fprintf(md, "> Este relatório foi gerado automaticamente pelo programa. Ele resume o que foi encontrado nos "
                "preços diários das ações da B3. Cada seção começa com uma explicação curta do que os números "
                "querem dizer, e no fim há um glossário.\n>\n"
                "> **Isto é um trabalho acadêmico, não uma recomendação de investimento.**\n\n");

    escreverResumoRapido(md, dados, geral, tempos, versao, trabalhadores);
    escreverDados(md, dados, geral);
    escreverMercado(md, geral);
    escreverExtremos(md, dados, geral);
    escreverRankings(md, dados, geral);
    escreverPrevisao(md, dados, geral);
    escreverDestaques(md, dados, geral, ativoPedido);
    escreverTempos(md, versao, trabalhadores, tempos);
    escreverRegrasEGlossario(md);

    fclose(md);
    return caminho;
}

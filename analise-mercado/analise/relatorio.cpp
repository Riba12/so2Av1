// relatorio.cpp - mostra os resultados no terminal e grava os CSVs (iguais nas tres versoes).

#include "comum.h"

#include <sys/stat.h>   // mkdir

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

// =============================================================================================
// Nome, empresa e datas de um ativo: ficam em registros (o ResultadoAtivo so guarda numeros)

string nomeAtivo(const DadosPreparados& dados, const ResultadoAtivo& r) {
    return dados.registros[r.inicio].ativo;
}

string nomeEmpresa(const DadosPreparados& dados, const ResultadoAtivo& r) {
    return dados.registros[r.fim - 1].empresa;   // o nome mais recente
}

string primeiraData(const DadosPreparados& dados, const ResultadoAtivo& r) {
    return dados.registros[r.inicio].data;
}

string ultimaData(const DadosPreparados& dados, const ResultadoAtivo& r) {
    return dados.registros[r.fim - 1].data;
}

// =============================================================================================
// Taxas da previsao

double taxaDeAcerto(const AvaliacaoPrevisao& p) {
    if (p.previsoes == 0) return 0;
    return (double)p.acertos / p.previsoes;
}

double taxaBaseline(const AvaliacaoPrevisao& p) {
    if (p.previsoes == 0) return 0;
    return (double)p.acertosBaseline / p.previsoes;
}

// Margem = acerto do modelo - acerto do baseline, nos mesmos dias
double margem(const AvaliacaoPrevisao& p) {
    return taxaDeAcerto(p) - taxaBaseline(p);
}

// Quanto a diferenca entre dois jeitos de prever (nos mesmos n dias) pode variar so por acaso (intervalo de 95%).
// "discordancias" = dias em que os dois previram direcoes diferentes (um acerta e o outro erra). Ver CALCULOS.md.
static double erroDaComparacao(long previsoes, long discordancias, double m) {
    double n = previsoes;
    if (n < 2) return 0;
    double variancia = (discordancias - n * m * m) / (n - 1);
    return 1.96 * sqrt(max(variancia, 0.0) / n);
}

double erroDaMargem(const AvaliacaoPrevisao& p) {
    return erroDaComparacao(p.previsoes, p.discordancias, margem(p));
}

bool margemSignificativa(const AvaliacaoPrevisao& p) {
    return p.previsoes >= 2 && fabs(margem(p)) > erroDaMargem(p);
}

// Chutes fixos: quem aposta "sempre ALTA" acerta exatamente os dias que subiram; "sempre QUEDA", o resto.
double taxaSempreAlta(const AvaliacaoPrevisao& p) {
    if (p.previsoes == 0) return 0;
    return (double)p.altasReais / p.previsoes;
}

double taxaSempreQueda(const AvaliacaoPrevisao& p) {
    if (p.previsoes == 0) return 0;
    return 1 - taxaSempreAlta(p);
}

// O modelo discorda de "sempre ALTA" nos dias em que previu QUEDA, e de "sempre QUEDA" nos dias em que previu ALTA
double erroSobreSempreAlta(const AvaliacaoPrevisao& p) {
    return erroDaComparacao(p.previsoes, p.previsoes - p.previsoesAlta, taxaDeAcerto(p) - taxaSempreAlta(p));
}

double erroSobreSempreQueda(const AvaliacaoPrevisao& p) {
    return erroDaComparacao(p.previsoes, p.previsoesAlta, taxaDeAcerto(p) - taxaSempreQueda(p));
}

// "+1.23 pp (+-0.90)" e, se for maior que o acaso, " *" no fim
static string textoComparacao(const AvaliacaoPrevisao& p, double m, double erro) {
    char texto[100];
    snprintf(texto, sizeof(texto), "%+.2f pp (+-%.2f)%s", m * 100, erro * 100,
             p.previsoes >= 2 && fabs(m) > erro ? " *" : "");
    return texto;
}

static string textoMargem(const AvaliacaoPrevisao& p) {
    return textoComparacao(p, margem(p), erroDaMargem(p));
}

// "sempre ALTA 48.10% (modelo +2.84 pp (+-0.21) *) | sempre QUEDA 51.90% (modelo -0.96 pp (+-0.20) *)"
static string textoChutesFixos(const AvaliacaoPrevisao& p) {
    char texto[200];
    snprintf(texto, sizeof(texto), "sempre ALTA %.2f%% (modelo %s) | sempre QUEDA %.2f%% (modelo %s)",
             taxaSempreAlta(p) * 100,
             textoComparacao(p, taxaDeAcerto(p) - taxaSempreAlta(p), erroSobreSempreAlta(p)).c_str(),
             taxaSempreQueda(p) * 100,
             textoComparacao(p, taxaDeAcerto(p) - taxaSempreQueda(p), erroSobreSempreQueda(p)).c_str());
    return texto;
}

// "ALTA (+0.123%)", "QUEDA (-0.050%)" ou "indisponivel"
static string textoProximoPregao(const AvaliacaoPrevisao& p) {
    if (!p.temProxima) return "indisponivel";
    char texto[100];
    snprintf(texto, sizeof(texto), "%s (%+.3f%%)", p.retornoEstimado > 0 ? "ALTA" : "QUEDA", p.retornoEstimado * 100);
    return texto;
}

// =============================================================================================
// Rankings: junta (valor, indice do ativo), ordena e fica com os primeiros

struct ItemRanking {
    double valor;
    int indice;   // posicao em geral.ativos
};

// Empate: o menor indice (= ativo que vem antes em ordem alfabetica)
static bool maiorPrimeiro(const ItemRanking& x, const ItemRanking& y) {
    if (x.valor != y.valor) return x.valor > y.valor;
    return x.indice < y.indice;
}

static bool menorPrimeiro(const ItemRanking& x, const ItemRanking& y) {
    if (x.valor != y.valor) return x.valor < y.valor;
    return x.indice < y.indice;
}

static vector<int> ordenar(vector<ItemRanking> itens, bool decrescente, int quantidade) {
    if (decrescente) sort(itens.begin(), itens.end(), maiorPrimeiro);
    else sort(itens.begin(), itens.end(), menorPrimeiro);

    vector<int> indices;
    for (int k = 0; k < (int)itens.size() && k < quantidade; k++) indices.push_back(itens[k].indice);
    return indices;
}

vector<int> maisNegociadas(const vector<ResultadoAtivo>& ativos) {
    vector<ItemRanking> itens;
    for (int a = 0; a < (int)ativos.size(); a++) {
        ItemRanking item = {ativos[a].volumeFinanceiroTotal, a};
        itens.push_back(item);
    }
    return ordenar(itens, true, TOP_RANKINGS);
}

vector<int> rankingVolatilidade(const vector<ResultadoAtivo>& ativos, bool maisVolateis) {
    vector<ItemRanking> itens;
    for (int a = 0; a < (int)ativos.size(); a++) {
        if (ativos[a].retornos <= 1) continue;   // sem desvio padrao
        ItemRanking item = {ativos[a].volatilidade, a};
        itens.push_back(item);
    }
    return ordenar(itens, maisVolateis, TOP_RANKINGS);
}

// So ativos com JANELA_TREINO+ previsoes: poucas previsoes dao margens extremas por sorte.
vector<int> rankingMargem(const vector<ResultadoAtivo>& ativos, bool maiores) {
    vector<ItemRanking> itens;
    for (int a = 0; a < (int)ativos.size(); a++) {
        if (ativos[a].previsao.previsoes < JANELA_TREINO) continue;
        ItemRanking item = {margem(ativos[a].previsao), a};
        itens.push_back(item);
    }
    return ordenar(itens, maiores, TOP_RANKINGS);
}

// Previsao para o proximo pregao: as maiores altas (alta = true) ou as maiores quedas previstas.
vector<int> rankingPrevisaoProxima(const vector<ResultadoAtivo>& ativos, bool alta, int quantidade) {
    vector<ItemRanking> itens;
    for (int a = 0; a < (int)ativos.size(); a++) {
        const AvaliacaoPrevisao& p = ativos[a].previsao;
        if (!p.temProxima || p.previsoes < JANELA_TREINO) continue;
        bool previsaoDeAlta = p.retornoEstimado > 0;
        if (previsaoDeAlta != alta) continue;
        ItemRanking item = {p.retornoEstimado, a};
        itens.push_back(item);
    }
    return ordenar(itens, alta, quantidade);
}

// =============================================================================================
// Terminal

void imprimirPreparacao(const DadosPreparados& dados) {
    printf("\nRegistros lidos: %d | acoes: %d | em ativos liquidos: %d\n", dados.registrosLidos,
           (int)dados.registros.size(), dados.registrosMantidos);
    printf("Pregoes no periodo: %d\n", (int)dados.pregoes.size());
    printf("Ativos (acoes): %d | liquidos (presenca >= %.0f%% e %d+ dias): %d | com fonte oficial de eventos: %d\n",
           dados.ativosAcoes, PRESENCA_MINIMA * 100, MINIMO_DIAS_NEGOCIADOS, dados.ativosLiquidos,
           (int)dados.blocos.size());
    printf("Eventos corporativos aplicados: %d (lista em %s)\n", dados.ajustesAplicados, ARQUIVO_AJUSTES.c_str());
}

static void imprimirExtremos(const DadosPreparados& dados, const char* titulo, const vector<Extremo>& extremos) {
    printf("\n%d %s\n", TOP_EXTREMOS, titulo);
    for (int k = 0; k < (int)extremos.size(); k++) {
        const Registro& dia = dados.registros[extremos[k].indice];
        printf("  %2d. %-7s%-13s%s  %+9.2f%%\n", k + 1, dia.ativo.c_str(), dia.empresa.c_str(), dia.data.c_str(),
               extremos[k].retorno * 100);
    }
}

// Comeco de uma linha de ranking: "   1. PETR4  PETROBRAS    "
static void imprimirInicioDaLinha(const DadosPreparados& dados, int posicao, const ResultadoAtivo& r) {
    printf("  %2d. %-7s%-13s", posicao, nomeAtivo(dados, r).c_str(), nomeEmpresa(dados, r).c_str());
}

static void imprimirRankingMargem(const DadosPreparados& dados, const ResultadoGeral& geral, const char* titulo,
                                  const vector<int>& ordem) {
    printf("\n%s\n", titulo);
    for (int k = 0; k < (int)ordem.size(); k++) {
        const ResultadoAtivo& r = geral.ativos[ordem[k]];
        char taxas[100];
        snprintf(taxas, sizeof(taxas), "%.2f%% x %.2f%% em %ld", taxaDeAcerto(r.previsao) * 100,
                 taxaBaseline(r.previsao) * 100, r.previsao.previsoes);
        imprimirInicioDaLinha(dados, k + 1, r);
        printf("%-25.25s%-25.25s| proximo pregao: %s\n", textoMargem(r.previsao).c_str(), taxas,
               textoProximoPregao(r.previsao).c_str());
    }
}

void imprimirResumo(const DadosPreparados& dados, const ResultadoGeral& geral) {
    const vector<ResultadoAtivo>& ativos = geral.ativos;
    vector<int> ordem;

    printf("\n================================ RESUMO ================================\n");
    printf("Periodo: %s a %s | ativos analisados: %d | retornos diarios: %lld\n", geral.menorData.c_str(),
           geral.maiorData.c_str(), (int)ativos.size(), geral.totalRetornos);
    printf("Indice proprio (media dos retornos de todos os ativos, pesos iguais): acumulado %+.2f%% | volatilidade "
           "diaria %.2f%%\n", geral.mercado.retornoAcumulado * 100, geral.mercado.volatilidade * 100);

    imprimirExtremos(dados, "maiores altas em um dia:", geral.maioresAltas);
    imprimirExtremos(dados, "maiores quedas em um dia:", geral.maioresQuedas);

    printf("\n%d mais negociadas (total em R$ no periodo):\n", TOP_RANKINGS);
    ordem = maisNegociadas(ativos);
    for (int k = 0; k < (int)ordem.size(); k++) {
        imprimirInicioDaLinha(dados, k + 1, ativos[ordem[k]]);
        printf("R$ %.1f bilhoes\n", ativos[ordem[k]].volumeFinanceiroTotal / 1e9);
    }

    printf("\n%d mais volateis (desvio padrao diario):\n", TOP_RANKINGS);
    ordem = rankingVolatilidade(ativos, true);
    for (int k = 0; k < (int)ordem.size(); k++) {
        imprimirInicioDaLinha(dados, k + 1, ativos[ordem[k]]);
        printf("%.2f%%\n", ativos[ordem[k]].volatilidade * 100);
    }

    printf("\n%d menos volateis:\n", TOP_RANKINGS);
    ordem = rankingVolatilidade(ativos, false);
    for (int k = 0; k < (int)ordem.size(); k++) {
        imprimirInicioDaLinha(dados, k + 1, ativos[ordem[k]]);
        printf("%.2f%%\n", ativos[ordem[k]].volatilidade * 100);
    }

    printf("\n%d pares mais correlacionados (empresas diferentes, %d+ dias em comum):\n",
           (int)geral.correlacoes.size(), MINIMO_DIAS_COMUNS);
    for (int k = 0; k < (int)geral.correlacoes.size(); k++) {
        const ParCorrelacao& par = geral.correlacoes[k];
        const ResultadoAtivo& a = ativos[par.a];
        const ResultadoAtivo& b = ativos[par.b];
        string empresas = nomeEmpresa(dados, a) + " x " + nomeEmpresa(dados, b);
        printf("  %2d. %-7s x %-7s%-28s%.3f  (%ld dias)\n", k + 1, nomeAtivo(dados, a).c_str(),
               nomeAtivo(dados, b).c_str(), empresas.c_str(), par.correlacao, par.diasComuns);
    }

    const AvaliacaoPrevisao& p = geral.previsao;
    printf("\nPrevisao (regressao linear com %d indicadores, treino com os %d pregoes anteriores, conferida dia a dia):\n",
           NUM_VARIAVEIS - 1, JANELA_TREINO);
    printf("  previsoes conferidas: %ld | acerto do modelo: %.2f%%\n", p.previsoes, taxaDeAcerto(p) * 100);
    printf("  baseline (aposta na direcao mais comum dos %d pregoes anteriores): %.2f%%\n", JANELA_TREINO,
           taxaBaseline(p) * 100);
    printf("  MARGEM do modelo sobre o baseline: %s   %s\n", textoMargem(p).c_str(),
           margemSignificativa(p) ? "(* = maior que o acaso explicaria)" : "(dentro do que o acaso explica)");
    printf("  chutes fixos (apostar todo dia na mesma direcao):\n    %s\n", textoChutesFixos(p).c_str());
    printf("  fora da conta: %ld dias sem movimento (retorno 0), %ld janelas sem solucao unica\n", p.semMovimento,
           p.semSolucao);

    printf("\n  margem por ativo (modelo x baseline em N previsoes; ativos com %d+ previsoes; * = maior que o acaso):",
           JANELA_TREINO);
    imprimirRankingMargem(dados, geral, "  maiores margens:", rankingMargem(ativos, true));
    imprimirRankingMargem(dados, geral, "  menores margens:", rankingMargem(ativos, false));
    printf("\n  proximo pregao: %ld ativos com previsao de ALTA, %ld de QUEDA\n", geral.proximasAlta,
           geral.proximasQueda);
}

static void imprimirAtivo(const DadosPreparados& dados, const ResultadoAtivo& r) {
    const Indicadores& d = r.ultimo;
    printf("\n------------------------------- %s (%s) -------------------------------\n", nomeAtivo(dados, r).c_str(),
           nomeEmpresa(dados, r).c_str());
    printf("Periodo: %s a %s (%d pregoes, %ld retornos validos)\n", primeiraData(dados, r).c_str(),
           ultimaData(dados, r).c_str(), r.pregoes, r.retornos);
    printf("Preco (ajustado): medio R$ %.2f | minimo R$ %.2f | maximo R$ %.2f\n", r.precoMedio, r.precoMinimo,
           r.precoMaximo);
    printf("Retorno medio diario: %+.3f%% | volatilidade diaria: %.2f%% | beta: ", r.retornoMedio * 100,
           r.volatilidade * 100);
    if (r.temBeta) printf("%.2f\n", r.beta);
    else printf("-\n");
    printf("Volume medio: R$ %.2f milhoes/dia\n", r.volumeFinanceiroMedio / 1e6);

    printf("\nUltimo pregao (%s): fechamento R$ %.2f\n", ultimaData(dados, r).c_str(), r.fechamentoUltimo);
    if (!d.valido) {
        printf("  indicadores: o ultimo trecho sem descartes tem menos de %d pregoes\n", AQUECIMENTO + 1);
    } else {
        printf("  Media movel (%d): R$ %.2f | Bollinger: R$ %.2f a R$ %.2f\n", JANELA_MEDIA, d.mm20,
               d.bollingerInferior, d.bollingerSuperior);
        printf("  Volatilidade (%dd): %.2f%% | IFR(%d): %.1f | Estocastico %%K(%d): %.1f\n", JANELA_VOLATILIDADE,
               d.volatilidade20 * 100, JANELA_IFR, d.ifr, JANELA_ESTOCASTICO, d.estocasticoK);
        printf("  MACD: %.3f (sinal %.3f) | Momento (%dd): %+.2f%% | ATR(%d): R$ %.2f | OBV (%dd): %+.1f%%\n", d.macd,
               d.macdSinal, JANELA_MOMENTO, d.momento * 100, JANELA_ATR, d.atr, JANELA_OBV, d.obvVariacao * 100);
    }

    const AvaliacaoPrevisao& p = r.previsao;
    printf("\nPrevisao:\n");
    if (p.previsoes > 0) {
        printf("  acerto historico: %.2f%% em %ld previsoes | baseline: %.2f%% | margem: %s\n", taxaDeAcerto(p) * 100,
               p.previsoes, taxaBaseline(p) * 100, textoMargem(p).c_str());
        printf("  chutes fixos: %s\n", textoChutesFixos(p).c_str());
    } else {
        printf("  acerto historico: sem previsoes (precisa de %d exemplos de treino)\n", JANELA_TREINO);
    }
    if (p.temProxima) {
        printf("  previsao para o proximo pregao: %s (retorno estimado %+.3f%%)\n",
               p.retornoEstimado > 0 ? "ALTA" : "QUEDA", p.retornoEstimado * 100);
    } else if (!r.negociadaNoUltimoPregao) {
        printf("  previsao para o proximo pregao: indisponivel (nao foi negociada no ultimo pregao dos dados;"
               " ultimo dia dela: %s)\n", ultimaData(dados, r).c_str());
    } else {
        printf("  previsao para o proximo pregao: indisponivel (dados insuficientes no fim do periodo)\n");
    }
}

void imprimirDestaques(const DadosPreparados& dados, const ResultadoGeral& geral, const string& ativoPedido) {
    if (ativoPedido == "") {
        vector<int> altas = rankingPrevisaoProxima(geral.ativos, true, ATIVOS_EM_DESTAQUE);
        printf("\n=========== %d maiores ALTAS previstas para o proximo pregao ===========\n", ATIVOS_EM_DESTAQUE);
        for (int k = 0; k < (int)altas.size(); k++) imprimirAtivo(dados, geral.ativos[altas[k]]);

        vector<int> quedas = rankingPrevisaoProxima(geral.ativos, false, ATIVOS_EM_DESTAQUE);
        printf("\n=========== %d maiores QUEDAS previstas para o proximo pregao ===========\n", ATIVOS_EM_DESTAQUE);
        for (int k = 0; k < (int)quedas.size(); k++) imprimirAtivo(dados, geral.ativos[quedas[k]]);
        return;
    }

    printf("\n=========== Pedida na execucao: %s ===========\n", ativoPedido.c_str());
    int posicao = procurarAtivo(dados, geral, ativoPedido);
    if (posicao >= 0) {
        imprimirAtivo(dados, geral.ativos[posicao]);
    } else {
        printf("\nAtivo %s nao encontrado entre os analisados (nao existe nos dados, nao e acao, ou saiu nos filtros"
               " de liquidez/fonte oficial).\n", ativoPedido.c_str());
    }
}

int procurarAtivo(const DadosPreparados& dados, const ResultadoGeral& geral, const string& ativoPedido) {
    for (int a = 0; a < (int)geral.ativos.size(); a++) {
        if (nomeAtivo(dados, geral.ativos[a]) == ativoPedido) return a;
    }
    return -1;
}

void imprimirTempos(const Tempos& tempos) {
    printf("\nTempos:\n");
    printf("  leitura:       %.3f s\n", tempos.leitura);
    printf("  preparacao:    %.3f s  (filtros + ordenacao + blocos + eventos)\n", tempos.preparacao);
    printf("  processamento: %.3f s\n", tempos.porAtivo + tempos.entreAtivos);
    printf("    por ativo (estatisticas + indicadores + previsao): %.3f s   <- a parte dividida entre os trabalhadores\n",
           tempos.porAtivo);
    printf("    entre ativos (mercado, beta, correlacao, combinacao): %.3f s\n", tempos.entreAtivos);
}

// =============================================================================================
// Arquivos (mesmo formato do programa original, para comparar com diff)

static void gravarRelatorioAtivos(const DadosPreparados& dados, const vector<ResultadoAtivo>& ativos) {
    FILE* arquivo = fopen(ARQUIVO_RELATORIO_ATIVOS.c_str(), "w");
    if (arquivo == NULL) {
        printf("Erro: nao foi possivel criar %s\n", ARQUIVO_RELATORIO_ATIVOS.c_str());
        return;
    }
    fprintf(arquivo, "ativo,empresa,primeira_data,ultima_data,pregoes,retornos,preco_medio,preco_minimo,preco_maximo,"
                     "retorno_medio,volatilidade,volume_financeiro_medio,beta,fechamento_ultimo,mm20,bollinger_inferior,"
                     "bollinger_superior,volatilidade20,ifr14,macd,macd_sinal,momento10,estocastico_k14,atr14,"
                     "obv_variacao10,previsoes,acertos,taxa_acerto,acertos_baseline,taxa_baseline,margem,erro_margem95,"
                     "altas_reais,previsao_proximo,retorno_estimado\n");

    for (int a = 0; a < (int)ativos.size(); a++) {
        const ResultadoAtivo& r = ativos[a];
        const Indicadores& d = r.ultimo;
        const AvaliacaoPrevisao& p = r.previsao;

        fprintf(arquivo, "%s,%s,%s,%s,%d,%ld,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,", nomeAtivo(dados, r).c_str(),
                nomeEmpresa(dados, r).c_str(), primeiraData(dados, r).c_str(), ultimaData(dados, r).c_str(),
                r.pregoes, r.retornos, r.precoMedio, r.precoMinimo, r.precoMaximo, r.retornoMedio, r.volatilidade,
                r.volumeFinanceiroMedio);
        if (r.temBeta) fprintf(arquivo, "%.8f", r.beta);
        fprintf(arquivo, ",%.8f,", r.fechamentoUltimo);

        if (d.valido) {
            fprintf(arquivo, "%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f", d.mm20, d.bollingerInferior,
                    d.bollingerSuperior, d.volatilidade20, d.ifr, d.macd, d.macdSinal, d.momento, d.estocasticoK,
                    d.atr, d.obvVariacao);
        } else {
            fprintf(arquivo, ",,,,,,,,,,");   // 11 colunas vazias
        }

        fprintf(arquivo, ",%ld,%ld,", p.previsoes, p.acertos);
        if (p.previsoes > 0) fprintf(arquivo, "%.8f", taxaDeAcerto(p));
        fprintf(arquivo, ",%ld,", p.acertosBaseline);
        if (p.previsoes > 0) fprintf(arquivo, "%.8f,%.8f,%.8f", taxaBaseline(p), margem(p), erroDaMargem(p));
        else fprintf(arquivo, ",,");
        fprintf(arquivo, ",%ld,", p.altasReais);
        if (p.temProxima) fprintf(arquivo, "%s,%.8f", p.retornoEstimado > 0 ? "ALTA" : "QUEDA", p.retornoEstimado);
        else fprintf(arquivo, ",");
        fprintf(arquivo, "\n");
    }
    fclose(arquivo);
}

static void gravarDistribuicao(const long long histograma[NUM_FAIXAS]) {
    FILE* arquivo = fopen(ARQUIVO_DISTRIBUICAO.c_str(), "w");
    if (arquivo == NULL) {
        printf("Erro: nao foi possivel criar %s\n", ARQUIVO_DISTRIBUICAO.c_str());
        return;
    }
    long long total = 0;
    for (int f = 0; f < NUM_FAIXAS; f++) total += histograma[f];

    fprintf(arquivo, "faixa,retornos,percentual\n");
    for (int f = 0; f < NUM_FAIXAS; f++) {
        double percentual = 0;
        if (total > 0) percentual = 100.0 * histograma[f] / total;
        fprintf(arquivo, "%s,%lld,%.4f\n", rotuloDaFaixa(f).c_str(), histograma[f], percentual);
    }
    fclose(arquivo);
}

// Grava cada retorno descartado com o motivo e mostra a contagem por motivo.
static void gravarDescartados(const DadosPreparados& dados) {
    FILE* arquivo = fopen(ARQUIVO_DESCARTADOS.c_str(), "w");
    if (arquivo == NULL) {
        printf("Erro: nao foi possivel criar %s\n", ARQUIVO_DESCARTADOS.c_str());
        return;
    }
    fprintf(arquivo, "ativo,data,motivo,fechamento_anterior,fechamento,retorno_desconsiderado\n");

    map<string, int> porMotivo;   // porMotivo["CISAO"] = quantos retornos foram descartados por cisao
    int total = 0;
    for (int b = 0; b < (int)dados.blocos.size(); b++) {
        for (int i = dados.blocos[b].inicio + 1; i < dados.blocos[b].fim; i++) {
            const Registro& dia = dados.registros[i];
            if (dia.motivoDescarte == "") continue;
            const Registro& anterior = dados.registros[i - 1];
            fprintf(arquivo, "%s,%s,%s,%.6f,%.6f,%.6f\n", dia.ativo.c_str(), dia.data.c_str(),
                    dia.motivoDescarte.c_str(), anterior.fechamento, dia.fechamento,
                    dia.fechamento / anterior.fechamento - 1);
            porMotivo[dia.motivoDescarte]++;
            total++;
        }
    }
    fclose(arquivo);

    printf("\nRetornos descartados: %d (lista em %s)\n", total, ARQUIVO_DESCARTADOS.c_str());
    for (map<string, int>::iterator it = porMotivo.begin(); it != porMotivo.end(); it++) {
        printf("  %s: %d\n", it->first.c_str(), it->second);
    }
}

void gravarArquivos(const DadosPreparados& dados, const ResultadoGeral& geral) {
    mkdir("resultados", 0755);         // se ja existir, nao faz nada
    mkdir(PASTA_RESULTADOS.c_str(), 0755);

    gravarRelatorioAtivos(dados, geral.ativos);
    gravarDistribuicao(geral.histograma);
    gravarDescartados(dados);
    printf("Relatorio por ativo: %s | distribuicao dos retornos: %s\n", ARQUIVO_RELATORIO_ATIVOS.c_str(),
           ARQUIVO_DISTRIBUICAO.c_str());
}

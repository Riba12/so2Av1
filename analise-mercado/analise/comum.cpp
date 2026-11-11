#include "comum.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>

#include <cctype>

// "nao existe" na grade de retornos (NaN = "not a number")
const double SEM_VALOR = NAN;

// (clock() nao serve: ele soma o tempo de CPU de todas as threads.)
double agoraEmSegundos() {
    return chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count();
}

string paraMaiusculas(string texto) {
    for (int i = 0; i < (int)texto.size(); i++) texto[i] = toupper(texto[i]);
    return texto;
}

// separar("2025-01-20,PETR4,37.22", ',') -> {"2025-01-20", "PETR4", "37.22"}
vector<string> separar(const string& linha, char separador) {
    vector<string> campos;
    string campo = "";
    for (int i = 0; i < (int)linha.size(); i++) {
        if (linha[i] == separador) {
            campos.push_back(campo);
            campo = "";
        } else {
            campo += linha[i];
        }
    }
    campos.push_back(campo);   // o ultimo campo vai ate o fim da linha
    return campos;
}

static vector<Registro> lerCsv(const string& caminho, ifstream& arquivo) {
    vector<Registro> registros;
    string linha;
    int numeroLinha = 1;

    getline(arquivo, linha);   // primeira linha e o cabecalho: descartamos

    while (getline(arquivo, linha)) {
        numeroLinha++;
        vector<string> campos = separar(linha, ',');
        if (campos.size() != 11) {
            printf("Erro: %s, linha %d: tem %d colunas, esperado 11\n", caminho.c_str(), numeroLinha,
                   (int)campos.size());
            exit(1);
        }

        Registro r;
        r.data = campos[0];
        r.ativo = campos[1];
        r.abertura = stod(campos[2]);     // stod: texto -> double
        r.maxima = stod(campos[3]);
        r.minima = stod(campos[4]);
        r.fechamento = stod(campos[5]);
        r.volume = stoll(campos[6]);      // stoll: texto -> long long
        r.volumeFinanceiro = stod(campos[7]);
        // campos[8] = numero de negocios: a analise nao usa
        r.tipo = campos[9];
        r.empresa = campos[10];
        registros.push_back(r);
    }
    return registros;
}

vector<Registro> lerDados() {
    vector<Registro> todos;
    for (int ano = PRIMEIRO_ANO; ano <= ULTIMO_ANO; ano++) {
        string caminho = PASTA_DADOS + "/COTAHIST_A" + to_string(ano) + ".csv";
        ifstream arquivo(caminho);
        if (!arquivo) continue;   // ano que nao foi baixado: pula

        vector<Registro> doArquivo = lerCsv(caminho, arquivo);
        printf("Lido %s: %d registros\n", caminho.c_str(), (int)doArquivo.size());
        for (int i = 0; i < (int)doArquivo.size(); i++) todos.push_back(doArquivo[i]);
    }
    if (todos.empty()) {
        printf("Erro: nenhum CSV encontrado em %s (rode o programa de dentro de analise-mercado/)\n",
               PASTA_DADOS.c_str());
        exit(1);
    }
    return todos;
}

vector<Ajuste> lerAjustes() {
    ifstream arquivo(ARQUIVO_AJUSTES);
    if (!arquivo) {
        printf("Erro: nao foi possivel abrir %s (rode o programa \"eventos\" antes)\n", ARQUIVO_AJUSTES.c_str());
        exit(1);
    }

    vector<Ajuste> ajustes;
    string linha;
    getline(arquivo, linha);   // cabecalho: ativo,data,acao,multiplicador_preco,tipo,fonte
    while (getline(arquivo, linha)) {
        vector<string> c = separar(linha, ',');
        if (c.size() < 6) continue;
        Ajuste a;
        a.ativo = c[0];
        a.data = c[1];
        a.acao = c[2];
        a.multiplicadorPreco = stod(c[3]);
        a.tipo = c[4];
        ajustes.push_back(a);   // a 6a coluna (fonte) serve so para conferencia humana
    }
    return ajustes;
}

// =============================================================================================
// Preparacao

static bool ehAcao(const Registro& r) {
    return r.tipo == "ON" || r.tipo == "PN" || r.tipo == "PNA" || r.tipo == "PNB" || r.tipo == "PNC" ||
           r.tipo == "UNT";
}

// "a deve vir antes de b?" Primeiro pelo ativo; se for o mesmo, pela data.
static bool registroVemAntes(const Registro& a, const Registro& b) {
    if (a.ativo != b.ativo) return a.ativo < b.ativo;
    return a.data < b.data;
}

// Percorre o vetor JA ORDENADO e anota onde comeca e termina cada ativo.
static vector<BlocoAtivo> separarBlocos(const vector<Registro>& registros) {
    vector<BlocoAtivo> blocos;
    if (registros.empty()) return blocos;

    int inicio = 0;
    for (int i = 1; i < (int)registros.size(); i++) {
        if (registros[i].ativo != registros[inicio].ativo) {   // mudou o ativo
            BlocoAtivo bloco = {registros[inicio].ativo, inicio, i};
            blocos.push_back(bloco);
            inicio = i;
        }
    }
    BlocoAtivo ultimo = {registros[inicio].ativo, inicio, (int)registros.size()};
    blocos.push_back(ultimo);
    return blocos;
}

// Todas as datas de pregao, sem repeticao e em ordem.
static vector<string> listarPregoes(const vector<Registro>& registros) {
    vector<string> datas;
    for (int i = 0; i < (int)registros.size(); i++) datas.push_back(registros[i].data);
    sort(datas.begin(), datas.end());
    datas.erase(unique(datas.begin(), datas.end()), datas.end());   // tira as repeticoes
    return datas;
}

// Posicao de "data" na lista ordenada de pregoes (lower_bound = busca binaria).
static int posicaoDoPregao(const vector<string>& pregoes, const string& data) {
    return lower_bound(pregoes.begin(), pregoes.end(), data) - pregoes.begin();
}

// Multiplica os precos por "fator" e divide a quantidade por ele (o volume em R$ nao muda).
static void aplicarFator(Registro& r, double fator) {
    if (fator == 1.0) return;
    r.abertura *= fator;
    r.maxima *= fator;
    r.minima *= fator;
    r.fechamento *= fator;
    r.volume = llround(r.volume / fator);
}

DadosPreparados prepararDados(const vector<Registro>& lidos, const vector<Ajuste>& ajustes) {
    DadosPreparados dados;
    dados.registrosLidos = lidos.size();

    // 1. so acoes
    for (int i = 0; i < (int)lidos.size(); i++) {
        if (ehAcao(lidos[i])) dados.registros.push_back(lidos[i]);
    }

    // 2. ordena por ativo e data, separa os blocos e lista os pregoes
    sort(dados.registros.begin(), dados.registros.end(), registroVemAntes);
    vector<BlocoAtivo> todos = separarBlocos(dados.registros);
    dados.ativosAcoes = todos.size();
    dados.pregoes = listarPregoes(dados.registros);
    vector<Registro>& registros = dados.registros;

    // 3. liquidez: 20+ dias negociados e presenca de 95%+ no periodo em que o ativo existiu
    vector<BlocoAtivo> liquidos;
    for (int b = 0; b < (int)todos.size(); b++) {
        int diasNegociados = todos[b].fim - todos[b].inicio;
        if (diasNegociados < MINIMO_DIAS_NEGOCIADOS) continue;

        int primeiro = posicaoDoPregao(dados.pregoes, registros[todos[b].inicio].data);
        int ultimo = posicaoDoPregao(dados.pregoes, registros[todos[b].fim - 1].data);
        int pregoesNoPeriodo = ultimo - primeiro + 1;

        double presenca = (double)diasNegociados / pregoesNoPeriodo;
        if (presenca >= PRESENCA_MINIMA) liquidos.push_back(todos[b]);
    }
    dados.ativosLiquidos = liquidos.size();

    // 4. tira os ativos marcados para "excluir" (sem fonte oficial de eventos)
    set<string> excluidos;
    for (int k = 0; k < (int)ajustes.size(); k++) {
        if (ajustes[k].acao == "excluir") excluidos.insert(ajustes[k].ativo);
    }
    for (int b = 0; b < (int)liquidos.size(); b++) {
        if (excluidos.count(liquidos[b].ativo) == 0) dados.blocos.push_back(liquidos[b]);
    }

    // 5. piso de preco: ANTES dos ajustes, porque vale para o preco realmente negociado
    for (int b = 0; b < (int)dados.blocos.size(); b++) {
        for (int i = dados.blocos[b].inicio + 1; i < dados.blocos[b].fim; i++) {
            if (registros[i - 1].fechamento < PRECO_MINIMO) registros[i].motivoDescarte = MOTIVO_PRECO_MINIMO;
        }
    }

    // 6. ajustes de eventos corporativos. O map junta os ajustes de cada ativo:
    //    porAtivo["PETR4"] = lista dos ajustes da PETR4 (na ordem do arquivo)
    map<string, vector<Ajuste>> porAtivo;
    for (int k = 0; k < (int)ajustes.size(); k++) {
        if (ajustes[k].acao != "excluir") porAtivo[ajustes[k].ativo].push_back(ajustes[k]);
    }
    for (int b = 0; b < (int)dados.blocos.size(); b++) {
        const BlocoAtivo& bloco = dados.blocos[b];
        if (porAtivo.count(bloco.ativo) == 0) continue;
        vector<Ajuste>& doAtivo = porAtivo[bloco.ativo];

        for (int k = 0; k < (int)doAtivo.size(); k++) {
            for (int i = bloco.inicio; i < bloco.fim; i++) {
                if (doAtivo[k].acao == "ajustar" && registros[i].data < doAtivo[k].data) {
                    aplicarFator(registros[i], doAtivo[k].multiplicadorPreco);   // varios eventos se acumulam
                } else if (doAtivo[k].acao == "descartar" && registros[i].data == doAtivo[k].data) {
                    registros[i].motivoDescarte = doAtivo[k].tipo;
                }
            }
            dados.ajustesAplicados++;
        }
    }

    for (int b = 0; b < (int)dados.blocos.size(); b++) {
        dados.registrosMantidos += dados.blocos[b].fim - dados.blocos[b].inicio;
    }
    return dados;
}

// =============================================================================================
// Indicadores

// Media movel exponencial: comeca pela media simples dos n primeiros valores e depois
// puxa a media na direcao do valor novo com peso 2 / (n + 1).
// "posicao" = quantos valores dessa serie ja vieram antes deste (0 = primeiro).
static void atualizarMME(double& mme, double valor, int posicao, int n) {
    if (posicao < n) mme += valor / n;
    else mme += 2.0 / (n + 1) * (valor - mme);
}

// Suavizacao de Wilder (IFR e ATR): igual a MME, mas com peso 1 / n.
static void atualizarWilder(double& media, double valor, int posicao, int n) {
    if (posicao < n) media += valor / n;
    else media = (media * (n - 1) + valor) / n;
}

// Indicadores de todos os dias de um ativo (posicao j = registros[bloco.inicio + j]).
// Um retorno descartado QUEBRA a serie: os indicadores recomecam do zero depois dele.
static vector<Indicadores> calcularIndicadores(const vector<Registro>& registros, const BlocoAtivo& bloco) {
    int n = bloco.fim - bloco.inicio;
    vector<Indicadores> ind(n);
    vector<double> macd(n, 0);   // o sinal do MACD e uma media do proprio MACD
    vector<double> obv(n, 0);

    int inicioTrecho = 0;
    double mmeCurta = 0, mmeLonga = 0, sinal = 0, ganhoMedio = 0, perdaMedia = 0, atr = 0;

    for (int j = 0; j < n; j++) {
        const Registro& hoje = registros[bloco.inicio + j];
        if (j == 0 || hoje.motivoDescarte != "") {
            // comeco de um trecho novo: tudo que depende do passado recomeca
            inicioTrecho = j;
            mmeCurta = mmeLonga = sinal = ganhoMedio = perdaMedia = atr = 0;
        }
        int p = j - inicioTrecho;   // posicao dentro do trecho (0 = sem dia anterior)
        Indicadores& d = ind[j];

        // --- o que depende do dia anterior: retorno, IFR, ATR, OBV
        if (p >= 1) {
            const Registro& ontem = registros[bloco.inicio + j - 1];
            d.temRetorno = true;
            d.retorno = hoje.fechamento / ontem.fechamento - 1;

            double variacao = hoje.fechamento - ontem.fechamento;
            atualizarWilder(ganhoMedio, variacao > 0 ? variacao : 0, p - 1, JANELA_IFR);
            atualizarWilder(perdaMedia, variacao < 0 ? -variacao : 0, p - 1, JANELA_IFR);

            // amplitude do dia, contando o salto desde o fechamento anterior
            double amplitude = max(hoje.maxima - hoje.minima,
                                   max(fabs(hoje.maxima - ontem.fechamento), fabs(hoje.minima - ontem.fechamento)));
            atualizarWilder(atr, amplitude, p - 1, JANELA_ATR);

            double volumeComSinal = 0;
            if (variacao > 0) volumeComSinal = (double)hoje.volume;
            if (variacao < 0) volumeComSinal = -(double)hoje.volume;
            obv[j] = obv[j - 1] + volumeComSinal;
        }

        // --- medias exponenciais e MACD
        atualizarMME(mmeCurta, hoje.fechamento, p, MACD_CURTA);
        atualizarMME(mmeLonga, hoje.fechamento, p, MACD_LONGA);
        if (p >= MACD_LONGA - 1) {
            macd[j] = mmeCurta - mmeLonga;
            atualizarMME(sinal, macd[j], p - (MACD_LONGA - 1), MACD_SINAL);
        }

        if (p < AQUECIMENTO) continue;   // ainda nao ha dias suficientes
        d.valido = true;
        d.macd = macd[j];
        d.macdSinal = sinal;

        // --- IFR
        if (perdaMedia == 0) d.ifr = (ganhoMedio == 0) ? 50 : 100;
        else d.ifr = 100 - 100 / (1 + ganhoMedio / perdaMedia);

        d.atr = atr;

        // --- MM20 e Bandas de Bollinger
        double soma = 0;
        for (int k = j + 1 - JANELA_MEDIA; k <= j; k++) soma += registros[bloco.inicio + k].fechamento;
        d.mm20 = soma / JANELA_MEDIA;
        double somaQuadrados = 0;
        for (int k = j + 1 - JANELA_MEDIA; k <= j; k++) {
            double diferenca = registros[bloco.inicio + k].fechamento - d.mm20;
            somaQuadrados += diferenca * diferenca;
        }
        double desvioPreco = sqrt(somaQuadrados / JANELA_MEDIA);
        d.bollingerInferior = d.mm20 - BOLLINGER_DESVIOS * desvioPreco;
        d.bollingerSuperior = d.mm20 + BOLLINGER_DESVIOS * desvioPreco;

        // --- volatilidade dos ultimos 20 retornos (desvio padrao amostral, n - 1)
        double somaRetornos = 0;
        for (int k = j + 1 - JANELA_VOLATILIDADE; k <= j; k++) somaRetornos += ind[k].retorno;
        double mediaRetornos = somaRetornos / JANELA_VOLATILIDADE;
        double somaQuadradosRetornos = 0;
        for (int k = j + 1 - JANELA_VOLATILIDADE; k <= j; k++) {
            double diferenca = ind[k].retorno - mediaRetornos;
            somaQuadradosRetornos += diferenca * diferenca;
        }
        d.volatilidade20 = sqrt(somaQuadradosRetornos / (JANELA_VOLATILIDADE - 1));

        // --- momento
        d.momento = hoje.fechamento / registros[bloco.inicio + j - JANELA_MOMENTO].fechamento - 1;

        // --- estocastico %K
        double minima = hoje.minima, maxima = hoje.maxima;
        for (int k = j + 1 - JANELA_ESTOCASTICO; k <= j; k++) {
            minima = min(minima, registros[bloco.inicio + k].minima);
            maxima = max(maxima, registros[bloco.inicio + k].maxima);
        }
        d.estocasticoK = (maxima > minima) ? 100 * (hoje.fechamento - minima) / (maxima - minima) : 50;

        // --- OBV dos ultimos 10 dias
        double volume10 = 0;
        for (int k = j + 1 - JANELA_OBV; k <= j; k++) volume10 += (double)registros[bloco.inicio + k].volume;
        d.obvVariacao = (volume10 > 0) ? (obv[j] - obv[j - JANELA_OBV]) / volume10 : 0;
    }
    return ind;
}

// =============================================================================================
// Previsao por regressao linear (minimos quadrados)

// Um exemplo de treino: variaveis de um dia (x) e o retorno real do dia seguinte (y).
struct Amostra {
    double x[NUM_VARIAVEIS];
    double y;
};

static void montarVariaveis(const Registro& r, const Indicadores& d, double x[NUM_VARIAVEIS]) {
    x[0] = 1;   // constante
    x[1] = d.retorno;
    x[2] = d.momento;
    x[3] = d.ifr / 100;
    x[4] = (d.macd - d.macdSinal) / r.fechamento;
    x[5] = d.estocasticoK / 100;
    double meiaBanda = d.bollingerSuperior - d.mm20;
    x[6] = (meiaBanda > 0) ? (r.fechamento - d.mm20) / meiaBanda : 0;
    x[7] = d.volatilidade20;
    x[8] = d.obvVariacao;
    x[9] = d.atr / r.fechamento;
}

// Minimos quadrados com as amostras de "inicio" ate "fim - 1": resolve (X'X) coef = X'y por
// eliminacao de Gauss. Devolve false se o sistema nao tiver solucao unica.
static bool ajustarRegressao(const vector<Amostra>& amostras, int inicio, int fim, double coef[NUM_VARIAVEIS]) {
    const int K = NUM_VARIAVEIS;
    double a[NUM_VARIAVEIS][NUM_VARIAVEIS + 1] = {};   // matriz [X'X | X'y]

    for (int m = inicio; m < fim; m++) {
        for (int linha = 0; linha < K; linha++) {
            for (int coluna = 0; coluna < K; coluna++) a[linha][coluna] += amostras[m].x[linha] * amostras[m].x[coluna];
            a[linha][K] += amostras[m].x[linha] * amostras[m].y;
        }
    }

    // Ridge: aumenta a diagonal de X'X em RIDGE (ex.: 0.1 = 10%) do proprio valor. Isso "puxa" os coeficientes
    // dos indicadores para perto de zero. A constante (linha 0) fica de fora: ela e a tendencia, nao e ruido.
    // Usar uma porcentagem do proprio valor (e nao um numero fixo) trata igual indicadores de escalas diferentes
    // (o IFR/100 fica perto de 0,5; a volatilidade, perto de 0,02).
    for (int linha = 1; linha < K; linha++) a[linha][linha] *= 1 + RIDGE;

    // eliminacao: zera tudo abaixo da diagonal, coluna por coluna
    for (int col = 0; col < K; col++) {
        int pivo = col;   // a linha com o maior valor na coluna (reduz erro de arredondamento)
        for (int linha = col + 1; linha < K; linha++) {
            if (fabs(a[linha][col]) > fabs(a[pivo][col])) pivo = linha;
        }
        if (fabs(a[pivo][col]) < 1e-12) return false;   // pivo "zero": sem solucao unica
        for (int coluna = 0; coluna <= K; coluna++) swap(a[col][coluna], a[pivo][coluna]);

        for (int linha = col + 1; linha < K; linha++) {
            double fator = a[linha][col] / a[col][col];
            for (int coluna = col; coluna <= K; coluna++) a[linha][coluna] -= fator * a[col][coluna];
        }
    }

    // substituicao de tras para frente
    for (int linha = K - 1; linha >= 0; linha--) {
        double soma = a[linha][K];
        for (int coluna = linha + 1; coluna < K; coluna++) soma -= a[linha][coluna] * coef[coluna];
        coef[linha] = soma / a[linha][linha];
    }
    return true;
}

static double estimar(const double coef[NUM_VARIAVEIS], const double x[NUM_VARIAVEIS]) {
    double estimativa = 0;
    for (int k = 0; k < NUM_VARIAVEIS; k++) estimativa += coef[k] * x[k];
    return estimativa;
}

// Avaliacao dia a dia (walk-forward): para prever o dia m, treina SO com os JANELA_TREINO anteriores.
static AvaliacaoPrevisao avaliarPrevisao(const vector<Registro>& registros, const BlocoAtivo& bloco,
                                         const vector<Indicadores>& indicadores) {
    AvaliacaoPrevisao resultado;
    int n = indicadores.size();

    // exemplos: dia j com todos os indicadores e com retorno valido no dia j + 1
    vector<Amostra> amostras;
    for (int j = 0; j + 1 < n; j++) {
        if (!indicadores[j].valido || !indicadores[j + 1].temRetorno) continue;
        Amostra s;
        montarVariaveis(registros[bloco.inicio + j], indicadores[j], s.x);
        s.y = indicadores[j + 1].retorno;
        amostras.push_back(s);
    }
    int numAmostras = amostras.size();

    double coef[NUM_VARIAVEIS];
    for (int m = JANELA_TREINO; m < numAmostras; m++) {
        if (!ajustarRegressao(amostras, m - JANELA_TREINO, m, coef)) {
            resultado.semSolucao++;
            continue;
        }
        double estimado = estimar(coef, amostras[m].x);
        double real = amostras[m].y;
        if (real == 0) {
            resultado.semMovimento++;
            continue;
        }
        resultado.previsoes++;
        if (real > 0) resultado.altasReais++;
        if (estimado > 0) resultado.previsoesAlta++;
        if ((estimado > 0) == (real > 0)) resultado.acertos++;

        // baseline: a direcao mais comum na mesma janela de treino (empate = QUEDA)
        long altas = 0, quedas = 0;
        for (int k = m - JANELA_TREINO; k < m; k++) {
            if (amostras[k].y > 0) altas++;
            if (amostras[k].y < 0) quedas++;
        }
        bool baselineAlta = altas > quedas;
        if (baselineAlta == (real > 0)) resultado.acertosBaseline++;
        if (baselineAlta != (estimado > 0)) resultado.discordancias++;
    }

    // previsao para o pregao seguinte ao ultimo dia dos dados
    if (n > 0 && indicadores[n - 1].valido && numAmostras >= JANELA_TREINO &&
        ajustarRegressao(amostras, numAmostras - JANELA_TREINO, numAmostras, coef)) {
        double x[NUM_VARIAVEIS];
        montarVariaveis(registros[bloco.fim - 1], indicadores[n - 1], x);
        resultado.temProxima = true;
        resultado.retornoEstimado = estimar(coef, x);
    }
    return resultado;
}

// =============================================================================================
// Rankings e histograma

// Maior retorno primeiro. Empate: o menor indice (registros estao ordenados por ativo e data).
bool altaVemAntes(const Extremo& x, const Extremo& y) {
    if (x.retorno != y.retorno) return x.retorno > y.retorno;
    return x.indice < y.indice;
}

bool quedaVemAntes(const Extremo& x, const Extremo& y) {
    if (x.retorno != y.retorno) return x.retorno < y.retorno;
    return x.indice < y.indice;
}

int faixaDoRetorno(double retorno) {
    // floor(-0.015 * 100) = -2 -> faixa "-2% a -1%"; somando FAIXAS_POR_LADO + 1 vira indice >= 0
    int faixa = (int)floor(retorno * 100) + FAIXAS_POR_LADO + 1;
    if (faixa < 0) faixa = 0;                             // cauda de baixo (< -10%)
    if (faixa > NUM_FAIXAS - 1) faixa = NUM_FAIXAS - 1;   // cauda de cima (>= +10%)
    return faixa;
}

string rotuloDaFaixa(int faixa) {
    if (faixa == 0) return "< -" + to_string(FAIXAS_POR_LADO) + "%";
    if (faixa == NUM_FAIXAS - 1) return ">= +" + to_string(FAIXAS_POR_LADO) + "%";
    int de = faixa - FAIXAS_POR_LADO - 1;
    return to_string(de) + "% a " + to_string(de + 1) + "%";
}

// =============================================================================================
// Um ativo

ResultadoAtivo analisarAtivo(const DadosPreparados& dados, int b) {
    const vector<Registro>& registros = dados.registros;
    const BlocoAtivo& bloco = dados.blocos[b];

    ResultadoAtivo r;
    r.inicio = bloco.inicio;
    r.fim = bloco.fim;
    r.pregoes = bloco.fim - bloco.inicio;

    vector<Indicadores> indicadores = calcularIndicadores(registros, bloco);

    // estatisticas do periodo
    double somaPreco = 0, somaRetornos = 0;
    r.precoMinimo = registros[bloco.inicio].minima;
    r.precoMaximo = registros[bloco.inicio].maxima;
    vector<Extremo> todosRetornos;
    for (int i = bloco.inicio; i < bloco.fim; i++) {
        const Registro& dia = registros[i];
        somaPreco += dia.fechamento;
        r.precoMinimo = min(r.precoMinimo, dia.minima);
        r.precoMaximo = max(r.precoMaximo, dia.maxima);
        r.volumeFinanceiroTotal += dia.volumeFinanceiro;

        const Indicadores& d = indicadores[i - bloco.inicio];
        if (!d.temRetorno) continue;
        r.retornos++;
        somaRetornos += d.retorno;
        r.histograma[faixaDoRetorno(d.retorno)]++;
        Extremo e = {d.retorno, i};
        todosRetornos.push_back(e);
    }
    r.precoMedio = somaPreco / r.pregoes;
    r.volumeFinanceiroMedio = r.volumeFinanceiroTotal / r.pregoes;

    if (r.retornos > 0) r.retornoMedio = somaRetornos / r.retornos;
    if (r.retornos > 1) {
        double somaQuadrados = 0;
        for (int j = 0; j < (int)indicadores.size(); j++) {
            if (!indicadores[j].temRetorno) continue;
            double diferenca = indicadores[j].retorno - r.retornoMedio;
            somaQuadrados += diferenca * diferenca;
        }
        r.volatilidade = sqrt(somaQuadrados / (r.retornos - 1));
    }

    // maiores altas e quedas: ordena todos os retornos e guarda os TOP_EXTREMOS primeiros
    sort(todosRetornos.begin(), todosRetornos.end(), altaVemAntes);
    r.numAltas = min((int)todosRetornos.size(), TOP_EXTREMOS);
    for (int k = 0; k < r.numAltas; k++) r.maioresAltas[k] = todosRetornos[k];

    sort(todosRetornos.begin(), todosRetornos.end(), quedaVemAntes);
    r.numQuedas = min((int)todosRetornos.size(), TOP_EXTREMOS);
    for (int k = 0; k < r.numQuedas; k++) r.maioresQuedas[k] = todosRetornos[k];

    r.ultimo = indicadores.back();
    r.fechamentoUltimo = registros[bloco.fim - 1].fechamento;

    r.previsao = avaliarPrevisao(registros, bloco, indicadores);
    // so quem foi negociado no ultimo pregao dos dados tem "proximo pregao" a prever
    r.negociadaNoUltimoPregao = (registros[bloco.fim - 1].data == dados.pregoes.back());
    if (!r.negociadaNoUltimoPregao) r.previsao.temProxima = false;
    return r;
}

// =============================================================================================
// Etapas entre ativos: mercado, beta, correlacao, combinacao

// Retornos validos numa grade [ativo][pregao]; NaN onde o ativo nao tem retorno naquele dia.
static vector<vector<double>> montarGrade(const DadosPreparados& dados) {
    int numAtivos = dados.blocos.size();
    int numPregoes = dados.pregoes.size();
    vector<vector<double>> grade(numAtivos, vector<double>(numPregoes, SEM_VALOR));
    for (int b = 0; b < numAtivos; b++) {
        for (int i = dados.blocos[b].inicio + 1; i < dados.blocos[b].fim; i++) {
            if (dados.registros[i].motivoDescarte != "") continue;
            int p = posicaoDoPregao(dados.pregoes, dados.registros[i].data);
            grade[b][p] = dados.registros[i].fechamento / dados.registros[i - 1].fechamento - 1;
        }
    }
    return grade;
}

// Indice proprio: em cada pregao, a media simples dos retornos de todos os ativos.
static Mercado calcularMercado(const vector<vector<double>>& grade) {
    Mercado mercado;
    int numPregoes = grade.empty() ? 0 : grade[0].size();
    mercado.indice.assign(numPregoes, SEM_VALOR);

    double acumulado = 1, soma = 0;
    for (int p = 0; p < numPregoes; p++) {
        double somaDia = 0;
        long ativosNoDia = 0;
        for (int b = 0; b < (int)grade.size(); b++) {
            if (isnan(grade[b][p])) continue;
            somaDia += grade[b][p];
            ativosNoDia++;
        }
        if (ativosNoDia == 0) continue;
        mercado.indice[p] = somaDia / ativosNoDia;
        acumulado *= 1 + mercado.indice[p];
        soma += mercado.indice[p];
        mercado.dias++;
    }
    mercado.retornoAcumulado = acumulado - 1;

    if (mercado.dias > 1) {
        double media = soma / mercado.dias, somaQuadrados = 0;
        for (int p = 0; p < numPregoes; p++) {
            if (isnan(mercado.indice[p])) continue;
            somaQuadrados += (mercado.indice[p] - media) * (mercado.indice[p] - media);
        }
        mercado.volatilidade = sqrt(somaQuadrados / (mercado.dias - 1));
    }
    return mercado;
}

// Beta = cov(ativo, mercado) / var(mercado)
static void calcularBetas(const vector<vector<double>>& grade, const Mercado& mercado, vector<ResultadoAtivo>& ativos) {
    for (int b = 0; b < (int)grade.size(); b++) {
        double n = 0, somaM = 0, somaA = 0, somaMM = 0, somaMA = 0;
        for (int p = 0; p < (int)mercado.indice.size(); p++) {
            if (isnan(grade[b][p]) || isnan(mercado.indice[p])) continue;
            double m = mercado.indice[p], a = grade[b][p];
            n++;
            somaM += m;
            somaA += a;
            somaMM += m * m;
            somaMA += m * a;
        }
        double varianciaM = n * somaMM - somaM * somaM;
        if (n < MINIMO_DIAS_COMUNS || varianciaM <= 0) continue;
        ativos[b].temBeta = true;
        ativos[b].beta = (n * somaMA - somaM * somaA) / varianciaM;
    }
}

// Maior correlacao primeiro. Empate: pelos ativos (a, b), que estao em ordem alfabetica.
static bool parVemAntes(const ParCorrelacao& x, const ParCorrelacao& y) {
    if (x.correlacao != y.correlacao) return x.correlacao > y.correlacao;
    if (x.a != y.a) return x.a < y.a;
    return x.b < y.b;
}

// Correlacao de Pearson entre todos os pares de ativos de empresas DIFERENTES.
static vector<ParCorrelacao> maioresCorrelacoes(const vector<vector<double>>& grade, const vector<BlocoAtivo>& blocos) {
    vector<ParCorrelacao> pares;
    for (int a = 0; a < (int)grade.size(); a++) {
        for (int b = a + 1; b < (int)grade.size(); b++) {
            // PETR3 e PETR4 sao da mesma empresa (mesmas 4 letras): ficam de fora
            if (blocos[a].ativo.substr(0, 4) == blocos[b].ativo.substr(0, 4)) continue;

            double n = 0, sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
            for (int p = 0; p < (int)grade[a].size(); p++) {
                double x = grade[a][p], y = grade[b][p];
                if (isnan(x) || isnan(y)) continue;
                n++;
                sx += x;
                sy += y;
                sxx += x * x;
                syy += y * y;
                sxy += x * y;
            }
            double vx = n * sxx - sx * sx, vy = n * syy - sy * sy;
            if (n < MINIMO_DIAS_COMUNS || vx <= 0 || vy <= 0) continue;

            ParCorrelacao par;
            par.a = a;
            par.b = b;
            par.correlacao = (n * sxy - sx * sy) / sqrt(vx * vy);
            par.diasComuns = (long)n;
            pares.push_back(par);
        }
    }
    sort(pares.begin(), pares.end(), parVemAntes);
    if ((int)pares.size() > TOP_EXTREMOS) pares.resize(TOP_EXTREMOS);
    return pares;
}

static void somar(AvaliacaoPrevisao& total, const AvaliacaoPrevisao& parcial) {
    total.previsoes += parcial.previsoes;
    total.acertos += parcial.acertos;
    total.acertosBaseline += parcial.acertosBaseline;
    total.discordancias += parcial.discordancias;
    total.altasReais += parcial.altasReais;
    total.previsoesAlta += parcial.previsoesAlta;
    total.semMovimento += parcial.semMovimento;
    total.semSolucao += parcial.semSolucao;
}

// Soma histogramas, contadores e rankings de todos os ativos.
static void combinarAtivos(const DadosPreparados& dados, ResultadoGeral& geral) {
    for (int b = 0; b < (int)geral.ativos.size(); b++) {
        const ResultadoAtivo& r = geral.ativos[b];
        for (int f = 0; f < NUM_FAIXAS; f++) geral.histograma[f] += r.histograma[f];
        geral.totalRetornos += r.retornos;
        for (int k = 0; k < r.numAltas; k++) geral.maioresAltas.push_back(r.maioresAltas[k]);
        for (int k = 0; k < r.numQuedas; k++) geral.maioresQuedas.push_back(r.maioresQuedas[k]);

        somar(geral.previsao, r.previsao);
        if (r.previsao.temProxima) {
            if (r.previsao.retornoEstimado > 0) geral.proximasAlta++;
            else geral.proximasQueda++;
        }

        string primeiraData = dados.registros[r.inicio].data;
        string ultimaData = dados.registros[r.fim - 1].data;
        if (geral.menorData == "" || primeiraData < geral.menorData) geral.menorData = primeiraData;
        if (ultimaData > geral.maiorData) geral.maiorData = ultimaData;
    }

    // os TOP_EXTREMOS de cada ativo juntos: ordena e fica com os TOP_EXTREMOS gerais
    sort(geral.maioresAltas.begin(), geral.maioresAltas.end(), altaVemAntes);
    if ((int)geral.maioresAltas.size() > TOP_EXTREMOS) geral.maioresAltas.resize(TOP_EXTREMOS);
    sort(geral.maioresQuedas.begin(), geral.maioresQuedas.end(), quedaVemAntes);
    if ((int)geral.maioresQuedas.size() > TOP_EXTREMOS) geral.maioresQuedas.resize(TOP_EXTREMOS);
}

void etapasEntreAtivos(const DadosPreparados& dados, ResultadoGeral& geral) {
    vector<vector<double>> grade = montarGrade(dados);
    geral.mercado = calcularMercado(grade);
    calcularBetas(grade, geral.mercado, geral.ativos);
    geral.correlacoes = maioresCorrelacoes(grade, dados.blocos);
    combinarAtivos(dados, geral);
}

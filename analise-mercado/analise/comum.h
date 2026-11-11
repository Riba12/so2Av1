// comum.h - tudo o que as tres versoes (sequencial, threads, processos) usam.
//
// Este header e incluido por todos os .cpp desta pasta. Por isso o "using namespace std" fica aqui:
// nenhum arquivo precisa escrever "std::" (em projeto grande isso se evita em header; aqui e para simplificar).

#ifndef COMUM_H
#define COMUM_H

#include <string>
#include <vector>

using namespace std;

// =============================================================================================
// Pastas e arquivos (o programa roda de dentro de analise-mercado/)

const string PASTA_DADOS = "dados/processados";       // COTAHIST_A2015.csv ... COTAHIST_A2025.csv
const int PRIMEIRO_ANO = 2015;
const int ULTIMO_ANO = 2025;
const string ARQUIVO_AJUSTES = "dados/eventos/ajustes.csv";

const string PASTA_RESULTADOS = "resultados/analise";
const string ARQUIVO_RELATORIO_ATIVOS = "resultados/analise/relatorio_ativos.csv";
const string ARQUIVO_DISTRIBUICAO = "resultados/analise/distribuicao_retornos.csv";
const string ARQUIVO_DESCARTADOS = "resultados/analise/retornos_descartados.csv";
const string PASTA_RELATORIOS = "resultados/analise/relatorios";   // um .md por execucao

// =============================================================================================
// Regras de limpeza dos dados

const int MINIMO_DIAS_NEGOCIADOS = 20;   // pelo menos 20 pregoes
const double PRESENCA_MINIMA = 0.95;     // negociado em 95%+ dos pregoes do periodo em que existiu
const double PRECO_MINIMO = 1.0;         // retorno com fechamento anterior abaixo de R$ 1,00 e descartado
const string MOTIVO_PRECO_MINIMO = "PRECO_ABAIXO_DE_1_REAL";

// =============================================================================================
// Indicadores (parametros classicos de mercado)

const int JANELA_MEDIA = 20;          // media movel de 20 e Bandas de Bollinger
const double BOLLINGER_DESVIOS = 2;
const int JANELA_VOLATILIDADE = 20;
const int JANELA_IFR = 14;
const int MACD_CURTA = 12;
const int MACD_LONGA = 26;
const int MACD_SINAL = 9;
const int JANELA_MOMENTO = 10;
const int JANELA_ESTOCASTICO = 14;
const int JANELA_ATR = 14;
const int JANELA_OBV = 10;
// dias que um trecho precisa ter antes de TODOS os indicadores existirem (o mais lento e o sinal do MACD)
const int AQUECIMENTO = MACD_LONGA + MACD_SINAL - 2;

// =============================================================================================
// Previsao e relatorio

const int NUM_VARIAVEIS = 10;     // variaveis da regressao (constante + 9 indicadores)
const int JANELA_TREINO = 500;    // treina com os 500 exemplos anteriores (~2 anos)
// Ridge: "freia" os coeficientes dos indicadores, para o modelo nao decorar o ruido da janela de treino.
// 0 = regressao comum (igual ao programa original); quanto maior, mais o modelo confia so na constante.
// 1 foi o melhor valor testado (0; 0,01; 0,1; 1; 10), com janela de 250 e de 500. Com 500: margem geral
// de -0,36 pp (sem Ridge) para -0,09 pp; com 250 seria de -0,53 para -0,26.
// Para comparar com o original (src/) byte a byte, volte para 0.
const double RIDGE = 1;

const int TOP_EXTREMOS = 10;      // maiores altas e quedas
const int FAIXAS_POR_LADO = 10;   // histograma: faixas de 1% de -10% a +10%
const int NUM_FAIXAS = 2 * FAIXAS_POR_LADO + 2;  // + as duas caudas
const int MINIMO_DIAS_COMUNS = 250;   // beta e correlacao: pelo menos ~1 ano em comum
const int TOP_RANKINGS = 5;
const int ATIVOS_EM_DESTAQUE = 3;

// =============================================================================================
// Estruturas

// Uma linha do CSV = uma cotacao diaria de um ativo.
struct Registro {
    string data;            // "2025-01-20" (comparar como texto ja ordena por data)
    string ativo;           // "PETR4"
    double abertura;
    double maxima;
    double minima;
    double fechamento;
    long long volume;       // quantidade de acoes negociadas
    double volumeFinanceiro;
    string tipo;            // ON, PN, UNT, DRN...
    string empresa;         // nome resumido ("PETROBRAS")
    string motivoDescarte;  // vazio = normal; senao o retorno deste dia nao entra na analise
};

// Depois de ordenar, cada ativo ocupa as posicoes de "inicio" ate "fim - 1" do vetor de registros.
struct BlocoAtivo {
    string ativo;
    int inicio;
    int fim;
};

// Uma linha de dados/eventos/ajustes.csv
//   "ajustar":   precos ANTES de "data" sao multiplicados por multiplicadorPreco
//   "descartar": o retorno do dia "data" nao entra na analise
//   "excluir":   o ativo inteiro sai da analise
struct Ajuste {
    string ativo;
    string data;
    string acao;
    double multiplicadorPreco;
    string tipo;
};

struct DadosPreparados {
    vector<Registro> registros;   // so acoes, ordenadas por ativo e data, com os ajustes aplicados
    vector<BlocoAtivo> blocos;    // os ativos que entram na analise
    vector<string> pregoes;       // todas as datas de pregao, sem repeticao e em ordem

    // contagens de cada filtro (para mostrar no terminal)
    int registrosLidos = 0;
    int ativosAcoes = 0;
    int ativosLiquidos = 0;
    int ajustesAplicados = 0;
    int registrosMantidos = 0;
};

// Indicadores de um dia de um ativo (so com dados ate aquele dia).
struct Indicadores {
    bool temRetorno = false;  // existe retorno em relacao ao dia anterior
    bool valido = false;      // todos os campos abaixo foram calculados
    double retorno = 0;
    double mm20 = 0;
    double bollingerInferior = 0, bollingerSuperior = 0;
    double volatilidade20 = 0;
    double ifr = 0;
    double macd = 0, macdSinal = 0;
    double momento = 0;
    double estocasticoK = 0;
    double atr = 0;
    double obvVariacao = 0;
};

// Resultado da previsao de um ativo (ou a soma de todos).
struct AvaliacaoPrevisao {
    long previsoes = 0;        // dias previstos e conferidos
    long acertos = 0;
    long acertosBaseline = 0;  // acertos do baseline ("aposta na direcao mais comum") nos mesmos dias
    long discordancias = 0;    // dias em que modelo e baseline previram direcoes diferentes
    long altasReais = 0;       // dias que de fato subiram (= acertos de quem chuta "sempre ALTA")
    long previsoesAlta = 0;    // dias em que o modelo previu ALTA
    long semMovimento = 0;     // retorno real = 0: fica fora da conta
    long semSolucao = 0;       // janelas em que a regressao nao tinha solucao unica
    bool temProxima = false;   // existe previsao para o pregao seguinte ao ultimo dia
    double retornoEstimado = 0;
};

// Um retorno diario (para as maiores altas e quedas).
// So numeros: ativo, data e empresa estao em registros[indice].
struct Extremo {
    double retorno;
    int indice;
};

// Tudo o que se calcula olhando UM ativo sozinho.
//
// ATENCAO: so numeros e arrays de tamanho fixo (nada de string ou vector). Assim esta struct pode ser
// gravada direto na memoria compartilhada da versao com processos: string e vector guardam ponteiros
// para a memoria do processo que os criou, que nao valem no outro processo.
// O nome do ativo, a empresa e as datas o programa busca em registros[inicio] e registros[fim - 1].
struct ResultadoAtivo {
    int inicio = 0, fim = 0;   // o bloco do ativo em registros
    int pregoes = 0;

    // estatisticas do periodo
    double precoMedio = 0, precoMinimo = 0, precoMaximo = 0;
    long retornos = 0;         // retornos validos
    double retornoMedio = 0;
    double volatilidade = 0;   // desvio padrao dos retornos diarios
    double volumeFinanceiroTotal = 0, volumeFinanceiroMedio = 0;

    Indicadores ultimo;        // indicadores do ultimo pregao
    double fechamentoUltimo = 0;

    AvaliacaoPrevisao previsao;
    bool negociadaNoUltimoPregao = false;

    int numAltas = 0, numQuedas = 0;   // quantas posicoes dos arrays abaixo estao preenchidas
    Extremo maioresAltas[TOP_EXTREMOS];
    Extremo maioresQuedas[TOP_EXTREMOS];
    long long histograma[NUM_FAIXAS] = {};

    bool temBeta = false;
    double beta = 0;   // preenchido depois (depende de todos os ativos)
};

struct Mercado {
    vector<double> indice;   // retorno medio de todos os ativos em cada pregao (NaN = nenhum)
    long dias = 0;
    double retornoAcumulado = 0;
    double volatilidade = 0;
};

struct ParCorrelacao {
    int a, b;   // posicao dos dois ativos em blocos (e em geral.ativos)
    double correlacao;
    long diasComuns;
};

// Os resultados de todos os ativos combinados.
struct ResultadoGeral {
    vector<ResultadoAtivo> ativos;   // na ordem dos blocos (alfabetica)
    vector<Extremo> maioresAltas, maioresQuedas;
    long long histograma[NUM_FAIXAS] = {};
    long long totalRetornos = 0;
    AvaliacaoPrevisao previsao;      // soma de todos os ativos
    long proximasAlta = 0, proximasQueda = 0;
    string menorData, maiorData;
    Mercado mercado;
    vector<ParCorrelacao> correlacoes;
};

// Tempo de cada etapa, em segundos (preenchido pelo main de cada versao).
struct Tempos {
    double leitura = 0;
    double preparacao = 0;
    double porAtivo = 0;      // a parte dividida entre as threads/processos
    double entreAtivos = 0;   // mercado, beta, correlacao, combinacao
};

// =============================================================================================
// comum.cpp

double agoraEmSegundos();
string paraMaiusculas(string texto);
vector<string> separar(const string& linha, char separador);

// Leitura: todos os CSVs de PASTA_DADOS e o arquivo de ajustes. Em erro, mostra a mensagem e sai.
vector<Registro> lerDados();
vector<Ajuste> lerAjustes();

// Preparacao: so acoes -> ordena -> separa por ativo -> liquidez -> excluidos -> piso de preco -> ajustes
DadosPreparados prepararDados(const vector<Registro>& lidos, const vector<Ajuste>& ajustes);

// A analise de UM ativo (estatisticas, indicadores, previsao, extremos, histograma).
// E essa a funcao que as versoes dividem entre threads/processos.
ResultadoAtivo analisarAtivo(const DadosPreparados& dados, int b);

// Depois que todos os ativos foram analisados: indice de mercado, beta, correlacao e combinacao.
void etapasEntreAtivos(const DadosPreparados& dados, ResultadoGeral& geral);

// Comparacoes usadas nos rankings (desempate pelo indice = por ativo e data)
bool altaVemAntes(const Extremo& x, const Extremo& y);
bool quedaVemAntes(const Extremo& x, const Extremo& y);

// Faixa do histograma de um retorno e o rotulo dela ("-2% a -1%")
int faixaDoRetorno(double retorno);
string rotuloDaFaixa(int faixa);

// =============================================================================================
// relatorio.cpp

void imprimirPreparacao(const DadosPreparados& dados);
void imprimirResumo(const DadosPreparados& dados, const ResultadoGeral& geral);
// Sem ativo pedido: as 3 maiores altas e quedas previstas. Com ativo pedido: so ele.
void imprimirDestaques(const DadosPreparados& dados, const ResultadoGeral& geral, const string& ativoPedido);
void imprimirTempos(const Tempos& tempos);
// Grava os 3 CSVs em PASTA_RESULTADOS
void gravarArquivos(const DadosPreparados& dados, const ResultadoGeral& geral);

// Usadas pelo terminal E pelo .md, para os dois mostrarem a mesma coisa
string nomeAtivo(const DadosPreparados& dados, const ResultadoAtivo& r);
string nomeEmpresa(const DadosPreparados& dados, const ResultadoAtivo& r);
string primeiraData(const DadosPreparados& dados, const ResultadoAtivo& r);
string ultimaData(const DadosPreparados& dados, const ResultadoAtivo& r);
double taxaDeAcerto(const AvaliacaoPrevisao& p);
double taxaBaseline(const AvaliacaoPrevisao& p);
double margem(const AvaliacaoPrevisao& p);
double erroDaMargem(const AvaliacaoPrevisao& p);
bool margemSignificativa(const AvaliacaoPrevisao& p);
// Chutes fixos: apostar todo dia na mesma direcao
double taxaSempreAlta(const AvaliacaoPrevisao& p);
double taxaSempreQueda(const AvaliacaoPrevisao& p);
double erroSobreSempreAlta(const AvaliacaoPrevisao& p);
double erroSobreSempreQueda(const AvaliacaoPrevisao& p);
// Rankings: devolvem as posicoes dos ativos em geral.ativos, ja cortadas no tamanho
vector<int> maisNegociadas(const vector<ResultadoAtivo>& ativos);
vector<int> rankingVolatilidade(const vector<ResultadoAtivo>& ativos, bool maisVolateis);
vector<int> rankingMargem(const vector<ResultadoAtivo>& ativos, bool maiores);
vector<int> rankingPrevisaoProxima(const vector<ResultadoAtivo>& ativos, bool alta, int quantidade);
// Posicao do ativo pedido em geral.ativos, ou -1 se ele nao foi analisado
int procurarAtivo(const DadosPreparados& dados, const ResultadoGeral& geral, const string& ativoPedido);

// =============================================================================================
// relatorio_md.cpp

// Relatorio da execucao em Markdown, explicado para quem nao conhece o projeto.
// Grava em PASTA_RELATORIOS/relatorio_<versao>[_<trabalhadores>]_<data>_<hora>.md e devolve o caminho.
string gravarRelatorioMarkdown(const string& versao, int trabalhadores, const DadosPreparados& dados,
                               const ResultadoGeral& geral, const Tempos& tempos, const string& ativoPedido);

#endif

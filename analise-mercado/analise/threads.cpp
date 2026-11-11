// Versao com THREADS: os ativos sao divididos entre N threads (pthread), como nos exemplos de aula.
// A thread principal (a do main) tambem trabalha: ela e a "thread 0" e cria so as outras N - 1.
//
// Uso (de dentro de analise-mercado/):
//   ./analise/build/threads 4          -> 4 threads (a principal + 3 criadas)
//   ./analise/build/threads 4 PETR4    -> 4 threads; detalha a PETR4

#include "comum.h"

#include <pthread.h>

#include <cstdio>
#include <cstdlib>

// Variaveis globais sao compartilhadas por todas as threads do processo.
DadosPreparados dados;           // so LIDO pelas threads
ResultadoGeral geral;            // cada thread escreve SO nas posicoes dos seus ativos em geral.ativos
int totalThreads;

// O que cada thread faz: analisa os ativos id, id + total, id + 2*total, ...
//
// Divisao INTERCALADA (e nao em blocos como "0 a 99, 100 a 199..."): os ativos tem tamanhos bem diferentes
// (de ~600 a ~2700 pregoes). Intercalando, cada thread pega uma mistura de longos e curtos e todas terminam
// perto do mesmo tempo.
//
// Nao precisa de mutex: duas threads nunca escrevem na mesma posicao de geral.ativos, e "dados" ninguem altera.
void* trabalhar(void* arg) {
    int id = (int)(long)arg;
    int numAtivos = dados.blocos.size();
    for (int b = id; b < numAtivos; b += totalThreads) {
        geral.ativos[b] = analisarAtivo(dados, b);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || atoi(argv[1]) < 1) {
        printf("Uso: threads <numero de threads> [ATIVO]   (ex.: threads 4 ou threads 4 PETR4)\n");
        return 1;
    }
    totalThreads = atoi(argv[1]);
    string ativoPedido = "";
    if (argc > 2) ativoPedido = paraMaiusculas(argv[2]);

    printf("Versao: threads | trabalhadores: %d\n", totalThreads);
    Tempos tempos;

    // 1. Ler os CSVs e os ajustes de eventos
    double inicio = agoraEmSegundos();
    vector<Registro> lidos = lerDados();
    vector<Ajuste> ajustes = lerAjustes();
    tempos.leitura = agoraEmSegundos() - inicio;

    // 2. Limpar e organizar
    inicio = agoraEmSegundos();
    dados = prepararDados(lidos, ajustes);
    tempos.preparacao = agoraEmSegundos() - inicio;
    imprimirPreparacao(dados);

    int numAtivos = dados.blocos.size();
    if (numAtivos == 0) {
        printf("Nenhum ativo passou pelos filtros.\n");
        return 0;
    }

    // 3. Analisar cada ativo, dividido entre as threads
    inicio = agoraEmSegundos();
    // o vetor ja nasce com uma posicao por ativo: as threads so preenchem.
    // (push_back de varias threads ao mesmo tempo NAO seria seguro: o vetor pode mudar de lugar na memoria)
    geral.ativos.resize(numAtivos);

    // cria as threads 1, 2, ..., N - 1 (a 0 e a propria thread principal)
    vector<pthread_t> ids(totalThreads);
    for (int i = 1; i < totalThreads; i++) {
        pthread_create(&ids[i], NULL, trabalhar, (void*)(long)i);
    }

    trabalhar((void*)0);   // a thread principal tambem trabalha: ela e a "thread 0"

    for (int i = 1; i < totalThreads; i++) {
        pthread_join(ids[i], NULL);   // espera cada thread terminar antes de usar os resultados
    }
    tempos.porAtivo = agoraEmSegundos() - inicio;

    // 4. Etapas que precisam de todos os ativos prontos
    inicio = agoraEmSegundos();
    etapasEntreAtivos(dados, geral);
    tempos.entreAtivos = agoraEmSegundos() - inicio;

    // 5. Mostrar e gravar
    imprimirResumo(dados, geral);
    imprimirDestaques(dados, geral, ativoPedido);
    gravarArquivos(dados, geral);
    imprimirTempos(tempos);
    string relatorio = gravarRelatorioMarkdown("threads", totalThreads, dados, geral, tempos, ativoPedido);
    printf("\nRelatorio completo: %s\n", relatorio.c_str());
    return 0;
}

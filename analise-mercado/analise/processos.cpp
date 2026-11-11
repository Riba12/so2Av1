// Versao com PROCESSOS: os ativos sao divididos entre N processos (fork), que devolvem os resultados por
// memoria compartilhada (shmget/shmat), como nos exemplos de aula.
// O pai tambem trabalha: ele e o "processo 0" e cria so os outros N - 1 (os filhos).
//
// Uso (de dentro de analise-mercado/):
//   ./analise/build/processos 4          -> 4 processos (o pai + 3 filhos)
//   ./analise/build/processos 4 PETR4    -> 4 processos; detalha a PETR4

#include "comum.h"

#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

// O que cada processo faz: analisa os ativos id, id + total, id + 2*total, ... (divisao intercalada: cada
// processo pega uma mistura de ativos longos e curtos) e grava o resultado na memoria compartilhada.
// Nao precisa de semaforo: cada processo escreve so nas posicoes dos seus ativos.
void trabalhar(int id, int totalProcessos, const DadosPreparados& dados, ResultadoAtivo* compartilhado) {
    int numAtivos = dados.blocos.size();
    for (int b = id; b < numAtivos; b += totalProcessos) {
        compartilhado[b] = analisarAtivo(dados, b);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2 || atoi(argv[1]) < 1) {
        printf("Uso: processos <numero de processos> [ATIVO]   (ex.: processos 4 ou processos 4 PETR4)\n");
        return 1;
    }
    int totalProcessos = atoi(argv[1]);
    string ativoPedido = "";
    if (argc > 2) ativoPedido = paraMaiusculas(argv[2]);

    printf("Versao: processos | trabalhadores: %d\n", totalProcessos);
    Tempos tempos;

    // 1. Ler os CSVs e os ajustes de eventos
    double inicio = agoraEmSegundos();
    vector<Registro> lidos = lerDados();
    vector<Ajuste> ajustes = lerAjustes();
    tempos.leitura = agoraEmSegundos() - inicio;

    // 2. Limpar e organizar
    inicio = agoraEmSegundos();
    DadosPreparados dados = prepararDados(lidos, ajustes);
    tempos.preparacao = agoraEmSegundos() - inicio;
    imprimirPreparacao(dados);

    int numAtivos = dados.blocos.size();
    if (numAtivos == 0) {
        printf("Nenhum ativo passou pelos filtros.\n");
        return 0;
    }

    // 3. Analisar cada ativo, dividido entre os processos
    inicio = agoraEmSegundos();

    // 3.1 Criar a memoria compartilhada: um vetor de ResultadoAtivo, uma posicao por ativo.
    //     IPC_PRIVATE: os filhos criados depois enxergam a mesma memoria automaticamente.
    //     0600: leitura e escrita para o dono.
    //     (ResultadoAtivo so tem numeros e arrays fixos - ver comum.h - por isso pode morar aqui)
    int idMemoria = shmget(IPC_PRIVATE, numAtivos * sizeof(ResultadoAtivo), IPC_CREAT | 0600);
    if (idMemoria == -1) {
        perror("shmget");
        exit(1);
    }
    // shmat "pendura" a memoria compartilhada neste processo e devolve o endereco dela
    ResultadoAtivo* compartilhado = (ResultadoAtivo*)shmat(idMemoria, NULL, 0);

    // 3.2 Criar os filhos 1, 2, ..., N - 1 (o 0 e o proprio pai). Cada filho nasce com uma COPIA da memoria
    //     do pai (inclusive "dados", ja preparados) e com a memoria compartilhada pendurada no mesmo lugar.
    fflush(stdout);   // esvazia o buffer do printf antes do fork, senao o texto pendente sairia duplicado
    for (int id = 1; id < totalProcessos; id++) {
        int pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            // --- FILHO: faz a parte dele e termina
            trabalhar(id, totalProcessos, dados, compartilhado);
            shmdt(compartilhado);
            exit(0);   // o filho termina aqui; nao pode continuar o codigo do pai
        }
        // --- PAI: segue no laco criando o proximo filho
    }

    // 3.3 O pai tambem trabalha: ele e o "processo 0"
    trabalhar(0, totalProcessos, dados, compartilhado);

    // 3.4 O pai espera todos os filhos terminarem
    for (int i = 1; i < totalProcessos; i++) {
        wait(NULL);
    }

    // 3.5 O pai copia os resultados da memoria compartilhada e depois a apaga
    ResultadoGeral geral;
    geral.ativos.resize(numAtivos);
    for (int b = 0; b < numAtivos; b++) {
        geral.ativos[b] = compartilhado[b];
    }
    shmdt(compartilhado);                  // solta a memoria deste processo
    shmctl(idMemoria, IPC_RMID, NULL);     // apaga a memoria compartilhada do sistema
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
    string relatorio = gravarRelatorioMarkdown("processos", totalProcessos, dados, geral, tempos, ativoPedido);
    printf("\nRelatorio completo: %s\n", relatorio.c_str());
    return 0;
}

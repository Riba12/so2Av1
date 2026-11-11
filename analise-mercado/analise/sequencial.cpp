// Versao SEQUENCIAL: um ativo depois do outro, num fluxo so.
//
// Uso (de dentro de analise-mercado/):
//   ./analise/build/sequencial          -> resumo + as 3 maiores altas e 3 maiores quedas previstas
//   ./analise/build/sequencial PETR4    -> resumo + a PETR4

#include "comum.h"

#include <cstdio>

int main(int argc, char* argv[]) {
    string ativoPedido = "";
    if (argc > 1) ativoPedido = paraMaiusculas(argv[1]);

    printf("Versao: sequencial\n");
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

    // 3. Analisar cada ativo (a parte que as outras versoes dividem entre threads/processos)
    inicio = agoraEmSegundos();
    ResultadoGeral geral;
    geral.ativos.resize(numAtivos);
    for (int b = 0; b < numAtivos; b++) {
        geral.ativos[b] = analisarAtivo(dados, b);
    }
    tempos.porAtivo = agoraEmSegundos() - inicio;

    // 4. Etapas que precisam de todos os ativos prontos (mercado, beta, correlacao, combinacao)
    inicio = agoraEmSegundos();
    etapasEntreAtivos(dados, geral);
    tempos.entreAtivos = agoraEmSegundos() - inicio;

    // 5. Mostrar e gravar
    imprimirResumo(dados, geral);
    imprimirDestaques(dados, geral, ativoPedido);
    gravarArquivos(dados, geral);
    imprimirTempos(tempos);
    string relatorio = gravarRelatorioMarkdown("sequencial", 1, dados, geral, tempos, ativoPedido);
    printf("\nRelatorio completo: %s\n", relatorio.c_str());
    return 0;
}

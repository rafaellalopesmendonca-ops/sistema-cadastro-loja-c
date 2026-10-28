#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* Nomes dos arquivos usados pelo programa. */
#define ARQUIVO "produtos.txt"
#define TEMPORARIO "temp.txt"
#define BACKUP "produtos_backup.txt"
#define ARQ_CSV "produtos.csv"

/* Limites de tamanho dos campos de texto. */
#define TAM_NOME 50
#define TAM_CAT 30
#define TAM_FORN 40

/* Estrutura que representa um produto da loja. */
typedef struct {
    int id;
    char produto[TAM_NOME];
    char categoria[TAM_CAT];
    float preco;
    int quantidade;
    char fornecedor[TAM_FORN];
} Produto;

/* Tipo de ponteiro para as funções que comparam produtos. */
typedef int (*Comparador)(const Produto *, const Produto *);

/* Lê um texto do teclado e remove a quebra de linha. */
void lerTexto(const char *msg, char *dest, size_t tam) {
    printf("%s", msg);
    if (fgets(dest, (int)tam, stdin) == NULL) {
        dest[0] = '\0';
        return;
    }

    size_t len = strlen(dest);
    if (len > 0 && dest[len - 1] == '\n') {
        dest[len - 1] = '\0';
    } else {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}
    }

    /* O caractere | separa campos no arquivo, então é substituído. */
    for (char *p = dest; *p != '\0'; p++) {
        if (*p == '|') *p = '-';
    }
}

/* Lê um número inteiro, repetindo até receber um valor válido. */
int lerInt(const char *msg) {
    char buf[100], extra;
    int valor;

    for (;;) {
        printf("%s", msg);
        if (fgets(buf, sizeof(buf), stdin) == NULL) return 0;
        if (sscanf(buf, " %d %c", &valor, &extra) == 1) return valor;
        printf("Valor invalido! Digite um numero inteiro.\n");
    }
}

/* Permite usar vírgula ou ponto como separador decimal. */
void trocaVirgula(char *texto) {
    for (; *texto != '\0'; texto++) {
        if (*texto == ',') *texto = '.';
    }
}

/* Lê um número decimal não negativo. */
float lerFloat(const char *msg) {
    char buf[100], extra;
    float valor;

    for (;;) {
        printf("%s", msg);
        if (fgets(buf, sizeof(buf), stdin) == NULL) return 0.0f;
        trocaVirgula(buf);
        if (sscanf(buf, " %f %c", &valor, &extra) == 1 && valor >= 0.0f) {
            return valor;
        }
        printf("Valor invalido! Digite um numero nao negativo.\n");
    }
}

/* Compara textos sem diferenciar maiúsculas e minúsculas. */
int comparaIgnoraCase(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* Verifica se um texto contém outro, ignorando maiúsculas/minúsculas. */
int contemIgnoraCase(const char *texto, const char *termo) {
    char a[256], b[256];
    size_t i;

    for (i = 0; i < sizeof(a) - 1 && texto[i]; i++)
        a[i] = (char)tolower((unsigned char)texto[i]);
    a[i] = '\0';

    for (i = 0; i < sizeof(b) - 1 && termo[i]; i++)
        b[i] = (char)tolower((unsigned char)termo[i]);
    b[i] = '\0';

    return strstr(a, b) != NULL;
}

/* Escreve o cabeçalho da tabela no arquivo de texto. */
void escreverCabecalho(FILE *fp) {
    fprintf(fp, "ID|Produto|Categoria|Preco|Quantidade|Fornecedor\n");
}

/* Escreve um produto no formato separado por |. */
void escreverProduto(FILE *fp, const Produto *p) {
    fprintf(fp, "%d|%s|%s|%.2f|%d|%s\n",
            p->id, p->produto, p->categoria,
            p->preco, p->quantidade, p->fornecedor);
}

/* Converte uma linha do arquivo em uma estrutura Produto. */
int lerLinha(const char *linha, Produto *p) {
    return sscanf(linha, "%d|%49[^|]|%29[^|]|%f|%d|%39[^\r\n]",
                  &p->id, p->produto, p->categoria,
                  &p->preco, &p->quantidade, p->fornecedor) == 6;
}

/* Verifica se o arquivo principal pode ser aberto para leitura. */
int arquivoExiste(void) {
    FILE *fp = fopen(ARQUIVO, "r");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

/* Carrega todos os produtos para um vetor alocado dinamicamente. */
Produto *carregar(int *qtd) {
    *qtd = 0;
    FILE *fp = fopen(ARQUIVO, "r");
    if (!fp) return NULL;

    int capacidade = 10;
    Produto *lista = malloc((size_t)capacidade * sizeof(Produto));
    if (!lista) {
        fclose(fp);
        return NULL;
    }

    char linha[256];
    while (fgets(linha, sizeof(linha), fp)) {
        Produto p;
        if (!lerLinha(linha, &p)) continue;

        if (*qtd == capacidade) {
            capacidade *= 2;
            Produto *novo = realloc(lista, (size_t)capacidade * sizeof(Produto));
            if (!novo) {
                free(lista);
                fclose(fp);
                *qtd = 0;
                return NULL;
            }
            lista = novo;
        }
        lista[*qtd] = p;
        (*qtd)++;
    }

    if (ferror(fp)) {
        free(lista);
        fclose(fp);
        *qtd = 0;
        return NULL;
    }

    fclose(fp);
    return lista;
}

/* Procura um produto pelo ID e copia os dados encontrados para saída. */
int buscarPorId(int id, Produto *saida) {
    int qtd = 0;
    Produto *lista = carregar(&qtd);
    if (!lista) return 0;

    int achou = 0;
    for (int i = 0; i < qtd; i++) {
        if (lista[i].id == id) {
            *saida = lista[i];
            achou = 1;
            break;
        }
    }
    free(lista);
    return achou;
}

/* Impede nomes de produtos duplicados, ignorando maiúsculas/minúsculas. */
int nomeExiste(const char *nome, int idIgnorar) {
    int qtd = 0;
    Produto *lista = carregar(&qtd);
    if (!lista) return 0;

    int existe = 0;
    for (int i = 0; i < qtd; i++) {
        if (lista[i].id != idIgnorar &&
            comparaIgnoraCase(lista[i].produto, nome) == 0) {
            existe = 1;
            break;
        }
    }
    free(lista);
    return existe;
}

/* Gera o próximo ID com base no maior ID existente. */
int proximoId(void) {
    int qtd = 0, maior = 0;
    Produto *lista = carregar(&qtd);

    if (lista) {
        for (int i = 0; i < qtd; i++)
            if (lista[i].id > maior) maior = lista[i].id;
        free(lista);
    }
    return maior + 1;
}

/* Exibe os produtos em formato de tabela sem alterar o arquivo. */
void imprimirTabela(const Produto *lista, int qtd) {
    if (qtd == 0) {
        printf("\nNenhum produto para exibir.\n");
        return;
    }

    printf("\n---------------------------------------------------------------------------------------------\n");
    printf("| %-4s | %-25s | %-18s | %10s | %5s | %-18s |\n",
           "ID", "Produto", "Categoria", "Preco (R$)", "Qtd", "Fornecedor");
    printf("---------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < qtd; i++) {
        printf("| %-4d | %-25.25s | %-18.18s | %10.2f | %5d | %-18.18s |\n",
               lista[i].id, lista[i].produto, lista[i].categoria,
               lista[i].preco, lista[i].quantidade, lista[i].fornecedor);
    }

    printf("---------------------------------------------------------------------------------------------\n");
    printf("Total: %d produto(s)\n", qtd);
}

/* Cria o arquivo com os quatro produtos indicados no enunciado. */
void criarArquivo(void) {
    if (arquivoExiste()) {
        char resposta[10];
        lerTexto("O arquivo ja existe. Recriar e perder os dados atuais? (s/n): ",
                 resposta, sizeof(resposta));
        if (tolower((unsigned char)resposta[0]) != 's') {
            printf("Operacao cancelada.\n");
            return;
        }
    }

    Produto iniciais[4] = {
        {1, "Camiseta Basica", "Vestuario", 49.90f, 30, "Moda Brasil"},
        {2, "Mouse Optico USB", "Informatica", 59.99f, 15, "TecnoShop"},
        {3, "Shampoo 300ml", "Higiene Pessoal", 22.50f, 40, "Beleza Pura"},
        {4, "Cafe 500g", "Alimentos", 18.00f, 25, "Graos do Sul"}
    };

    FILE *fp = fopen(ARQUIVO, "w");
    if (!fp) {
        printf("Erro ao criar o arquivo!\n");
        return;
    }

    escreverCabecalho(fp);
    for (int i = 0; i < 4; i++) escreverProduto(fp, &iniciais[i]);

    if (fclose(fp) != 0) {
        printf("Erro ao finalizar o arquivo.\n");
        return;
    }
    printf("Arquivo '%s' criado com 4 produtos iniciais.\n", ARQUIVO);
}

/* Lista os produtos lidos do arquivo. */
void listarProdutos(void) {
    int qtd = 0;
    Produto *lista = carregar(&qtd);

    if (!lista) {
        printf("Nao foi possivel carregar '%s'. Confira a pasta atual e o arquivo.\n", ARQUIVO);
        return;
    }

    imprimirTabela(lista, qtd);
    free(lista);
}

/* Insere um produto novo, com ID automático e sem duplicar nomes. */
void inserirProduto(void) {
    if (!arquivoExiste()) {
        FILE *novo = fopen(ARQUIVO, "w");
        if (!novo) {
            printf("Erro ao criar o arquivo!\n");
            return;
        }
        escreverCabecalho(novo);
        fclose(novo);
    }

    Produto p;
    lerTexto("Nome do produto: ", p.produto, sizeof(p.produto));
    if (p.produto[0] == '\0') {
        printf("O nome nao pode ser vazio.\n");
        return;
    }
    if (nomeExiste(p.produto, 0)) {
        printf("Ja existe um produto com esse nome.\n");
        return;
    }

    lerTexto("Categoria: ", p.categoria, sizeof(p.categoria));
    if (p.categoria[0] == '\0') strcpy(p.categoria, "Geral");

    p.preco = lerFloat("Preco (R$): ");
    do {
        p.quantidade = lerInt("Quantidade: ");
        if (p.quantidade < 0) printf("A quantidade nao pode ser negativa.\n");
    } while (p.quantidade < 0);

    lerTexto("Fornecedor: ", p.fornecedor, sizeof(p.fornecedor));
    if (p.fornecedor[0] == '\0') strcpy(p.fornecedor, "-");

    p.id = proximoId();
    FILE *fp = fopen(ARQUIVO, "a");
    if (!fp) {
        printf("Erro ao abrir o arquivo!\n");
        return;
    }

    escreverProduto(fp, &p);
    if (fclose(fp) != 0) {
        printf("Erro ao salvar o produto.\n");
        return;
    }
    printf("Produto cadastrado! ID gerado: %d\n", p.id);
}

/* Copia o arquivo original para um backup, para permitir recuperação. */
int criarBackup(void) {
    FILE *orig = fopen(ARQUIVO, "rb");
    if (!orig) {
        printf("Erro ao abrir o arquivo original: %s\n", strerror(errno));
        return 0;
    }

    FILE *copia = fopen(BACKUP, "wb");
    if (!copia) {
        printf("Erro ao criar o backup: %s\n", strerror(errno));
        fclose(orig);
        return 0;
    }

    int c, ok = 1;
    while ((c = fgetc(orig)) != EOF) {
        if (fputc(c, copia) == EOF) {
            ok = 0;
            break;
        }
    }
    if (ferror(orig)) ok = 0;
    if (fclose(orig) != 0) ok = 0;
    if (fclose(copia) != 0) ok = 0;

    if (!ok) {
        printf("Erro ao copiar o arquivo para o backup.\n");
        return 0;
    }
    return 1;
}

/* Substitui o arquivo original de forma compatível com Windows.
   Mantém o backup se a operação não puder ser concluída. */
int substituirArquivo(void) {
    if (!criarBackup()) return 0;

    /* No Windows, rename não substitui um destino já existente.
       Removemos o original somente depois de confirmar o backup. */
    if (remove(ARQUIVO) != 0) {
        printf("Nao foi possivel remover o original: %s\n", strerror(errno));
        printf("O original e o temporario foram preservados.\n");
        return 0;
    }

    if (rename(TEMPORARIO, ARQUIVO) == 0) {
        printf("Backup de seguranca criado: %s\n", BACKUP);
        return 1;
    }

    int erroRename = errno;
    printf("Falha ao renomear o temporario: %s\n", strerror(erroRename));

    /* Se a troca falhar, tenta restaurar o arquivo original do backup. */
    if (rename(BACKUP, ARQUIVO) != 0) {
        printf("ATENCAO: nao foi possivel restaurar o backup.\n");
        printf("Preserve '%s' e '%s'.\n", BACKUP, TEMPORARIO);
    } else {
        printf("O arquivo original foi restaurado pelo backup.\n");
    }
    return 0;
}

/* Altera um produto regravando o arquivo por meio de um temporário. */
void alterarProduto(void) {
    int id = lerInt("ID do produto a alterar: ");
    Produto atual;

    if (!buscarPorId(id, &atual)) {
        printf("Produto nao encontrado.\n");
        return;
    }

    Produto novo = atual;
    char pergunta[120], texto[100];

    printf("\nEditando: %s\nPressione Enter para manter o valor atual.\n", atual.produto);

    snprintf(pergunta, sizeof(pergunta), "Nome [%s]: ", atual.produto);
    lerTexto(pergunta, texto, sizeof(texto));
    if (texto[0]) {
        if (strlen(texto) >= sizeof(novo.produto)) {
            printf("Nome muito longo.\n");
            return;
        }
        if (nomeExiste(texto, atual.id)) {
            printf("Ja existe outro produto com esse nome.\n");
            return;
        }
        strcpy(novo.produto, texto);
    }

    snprintf(pergunta, sizeof(pergunta), "Categoria [%s]: ", atual.categoria);
    lerTexto(pergunta, texto, sizeof(texto));
    if (texto[0]) {
        if (strlen(texto) >= sizeof(novo.categoria)) {
            printf("Categoria muito longa.\n");
            return;
        }
        strcpy(novo.categoria, texto);
    }

    snprintf(pergunta, sizeof(pergunta), "Preco [%.2f]: ", atual.preco);
    lerTexto(pergunta, texto, sizeof(texto));
    if (texto[0]) {
        float valor;
        char extra;
        trocaVirgula(texto);
        if (sscanf(texto, " %f %c", &valor, &extra) == 1 && valor >= 0)
            novo.preco = valor;
        else
            printf("Preco invalido; valor anterior mantido.\n");
    }

    snprintf(pergunta, sizeof(pergunta), "Quantidade [%d]: ", atual.quantidade);
    lerTexto(pergunta, texto, sizeof(texto));
    if (texto[0]) {
        int valor;
        char extra;
        if (sscanf(texto, " %d %c", &valor, &extra) == 1 && valor >= 0)
            novo.quantidade = valor;
        else
            printf("Quantidade invalida; valor anterior mantido.\n");
    }

    snprintf(pergunta, sizeof(pergunta), "Fornecedor [%s]: ", atual.fornecedor);
    lerTexto(pergunta, texto, sizeof(texto));
    if (texto[0]) {
        if (strlen(texto) >= sizeof(novo.fornecedor)) {
            printf("Fornecedor muito longo.\n");
            return;
        }
        strcpy(novo.fornecedor, texto);
    }

    FILE *orig = fopen(ARQUIVO, "r");
    FILE *temp = fopen(TEMPORARIO, "w");
    if (!orig || !temp) {
        printf("Erro ao abrir os arquivos: %s\n", strerror(errno));
        if (orig) fclose(orig);
        if (temp) fclose(temp);
        return;
    }

    escreverCabecalho(temp);
    char linha[256];
    while (fgets(linha, sizeof(linha), orig)) {
        Produto p;
        if (lerLinha(linha, &p)) {
            if (p.id == id) escreverProduto(temp, &novo);
            else escreverProduto(temp, &p);
        }
    }

    int erro = ferror(orig) || ferror(temp);
    if (fclose(orig) != 0) erro = 1;
    if (fclose(temp) != 0) erro = 1;

    if (erro) {
        printf("Erro durante a gravacao. O original foi preservado.\n");
        return;
    }

    if (substituirArquivo()) printf("Produto alterado com sucesso!\n");
}

/* Exclui um produto copiando os demais para o arquivo temporário. */
void excluirProduto(void) {
    int id = lerInt("ID do produto a excluir: ");
    Produto alvo;

    if (!buscarPorId(id, &alvo)) {
        printf("Produto nao encontrado.\n");
        return;
    }

    printf("Produto: %s | Categoria: %s | Preco: R$ %.2f\n",
           alvo.produto, alvo.categoria, alvo.preco);

    char resposta[10];
    lerTexto("Confirma a exclusao? (s/n): ", resposta, sizeof(resposta));
    if (tolower((unsigned char)resposta[0]) != 's') {
        printf("Exclusao cancelada.\n");
        return;
    }

    FILE *orig = fopen(ARQUIVO, "r");
    FILE *temp = fopen(TEMPORARIO, "w");
    if (!orig || !temp) {
        printf("Erro ao abrir os arquivos: %s\n", strerror(errno));
        if (orig) fclose(orig);
        if (temp) fclose(temp);
        return;
    }

    escreverCabecalho(temp);
    char linha[256];
    while (fgets(linha, sizeof(linha), orig)) {
        Produto p;
        if (lerLinha(linha, &p) && p.id != id) escreverProduto(temp, &p);
    }

    int erro = ferror(orig) || ferror(temp);
    if (fclose(orig) != 0) erro = 1;
    if (fclose(temp) != 0) erro = 1;

    if (erro) {
        printf("Erro durante a gravacao. O original foi preservado.\n");
        return;
    }

    if (substituirArquivo()) printf("Produto excluido com sucesso!\n");
}

/* Busca produtos por nome ou categoria, sem diferenciar caixa. */
void buscarProdutos(int porCategoria) {
    char termo[TAM_NOME];
    lerTexto(porCategoria ? "Categoria a buscar: " : "Nome a buscar: ",
             termo, sizeof(termo));
    if (!termo[0]) {
        printf("Digite algo para buscar.\n");
        return;
    }

    int qtd = 0;
    Produto *lista = carregar(&qtd);
    if (!lista) {
        printf("Nao foi possivel carregar '%s'.\n", ARQUIVO);
        return;
    }

    Produto *achados = malloc((size_t)(qtd > 0 ? qtd : 1) * sizeof(Produto));
    if (!achados) {
        printf("Erro de memoria.\n");
        free(lista);
        return;
    }

    int n = 0;
    for (int i = 0; i < qtd; i++) {
        const char *campo = porCategoria ? lista[i].categoria : lista[i].produto;
        if (contemIgnoraCase(campo, termo)) achados[n++] = lista[i];
    }

    if (n == 0) printf("Nenhum produto encontrado para '%s'.\n", termo);
    else imprimirTabela(achados, n);

    free(achados);
    free(lista);
}

/* Troca dois produtos de lugar usando ponteiros. */
void trocar(Produto *a, Produto *b) {
    Produto aux = *a;
    *a = *b;
    *b = aux;
}

/* Comparadores para os quatro tipos de ordenação. */
int cmpNomeAZ(const Produto *a, const Produto *b) {
    return comparaIgnoraCase(a->produto, b->produto);
}
int cmpNomeZA(const Produto *a, const Produto *b) {
    return comparaIgnoraCase(b->produto, a->produto);
}
int cmpPrecoAsc(const Produto *a, const Produto *b) {
    return (a->preco > b->preco) - (a->preco < b->preco);
}
int cmpPrecoDesc(const Produto *a, const Produto *b) {
    return (b->preco > a->preco) - (b->preco < a->preco);
}

/* Ordena o vetor com Bubble Sort, usando o comparador escolhido. */
void bubbleSort(Produto *vetor, int n, Comparador cmp) {
    for (int i = 0; i < n - 1; i++) {
        int trocou = 0;
        for (int j = 0; j < n - i - 1; j++) {
            if (cmp(&vetor[j], &vetor[j + 1]) > 0) {
                trocar(&vetor[j], &vetor[j + 1]);
                trocou = 1;
            }
        }
        if (!trocou) break;
    }
}

/* Mostra o resultado da ordenação sem alterar o arquivo original. */
void ordenarProdutos(void) {
    printf("\n--- ORDENAR PRODUTOS ---\n");
    printf("1 - Nome (A-Z)\n");
    printf("2 - Nome (Z-A)\n");
    printf("3 - Preco (menor para maior)\n");
    printf("4 - Preco (maior para menor)\n");

    int op = lerInt("Escolha: ");
    Comparador cmp = NULL;
    switch (op) {
        case 1: cmp = cmpNomeAZ; break;
        case 2: cmp = cmpNomeZA; break;
        case 3: cmp = cmpPrecoAsc; break;
        case 4: cmp = cmpPrecoDesc; break;
        default:
            printf("Opcao invalida!\n");
            return;
    }

    int qtd = 0;
    Produto *lista = carregar(&qtd);
    if (!lista) {
        printf("Nao foi possivel carregar '%s'.\n", ARQUIVO);
        return;
    }

    bubbleSort(lista, qtd, cmp);
    imprimirTabela(lista, qtd);
    free(lista);
}

/* Exporta os produtos para CSV, com campos separados por vírgula. */
void exportarCSV(void) {
    int qtd = 0;
    Produto *lista = carregar(&qtd);
    if (!lista) {
        printf("Nao foi possivel carregar '%s'.\n", ARQUIVO);
        return;
    }

    FILE *fp = fopen(ARQ_CSV, "w");
    if (!fp) {
        printf("Erro ao criar o CSV: %s\n", strerror(errno));
        free(lista);
        return;
    }

    fprintf(fp, "ID,Produto,Categoria,Preco,Quantidade,Fornecedor\n");
    for (int i = 0; i < qtd; i++) {
        /* As aspas protegem campos de texto no CSV. */
        fprintf(fp, "%d,\"%s\",\"%s\",%.2f,%d,\"%s\"\n",
                lista[i].id, lista[i].produto, lista[i].categoria,
                lista[i].preco, lista[i].quantidade, lista[i].fornecedor);
    }

    int erro = fclose(fp) != 0;
    free(lista);
    if (erro) printf("Erro ao finalizar o CSV.\n");
    else printf("Dados exportados para '%s'.\n", ARQ_CSV);
}

/* Exibe o menu e chama a função escolhida. */
int main(void) {
    int opcao;
    do {
        printf("\n========== CADASTRO DE LOJA ==========\n");
        printf("1 - Criar arquivo com dados iniciais\n");
        printf("2 - Listar produtos\n");
        printf("3 - Inserir produto\n");
        printf("4 - Alterar produto\n");
        printf("5 - Excluir produto\n");
        printf("6 - Buscar por nome\n");
        printf("7 - Buscar por categoria\n");
        printf("8 - Ordenar produtos\n");
        printf("9 - Exportar para CSV\n");
        printf("0 - Sair\n");

        opcao = lerInt("Escolha: ");
        switch (opcao) {
            case 1: criarArquivo(); break;
            case 2: listarProdutos(); break;
            case 3: inserirProduto(); break;
            case 4: alterarProduto(); break;
            case 5: excluirProduto(); break;
            case 6: buscarProdutos(0); break;
            case 7: buscarProdutos(1); break;
            case 8: ordenarProdutos(); break;
            case 9: exportarCSV(); break;
            case 0: printf("Encerrando o programa...\n"); break;
            default: printf("Opcao invalida!\n");
        }
    } while (opcao != 0);

    return 0;
}

#include <windows.h>
#include <tchar.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h> 
#include <io.h>
#include <stdbool.h>
#include <time.h>

#define NEMPRESAS  30
#define NCLIENTES  5

#define SHM_NAME _T("SHM")
#define MUTEX_NAME _T("MUTEX")
#define EVENT_NAME TEXT("ATEMPRESAS")  
#define REGISTRY_NAME _T("Software\\Bolsa\\")
#define ValorRegistry _T("NCLIENTES")
#define EVENTO_SAIR TEXT("EventoSair")
#define PIPE_NAME TEXT("\\\\.\\pipe\\cliente") // nome do pipe
#define FILE_USER _T("clientes.txt")
#define FILE_EMPRESA _T("empresas.txt")
bool terminou = false;
bool tempo = true;

typedef struct _Empresas {
    TCHAR nome[50];
    unsigned int numAcoes;
    float precoAcao;
}Empresas;

typedef struct _SharedMemory {
    Empresas empresas[NEMPRESAS];
    TCHAR UltimaTransacao[100];
    int posEmpresa;
} SharedMemory;

typedef struct _DadosSM {
    HANDLE hMapFile;
    HANDLE shMutex;
    HANDLE shEvento;
    SharedMemory* sMemory;
}DadosSM;

typedef struct _CarteiraCliente {
    TCHAR nomeEmpresa[50];
    int nAcoes;
}CarteiraCliente;

typedef struct _Cliente {
    TCHAR nome[50];
    TCHAR password[50];
    float saldo;
    CarteiraCliente carteira[5];
    struct _Cliente* prox;
} Cliente;

typedef struct _Dados {
    TCHAR mensagem[2000];
    BOOL validacaoLogin;
    // divisão de comandos
    TCHAR parametro1[50];
    TCHAR parametro2[50];
    TCHAR nomeCliente[50];
    int opcao;
    float saldo;
} Dados;

typedef struct _NamedPipes {
    HANDLE hInstancia;
    BOOL ativo;
    Dados aux;
    OVERLAPPED overlap;
}NamedPipes;

typedef struct {
    NamedPipes hPipes[NCLIENTES];
    HANDLE hEvents[NCLIENTES];
    HANDLE hMutex;
    HANDLE hThread[NCLIENTES + 1];
    DadosSM cdados;
    NamedPipes* naux;
    Cliente* lista;
    Empresas* lempresa;
    SharedMemory shm;
} ThreadDados;

#define SMBUFSIZE sizeof(SharedMemory)


// Recebe o numero de clientes Registry
int ValorNClientes() {
    HKEY hKey;
    DWORD dwDisposition;
    DWORD nClientes = 10;
    DWORD dataSize = sizeof(DWORD);
    DWORD valueType;

    //Tenta Criar o registo
    LONG result = RegCreateKeyEx(
        HKEY_CURRENT_USER, REGISTRY_NAME, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisposition);

    if (result != ERROR_SUCCESS) {
        return -1;
    }
    //se sucesso indica que cria 
    if (dwDisposition == REG_CREATED_NEW_KEY) {
        result = RegSetValueEx(hKey, ValorRegistry, 0, REG_DWORD, (const BYTE*)&nClientes, sizeof(DWORD));
        if (result != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return -1;
        }
    }
    else if (dwDisposition == REG_OPENED_EXISTING_KEY) {
        result = RegQueryValueEx(hKey, ValorRegistry, NULL,
            &valueType, (LPBYTE)&nClientes, &dataSize);

        if (result != ERROR_SUCCESS || valueType != REG_DWORD) {
            RegCloseKey(hKey);
            return -1;
        }
    }

    RegCloseKey(hKey);
    return nClientes;
}

//Inicializar o Shared Memory
BOOL initMemAndSync(DadosSM* dados) {
    dados->hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedMemory), SHM_NAME);
    if (dados->hMapFile == NULL) {
        _tprintf(TEXT("Error->CreateFileMapping (%d)"), GetLastError());
        return FALSE;
    }

    dados->sMemory = (SharedMemory*)MapViewOfFile(dados->hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemory));
    if (dados->sMemory == NULL) {
        _tprintf(TEXT("Error->MapViewOfFile (%d)"), GetLastError());
        CloseHandle(dados->hMapFile);
        return FALSE;
    }

    dados->shMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
    if (dados->shMutex == NULL) {
        _tprintf(TEXT("Error-> CreateMutex (%d)"), GetLastError());
        UnmapViewOfFile(dados->sMemory);
        CloseHandle(dados->hMapFile);
        return FALSE;
    }

    dados->shEvento = CreateEvent(NULL, TRUE, FALSE, EVENT_NAME);
    if (dados->shEvento == NULL) {
        _tprintf(TEXT("Error-> CreateEvent (%d)"), GetLastError());
        UnmapViewOfFile(dados->sMemory);
        CloseHandle(dados->hMapFile);
        CloseHandle(dados->shMutex);
        return FALSE;
    }

    return TRUE;
}

//Mostrar a lista de empresas
void EmpresasList(SharedMemory* shm) {
    _tprintf(_T("\n\nLista das Empresas: \n"));
    for (int i = 0; i < shm->posEmpresa; i++)
        _tprintf(_T("Nome da empresa: %s Número de ações: %d Preço da ação: %0.2f.\n"), shm->empresas[i].nome, shm->empresas[i].numAcoes, shm->empresas[i].precoAcao);
    _tprintf(_T("\n------------------------------\n"));
}

void Ordena(SharedMemory* shm) {
    for (int i = shm->posEmpresa - 1; i > 0 && shm->empresas[i].precoAcao > shm->empresas[i - 1].precoAcao; i--) {

        Empresas temp = shm->empresas[i];
        shm->empresas[i] = shm->empresas[i - 1];
        shm->empresas[i - 1] = temp;
    }
}
//Atualiza sharedMemory
void EnviaSharedMemory(DadosSM* pcd, SharedMemory shEmpresas) {
    Ordena(&shEmpresas);
    WaitForSingleObject(pcd->shMutex, INFINITE);
    CopyMemory(pcd->sMemory, &shEmpresas, sizeof(SharedMemory));
    ReleaseMutex(pcd->shMutex);
    SetEvent(pcd->shEvento);

}

//Adiciona às empresas
void AdicionaEmpresasSH(SharedMemory* shm, TCHAR nome[50], int numacoes, float precoacao) {
    wcscpy_s(shm->empresas[shm->posEmpresa].nome, 50, nome);
    shm->empresas[shm->posEmpresa].numAcoes = numacoes;
    shm->empresas[shm->posEmpresa].precoAcao = precoacao;
    shm->posEmpresa++;
    wcscpy_s(shm->UltimaTransacao, sizeof(_T("Ainda não existe")), _T("Ainda não existe"));

    // Ordena as empresas pelo número de ações em ordem decrescente.
    for (int i = shm->posEmpresa - 1; i > 0 && shm->empresas[i].precoAcao > shm->empresas[i - 1].precoAcao; i--) {

        Empresas temp = shm->empresas[i];
        shm->empresas[i] = shm->empresas[i - 1];
        shm->empresas[i - 1] = temp;
    }
}

//Lê o ficheiro empresa
void ReadFileEmpresa(SharedMemory* shm) {
    FILE* f;
    _tfopen_s(&f, FILE_EMPRESA, _T("r"));
    TCHAR linha[100], nome[100];
    int numEmpresa = 0, numacao;
    float preco;

    if (f == NULL) {
        _tprintf(_T("[ERRO] Ao abrir o arquivo %s\n"), FILE_EMPRESA);
        return;
    }
    while (_fgetts(linha, sizeof(linha), f) != NULL) {
        if (numEmpresa == NEMPRESAS) {
            _tprintf(_T("[SERVIDOR] Número máximo de empresas alcançado\n"));
            fclose(f);
            break;
        }
        _stscanf_s(linha, _T("%s %d %f"), nome, _countof(nome), &numacao, &preco);
        AdicionaEmpresasSH(shm, nome, numacao, preco);

        numEmpresa++;
    }
    fclose(f);
}

//Envia mensagens ao cliente [Named Pipes]
void EnviarMensagensCliente(ThreadDados* TDaux) {
    DWORD n;

    WaitForSingleObject(TDaux->hMutex, INFINITE);

    if (TDaux->hPipes[0].ativo) {
        if (!WriteFile(TDaux->hPipes[0].hInstancia, &TDaux->hPipes[0].aux, sizeof(Dados), &n, NULL)) {
            _tprintf(TEXT("[ERRO] Falha ao escrever no pipe! \n"));
            CloseHandle(TDaux->hPipes[0].hInstancia);
            exit(-1);
        }
    }
    memset(&TDaux->hPipes[0].aux.mensagem, 0, sizeof(TDaux->hPipes[0].aux.mensagem));
    memset(&TDaux->hPipes[0].aux.opcao, 6, sizeof(TDaux->hPipes[0].aux.opcao));
    memset(&TDaux->hPipes[0].aux.parametro1, 0, sizeof(TDaux->hPipes[0].aux.parametro1));
    memset(&TDaux->hPipes[0].aux.parametro2, 0, sizeof(TDaux->hPipes[0].aux.parametro2));
    ReleaseMutex(TDaux->hMutex);
}

//Cliente Saiu
void ClienteSaiu(ThreadDados* TDaux, int i) {
    //Fechar o pipe
    _tprintf(_T("O cliente %d saiu.\n"), i);
    TDaux->hPipes[i].ativo = FALSE;
    if (!DisconnectNamedPipe(TDaux->hPipes[i].hInstancia)) {
        _tprintf(TEXT("[ERRO] Desligar o pipe! (DisconnectNamedPipe)"));
        exit(-1);
    }
    CloseHandle(TDaux->hPipes[i].hInstancia);
    CloseHandle(TDaux->hThread[i]);
    CloseHandle(TDaux->hPipes[i].overlap.hEvent);
    TDaux->hPipes[i].overlap.hEvent = NULL;
    //limpar dados do cliente
    memset(&TDaux->hPipes[i].aux, 0, sizeof(TDaux->hPipes[i].aux));
    memset(&TDaux->hPipes[i].overlap, 0, sizeof(TDaux->hPipes[i].overlap));

    //Recuperar o overlap
    TDaux->hPipes[i].overlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (TDaux->hPipes[i].overlap.hEvent == NULL) {
        _tprintf(TEXT("[ERRO] Falha ao criar o evento de sobreposição.\n"));
        exit(-1);
    }
    //Voltar a abrir o pipe

    TDaux->hPipes[i].hInstancia = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, NCLIENTES, sizeof(TCHAR) * 256, sizeof(TCHAR) * 256, 1000, NULL);
    if (TDaux->hPipes[i].hInstancia == INVALID_HANDLE_VALUE) {
        _tprintf(TEXT("[ERRO] Falha ao criar o pipe '%s'! \n"), PIPE_NAME);
        exit(-1);
    }
    _tprintf(TEXT("[SERVIDOR] Pipe criado com sucesso...\n"));

    if (!ConnectNamedPipe(TDaux->hPipes[i].hInstancia, &TDaux->hPipes[i].overlap)) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING && err != ERROR_PIPE_CONNECTED) {
            _tprintf(TEXT("[ERRO] Ligação ao leitor! (ConnectNamedPipe), erro: %d\n"), err);
            exit(-1);
        }
    }
    TDaux->hPipes[i].ativo = TRUE;

}

//Fecha Servidor
void ServidorSaiu(ThreadDados* Tdaux) {
    _tprintf(_T("A mandar mensagem aos clientes...\n"));
    HANDLE hEventoSair = OpenEvent(EVENT_MODIFY_STATE, FALSE, EVENTO_SAIR);
    if (hEventoSair == NULL) {
        _tprintf(TEXT("[ERRO] Falha ao abrir o evento para sinalizar a saída dos clientes.\n"));
        return;
    }
    SetEvent(hEventoSair);
    EnviaSharedMemory(&Tdaux->cdados, Tdaux->shm);

    for (int i = 0; i < NCLIENTES; i++) {
        if (Tdaux->hPipes[i].ativo) {
            wcscpy_s(Tdaux->hPipes[i].aux.mensagem, 2000, _T("O servidor vai fechar\n Adeus cliente\n\n"));
            DWORD n;
            if (!WriteFile(Tdaux->hPipes[i].hInstancia, &Tdaux->hPipes[i].aux, sizeof(Dados), &n, NULL)) {
                _tprintf(TEXT("[ERRO] Falha ao enviar mensagem ao cliente.\n"));
            }
        }
    }
    Sleep(5000);

    for (int i = 0; i < NCLIENTES; i++) {
        SetEvent(Tdaux->hEvents[i + 1]);
    }
    SetEvent(Tdaux->hEvents[0]);

    WaitForMultipleObjects(NCLIENTES + 1, Tdaux->hThread, TRUE, INFINITE);

    for (int i = 0; i < NCLIENTES; i++) {
        if (Tdaux->hPipes[i].ativo) {
            if (!DisconnectNamedPipe(Tdaux->hPipes[i].hInstancia)) {
                _tprintf(TEXT("[ERRO] Falha ao desligar o pipe! (DisconnectNamedPipe)\n"));
            }
            CloseHandle(Tdaux->hPipes[i].hInstancia);
        }
        CloseHandle(Tdaux->hPipes[i].overlap.hEvent);
        Tdaux->hPipes[i].ativo = FALSE;
        if (!DisconnectNamedPipe(Tdaux->hPipes[i].hInstancia)) {
            _tprintf(TEXT("[ERRO] Desligar o pipe! (DisconnectNamedPipe)"));
            exit(-1);
        }
        CloseHandle(Tdaux->hPipes[i].hInstancia);
        CloseHandle(Tdaux->hThread[i]);

    }
    CloseHandle(Tdaux->hEvents);

    UnmapViewOfFile(Tdaux->cdados.sMemory);
    CloseHandle(Tdaux->cdados.hMapFile);
    CloseHandle(Tdaux->cdados.shMutex);
    CloseHandle(Tdaux->cdados.shEvento);
    CloseHandle(Tdaux->hThread[NCLIENTES + 1]);
    CloseHandle(hEventoSair);
    free(Tdaux->lista);
    _tprintf(_T("Servidor encerrado com sucesso.\n"));
    ExitProcess(0);
}

//Função de variação de compra e venda
float VariarPreco(float preco, int numacoes, BOOL aumentou) {
    srand(time(NULL));
    float percentagem;

    if (numacoes > 100)
        //faz uma percentagem random de 0.4 a 0.6
        percentagem = 0.4 + ((float)rand() / RAND_MAX) * (0.6 - 0.4);
    else
        //a percentagem random de 0.05 a 0.39
        percentagem = 0.05 + ((float)rand() / RAND_MAX) * (0.39 - 0.05);

    if (aumentou)
        preco += preco * percentagem;
    else
        preco -= preco * percentagem;

    return preco;
}

//Trata os comandos do cliente
void ValidarComandosCliente(ThreadDados* TDaux) {
    Cliente* atual = TDaux->lista;
    BOOL encontrado = FALSE, encontrei = FALSE;
    TCHAR msg[900];

    switch (TDaux->hPipes[0].aux.opcao) {
    case 0:
        //o cliente vai sair, obter o processo do cliente para o tirar da estrutura
        wcscpy_s(TDaux->hPipes[0].aux.mensagem, sizeof(_T("Adeus cliente\n\n")), _T("Adeus cliente\n\n"));
        EnviarMensagensCliente(TDaux);
        ClienteSaiu(TDaux, 0);
        break;
    case 1:
        //Login
        while (atual != NULL) {
            if (_tcscmp(atual->nome, TDaux->hPipes[0].aux.parametro1) == 0 && _tcscmp(atual->password, TDaux->hPipes[0].aux.parametro2) == 0) {
                TDaux->hPipes[0].aux.validacaoLogin = TRUE;
                _tprintf(TEXT("Bem vindo!\n"));
                wcscpy_s(TDaux->hPipes[0].aux.mensagem, sizeof(_T("Bem-vindo, cliente!\n")), _T("Bem-vindo, cliente!\n"));
                wcscpy_s(TDaux->hPipes[0].aux.nomeCliente, 50, atual->nome);
                encontrado = TRUE;
                break;
            }
            atual = atual->prox;
        }
        if (!encontrado) {
            _tprintf(TEXT("[ERRO] Login falhou!\n"));
            wcscpy_s(TDaux->hPipes[0].aux.mensagem, sizeof(_T("Nome de utilizador ou senha incorretos!\n")), _T("Nome de utilizador ou senha incorretos!\n"));
        }

        break;
    case 2:
        //Enviar a lista de cenas
        _tcscpy_s(&TDaux->hPipes[0].aux.mensagem, sizeof(_T("Lista de empresas:\n\n")), _T("Lista de empresas:\n\n"));
        for (int i = 0; i < TDaux->cdados.sMemory->posEmpresa; i++) {
            _stprintf_s(msg, 25, TEXT("%s %i %.2f \n"), TDaux->cdados.sMemory->empresas[i].nome, TDaux->cdados.sMemory->empresas[i].numAcoes, TDaux->cdados.sMemory->empresas[i].precoAcao);
            _tcscat_s(&TDaux->hPipes[0].aux.mensagem, sizeof(msg), msg);
        }

        break;

    case 3:
        //comprar acoes
        //calcular se o cliente tem saldo para comprar
        //se sim compra e varia o preco e envia mensagem
        //se não mantem o preco e envia mensagem
        if (tempo) {
            for (int i = 0; i < TDaux->shm.posEmpresa && !encontrei; i++) {
                if (_tcscmp(TDaux->shm.empresas[i].nome, TDaux->hPipes[0].aux.parametro1) == 0) {
                    while (atual != NULL && !encontrei) {
                        if (_tcscmp(atual->nome, TDaux->hPipes[0].aux.nomeCliente) == 0) {
                            int nacoescliente = _ttoi(TDaux->hPipes[0].aux.parametro2);
                            float precofinal = TDaux->shm.empresas[i].precoAcao * nacoescliente;
                            _tprintf(_T("[SERVIDOR] Cliente '%s' quer comprar %d ações por %f\n"), atual->nome, nacoescliente, precofinal);

                            if (precofinal <= atual->saldo) {
                                atual->saldo -= precofinal;
                                int j;
                                TDaux->lista = atual;

                                for (j = 0; j < 5; j++) {
                                    if (_tcslen(TDaux->lista->carteira[j].nomeEmpresa) == 0) {
                                        wcscpy_s(TDaux->lista->carteira[j].nomeEmpresa, 50, TDaux->cdados.sMemory->empresas[i].nome);
                                        TDaux->lista->carteira[j].nAcoes = nacoescliente;
                                        break;
                                    }
                                    else if (_tcscmp(TDaux->lista->carteira[j].nomeEmpresa, TDaux->cdados.sMemory->empresas[i].nome) == 0) {
                                        TDaux->lista->carteira[j].nAcoes += nacoescliente;
                                        break;
                                    }
                                }
                                _tprintf(_T("[SERVIDOR] Nome da empresa comprada: %s, Total de ações: %d, Saldo do cliente: %f\n"), TDaux->lista->carteira[j].nomeEmpresa, TDaux->lista->carteira[j].nAcoes, TDaux->lista->saldo);
                                // Atualizar alterações nas empresas
                                TDaux->shm.empresas[i].precoAcao = VariarPreco(TDaux->shm.empresas[i].precoAcao, nacoescliente, TRUE);

                                _stprintf_s(TDaux->shm.UltimaTransacao, 100, _T("Nome da empresa comprada: %s, Total de ações: %d, PrecoAcao: %0.2f\n"), TDaux->lista->carteira[j].nomeEmpresa, TDaux->lista->carteira[j].nAcoes, TDaux->shm.empresas[i].precoAcao);
                                TDaux->shm.empresas[i].numAcoes -= nacoescliente;
                                EnviaSharedMemory(&TDaux->cdados, TDaux->shm);

                                TDaux->hPipes[0].aux.saldo = atual->saldo;
                                wcscpy_s(TDaux->hPipes[0].aux.mensagem, sizeof(_T("Compra feita com sucesso!\n")), _T("Compra feita com sucesso!\n"));
                                encontrei = TRUE;
                                break;
                            }
                            else
                                wcscpy_s(TDaux->hPipes[0].aux.mensagem, sizeof(_T("Saldo insuficiente!\n")), _T("Saldo insuficiente!\n"));
                        }
                        atual = atual->prox;
                    }
                }
            }
        }
        else
            wcscpy_s(TDaux->hPipes[0].aux.mensagem, sizeof(_T("As vendas e compras encontram-se em pausa!\n")), _T("As vendas e compras encontram-se em pausa!\n"));

        break;
    case 4:
        //+/- mesma logica de comprar ao contrario
        if (tempo) {
            for (int i = 0; i < TDaux->shm.posEmpresa && !encontrado; i++) {
                if (_tcscmp(TDaux->shm.empresas[i].nome, TDaux->hPipes[0].aux.parametro1) == 0) {
                    while (atual != NULL && !encontrado) {
                        if (_tcscmp(atual->nome, TDaux->hPipes[0].aux.nomeCliente) == 0) {
                            int j;
                            int nacoescliente = _ttoi(TDaux->hPipes[0].aux.parametro2);
                            float precofinal = TDaux->shm.empresas[i].precoAcao * nacoescliente;
                            TDaux->lista = atual;
                            _tprintf(_T("[SERVIDOR] Cliente '%s' quer vender %d ações total de: %f\n"), atual->nome, nacoescliente, precofinal);

                            for (j = 0; j < 5; j++) {
                                if (_tcscmp(TDaux->lista->carteira[j].nomeEmpresa, TDaux->cdados.sMemory->empresas[i].nome) == 0) {
                                    if (TDaux->lista->carteira[j].nAcoes >= nacoescliente) {
                                        TDaux->lista->carteira[j].nAcoes -= nacoescliente;
                                        if (TDaux->lista->carteira[j].nAcoes == 0)
                                            wcscpy_s(TDaux->lista->carteira[i].nomeEmpresa, 1, _T(""));

                                        TDaux->lista->saldo += precofinal;
                                        _tprintf(_T("[SERVIDOR] Nome da empresa vendida: %s, Total de ações: %d, Saldo do cliente: %f\n"), TDaux->lista->carteira[j].nomeEmpresa, TDaux->lista->carteira[j].nAcoes, TDaux->lista->saldo);

                                        // Atualizar alterações nas empresas
                                        TDaux->shm.empresas[i].precoAcao = VariarPreco(TDaux->shm.empresas[i].precoAcao, nacoescliente, FALSE);

                                        _stprintf_s(TDaux->shm.UltimaTransacao, 100, _T("Nome da empresa vendida: %s, Total de ações: %d, PrecoAcao: %0.2f\n"), TDaux->lista->carteira[j].nomeEmpresa, TDaux->lista->carteira[j].nAcoes, TDaux->shm.empresas[i].precoAcao);
                                        EnviaSharedMemory(&TDaux->cdados, TDaux->shm);

                                        TDaux->hPipes[0].aux.saldo = atual->saldo;
                                        wcscpy_s(TDaux->hPipes[0].aux.mensagem, sizeof(_T("Venda feita com sucesso!\n")), _T("Venda feita com sucesso!\n"));

                                        encontrado = TRUE;
                                        break;
                                    }
                                    else {
                                        encontrado = TRUE;
                                        wcscpy_s(TDaux->hPipes[0].aux.mensagem, sizeof(_T("Não tem ações suficientes para vender!\n")), _T("Não tem ações suficientes para vender!\n"));
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    atual = atual->prox;
                }
            }
        }
        else
            wcscpy_s(TDaux->hPipes[0].aux.mensagem, sizeof(_T("As vendas e compras encontram-se em pausa!\n")), _T("As vendas e compras encontram-se em pausa!\n"));
        break;
    case 5:
        //mostrar o saldo do cliente
        while (atual != NULL) {
            if (_tcscmp(atual->nome, TDaux->hPipes[0].aux.nomeCliente) == 0) {
                wcscpy_s(TDaux->hPipes[0].aux.mensagem, sizeof(_T(" ")), _T(" "));
                TDaux->hPipes[0].aux.saldo = atual->saldo;
                break;
            }
            atual = atual->prox;
        }


        break;
    default:
        // comando invalido
        wcscpy_s(TDaux->hPipes[0].aux.mensagem, 256, _T("Comando Inválido"));
        break;
    }

    EnviarMensagensCliente(TDaux);

}

//Recebe Mensagens do cliente [Named Pipes]
DWORD WINAPI LerMensagensCliente(LPVOID param) {
    ThreadDados* TDaux = (ThreadDados*)param;
    DWORD n;

    while (1) {
        WaitForSingleObject(TDaux->hMutex, INFINITE);
        if (TDaux->hPipes[0].ativo) {
            if (!ReadFile(TDaux->hPipes[0].hInstancia, &TDaux->hPipes[0].aux, sizeof(Dados), &n, NULL)) {
                _tprintf(TEXT("[ERRO] Ler ao pipe!\n"));
                exit(-1);
            }
            _tprintf(TEXT("[SERVIDOR] Mensagem recebida do cliente: "));
            /* _tprintf(TEXT("   Parâmetro 1: %s "), TDaux->hPipes[0].aux.parametro1);
             _tprintf(TEXT("   Parâmetro 2: %s "), TDaux->hPipes[0].aux.parametro2);*/
            _tprintf(TEXT("   Opção: %d\n"), TDaux->hPipes[0].aux.opcao);
            ValidarComandosCliente(TDaux);
        }
        ReleaseMutex(TDaux->hMutex);
    }

    return 0;
}

//Mostrar a lista clientes
void ClientList(Cliente* lista) {
    Cliente* atual = lista;

    _tprintf(_T("\n\nLista dos Clientes: \n"));
    while (atual != NULL) {
        _tprintf(_T("\nNome: %s Password: %s Saldo: %0.2f"), atual->nome, atual->password, atual->saldo);
        atual = atual->prox;
    }
    _tprintf(_T("\n\n"));
}

//adidicona à lista os clientes
Cliente* adicionaCliente(Cliente* lista, TCHAR nome[50], TCHAR password[50], float saldo) {

    Cliente* novo = (Cliente*)malloc(sizeof(Cliente));
    if (novo != NULL) {
        wcscpy_s(novo->nome, 50, nome);
        wcscpy_s(novo->password, 50, password);
        novo->saldo = saldo;
        for (int i = 0; i < 5; i++)
            wcscpy_s(novo->carteira[i].nomeEmpresa, 1, _T(""));
        novo->prox = lista;
        lista = novo;
    }
    return lista;
}

//Lê o ficheiro cliente com file mapping, não era preciso 
Cliente* ReadFileCliente(Cliente* atual, char nomefich[20]) {
    HANDLE file, filemap;
    char* fileview;
    DWORD fileSize;
    TCHAR nome[50], password[50];
    char aux[7];
    float saldo;
    int i = 0, j, l;
    _tprintf(_T("%s\n"), nomefich);

    file = CreateFile(nomefich, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        _tprintf(_T("Erro de CreateFile(%d)\n"), GetLastError());
        return NULL;
    }
    fileSize = GetFileSize(file, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        _tprintf(_T("Erro ao obter o tamanho do arquivo (%d)\n"), GetLastError());
        CloseHandle(file);
        return NULL;
    }
    //_tprintf(_T("%i\n"), fileSize);
    filemap = CreateFileMapping(file, NULL, PAGE_READWRITE, 0, fileSize, NULL);
    if (filemap == NULL) {
        _tprintf(_T("Erro de criacao de mapping (%s)\n"), GetLastError());
        return NULL;
    }

    fileview = (char*)MapViewOfFile(filemap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, fileSize);
    if (fileview == NULL) {
        CloseHandle(fileview);
        CloseHandle(file);
        _tprintf(_T("Erro de MapViewOfFile(%s)\n"), GetLastError());
        return NULL;
    }

    do {
        for (l = 0; fileview[i] != ' '; l++, i++)
            nome[l] = fileview[i];
        nome[l++] = '\0';
        i++;
        for (j = 0; fileview[i] != ' '; j++, i++)
            password[j] = fileview[i];
        password[j++] = '\0';
        i++;
        for (int k = 0; fileview[i] != ' '; k++, i++)
            aux[k] = fileview[i];
        saldo = atof(aux);

        atual = adicionaCliente(atual, nome, password, saldo);
        i = i + 3;
    } while (i < fileSize);

    UnmapViewOfFile(fileview);
    CloseHandle(filemap);
    CloseHandle(file);
    return atual;
}

DWORD WINAPI espera(LPVOID lpParam) {
    int segundos = *(int*)lpParam;
    tempo = FALSE;
    Sleep(segundos * 1000);
    tempo = TRUE;
    return 0;
}

//Validação dos comandos do servidor
DWORD WINAPI ValidaComandos(LPVOID param) {
    ThreadDados* TDaux = (ThreadDados*)param;
    TCHAR comando[200], comandoAux[10], parametro1[50], parametro2[50], parametro3[50];
    int nacoes;
    float pacao;

    while (!terminou) {
        _tprintf(_T("[SERVIDOR] Introduza o comando: "));
        _fgetts(comando, 100, stdin);
        int numParametros = swscanf_s(comando, L"%s %s %s %s", comandoAux, (unsigned)_countof(comandoAux), parametro1, (unsigned)_countof(parametro1), parametro2, (unsigned)_countof(parametro2), parametro3, (unsigned)_countof(parametro3));

        if (_tcsncmp(comandoAux, _T("listc"), 5) == 0) {
            if (numParametros != 1) {
                _tprintf(_T("Demasiados parâmetros.\n"));
                continue;
            }
            EmpresasList(TDaux->cdados.sMemory);
        }
        else if (_tcsncmp(comandoAux, _T("addc"), 4) == 0) {
            if (numParametros != 4) {
                _tprintf(_T("O comando requer 3 parâmetros: <nome empresa>, <número ações> e <preço ação>.\n"));
                continue;
            }
            nacoes = _ttoi(parametro2);
            pacao = _ttof(parametro3);
            AdicionaEmpresasSH(TDaux->cdados.sMemory, parametro1, nacoes, pacao);
            EnviaSharedMemory(&TDaux->cdados, *TDaux->cdados.sMemory);
        }
        else if (_tcsncmp(comandoAux, _T("stock"), 5) == 0) {
            BOOL stoc = FALSE;
            if (numParametros != 3) {
                _tprintf(_T("O comando requer 2 parâmetros: <nome empresa> e <preço ações>.\n"));
                continue;
            }
            for (int i = 0; i < TDaux->cdados.sMemory->posEmpresa; i++) {
                if (_tcsncmp(TDaux->cdados.sMemory->empresas[i].nome, parametro1, sizeof(parametro1)) == 0) {
                    TDaux->cdados.sMemory->empresas[i].precoAcao = _ttof(parametro2);
                    EnviaSharedMemory(&TDaux->cdados, *TDaux->cdados.sMemory);
                    stoc = TRUE;
                    _stprintf_s(TDaux->hPipes[0].aux.mensagem, 500, _T("[SERVIDOR] Preço da empresa %s alterado para %0.2f\n"), TDaux->cdados.sMemory->empresas[i].nome, TDaux->cdados.sMemory->empresas[i].precoAcao);
                    _tprintf(_T("[SERVIDOR] Preço da empresa %s alterada com sucesso\n"), TDaux->cdados.sMemory->empresas[i].nome);
                    break;
                }
            }
            if (!stoc)
                _tprintf(_T("[SERVIDOR] Essa empresa não existe!"));
        }
        else if (_tcsncmp(comandoAux, _T("users"), 5) == 0) {
            if (numParametros != 1) {
                _tprintf(_T("Demasiados parâmetros.\n"));
                continue;
            }
            ClientList(TDaux->lista);
        }
        else if (_tcsncmp(comandoAux, _T("pause"), 5) == 0) {
            if (numParametros != 2) {
                _tprintf(_T("O comando requer 1 parâmetros: <número segundos> .\n"));
                continue;
            }
            int s = _ttoi(parametro1);
            HANDLE hTime = CreateThread(NULL, 0, espera, &s, 0, NULL);
            _tprintf(_T("Espere %d segundos.\n"), s);
        }
        else if (_tcsncmp(comandoAux, _T("close"), 5) == 0) {
            if (numParametros != 1) {
                _tprintf(_T("Comando exit não requer parâmetros adicionais.\n"));
                continue;
            }
            ServidorSaiu(TDaux);
            _tprintf(_T("O servidor vai encerrar...\n"));
            terminou = true;
        }
        else
            _tprintf(_T("Comando inválido.\n"));
    }
}

int _tmain(int argc, LPTSTR argv[]) {
    ThreadDados TDaux;
    DadosSM cdados;
    Cliente* atual = NULL;
    SharedMemory shm;

    HANDLE hPipe, hEventTemp;
    int numclientes = 0, i, nBytes = 0;
    DWORD nc;
    shm.posEmpresa = 0;
    TCHAR nomeFichero[20];

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    //Verificar argumentos 
    //Ler Ficheiro Cliente
    if (argc == 2)
        _tcscpy_s(nomeFichero, 20, argv[1]);
    else {
        _tprintf(_T("Falta de argumentos\n"));
        exit(1);
    }
    atual = ReadFileCliente(atual, nomeFichero);
    TDaux.lista = atual;
    ClientList(atual);

    // Registry
    nc = ValorNClientes();
    if (nc == -1) {
        nc = NCLIENTES;
    }

    //Ler Ficheiro Empresas    
    ReadFileEmpresa(&shm);
    //EmpresasList(&shm);

    //Shared Memory 
    if (!initMemAndSync(&cdados)) {
        exit(1);
    }
    EnviaSharedMemory(&cdados, shm);
    TDaux.shm = shm;
    TDaux.cdados = cdados;

    //Lança a board
    //STARTUPINFO si;
    //PROCESS_INFORMATION pi;
    //ZeroMemory(&si, sizeof(STARTUPINFO));
    //ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    //si.cb = sizeof(STARTUPINFO);
    //if (!CreateProcess(NULL, _T("board.exe"), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
    //    _tprintf(_T("[ERRO] a Lançar a board"), GetLastError());
    //    exit(-1);
    //}
    //else {
    //    _tprintf(_T("Board lamçada com sucesso\n"));
    //    CloseHandle(pi.hProcess);
    //    CloseHandle(pi.hThread);
    //}

    TDaux.hMutex = CreateMutex(NULL, FALSE, NULL);
    if (TDaux.hMutex == NULL) {
        _tprintf(TEXT("[ERRO] Criar Mutex! "));
        exit(-1);
    }

    //f6 ex7
    for (i = 0; i < nc; i++) {
        hEventTemp = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (hEventTemp == NULL) {
            _tprintf(TEXT("[ERRO] Criar Evento! (CreateEvent)"));
            exit(-1);
        }

        hPipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, NCLIENTES, sizeof(TCHAR) * 256, sizeof(TCHAR) * 256, 1000, NULL);
        if (hPipe == INVALID_HANDLE_VALUE) {
            _tprintf(TEXT("[ERRO] Falha ao criar o pipe '%s'! \n"), PIPE_NAME);
            exit(-1);
        }
        _tprintf(TEXT("[SERVIDOR] Pipe criado com sucesso...\n"));
        ZeroMemory(&TDaux.hPipes[i].overlap, sizeof(TDaux.hPipes[i].overlap));
        TDaux.hPipes[i].hInstancia = hPipe;
        TDaux.hPipes[i].overlap.hEvent = hEventTemp;
        TDaux.hPipes[i].ativo = FALSE;
        TDaux.hEvents[i] = hEventTemp;

        _tprintf(TEXT("[SERVIDOR] Esperando por um cliente... \n"));
        if (!ConnectNamedPipe(TDaux.hPipes[i].hInstancia, &TDaux.hPipes[i].overlap)) {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING && err != ERROR_PIPE_CONNECTED) {
                _tprintf(TEXT("[ERRO] Ligação ao leitor! (ConnectNamedPipe), erro: %d\n"), err);
                exit(-1);
            }
        }
        //Inicializar as threads LerMensagensCliente
        //LerMensagensCliente encaminha para a validação do login e outros pedidos do cliente
        TDaux.hThread[i] = CreateThread(NULL, 0, LerMensagensCliente, &TDaux, 0, NULL);
        if (TDaux.hThread == NULL) {
            _tprintf(TEXT("[ERRO] Criar Thread! (CreateThread)"));
            CloseHandle(TDaux.hMutex);
            exit(-1);
        }
    }

    TDaux.hThread[nc + 1] = CreateThread(NULL, 0, ValidaComandos, &TDaux, 0, NULL);
    if (TDaux.hThread[nc + 1] == NULL) {
        _tprintf(TEXT("[ERRO] Criar Thread! (CreateThread)"));
        CloseHandle(TDaux.hMutex);
        exit(-1);
    }

    while (numclientes < NCLIENTES) {
        _tprintf(TEXT("[ESCRITOR] Aguardando clientes...\n"));
        int offset = WaitForMultipleObjects(NCLIENTES, TDaux.hEvents, FALSE, INFINITE);
        i = offset - WAIT_OBJECT_0;
        if (i >= 0 && i < NCLIENTES) {
            _tprintf(_T("[ESCRITOR] Cliente %d chegou...\n"), i);
            if (GetOverlappedResult(TDaux.hPipes[i].hInstancia, &TDaux.hPipes[i].overlap, &nBytes, FALSE)) {
                ResetEvent(TDaux.hEvents[i]);
                WaitForSingleObject(TDaux.hMutex, INFINITE);
                TDaux.hPipes[i].ativo = TRUE;
                ReleaseMutex(TDaux.hMutex);
            }
            numclientes++;
        }
    }

    WaitForMultipleObjects(nc + 1, TDaux.hThread, TRUE, INFINITE);

    for (i = 0; i < nc; i++) {
        _tprintf(TEXT("[ESCRITOR] Desligar o pipe (DisconnectNamedPipe)\n"));
        if (!DisconnectNamedPipe(TDaux.hPipes[i].hInstancia)) {
            _tprintf(TEXT("[ERRO] Desligar o pipe! (DisconnectNamedPipe)"));
            exit(-1);
        }
        CloseHandle(TDaux.hPipes[i].hInstancia);
        TDaux.hPipes[i].ativo = FALSE;
        CloseHandle(TDaux.hThread[i]);
    }

    UnmapViewOfFile(cdados.sMemory);
    CloseHandle(cdados.hMapFile);
    CloseHandle(cdados.shMutex);
    CloseHandle(cdados.shEvento);
    CloseHandle(TDaux.hThread[NCLIENTES + 1]);
    _tprintf(_T("\n\n"));
    return 0;
}
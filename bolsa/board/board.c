#include <windows.h>
#include <tchar.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h> 
#include <io.h>

#define NEMPRESAS 30
#define EVENTO_SAIR TEXT("EventoSair")

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
    int MaxEmpresa;
    SharedMemory* sMemory;
}DadosSM;

HANDLE hEventoSair;
#define SHM_NAME _T("SHM")
#define EVENT_NAME TEXT("ATEMPRESAS")  
#define SMBUFSIZE sizeof(SharedMemory)
#define MUTEX_NAME _T("MUTEX")

BOOL initMemAndSync(DadosSM* dados) {
    dados->hMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, SHM_NAME);
    if (dados->hMapFile == NULL) {
        _tprintf(TEXT("Erro ao abrir o arquivo de mapeamento (%d)\n"), GetLastError());
        return FALSE;
    }

    dados->sMemory = (SharedMemory*)MapViewOfFile(dados->hMapFile, FILE_MAP_READ, 0, 0, sizeof(SharedMemory));
    if (dados->sMemory == NULL) {
        _tprintf(TEXT("Erro ao visualizar o arquivo de mapeamento (%d)\n"), GetLastError());
        CloseHandle(dados->hMapFile);
        return FALSE;
    }

    dados->shMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
    if (dados->shMutex == NULL) {
        _tprintf(TEXT("Erro ao abrir o mutex (%d)\n"), GetLastError());
        CloseHandle(dados->hMapFile);
        UnmapViewOfFile(dados->sMemory);
        return FALSE;
    }

    dados->shEvento = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_NAME);
    if (dados->shEvento == NULL) {
        _tprintf(TEXT("Erro ao abrir o evento (%d)\n"), GetLastError());
        CloseHandle(dados->shMutex);
        CloseHandle(dados->hMapFile);
        UnmapViewOfFile(dados->sMemory);
        return FALSE;
    }

    return TRUE;
}

void receiveMsg(DadosSM* dados) {
    while (1) {
        if (WaitForSingleObject(hEventoSair, 0) == WAIT_OBJECT_0) {
            _tprintf(TEXT("[BOLSA] O servidor vai fechar.\n"));
            ExitProcess(0);
        }
        SharedMemory shEmpresas;
        WaitForSingleObject(dados->shEvento, INFINITE);
        WaitForSingleObject(dados->shMutex, INFINITE);
        CopyMemory(&shEmpresas, dados->sMemory, sizeof(SharedMemory));
        ReleaseMutex(dados->shMutex);

        if (shEmpresas.posEmpresa > 0 && shEmpresas.posEmpresa <= NEMPRESAS) {

            for (int i = 0; i < shEmpresas.posEmpresa; ++i) {
                _tprintf(_T("Empresa: %s, Número de Ações: %d, Preço da Ação: %0.2f \n"),
                    dados->sMemory->empresas[i].nome,
                    dados->sMemory->empresas[i].numAcoes,
                    dados->sMemory->empresas[i].precoAcao);
            }
            _tprintf(_T("\n--------------------------------------------------------------------------\n"));
            _tprintf(_T("\nÚltima transação:%s\n"), dados->sMemory->UltimaTransacao);
            _tprintf(_T("\n--------------------------------------------------------------------------\n"));


            Sleep(1000);
        }
        else {
            _tprintf(_T("Nenhuma empresa encontrada ou número de empresas inválido.\n"));
        }
        ResetEvent(dados->shEvento);
    }
}

int _tmain(int argc, TCHAR* argv[]) {
    DadosSM cdados;
    SharedMemory shm;

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
#endif 

    //Verificar argumentos
    if (argc == 2)
        cdados.MaxEmpresa = _ttoi(argv[1]);
    else {
        _tprintf(_T("Falta de argumentos\n"));
        exit(1);
    }

    hEventoSair = CreateEvent(NULL, TRUE, FALSE, EVENTO_SAIR);
    if (!initMemAndSync(&cdados)) {
        exit(1);
    }
    receiveMsg(&cdados);

    UnmapViewOfFile(cdados.sMemory);
    CloseHandle(cdados.hMapFile);
    CloseHandle(cdados.shMutex);
    CloseHandle(cdados.shEvento);
    return 0;
}
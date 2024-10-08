#include <windows.h>
#include <tchar.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h> 
#include <io.h>
#include <stdbool.h>

#define PIPE_NAME TEXT("\\\\.\\pipe\\cliente")
#define EVENTO_SAIR TEXT("EventoSair")
bool terminou = false;

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
    HANDLE hPipe;
    HANDLE hMutex;
    HANDLE hThread[3];
    HANDLE hEventoSair;
    Dados aux;
    OVERLAPPED overlapped;
    HANDLE hEventm;
    HANDLE hEventr;
}NamedPipes;

void Sair(NamedPipes* naux) {
    terminou = true;
    Sleep(100);

    ZeroMemory(&naux->overlapped, sizeof(naux->overlapped));
    ZeroMemory(&naux->aux, sizeof(naux->aux));
    CloseHandle(naux->hPipe);
    CloseHandle(naux->hMutex);
    CloseHandle(naux->hThread[0]);
    CloseHandle(naux->hThread[1]);
    CloseHandle(naux->hThread[2]);
    CloseHandle(naux->hEventm);
    CloseHandle(naux->hEventr);
    CloseHandle(naux->overlapped.hEvent);
    CloseHandle(naux->hEventoSair);
    ExitProcess(0);
}

DWORD WINAPI EnviaMensagens(LPVOID lpParam) {
    NamedPipes* naux = (NamedPipes*)lpParam;
    DWORD n;
    BOOL ret;
    DWORD waitr;
    HANDLE hEventw = CreateEvent(NULL, TRUE, FALSE, NULL);
    naux->overlapped.hEvent = hEventw;

    while (!terminou) {
        if (WaitForSingleObject(naux->hEventm, INFINITE) != WAIT_OBJECT_0) {
            _tprintf(TEXT("[ERRO] Falha ao esperar por evento!\n"));
            CloseHandle(hEventw);
            return -1;
        }

        ZeroMemory(&naux->overlapped, sizeof(naux->overlapped));
        ResetEvent(naux->overlapped.hEvent);
        naux->overlapped.hEvent = hEventw;

        ret = WriteFile(naux->hPipe, &naux->aux, sizeof(Dados), &n, &naux->overlapped);
        if (!ret && GetLastError() != ERROR_IO_PENDING) {
            _tprintf(TEXT("[ERRO] Falha ao escrever no pipe! (WriteFile)\n"));
            CloseHandle(naux->hPipe);
            exit(-1);
        }

        waitr = WaitForSingleObject(naux->overlapped.hEvent, INFINITE);
        if (waitr != WAIT_OBJECT_0) {
            _tprintf(TEXT("[ERRO] Falha ao esperar no overlapped!\n"));
            CloseHandle(naux->hPipe);
            CloseHandle(hEventw);
            return -1;
        }
        if (!GetOverlappedResult(naux->hPipe, &naux->overlapped, &n, TRUE)) {
            _tprintf(TEXT("[ERRO]W Falha ao obter resultado de operação overlapped! (GetOverlappedResult)\n"));
            CloseHandle(naux->hPipe);
            CloseHandle(hEventw);
            return -1;
        }

        ResetEvent(naux->hEventm);
        ZeroMemory(&naux->aux.mensagem, sizeof(naux->aux.mensagem));
    }
    return 0;
}

DWORD WINAPI RecebeMensagens(LPVOID lpParam) {
    NamedPipes* naux = (NamedPipes*)lpParam;
    DWORD n;
    BOOL ret;
    DWORD waitr;
    naux->hEventr = CreateEvent(NULL, TRUE, FALSE, NULL);
    naux->overlapped.hEvent = naux->hEventr;

    while (!terminou) {
        ZeroMemory(&naux->overlapped, sizeof(naux->overlapped));
        ResetEvent(naux->overlapped.hEvent);
        naux->overlapped.hEvent = naux->hEventr;

        ret = ReadFile(naux->hPipe, &naux->aux, sizeof(Dados), &n, &naux->overlapped);
        if (!ret && GetLastError() != ERROR_IO_PENDING) {
            _tprintf(TEXT("[ERRO] Falha ao ler o pipe! \n"));
            CloseHandle(naux->hPipe);
            exit(-1);
        }
        waitr = WaitForSingleObject(naux->overlapped.hEvent, INFINITE);
        if (waitr != WAIT_OBJECT_0) {
            _tprintf(TEXT("[ERRO] Falha ao esperar no overlapped!\n"));
            CloseHandle(naux->hPipe);
            CloseHandle(naux->hEventr);
            return -1;
        }
        if (!GetOverlappedResult(naux->hPipe, &naux->overlapped, &n, TRUE)) {
            _tprintf(TEXT("[ERRO]R Falha ao obter resultado de operação overlapped! (GetOverlappedResult)\n"));
            CloseHandle(naux->hPipe);
            CloseHandle(naux->hEventr);
            return -1;
        }

        _tprintf(_T("%s"), naux->aux.mensagem);
        if (WaitForSingleObject(naux->hEventoSair, 0) == WAIT_OBJECT_0) {
            _tprintf(TEXT("[CLIENTE] O servidor vai fechar.\n"));
            Sair(naux);
        }
    }

    return 0;
}

DWORD WINAPI  ValidarComandos(LPVOID lpParam) {
    NamedPipes* naux = (NamedPipes*)lpParam;
    TCHAR comando[256], comandoAux[10];

    while (!naux->aux.validacaoLogin || terminou) {
        _tprintf(_T("[CLIENTE] Introduza o comando: "));
        _fgetts(comando, 100, stdin);
        int numParametros = _stscanf_s(comando, _T("%s %s %s"), comandoAux, _countof(comandoAux), naux->aux.parametro1, _countof(naux->aux.parametro1), naux->aux.parametro2, _countof(naux->aux.parametro2));

        if (_tcsncmp(comandoAux, _T("login"), 5) == 0) {
            if (numParametros != 3) {
                _tprintf(_T("O comando requer 2 parâmetros: <username> e <password>.\n"));
                continue;
            }
            naux->aux.opcao = 1;
            SetEvent(naux->hEventm);
            WaitForSingleObject(naux->hEventr, INFINITE);
        }
        else if (_tcsncmp(comandoAux, _T("exit"), 4) == 0) {
            if (numParametros != 1) {
                _tprintf(_T("Demasiados parâmetros.\n"));
                continue;
            }
            naux->aux.opcao = 0;
            SetEvent(naux->hEventm);
            WaitForSingleObject(naux->hEventr, INFINITE);
            Sair(naux);
        }
        else
            _tprintf(_T("[ERRO] Faça o login primeiro!\n"));
    }
    WaitForSingleObject(naux->hEventr, INFINITE);
    while (!terminou) {
        _tprintf(_T("[CLIENTE] Introduza o comando: "));
        _fgetts(comando, 100, stdin);

        int numParametros = _stscanf_s(comando, _T("%s %s %s"), comandoAux, _countof(comandoAux), naux->aux.parametro1, _countof(naux->aux.parametro1), naux->aux.parametro2, _countof(naux->aux.parametro2));

        if (_tcsncmp(comandoAux, _T("listc"), 5) == 0) {
            if (numParametros != 1) {
                _tprintf(_T("Demasiados parâmetros.\n"));
                continue;
            }
            naux->aux.opcao = 2;
            SetEvent(naux->hEventm);
            WaitForSingleObject(naux->hEventr, INFINITE);
        }
        else if (_tcsncmp(comandoAux, _T("buy"), 3) == 0) {
            if (numParametros != 3) {
                _tprintf(_T("O comando requer 2 parâmetros: <nome empresa> e <número ações>.\n"));
                continue;
            }
            naux->aux.opcao = 3;
            SetEvent(naux->hEventm);
            WaitForSingleObject(naux->hEventr, INFINITE);
        }
        else if (_tcsncmp(comandoAux, _T("sell"), 4) == 0) {
            if (numParametros != 3) {
                _tprintf(_T("O comando requer 2 parâmetros: <nome empresa> e <número ações>.\n"));
                continue;
            }
            naux->aux.opcao = 4;
            SetEvent(naux->hEventm);
            WaitForSingleObject(naux->hEventr, INFINITE);
        }
        else if (_tcsncmp(comandoAux, _T("balance"), 7) == 0) {
            if (numParametros != 1) {
                _tprintf(_T("Demasiados parâmetros.\n"));
                continue;
            }
            naux->aux.opcao = 5;
            SetEvent(naux->hEventm);
            WaitForSingleObject(naux->hEventr, INFINITE);
            _tprintf(_T("%0.2f\n"), naux->aux.saldo);
        }
        else if (_tcsncmp(comandoAux, _T("exit"), 4) == 0) {
            if (numParametros != 1) {
                _tprintf(_T("Demasiados parâmetros.\n"));
                continue;
            }
            naux->aux.opcao = 0;
            SetEvent(naux->hEventm);
            WaitForSingleObject(naux->hEventr, INFINITE);
            Sair(naux);
        }
        else
            _tprintf(_T("Comando inválido.\n"));
    }

    return 0;
}

int _tmain(int argc, TCHAR* argv[]) {
    NamedPipes naux;
    naux.aux.validacaoLogin = false;

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif
    naux.hEventm = CreateEvent(NULL, TRUE, FALSE, NULL);
    _tprintf(TEXT("[CLIENTE] À espera do pipe '%s'\n"), PIPE_NAME);
    if (!WaitNamedPipe(PIPE_NAME, NMPWAIT_WAIT_FOREVER)) {
        _tprintf(TEXT("[ERRO] Falha ao conectar ao pipe '%s'!\n"), PIPE_NAME);
        exit(-1);
    }
    naux.hEventoSair = CreateEvent(NULL, TRUE, FALSE, EVENTO_SAIR);
    naux.hPipe = CreateFile(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (naux.hPipe == INVALID_HANDLE_VALUE) {
        _tprintf(TEXT("[ERRO] Falha ao conectar ao pipe '%s'! \n"), PIPE_NAME);
        exit(-1);
    }
    _tprintf(TEXT("[CLIENTE] Conectado ao pipe.\n"));

    naux.overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (naux.overlapped.hEvent == NULL) {
        _tprintf(TEXT("[ERRO] Falha ao criar evento para operações assíncronas! \n"));
        exit(-1);
    }

    naux.hThread[0] = CreateThread(NULL, 0, ValidarComandos, &naux, 0, NULL);
    if (naux.hThread[0] == NULL) {
        _tprintf(TEXT("[ERRO] Falha ao criar a thread!\n"));
        CloseHandle(naux.hPipe);
        exit(-1);
    }

    naux.hThread[1] = CreateThread(NULL, 0, RecebeMensagens, &naux, 0, NULL);
    if (naux.hThread[1] == NULL) {
        _tprintf(TEXT("[ERRO] Falha ao criar a thread!\n"));
        CloseHandle(naux.hPipe);
        exit(-1);
    }

    naux.hThread[2] = CreateThread(NULL, 0, EnviaMensagens, &naux, 0, NULL);
    if (naux.hThread[2] == NULL) {
        _tprintf(TEXT("[ERRO] Falha ao criar a thread!\n"));
        CloseHandle(naux.hPipe);
        exit(-1);
    }

    WaitForMultipleObjects(3, naux.hThread, TRUE, INFINITE);
    CloseHandle(naux.hThread[0]);
    CloseHandle(naux.hThread[1]);
    CloseHandle(naux.hThread[2]);
    CloseHandle(naux.hPipe);
    return 0;
}
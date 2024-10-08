
#include <windows.h>
#include <tchar.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h> 
#include <io.h>
#include "resource.h"

#define MAXEMPRESAS 100

LRESULT CALLBACK TrataEventos(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK TrataEventosPerso(HWND, UINT, WPARAM, LPARAM);

TCHAR szProgName[] = TEXT("Base");

RECT rectangles[MAXEMPRESAS];
RECT ultimaEmpresaRect;
int valores[MAXEMPRESAS];
int numEmpresas = 10;
int escalaMin = 1;
int escalamax = 500;


//typedef struct _Empresas {
//    TCHAR nome[50];
//    unsigned int numAcoes;
//    float precoAcao;
//}Empresas;
//
//typedef struct _SharedMemory {
//    Empresas empresas[MAXEMPRESAS];
//    int posEmpresa;
//} SharedMemory;
//
//typedef struct _DadosSM {
//    HANDLE hMapFile;
//    HANDLE shMutex;
//    HANDLE shEvento;
//    SharedMemory* sMemory;
//}DadosSM;
//
//#define SHM_NAME _T("SHM")
//#define EVENT_NAME TEXT("ATEMPRESAS")  
//#define SMBUFSIZE sizeof(SharedMemory)
//#define MUTEX_NAME _T("MUTEX")
//
//BOOL initMemAndSync(DadosSM* dados) {
//    dados->hMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, SHM_NAME);
//    if (dados->hMapFile == NULL) {
//        _tprintf(TEXT("Erro ao abrir o arquivo de mapeamento (%d)\n"), GetLastError());
//        return FALSE;
//    }
//
//    dados->sMemory = (SharedMemory*)MapViewOfFile(dados->hMapFile, FILE_MAP_READ, 0, 0, sizeof(SharedMemory));
//    if (dados->sMemory == NULL) {
//        _tprintf(TEXT("Erro ao visualizar o arquivo de mapeamento (%d)\n"), GetLastError());
//        CloseHandle(dados->hMapFile);
//        return FALSE;
//    }
//
//    dados->shMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
//    if (dados->shMutex == NULL) {
//        _tprintf(TEXT("Erro ao abrir o mutex (%d)\n"), GetLastError());
//        CloseHandle(dados->hMapFile);
//        UnmapViewOfFile(dados->sMemory);
//        return FALSE;
//    }
//
//    dados->shEvento = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_NAME);
//    if (dados->shEvento == NULL) {
//        _tprintf(TEXT("Erro ao abrir o evento (%d)\n"), GetLastError());
//        CloseHandle(dados->shMutex);
//        CloseHandle(dados->hMapFile);
//        UnmapViewOfFile(dados->sMemory);
//        return FALSE;
//    }
//
//    return TRUE;
//}
//
//DWORD WINAPI receiveMsg(DadosSM* dados) {
//
//    while (1) {
//        SharedMemory shEmpresas;
//        WaitForSingleObject(dados->shEvento, INFINITE);
//        WaitForSingleObject(dados->shMutex, INFINITE);
//        CopyMemory(&shEmpresas, dados->sMemory, sizeof(SharedMemory));
//        ReleaseMutex(dados->shMutex);
//
//      /*  if (shEmpresas.posEmpresa > 0 && shEmpresas.posEmpresa <= MAXEMPRESAS) {
//
//            for (int i = 0; i < shEmpresas.posEmpresa; ++i) {
//                _tprintf(_T("Empresa: %s, Número de Ações: %d, Preço da Ação: %0.2f \n"),
//                    dados->sMemory->empresas[i].nome,
//                    dados->sMemory->empresas[i].numAcoes,
//                    dados->sMemory->empresas[i].precoAcao);
//            }
//            _tprintf(_T("\n-----------------------------------------------------------\n"));
//            Sleep(1000);
//        }
//        else {
//            _tprintf(_T("Nenhuma empresa encontrada ou número de empresas inválido.\n"));
//        }*/
//        ResetEvent(dados->shEvento);
//    }
//
//    return 0;
//}
//
//int _tmain(int argc, TCHAR* argv[]) {
//    DadosSM cdados;
//    SharedMemory shm;
//
//#ifdef UNICODE
//    _setmode(_fileno(stdin), _O_WTEXT);
//    _setmode(_fileno(stdout), _O_WTEXT);
//#endif 
//
//    //Verificar argumentos
//
//    if (!initMemAndSync(&cdados)) {
//        exit(1);
//    }
//    receiveMsg(&cdados);
//
//    UnmapViewOfFile(cdados.sMemory);
//    CloseHandle(cdados.hMapFile);
//    CloseHandle(cdados.shMutex);
//    CloseHandle(cdados.shEvento);
//    return 0;
//}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {
    HWND hWnd;
    MSG lpMsg;
    WNDCLASSEX wcApp;

    wcApp.cbSize = sizeof(WNDCLASSEX);
    wcApp.hInstance = hInst;
    wcApp.lpszClassName = szProgName;
    wcApp.lpfnWndProc = TrataEventos;
    wcApp.style = CS_HREDRAW | CS_VREDRAW;
    wcApp.hIcon = LoadIcon(hInst, IDI_ICON1);
    wcApp.hIconSm = LoadIcon(hInst, IDI_ICON1);
    wcApp.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcApp.lpszMenuName = NULL;
    wcApp.cbClsExtra = 0;
    wcApp.cbWndExtra = 0;
    wcApp.hbrBackground = CreateSolidBrush(RGB(107, 122, 189));

    if (!RegisterClassEx(&wcApp))
        return 0;

    hWnd = CreateWindow(
        szProgName,
        TEXT("Bolsa de Valores"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1000,
        600,
        (HWND)HWND_DESKTOP,
        (HMENU)NULL,
        (HINSTANCE)hInst,
        0
    );

    if (!hWnd) {
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    while (GetMessage(&lpMsg, NULL, 0, 0)) {
        TranslateMessage(&lpMsg);
        DispatchMessage(&lpMsg);
    }

    return (int)lpMsg.wParam;
}

LRESULT CALLBACK TrataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam) {
    PAINTSTRUCT ps;
    HDC hdc;

    switch (messg) {
    case WM_CREATE: {
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        int windowWidth = clientRect.right - clientRect.left;
        int windowHeight = clientRect.bottom - clientRect.top;

        int buttonWidth = 120;
        int buttonHeight = 25;
        int buttonSpacing = 20;
        int totalWidth = 3 * buttonWidth + 2 * buttonSpacing;

        int xStart = (clientRect.right - totalWidth) / 2;

        CreateWindow(
            TEXT("button"),
            TEXT("Personalização"),
            WS_VISIBLE | WS_CHILD,
            xStart, 20, buttonWidth, buttonHeight, // Alteração na coordenada y
            hWnd,
            (HMENU)1,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
            NULL
        );

        CreateWindow(
            TEXT("button"),
            TEXT("Info"),
            WS_VISIBLE | WS_CHILD,
            xStart + buttonWidth + buttonSpacing, 20, buttonWidth, buttonHeight, // Alteração na coordenada y
            hWnd,
            (HMENU)2,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
            NULL
        );

        CreateWindow(
            TEXT("button"),
            TEXT("Sair"),
            WS_VISIBLE | WS_CHILD,
            xStart + 2 * (buttonWidth + buttonSpacing), 20, buttonWidth, buttonHeight, // Alteração na coordenada y
            hWnd,
            (HMENU)3,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
            NULL
        );

        // Inicializar valores padrão
        for (int i = 0; i < MAXEMPRESAS; i++) {
            valores[i] = rand() % escalamax + escalaMin;
        }

        ultimaEmpresaRect.left = 50;
        ultimaEmpresaRect.top = 120;
        ultimaEmpresaRect.right = ultimaEmpresaRect.left + 200;
        ultimaEmpresaRect.bottom = ultimaEmpresaRect.top + 100;
        break;
    }
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);

        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        // Espaçamento adicional para a linha horizontal
        int horizontalLineRightShift = 50;
        int verticalTextSpacing = 20;

        int windowWidth = clientRect.right - clientRect.left;
        int windowHeight = clientRect.bottom - clientRect.top;

        int barWidth = windowWidth / (2 * numEmpresas);
        int barSpacing = barWidth / 2;

        int xStart = (windowWidth - (numEmpresas * barWidth + (numEmpresas - 1) * barSpacing)) / 2;

        // Criar os retângulos
        for (int i = 0; i < numEmpresas; i++) {
            rectangles[i].left = xStart + (barWidth + barSpacing) * i;
            rectangles[i].top = clientRect.bottom - valores[i] * (windowHeight - 200) / escalamax - 150;
            rectangles[i].right = rectangles[i].left + barWidth;
            rectangles[i].bottom = clientRect.bottom - 150;
        }

        // Desenhar os retângulos
        for (int i = 0; i < numEmpresas; i++) {
            HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 255)); // Azul
            FillRect(hdc, &rectangles[i], hBrush);
            DeleteObject(hBrush);
        }

        //// Desenhar a linha horizontal
        MoveToEx(hdc, rectangles[0].left - 100 + verticalTextSpacing, rectangles[9].bottom, NULL);
        LineTo(hdc, rectangles[numEmpresas - 1].right + 100 + verticalTextSpacing, rectangles[9].bottom);

        //// Desenhar o texto "Nome das empresas"
        RECT textRect1 = { rectangles[0].left - 100, rectangles[9].bottom + 40, rectangles[numEmpresas - 1].right + 100, rectangles[9].bottom + 80 };
        HBRUSH hTextBgBrush = CreateSolidBrush(RGB(107, 122, 189));
        FillRect(hdc, &textRect1, hTextBgBrush);
        DeleteObject(hTextBgBrush);
        SetBkMode(hdc, TRANSPARENT);
        DrawText(hdc, _T("Nome"), -1, &textRect1, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        //// Desenhar a linha vertical
        MoveToEx(hdc, rectangles[0].left, rectangles[0].top, NULL);
        LineTo(hdc, rectangles[0].left, rectangles[0].bottom - 250);

        //// Desenhar o texto "Valor das empresas"
        RECT textRect2 = { rectangles[0].left - 120, rectangles[0].top - 30 - verticalTextSpacing, rectangles[0].left, rectangles[numEmpresas - 1].bottom + verticalTextSpacing };
        DrawText(hdc, _T("Valor"), -1, &textRect2, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        // Desenhar a mensagem "Última Comprada"
        RECT messageRect = { ultimaEmpresaRect.left, ultimaEmpresaRect.top + 350, ultimaEmpresaRect.right, ultimaEmpresaRect.top + 390 };
        DrawText(hdc, _T("Última Comprada"), -1, &messageRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        // Ajustar as coordenadas da última empresa comprada para ao lado do messageRect
        RECT newUltimaEmpresaRect = { messageRect.right, messageRect.top, messageRect.right + 150, messageRect.bottom };

        // Desenhar a última empresa comprada
        HBRUSH hLastCompanyBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &newUltimaEmpresaRect, hLastCompanyBrush);
        DeleteObject(hLastCompanyBrush);

        EndPaint(hWnd, &ps);

        EndPaint(hWnd, &ps);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            DialogBox((HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), MAKEINTRESOURCE(IDD_DIALOG1), hWnd, TrataEventosPerso);
        }
        else if (LOWORD(wParam) == 2) {
            MessageBox(hWnd, _T("Trabalho Realizado por\n Diogo Oliveira - 2021146037\n Lara Bizarro - 2021130066"), _T("Info"), MB_ICONINFORMATION);
        }
        else if (LOWORD(wParam) == 3) {
            if (MessageBox(hWnd, _T("Quer mesmo sair?"), _T("Confirmação"), MB_ICONQUESTION | MB_YESNO) == IDYES)
                DestroyWindow(hWnd);
        }
        break;

    case WM_CLOSE:
        if (MessageBox(hWnd, _T("Quer mesmo sair?"), _T("Confirmação"), MB_ICONQUESTION | MB_YESNO) == IDYES)
            DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, messg, wParam, lParam);
    }
    return 0;
}

BOOL IsNumeric(const TCHAR* str) {
    while (*str) {
        if (!_istdigit(*str)) {
            return FALSE;
        }
        str++;
    }
    return TRUE;
}

INT_PTR CALLBACK TrataEventosPerso(HWND hDlg, UINT messg, WPARAM wParam, LPARAM lParam) {
    switch (messg) {
    case WM_INITDIALOG: {
        RECT rcDlg, rcOwner, rcScreen;
        int posX, posY;

        HWND hOwner = GetParent(hDlg);
        if (hOwner == NULL) {
            hOwner = GetDesktopWindow();
        }

        GetWindowRect(hOwner, &rcOwner);
        GetWindowRect(hDlg, &rcDlg);
        GetWindowRect(GetDesktopWindow(), &rcScreen);

        posX = (rcScreen.right - (rcDlg.right - rcDlg.left)) / 2;
        posY = (rcScreen.bottom - (rcDlg.bottom - rcDlg.top)) / 2;

        SetWindowPos(hDlg, HWND_TOP, posX, posY, 0, 0, SWP_NOSIZE);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        case IDC_BUTTON1: {
            TCHAR buffer[10];

            if (GetDlgItemText(hDlg, IDC_EDIT1, buffer, 10) == 0 ||
                GetDlgItemText(hDlg, IDC_EDIT2, buffer, 10) == 0 ||
                GetDlgItemText(hDlg, IDC_EDIT4, buffer, 10) == 0) {
                MessageBox(hDlg, _T("Por favor, preencha todos os campos."), _T("Erro"), MB_OK | MB_ICONERROR);
                return TRUE;
            }

            GetDlgItemText(hDlg, IDC_EDIT1, buffer, 10);
            if (!IsNumeric(buffer)) {
                MessageBox(hDlg, _T("Por favor, insira apenas números válidos."), _T("Erro"), MB_OK | MB_ICONERROR);
                return TRUE;
            }
            escalaMin = _ttoi(buffer);

            GetDlgItemText(hDlg, IDC_EDIT2, buffer, 10);
            if (!IsNumeric(buffer)) {
                MessageBox(hDlg, _T("Por favor, insira apenas números válidos."), _T("Erro"), MB_OK | MB_ICONERROR);
                return TRUE;
            }
            escalamax = _ttoi(buffer);

            GetDlgItemText(hDlg, IDC_EDIT4, buffer, 10);
            if (!IsNumeric(buffer)) {
                MessageBox(hDlg, _T("Por favor, insira apenas números válidos."), _T("Erro"), MB_OK | MB_ICONERROR);
                return TRUE;
            }
            numEmpresas = _ttoi(buffer);

            if (numEmpresas > 10) {
                MessageBox(hDlg, _T("O número de empresas não pode ser maior que 10."), _T("Erro"), MB_OK | MB_ICONERROR);
                return TRUE;
            }

            // Regerar valores das empresas
            for (int i = 0; i < numEmpresas; i++) {
                valores[i] = rand() % (escalamax - escalaMin + 1) + escalaMin;
            }

            // Redesenhar a janela principal
            InvalidateRect(GetParent(hDlg), NULL, TRUE);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}
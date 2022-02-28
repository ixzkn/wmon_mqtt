#define STRICT
#include <windows.h>
#include <tchar.h>
#include <windowsx.h>
#include <ole2.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <wstring>

using namespace std;

HINSTANCE g_hinst;
HWND g_hwndChild;

bool IsToplevel(HWND win)
{
    if (!IsWindow(win)) return false;
    LONG style = GetWindowLong(win, GWL_STYLE);
    LONG exstyle = GetWindowLong(win, GWL_EXSTYLE);
    style = style & (WS_CHILD | WS_DISABLED);
    exstyle = exstyle & WS_EX_TOOLWINDOW;
    HWND parent = GetParent(win);
    if (parent == 0 && GetWindowTextLength(win) != 0 && style == 0 && exstyle == 0)
    {
        return true;
    }
    return false;
}

wstring GetProcessNameForWindow(HWND window)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    wchar_t string[2048] = {0};
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess)
    {
        GetProcessImageFileName(hProcess, string, 2048);
        return string;
        CloseHandle(hProcess);
    }
    return L"";
}

/*
 *  OnSize
 *      If we have an inner child, resize it to fit.
 */
void OnSize(HWND hwnd, UINT state, int cx, int cy)
{
    if (g_hwndChild) {
        MoveWindow(g_hwndChild, 0, 0, cx, cy, TRUE);
    }
}

/*
 *  OnCreate
 *      Applications will typically override this and maybe even
 *      create a child window.
 */
BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpcs)
{
    g_hwndChild = CreateWindow(TEXT("listbox"), NULL, LBS_HASSTRINGS | WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 0, 0, 0, hwnd, NULL, g_hinst, 0);
    if (!g_hwndChild) return FALSE;
    return TRUE;
}

/*
 *  OnDestroy
 *      Post a quit message because our application is over when the
 *      user closes this window.
 */
void OnDestroy(HWND hwnd)
{
    PostQuitMessage(0);
}

/*
 *  PaintContent
 *      Interesting things will be painted here eventually.
 */
void PaintContent(HWND hwnd, PAINTSTRUCT *pps)
{
}

/*
 *  OnPaint
 *      Paint the content as part of the paint cycle.
 */
void OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    PaintContent(hwnd, &ps);
    EndPaint(hwnd, &ps);
}

/*
 *  OnPrintClient
 *      Paint the content as requested by USER.
 */
void OnPrintClient(HWND hwnd, HDC hdc)
{
    PAINTSTRUCT ps;
    ps.hdc = hdc;
    GetClientRect(hwnd, &ps.rcPaint);
    PaintContent(hwnd, &ps);
}

void CALLBACK WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD dwEventThread,
    DWORD dwmsEventTime )
{
 if (hwnd &&
     idObject == OBJID_WINDOW &&
     idChild == CHILDID_SELF &&
     IsToplevel(hwnd))
 {
  PCTSTR pszAction = NULL;
  switch (event) {
  case EVENT_OBJECT_CREATE:
   pszAction = TEXT("created");
   break;
  case EVENT_OBJECT_DESTROY:
   pszAction = TEXT("destroyed");
   break;
  }
  if (pszAction) {
   _TCHAR szClass[80] = {0};
   _TCHAR szName[80] = {0};
   if (IsWindow(hwnd)) {
    GetClassName(hwnd, szClass, ARRAYSIZE(szClass));
    GetWindowText(hwnd, szName, ARRAYSIZE(szName));
   }
   _TCHAR szBuf[80];
   StringCchPrintf(szBuf, ARRAYSIZE(szBuf), TEXT("%p %s \"%s\" (%s)"), hwnd, pszAction, szName, szClass);
   ListBox_AddString(g_hwndChild, szBuf);
  }
 }
}

/*
 *  Window procedure
 */
LRESULT CALLBACK WndProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uiMsg) {
    HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
    HANDLE_MSG(hwnd, WM_SIZE, OnSize);
    HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
    case WM_PRINTCLIENT: OnPrintClient(hwnd, (HDC)wParam); return 0;
    }
    return DefWindowProc(hwnd, uiMsg, wParam, lParam);
}

BOOL InitApp(void)
{
    WNDCLASS wc;
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = g_hinst;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = TEXT("Scratch");
    if (!RegisterClass(&wc)) return FALSE;
    InitCommonControls();               /* In case we use a common control */
    return TRUE;
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hinstPrev,
                   LPSTR lpCmdLine, int nShowCmd)
{
    MSG msg;
    HWND hwnd;
    g_hinst = hinst;
    if (!InitApp()) return 0;
    if (SUCCEEDED(CoInitialize(NULL))) {/* In case we use COM */
        hwnd = CreateWindow(
            TEXT("Scratch"),                /* Class Name */
            TEXT("Scratch"),                /* Title */
            WS_OVERLAPPEDWINDOW,            /* Style */
            CW_USEDEFAULT, CW_USEDEFAULT,   /* Position */
            CW_USEDEFAULT, CW_USEDEFAULT,   /* Size */
            NULL,                           /* Parent */
            NULL,                           /* No menu */
            hinst,                          /* Instance */
            0);                             /* No special parameters */
        ShowWindow(hwnd, nShowCmd);

        HWINEVENTHOOK hWinEventHook = SetWinEventHook(
             EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY,
             NULL, WinEventProc, 0, 0,
             WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (hWinEventHook) UnhookWinEvent(hWinEventHook);
        CoUninitialize();
    }
    return 0;
}

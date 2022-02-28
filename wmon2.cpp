#define STRICT
#include "mqtt/client.h"
#include <windows.h>
#include <tchar.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <string>
#include <fstream>
#include <list>
#include <psapi.h>
#include <algorithm>
#include "inipp/inipp/inipp.h"

using namespace std;

struct WindowConfig
{
    string title;
    string classname;
    string process;
    string message;
    bool checkFullscreen;
};

HINSTANCE g_hinst;
std::list<WindowConfig> searchList;
ofstream logfile;
string topic;
std::list<HWND> trackedWindows;
mqtt::client *mqtt_client = nullptr;


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

string GetProcessNameForWindow(HWND window)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    char str[2048] = {0};
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess)
    {
        GetProcessImageFileName(hProcess, str, 2048);
        CloseHandle(hProcess);
        return string(str);
    }
    return string();
}

void alert_window_created(DWORD event, HWND hwnd)
{
    logfile << "   MATCHED hwnd=" << hwnd << "\n";
    trackedWindows.push_back(hwnd);
}

void alert_window_destroyed(std::list<HWND>::iterator it)
{
    logfile << "   REMOVED hwnd=" << *it << "\n";
    trackedWindows.erase(it);
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
        if(EVENT_OBJECT_CREATE == event)
        {
            _TCHAR szClass[255] = {0};
            _TCHAR szName[255] = {0};
            GetClassName(hwnd, szClass, ARRAYSIZE(szClass));
            GetWindowText(hwnd, szName, ARRAYSIZE(szName));
            string name(szName);
            string classname(szClass);
            logfile << "Event: class=\"" << classname << "\" title=\"" << name << "\"\n";
            for(auto &cfg : searchList)
            {
                logfile << "  " << cfg.title << "\n";
                if(cfg.classname.length() != 0 && classname.find(cfg.classname) != string::npos)
                {
                    alert_window_created(event, hwnd);
                    break;
                }
                else if(cfg.title.length() != 0 && name.find(cfg.title) != string::npos)
                {
                    alert_window_created(event, hwnd);
                    break;
                }
                // only check process if needed... it can be slower!
                //  recommend finding a way to avoid this
                else if(cfg.process.length() != 0)
                {
                    string procname = GetProcessNameForWindow(hwnd);
                    if(procname.find(cfg.process) != string::npos)
                    {
                        alert_window_created(event, hwnd);
                        break;
                    }
                    break;
                }
            }
        }
        else if(EVENT_OBJECT_DESTROY == event)
        {
            auto it = std::find(trackedWindows.begin(), trackedWindows.end(), hwnd);
            if(it != std::end(trackedWindows))
            {
                alert_window_destroyed(it);
            }
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    if(uiMsg == WM_DESTROY)
    {
        PostQuitMessage(0);
    }
    return DefWindowProc(hwnd, uiMsg, wParam, lParam);
}

bool InitApp(void)
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
    wc.lpszClassName = TEXT("wmon_mqtt");
    if (!RegisterClass(&wc)) return false;
    return true;
}

bool ReadConfig()
{
    inipp::Ini<char> ini;
    std::ifstream is("config.ini");
    ini.parse(is);
    ini.default_section(ini.sections["default"]);
    ini.interpolate();
    for(auto& section : ini.sections)
    {
        if(section.first == "default")
        {
            // general config
            topic = "wmon_mqtt";
            inipp::get_value(section.second, "topic", topic);
            logfile << "MQTT topic: " << topic << "\n";
            string server;
            inipp::get_value(section.second, "server", server);
            logfile << "MQTT server: " << server << "\n";
            string client_id = "wmon_mqtt";
            inipp::get_value(section.second, "client_id", client_id);
            logfile << "MQTT client_id: " << client_id << "\n";
            bool registerHA = false;
            inipp::get_value(section.second, "homeassistant", registerHA);
            if(registerHA) logfile << "Sending HA registration\n";
            mqtt_client = new mqtt::client(server, client_id);
            mqtt::connect_options connOpts;
            connOpts.set_keep_alive_interval(20);
            connOpts.set_clean_session(true);
            mqtt_client->connect(connOpts);
        }
        else
        {
            // per-window config
            WindowConfig data;
            if(section.second.count("title") != 0)
            {
                inipp::get_value(section.second, "title", data.title);
                logfile << "Searching for: title=\"" << data.title << "\"\n";
            }
            else if(section.second.count("class") != 0)
            {
                inipp::get_value(section.second, "class", data.classname);
                logfile << "Searching for: class=\"" << data.classname << "\"\n";
            }
            else
            {  
                logfile << "Invalid section" << section.first << "\n";
                continue;
            }
            searchList.push_back(data);
        }
    }
    return true;
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hinstPrev,
                   LPSTR lpCmdLine, int nShowCmd)
{
    MSG msg;
    HWND hwnd;
    g_hinst = hinst;
    mqtt_client = nullptr;
    logfile.open("log.txt");
    if(!InitApp()) return 0;
    if(!ReadConfig()) return 0;

    hwnd = CreateWindow(
        TEXT("wmon_mqtt"),              /* Class Name */
        TEXT("wmon_mqtt"),              /* Title */
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
    logfile.close();
    if(mqtt_client != nullptr)
    {
        mqtt_client->disconnect();
        delete mqtt_client;
    }
    return 0;
}

#define STRICT
#include "mqtt/async_client.h"
#include <windows.h>
#include <tchar.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <string>
#include <fstream>
#include <list>
#include <psapi.h>
#include <map>
#include <algorithm>
#include <commctrl.h>
#include "inipp/inipp/inipp.h"
#include "resource.h"
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace std;

struct WindowConfig
{
    string title;
    string classname;
    string process;
    string section;
    bool onlyFullscreen;
};

struct WindowTracking
{
    WindowTracking() {}
    WindowTracking(const WindowConfig& cfg) : section(cfg.section), onlyFullscreen(cfg.onlyFullscreen) {}
    string section;
    bool onlyFullscreen;
};

HINSTANCE g_hinst;
std::list<WindowConfig> searchList;
ofstream logfile;
string topic;
typedef std::map<HWND, WindowTracking> WindowTrackingMap;
WindowTrackingMap trackedWindows;
mqtt::async_client *mqtt_client = nullptr;

class __declspec(uuid("b0cd6357-6468-43a0-be76-a9a16e494920")) TrayIcon;
UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;

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

BOOL AddNotificationIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = {sizeof(nid)};
    nid.hWnd = hwnd;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
    nid.guidItem = __uuidof(TrayIcon);
    nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
    LoadIconMetric(g_hinst, MAKEINTRESOURCEW(IDI_NOTIFICATIONICON), LIM_SMALL, &nid.hIcon);
    Shell_NotifyIcon(NIM_ADD, &nid);
    nid.uVersion = NOTIFYICON_VERSION_4;
    return Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

BOOL DeleteNotificationIcon()
{
    NOTIFYICONDATA nid = {sizeof(nid)};
    nid.uFlags = NIF_GUID;
    nid.guidItem = __uuidof(TrayIcon);
    return Shell_NotifyIcon(NIM_DELETE, &nid);
}

void ShowContextMenu(HWND hwnd, int x, int y)
{
    HMENU hMenu = LoadMenu(g_hinst, MAKEINTRESOURCE(IDC_CONTEXTMENU));
    if (hMenu)
    {
        HMENU hSubMenu = GetSubMenu(hMenu, 0);
        if (hSubMenu)
        {
            SetForegroundWindow(hwnd);
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
            {
                uFlags |= TPM_RIGHTALIGN;
            }
            else
            {
                uFlags |= TPM_LEFTALIGN;
            }
            TrackPopupMenuEx(hSubMenu, uFlags, x, y, hwnd, NULL);
        }
        DestroyMenu(hMenu);
    }
}

void reconnectMQTT()
{
    if(mqtt_client)
    {
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);
        logfile << "Connecting to MQTT...\n";
        mqtt::token_ptr conntok = mqtt_client->connect(connOpts);
        conntok->wait();
        logfile << "Connected\n";
    }
}

void alert_window_created(HWND hwnd, WindowConfig& config)
{
    string this_topic = topic + "/wmon/" + config.section;
    logfile << "   MATCHED hwnd=" << hwnd << "\n";
    trackedWindows[hwnd] = WindowTracking(config);
    mqtt::message_ptr pubmsg = mqtt::make_message(this_topic, "ON");
    pubmsg->set_retained(true);
    try {
        mqtt_client->publish(pubmsg);
    } catch(const mqtt::exception &e) {
        reconnectMQTT();
        mqtt_client->publish(pubmsg);
    }
}

void alert_window_destroyed(WindowTrackingMap::iterator& it)
{
    string this_topic = topic + "/wmon/" + it->second.section;
    logfile << "   REMOVED hwnd=" << it->first << "\n";
    trackedWindows.erase(it);
    mqtt::message_ptr pubmsg = mqtt::make_message(this_topic, "OFF");
    pubmsg->set_retained(true);
    try {
        mqtt_client->publish(pubmsg);
    } catch(const mqtt::exception &e) {
        reconnectMQTT();
        mqtt_client->publish(pubmsg);
    }
}

string gethostname_string()
{
    TCHAR hostname[128] = {0};
    DWORD count = 128;
    if( !GetComputerName( hostname, &count ) )
    {
        return string("unknown");
    }
    return string(hostname);
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
                    alert_window_created(hwnd, cfg);
                    break;
                }
                else if(cfg.title.length() != 0 && name.find(cfg.title) != string::npos)
                {
                    alert_window_created(hwnd, cfg);
                    break;
                }
                // only check process if needed... it can be slower!
                //  recommend finding a way to avoid this
                else if(cfg.process.length() != 0)
                {
                    string procname = GetProcessNameForWindow(hwnd);
                    if(procname.find(cfg.process) != string::npos)
                    {
                        alert_window_created(hwnd, cfg);
                        break;
                    }
                    break;
                }
            }
        }
        else if(EVENT_OBJECT_DESTROY == event)
        {
            auto it = trackedWindows.find(hwnd);
            if(it != std::end(trackedWindows))
            {
                alert_window_destroyed(it);
            }
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uiMsg)
    {
    case WM_DESTROY:
        DeleteNotificationIcon();
        PostQuitMessage(0);
        break;
    case WM_CREATE:
        AddNotificationIcon(hwnd);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_SETTINGS:
            // todo
            break;
        case IDM_RELOAD:
            // todo
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        default:
            break;
        }
        break;
    case WMAPP_NOTIFYCALLBACK:
        switch (LOWORD(lParam))
        {
        case WM_CONTEXTMENU:
            {
                ShowContextMenu(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            break;
        }
        break;
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
    bool registerHA = false;
    inipp::Ini<char> ini;
    std::ifstream is("config.ini");
    ini.parse(is);
    ini.default_section(ini.sections["default"]);
    ini.interpolate();
    for(auto& section : ini.sections)
    {
        if(section.first == "default")
        {
            string logfilename = "";
            inipp::get_value(section.second, "logfile", logfilename);
            if(logfilename.length() != 0)
            {
                logfile.open(logfilename);
            }
            // general config
            topic = gethostname_string();
            inipp::get_value(section.second, "topic", topic);
            logfile << "MQTT topic: " << topic << "/wmon\n";
            string server;
            inipp::get_value(section.second, "server", server);
            logfile << "MQTT server: " << server << "\n";
            string client_id = "wmon_mqtt";
            inipp::get_value(section.second, "client_id", client_id);
            logfile << "MQTT client_id: " << client_id << "\n";
            inipp::get_value(section.second, "homeassistant", registerHA);
            if(registerHA) logfile << "Sending HA registration\n";
            mqtt_client = new mqtt::async_client(server, client_id);
            reconnectMQTT();
        }
        else
        {
            // per-window config
            WindowConfig data;
            data.section = section.first;
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
            else if(section.second.count("process") != 0)
            {
                inipp::get_value(section.second, "process", data.process);
                logfile << "Searching for: process=\"" << data.process << "\"\n";
            }
            else
            {  
                logfile << "Invalid section" << section.first << "\n";
                continue;
            }
            inipp::get_value(section.second, "fullscreen", data.onlyFullscreen);
            searchList.push_back(data);

            // send off by default for now:
            string this_topic = topic + "/wmon/" + section.first;
            mqtt::message_ptr pubmsg = mqtt::make_message(this_topic, "OFF");
            pubmsg->set_retained(true);
            mqtt_client->publish(pubmsg);
            // TODO: scan currently open windows to see if the window is already open when the app starts
        }
    }
    
    if(registerHA)
    {
        for(auto& config : searchList)
        {
            string this_topic = topic + "/wmon/" + config.section;
            mqtt::message_ptr pubmsg = mqtt::make_message("homeassistant/binary_sensor/"+topic+"_"+config.section+"/config", 
                "{\"name\": \""+topic+" "+config.section+"\", \"device_class\": \"running\", \"state_topic\": \""+this_topic+"\"}");
            pubmsg->set_retained(true);
            mqtt_client->publish(pubmsg);
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
    ShowWindow(hwnd, SW_HIDE);

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

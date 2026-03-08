#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#include <math.h>
#include <objbase.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include <wininet.h>
#include <shellapi.h>
#include <atomic>
#include <mutex>
#include <thread>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")

struct WindowPreset {
    const wchar_t* label;
    int w;
    int h;
};

static const WindowPreset g_presets[] = {
    { L"800\u00D7600",   800,  600 },
    { L"1024\u00D7768", 1024,  768 },
    { L"1280\u00D7768", 1280,  768 },
    { L"1440\u00D7900", 1440,  900 },
    { L"1600\u00D7900", 1600,  900 },
};
static const int g_presetCount = sizeof(g_presets) / sizeof(g_presets[0]);
static int g_currentPreset = 2;

static int g_wndW = 1280;
static int g_wndH = 768;

static constexpr int   TITLEBAR_H = 38;
static constexpr int   CLOSE_BTN_W = 46;
static constexpr int   CLOSE_BTN_H = TITLEBAR_H;
static constexpr int   LAUNCH_BTN_W = 320;
static constexpr int   LAUNCH_BTN_H = 52;
static constexpr float SPINNER_RADIUS = 80.0f;
static constexpr float SPINNER_THICKNESS = 6.0f;
static constexpr float SPINNER_ARC_DEG = 270.0f;
static constexpr float SPINNER_SPEED = 8.0f;
static constexpr UINT_PTR TIMER_SPINNER = 1;
static constexpr UINT_PTR TIMER_RPC = 2;
static constexpr UINT_PTR TIMER_GAMEWATCH = 3;
static constexpr UINT     TIMER_MS = 16;
static constexpr UINT     RPC_TIMER_MS = 15000;
static constexpr UINT     GAMEWATCH_TIMER_MS = 2000;

static constexpr int   NAV_BTN_W = 90;
static constexpr int   NAV_BTN_H = 28;
static constexpr int   NAV_BTN_Y = 44;
static constexpr int   NAV_BTN_X_START = 12;
static constexpr int   NAV_BTN_GAP = 8;

static constexpr int   PRESET_BTN_W = 110;
static constexpr int   PRESET_BTN_H = 28;

static constexpr int   RPC_PANEL_W = 260;
static constexpr int   RPC_CHECK_H = 24;

static const wchar_t* GITHUB_URL = L"https://github.com/dragonmirror0/SilkWare-gmod";
static const char* GITHUB_API_HOST = "api.github.com";
static const char* GITHUB_API_PATH = "/repos/dragonmirror0/SilkWare-gmod/commits?per_page=50";

static const char* DISCORD_APP_ID = "1479703856534261813";

static constexpr UINT WM_TRAYICON = WM_USER + 1;
static constexpr UINT TRAY_ID = 1;
static constexpr UINT IDM_TRAY_SHOW = 4001;
static constexpr UINT IDM_TRAY_EXIT = 4002;

struct CommitInfo {
    std::wstring sha;
    std::wstring message;
    std::wstring author;
    std::wstring date;
};

static std::vector<CommitInfo> g_commits;
static bool g_commitsLoaded = false;
static bool g_commitsLoading = false;
static std::wstring g_commitsError;

enum class AppState { Menu, Launching, Injecting, Done, Error };
enum class ViewMode { Main, Commits, Settings };

static AppState  g_state = AppState::Menu;
static ViewMode  g_viewMode = ViewMode::Main;
static float     g_spinnerAngle = 0.0f;
static bool      g_closeHovered = false;
static bool      g_launchHovered = false;
static bool      g_launchPressed = false;
static bool      g_githubHovered = false;
static bool      g_commitsHovered = false;
static bool      g_settingsHovered = false;
static bool      g_backHovered = false;
static bool      g_dragging = false;
static POINT     g_dragStart = {};
static HWND      g_hwnd = nullptr;
static int       g_commitsScroll = 0;
static bool      g_presetDropdownOpen = false;
static int       g_presetHoveredIdx = -1;

static bool      g_rpcEnabled = true;
static bool      g_rpcEnabledHovered = false;

static HANDLE    g_discordPipe = INVALID_HANDLE_VALUE;
static bool      g_discordConnected = false;
static std::mutex g_discordMutex;
static int       g_discordNonce = 1;
static ULONGLONG g_rpcStartTime = 0;

static bool      g_trayCreated = false;
static NOTIFYICONDATAW g_nid = {};
static bool      g_reallyQuit = false;

static std::atomic<DWORD> g_gamePid{ 0 };
static std::atomic<bool>  g_gameInjected{ false };
static std::atomic<bool>  g_gameWasRunning{ false };

static std::wstring g_statusText = L"";
static std::wstring g_loaderDir;

static ID2D1Factory* g_d2dFactory = nullptr;
static ID2D1HwndRenderTarget* g_renderTarget = nullptr;
static IDWriteFactory* g_dwFactory = nullptr;
static IDWriteTextFormat* g_fmtTitle = nullptr;
static IDWriteTextFormat* g_fmtButton = nullptr;
static IDWriteTextFormat* g_fmtStatus = nullptr;
static IDWriteTextFormat* g_fmtCloseX = nullptr;
static IDWriteTextFormat* g_fmtNavBtn = nullptr;
static IDWriteTextFormat* g_fmtCommitMsg = nullptr;
static IDWriteTextFormat* g_fmtCommitMeta = nullptr;
static IDWriteTextFormat* g_fmtSettingsLabel = nullptr;
static IDWriteTextFormat* g_fmtSettingsSmall = nullptr;

static void DiscardD2DResources();
static void CreateD2DResources(HWND hwnd);

static std::wstring GetLoaderDirectory() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath);
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) return path.substr(0, pos + 1);
    return L"";
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
    return w;
}

#pragma pack(push, 1)
struct DiscordHeader {
    int32_t opcode;
    int32_t length;
};
#pragma pack(pop)

static bool DiscordPipeWrite(int32_t opcode, const std::string& json) {
    if (g_discordPipe == INVALID_HANDLE_VALUE) return false;
    DiscordHeader hdr;
    hdr.opcode = opcode;
    hdr.length = (int32_t)json.size();
    DWORD written = 0;
    if (!WriteFile(g_discordPipe, &hdr, sizeof(hdr), &written, nullptr)) return false;
    if (!WriteFile(g_discordPipe, json.c_str(), (DWORD)json.size(), &written, nullptr)) return false;
    return true;
}

static bool DiscordPipeRead(std::string& outJson) {
    DiscordHeader hdr;
    DWORD bytesRead = 0;
    if (!ReadFile(g_discordPipe, &hdr, sizeof(hdr), &bytesRead, nullptr)) return false;
    if (bytesRead < sizeof(hdr)) return false;
    if (hdr.length <= 0 || hdr.length > 65536) return false;
    std::string buf(hdr.length, '\0');
    if (!ReadFile(g_discordPipe, &buf[0], hdr.length, &bytesRead, nullptr)) return false;
    outJson = buf;
    return true;
}

static void DiscordConnect() {
    std::lock_guard<std::mutex> lock(g_discordMutex);
    if (g_discordConnected) return;
    for (int i = 0; i < 10; i++) {
        wchar_t pipeName[64];
        wsprintfW(pipeName, L"\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE pipe = CreateFileW(pipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            g_discordPipe = pipe;
            break;
        }
    }
    if (g_discordPipe == INVALID_HANDLE_VALUE) return;
    std::string handshake = "{\"v\":1,\"client_id\":\"";
    handshake += DISCORD_APP_ID;
    handshake += "\"}";
    if (!DiscordPipeWrite(0, handshake)) {
        CloseHandle(g_discordPipe); g_discordPipe = INVALID_HANDLE_VALUE; return;
    }
    std::string response;
    if (!DiscordPipeRead(response)) {
        CloseHandle(g_discordPipe); g_discordPipe = INVALID_HANDLE_VALUE; return;
    }
    g_discordConnected = true;
    g_rpcStartTime = (ULONGLONG)time(nullptr);
}

static void DiscordDisconnect() {
    std::lock_guard<std::mutex> lock(g_discordMutex);
    if (g_discordPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(g_discordPipe); g_discordPipe = INVALID_HANDLE_VALUE;
    }
    g_discordConnected = false;
}

static void DiscordClearPresence() {
    std::lock_guard<std::mutex> lock(g_discordMutex);
    if (!g_discordConnected || g_discordPipe == INVALID_HANDLE_VALUE) return;
    int nonce = g_discordNonce++;
    char nonceStr[32]; sprintf_s(nonceStr, "%d", nonce);
    char pidStr[16]; sprintf_s(pidStr, "%lu", GetCurrentProcessId());
    std::string json = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
    json += pidStr;
    json += ",\"activity\":null},\"nonce\":\"";
    json += nonceStr;
    json += "\"}";
    if (!DiscordPipeWrite(1, json)) {
        CloseHandle(g_discordPipe); g_discordPipe = INVALID_HANDLE_VALUE;
        g_discordConnected = false;
    }
    else {
        std::string response;
        DWORD mode = PIPE_NOWAIT;
        SetNamedPipeHandleState(g_discordPipe, &mode, nullptr, nullptr);
        DiscordPipeRead(response);
        mode = PIPE_WAIT;
        SetNamedPipeHandleState(g_discordPipe, &mode, nullptr, nullptr);
    }
}

static void DiscordUpdatePresence() {
    std::lock_guard<std::mutex> lock(g_discordMutex);
    if (!g_discordConnected || g_discordPipe == INVALID_HANDLE_VALUE) return;

    std::string state, details;

    if (g_gameInjected.load()) {
        details = "In Game";
        state = "Playing Garry's Mod";
    }
    else {
        details = "In Loader";
        state = "Selecting options...";
    }

    int nonce = g_discordNonce++;
    char nonceStr[32]; sprintf_s(nonceStr, "%d", nonce);
    char pidStr[16]; sprintf_s(pidStr, "%lu", GetCurrentProcessId());
    char tsStr[32]; sprintf_s(tsStr, "%llu", g_rpcStartTime);

    std::string json = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
    json += pidStr;
    json += ",\"activity\":{";
    json += "\"details\":\"" + details + "\",";
    json += "\"state\":\"" + state + "\",";
    json += "\"timestamps\":{\"start\":";
    json += tsStr;
    json += "},";
    json += "\"assets\":{\"large_image\":\"silkware_logo\",\"large_text\":\"SilkWare\"}";
    json += "}},\"nonce\":\"";
    json += nonceStr;
    json += "\"}";

    if (!DiscordPipeWrite(1, json)) {
        CloseHandle(g_discordPipe); g_discordPipe = INVALID_HANDLE_VALUE;
        g_discordConnected = false;
    }
    else {
        std::string response;
        DWORD mode = PIPE_NOWAIT;
        SetNamedPipeHandleState(g_discordPipe, &mode, nullptr, nullptr);
        DiscordPipeRead(response);
        mode = PIPE_WAIT;
        SetNamedPipeHandleState(g_discordPipe, &mode, nullptr, nullptr);
    }
}

static void RpcTick() {
    if (!g_rpcEnabled) {
        if (g_discordConnected) { DiscordClearPresence(); DiscordDisconnect(); }
        return;
    }
    if (!g_discordConnected) DiscordConnect();
    if (g_discordConnected) DiscordUpdatePresence();
}

static void TrayCreate(HWND hwnd) {
    if (g_trayCreated) return;
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = TRAY_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"SilkWare Loader");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_trayCreated = true;
}

static void TrayRemove() {
    if (!g_trayCreated) return;
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    g_trayCreated = false;
}

static void TrayShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHOW, L"Show");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"Exit");
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}

static DWORD FindProcessId(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static bool IsProcessRunning(DWORD pid) {
    if (pid == 0) return false;
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return (GetLastError() == ERROR_ACCESS_DENIED);
    DWORD exitCode = 0;
    BOOL ok = GetExitCodeProcess(hProc, &exitCode);
    CloseHandle(hProc);
    if (!ok) return true;
    return (exitCode == STILL_ACTIVE);
}

static bool HasModule(DWORD pid, const wchar_t* moduleName) {
    for (int attempt = 0; attempt < 5; attempt++) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_BAD_LENGTH || err == ERROR_PARTIAL_COPY || err == ERROR_ACCESS_DENIED) { Sleep(300); continue; }
            return false;
        }
        MODULEENTRY32W me; me.dwSize = sizeof(me);
        bool found = false;
        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, moduleName) == 0) { found = true; break; }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return found;
    }
    return false;
}

static bool WaitForModule(DWORD pid, const wchar_t* moduleName, const wchar_t* displayName, DWORD timeoutMs = 180000) {
    DWORD startTime = GetTickCount();
    g_statusText = std::wstring(L"Waiting for ") + displayName + L"...";
    InvalidateRect(g_hwnd, nullptr, FALSE);
    while (GetTickCount() - startTime < timeoutMs) {
        if (!IsProcessRunning(pid)) {
            DWORD freshPid = FindProcessId(L"gmod.exe");
            if (freshPid != 0 && freshPid != pid) pid = freshPid;
            else if (freshPid == 0) { g_statusText = L"Error: Game process exited!"; return false; }
        }
        if (HasModule(pid, moduleName)) return true;
        Sleep(500);
    }
    g_statusText = std::wstring(L"Error: Timeout waiting for ") + moduleName;
    return false;
}

static bool WaitForGameReady(DWORD& pid) {
    struct ModuleInfo { const wchar_t* name; const wchar_t* displayName; };
    ModuleInfo modules[] = {
        { L"tier0.dll", L"tier0" }, { L"vstdlib.dll", L"vstdlib" },
        { L"engine.dll", L"Engine" }, { L"vgui2.dll", L"VGUI" },
        { L"client.dll", L"Client" }, { L"lua_shared.dll", L"Lua" }
    };
    for (const auto& mod : modules) {
        DWORD freshPid = FindProcessId(L"gmod.exe");
        if (freshPid != 0) pid = freshPid;
        if (!WaitForModule(pid, mod.name, mod.displayName, 180000)) return false;
    }
    return true;
}

static bool InjectDLL(DWORD pid, const wchar_t* dllPath) {
    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!hProc) return false;
    size_t pathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    void* remoteMem = VirtualAllocEx(hProc, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) { CloseHandle(hProc); return false; }
    if (!WriteProcessMemory(hProc, remoteMem, dllPath, pathSize, nullptr)) {
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE); CloseHandle(hProc); return false;
    }
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    FARPROC pLoadLib = GetProcAddress(hKernel, "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, (LPTHREAD_START_ROUTINE)pLoadLib, remoteMem, 0, nullptr);
    if (!hThread) { VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE); CloseHandle(hProc); return false; }
    WaitForSingleObject(hThread, 30000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return exitCode != 0;
}

static std::wstring GetDLLPath() {
    return g_loaderDir + L"SilkWareDLL.dll";
}

static void SetStatus(const std::wstring& text, AppState newState) {
    g_statusText = text;
    g_state = newState;
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void ResetToMenu() {
    g_state = AppState::Menu;
    g_statusText = L"";
    g_gamePid = 0;
    g_gameInjected = false;
    g_gameWasRunning = false;
    KillTimer(g_hwnd, TIMER_SPINNER);
    KillTimer(g_hwnd, TIMER_GAMEWATCH);
    RpcTick();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void CheckGameState() {
    DWORD pid = g_gamePid.load();
    if (pid == 0) {
        pid = FindProcessId(L"gmod.exe");
        if (pid != 0) g_gamePid = pid;
    }

    if (pid == 0) {
        if (g_gameWasRunning.load()) ResetToMenu();
        return;
    }

    if (!IsProcessRunning(pid)) {
        DWORD freshPid = FindProcessId(L"gmod.exe");
        if (freshPid == 0) {
            if (g_gameWasRunning.load()) ResetToMenu();
            return;
        }
        g_gamePid = freshPid;
    }

    g_gameWasRunning = true;
}

static DWORD WINAPI InjectionThread(LPVOID) {
    std::wstring dllPath = GetDLLPath();
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        SetStatus(L"Error: SilkWareDLL.dll not found!", AppState::Error);
        return 1;
    }
    DWORD pid = FindProcessId(L"gmod.exe");
    bool wasAlreadyRunning = (pid != 0);
    if (!pid) {
        SetStatus(L"Launching Garry's Mod...", AppState::Launching);
        RpcTick();
        ShellExecuteW(nullptr, L"open", L"steam://rungameid/4000", nullptr, nullptr, SW_SHOWNORMAL);
        SetStatus(L"Waiting for gmod.exe...", AppState::Launching);
        DWORD waitStart = GetTickCount();
        while (GetTickCount() - waitStart < 120000) {
            pid = FindProcessId(L"gmod.exe");
            if (pid) break;
            Sleep(1000);
        }
        if (!pid) { SetStatus(L"Error: Garry's Mod not started!", AppState::Error); return 1; }
        SetStatus(L"Process found, waiting...", AppState::Launching);
        Sleep(5000);
        DWORD freshPid = FindProcessId(L"gmod.exe");
        if (freshPid != 0 && freshPid != pid) pid = freshPid;
    }

    g_gamePid = pid;
    g_gameWasRunning = true;
    RpcTick();

    if (!WaitForGameReady(pid)) {
        if (g_state != AppState::Error) SetStatus(g_statusText, AppState::Error);
        return 1;
    }
    g_gamePid = pid;

    SetStatus(L"Waiting for full initialization...", AppState::Launching);
    int waitSeconds = wasAlreadyRunning ? 3 : 10;
    for (int i = 0; i < waitSeconds; i++) {
        DWORD freshPid = FindProcessId(L"gmod.exe");
        if (freshPid == 0) { SetStatus(L"Error: Game process exited!", AppState::Error); return 1; }
        if (freshPid != pid) { pid = freshPid; g_gamePid = pid; }
        Sleep(1000);
    }

    SetStatus(L"Injecting SilkWare...", AppState::Injecting);
    bool injected = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        DWORD freshPid = FindProcessId(L"gmod.exe");
        if (freshPid != 0) { pid = freshPid; g_gamePid = pid; }
        if (InjectDLL(pid, dllPath.c_str())) { injected = true; break; }
        Sleep(2000);
    }
    if (!injected) { SetStatus(L"Error: Injection failed!", AppState::Error); return 1; }

    g_gameInjected = true;
    SetStatus(L"SilkWare injected successfully!", AppState::Done);
    RpcTick();

    SetTimer(g_hwnd, TIMER_GAMEWATCH, GAMEWATCH_TIMER_MS, nullptr);
    return 0;
}

static D2D1_RECT_F GetCloseButtonRect(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    return D2D1::RectF((float)(rc.right - CLOSE_BTN_W), 0.0f, (float)rc.right, (float)CLOSE_BTN_H);
}
static D2D1_RECT_F GetLaunchButtonRect(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    float cx = (rc.right - LAUNCH_BTN_W) / 2.0f;
    float cy = (rc.bottom - LAUNCH_BTN_H) / 2.0f;
    return D2D1::RectF(cx, cy, cx + LAUNCH_BTN_W, cy + LAUNCH_BTN_H);
}
static D2D1_RECT_F GetGitHubButtonRect() {
    return D2D1::RectF((float)NAV_BTN_X_START, (float)NAV_BTN_Y, (float)(NAV_BTN_X_START + NAV_BTN_W), (float)(NAV_BTN_Y + NAV_BTN_H));
}
static D2D1_RECT_F GetCommitsButtonRect() {
    float x = (float)(NAV_BTN_X_START + NAV_BTN_W + NAV_BTN_GAP);
    return D2D1::RectF(x, (float)NAV_BTN_Y, x + NAV_BTN_W, (float)(NAV_BTN_Y + NAV_BTN_H));
}
static D2D1_RECT_F GetSettingsButtonRect() {
    float x = (float)(NAV_BTN_X_START + (NAV_BTN_W + NAV_BTN_GAP) * 2);
    return D2D1::RectF(x, (float)NAV_BTN_Y, x + NAV_BTN_W, (float)(NAV_BTN_Y + NAV_BTN_H));
}
static D2D1_RECT_F GetBackButtonRect() {
    return D2D1::RectF((float)NAV_BTN_X_START, (float)NAV_BTN_Y, (float)(NAV_BTN_X_START + NAV_BTN_W), (float)(NAV_BTN_Y + NAV_BTN_H));
}
static bool HitTest(D2D1_RECT_F r, int mx, int my) {
    return mx >= r.left && mx <= r.right && my >= r.top && my <= r.bottom;
}
static D2D1_RECT_F GetRpcEnabledCheckRect() {
    float contentTop = (float)(NAV_BTN_Y + NAV_BTN_H + 20);
    float rpcSectionY = contentTop + 80;
    float chkY = rpcSectionY + 28;
    return D2D1::RectF(24.0f - 4, chkY, 24.0f + RPC_PANEL_W, chkY + RPC_CHECK_H);
}
static float GetPresetDropdownY() {
    float contentTop = (float)(NAV_BTN_Y + NAV_BTN_H + 20);
    return contentTop + 28;
}
static D2D1_RECT_F GetPresetButtonRect() {
    float presetY = GetPresetDropdownY();
    return D2D1::RectF(24.0f, presetY, 24.0f + PRESET_BTN_W, presetY + PRESET_BTN_H);
}
static D2D1_RECT_F GetPresetDropdownItemRect(int idx) {
    float presetY = GetPresetDropdownY();
    float itemY = presetY + PRESET_BTN_H + idx * PRESET_BTN_H;
    return D2D1::RectF(24.0f, itemY, 24.0f + PRESET_BTN_W, itemY + PRESET_BTN_H);
}

static void DisableRoundedCorners(HWND hwnd) {
    enum { DWMWA_WINDOW_CORNER_PREFERENCE_VAL = 33 };
    enum { DWMWCP_DONOTROUND = 1 };
    int pref = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE_VAL, &pref, sizeof(pref));
}

static void ApplyPreset(int idx) {
    if (idx < 0 || idx >= g_presetCount) return;
    g_currentPreset = idx;
    g_wndW = g_presets[idx].w;
    g_wndH = g_presets[idx].h;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(g_hwnd, nullptr, (screenW - g_wndW) / 2, (screenH - g_wndH) / 2, g_wndW, g_wndH, SWP_NOZORDER);
    DiscardD2DResources();
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void DiscardD2DResources() {
    if (g_renderTarget) { g_renderTarget->Release(); g_renderTarget = nullptr; }
}

static void CreateD2DResources(HWND hwnd) {
    if (g_renderTarget) return;
    RECT rc; GetClientRect(hwnd, &rc);
    g_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(rc.right, rc.bottom)),
        &g_renderTarget);
    if (g_renderTarget) g_renderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}

static void RenderCheckbox(float x, float y, float w, float h, const wchar_t* label, bool checked, bool hovered) {
    ID2D1SolidColorBrush* brBox = nullptr, * brCheck = nullptr, * brText = nullptr, * brHov = nullptr;
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.30f, 0.30f, 0.30f), &brBox);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.12f, 0.12f), &brCheck);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.86f, 0.86f), &brText);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.18f, 0.18f), &brHov);
    float boxSize = 16.0f, boxX = x, boxY = y + (h - boxSize) / 2.0f;
    if (hovered) { D2D1_RECT_F bgRc = D2D1::RectF(x - 4, y, x + w, y + h); g_renderTarget->FillRectangle(bgRc, brHov); }
    D2D1_RECT_F boxRc = D2D1::RectF(boxX, boxY, boxX + boxSize, boxY + boxSize);
    g_renderTarget->DrawRectangle(boxRc, brBox, 1.5f);
    if (checked) { D2D1_RECT_F innerRc = D2D1::RectF(boxX + 3, boxY + 3, boxX + boxSize - 3, boxY + boxSize - 3); g_renderTarget->FillRectangle(innerRc, brCheck); }
    D2D1_RECT_F textRc = D2D1::RectF(boxX + boxSize + 8, y, x + w, y + h);
    g_renderTarget->DrawText(label, (UINT32)wcslen(label), g_fmtSettingsSmall, textRc, brText);
    if (brBox) brBox->Release(); if (brCheck) brCheck->Release(); if (brText) brText->Release(); if (brHov) brHov->Release();
}

static void RenderCommitsView(float w, float h) {
    ID2D1SolidColorBrush* brText = nullptr, * brDim = nullptr, * brBtnBg = nullptr, * brBtnHov = nullptr, * brSeparator = nullptr, * brSha = nullptr, * brError = nullptr;
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.86f, 0.86f), &brText);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.50f, 0.50f, 0.50f), &brDim);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.15f, 0.15f, 0.15f), &brBtnBg);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.22f, 0.22f), &brBtnHov);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.12f), &brSeparator);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.12f, 0.12f), &brSha);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.15f, 0.15f), &brError);
    D2D1_RECT_F backRc = GetBackButtonRect();
    g_renderTarget->FillRectangle(backRc, g_backHovered ? brBtnHov : brBtnBg);
    g_renderTarget->DrawText(L"\u2190 Back", 6, g_fmtNavBtn, backRc, brText);
    float contentTop = (float)(NAV_BTN_Y + NAV_BTN_H + 16);
    float contentLeft = 24.0f, contentRight = w - 24.0f;
    if (g_commitsLoading) {
        D2D1_RECT_F loadRc = D2D1::RectF(0, contentTop, w, contentTop + 60);
        g_renderTarget->DrawText(L"Loading commits...", 18, g_fmtStatus, loadRc, brDim);
    }
    else if (!g_commitsError.empty() && g_commits.empty()) {
        D2D1_RECT_F errRc = D2D1::RectF(0, contentTop, w, contentTop + 60);
        g_renderTarget->DrawText(g_commitsError.c_str(), (UINT32)g_commitsError.size(), g_fmtStatus, errRc, brError);
    }
    else {
        float y = contentTop - (float)g_commitsScroll;
        float itemHeight = 64.0f;
        for (size_t i = 0; i < g_commits.size(); i++) {
            float itemTop = y + i * itemHeight;
            float itemBottom = itemTop + itemHeight;
            if (itemBottom < contentTop || itemTop > h) continue;
            if (i > 0) { D2D1_RECT_F sepRc = D2D1::RectF(contentLeft, itemTop, contentRight, itemTop + 1); g_renderTarget->FillRectangle(sepRc, brSeparator); }
            D2D1_RECT_F shaRc = D2D1::RectF(contentLeft, itemTop + 4, contentLeft + 80, itemTop + 24);
            g_renderTarget->DrawText(g_commits[i].sha.c_str(), (UINT32)g_commits[i].sha.size(), g_fmtCommitMeta, shaRc, brSha);
            D2D1_RECT_F msgRc = D2D1::RectF(contentLeft + 88, itemTop + 4, contentRight, itemTop + 28);
            std::wstring msg = g_commits[i].message;
            if (msg.size() > 100) msg = msg.substr(0, 100) + L"...";
            g_renderTarget->DrawText(msg.c_str(), (UINT32)msg.size(), g_fmtCommitMsg, msgRc, brText);
            std::wstring meta = g_commits[i].author + L"  \u00B7  " + g_commits[i].date;
            D2D1_RECT_F metaRc = D2D1::RectF(contentLeft + 88, itemTop + 30, contentRight, itemTop + 50);
            g_renderTarget->DrawText(meta.c_str(), (UINT32)meta.size(), g_fmtCommitMeta, metaRc, brDim);
        }
    }
    if (brText) brText->Release(); if (brDim) brDim->Release(); if (brBtnBg) brBtnBg->Release();
    if (brBtnHov) brBtnHov->Release(); if (brSeparator) brSeparator->Release();
    if (brSha) brSha->Release(); if (brError) brError->Release();
}

static void RenderSettingsContent(float w, float h) {
    ID2D1SolidColorBrush* brText = nullptr, * brDim = nullptr, * brBtnBg = nullptr, * brBtnHov = nullptr, * brSec = nullptr;
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.86f, 0.86f), &brText);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.50f, 0.50f, 0.50f), &brDim);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.15f, 0.15f, 0.15f), &brBtnBg);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.22f, 0.22f), &brBtnHov);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.12f, 0.12f), &brSec);
    D2D1_RECT_F backRc = GetBackButtonRect();
    g_renderTarget->FillRectangle(backRc, g_backHovered ? brBtnHov : brBtnBg);
    g_renderTarget->DrawText(L"\u2190 Back", 6, g_fmtNavBtn, backRc, brText);
    float contentTop = (float)(NAV_BTN_Y + NAV_BTN_H + 20);
    float leftCol = 24.0f;
    D2D1_RECT_F secRc1 = D2D1::RectF(leftCol, contentTop, leftCol + 200, contentTop + 22);
    g_renderTarget->DrawText(L"Window Scale", 12, g_fmtSettingsLabel, secRc1, brSec);
    float presetY = contentTop + 28;
    D2D1_RECT_F presetBtnRc = D2D1::RectF(leftCol, presetY, leftCol + PRESET_BTN_W, presetY + PRESET_BTN_H);
    g_renderTarget->FillRectangle(presetBtnRc, brBtnBg);
    g_renderTarget->DrawRectangle(presetBtnRc, brDim, 1.0f);
    std::wstring presetLabel = g_presets[g_currentPreset].label;
    presetLabel += L" \u25BC";
    g_renderTarget->DrawText(presetLabel.c_str(), (UINT32)presetLabel.size(), g_fmtSettingsSmall, presetBtnRc, brText);
    float rpcSectionY = contentTop + 80;
    D2D1_RECT_F secRc2 = D2D1::RectF(leftCol, rpcSectionY, leftCol + 200, rpcSectionY + 22);
    g_renderTarget->DrawText(L"Discord Rich Presence", 21, g_fmtSettingsLabel, secRc2, brSec);
    float chkY = rpcSectionY + 28;
    RenderCheckbox(leftCol, chkY, (float)RPC_PANEL_W, (float)RPC_CHECK_H, L"Enable Discord RPC", g_rpcEnabled, g_rpcEnabledHovered);
    if (brText) brText->Release(); if (brDim) brDim->Release(); if (brBtnBg) brBtnBg->Release();
    if (brBtnHov) brBtnHov->Release(); if (brSec) brSec->Release();
}

static void RenderSettingsDropdown() {
    if (!g_presetDropdownOpen) return;
    ID2D1SolidColorBrush* brText = nullptr, * brDim = nullptr, * brDropBg = nullptr, * brDropHov = nullptr, * brShadow = nullptr;
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.86f, 0.86f), &brText);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.50f, 0.50f, 0.50f), &brDim);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.10f, 0.10f), &brDropBg);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.25f, 0.25f), &brDropHov);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.5f), &brShadow);
    float leftCol = 24.0f, presetY = GetPresetDropdownY();
    D2D1_RECT_F shadowRc = D2D1::RectF(leftCol + 2, presetY + PRESET_BTN_H + 2, leftCol + PRESET_BTN_W + 4, presetY + PRESET_BTN_H + g_presetCount * PRESET_BTN_H + 4);
    g_renderTarget->FillRectangle(shadowRc, brShadow);
    D2D1_RECT_F fullDropRc = D2D1::RectF(leftCol, presetY + PRESET_BTN_H, leftCol + PRESET_BTN_W, presetY + PRESET_BTN_H + g_presetCount * PRESET_BTN_H);
    g_renderTarget->FillRectangle(fullDropRc, brDropBg);
    for (int i = 0; i < g_presetCount; i++) {
        float itemY = presetY + PRESET_BTN_H + i * PRESET_BTN_H;
        D2D1_RECT_F itemRc = D2D1::RectF(leftCol, itemY, leftCol + PRESET_BTN_W, itemY + PRESET_BTN_H);
        if (i == g_presetHoveredIdx) g_renderTarget->FillRectangle(itemRc, brDropHov);
        g_renderTarget->DrawText(g_presets[i].label, (UINT32)wcslen(g_presets[i].label), g_fmtSettingsSmall, itemRc, brText);
    }
    g_renderTarget->DrawRectangle(fullDropRc, brDim, 1.0f);
    if (brText) brText->Release(); if (brDim) brDim->Release(); if (brDropBg) brDropBg->Release();
    if (brDropHov) brDropHov->Release(); if (brShadow) brShadow->Release();
}

static void Render(HWND hwnd) {
    CreateD2DResources(hwnd);
    if (!g_renderTarget) return;
    RECT clientRc; GetClientRect(hwnd, &clientRc);
    float w = (float)clientRc.right, h = (float)clientRc.bottom;
    g_renderTarget->BeginDraw();
    g_renderTarget->Clear(D2D1::ColorF(0.047f, 0.047f, 0.047f));
    ID2D1SolidColorBrush* brText = nullptr, * brRed = nullptr, * brRedHov = nullptr, * brRedPress = nullptr;
    ID2D1SolidColorBrush* brCloseHov = nullptr, * brSpinner = nullptr, * brTitleText = nullptr;
    ID2D1SolidColorBrush* brGreen = nullptr, * brError = nullptr, * brBtnBg = nullptr, * brBtnHov = nullptr;
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.86f, 0.86f), &brText);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.12f, 0.12f), &brRed);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.82f, 0.18f, 0.18f), &brRedHov);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.08f, 0.08f), &brRedPress);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.78f, 0.16f, 0.16f), &brCloseHov);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.12f, 0.12f), &brSpinner);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.75f, 0.12f, 0.12f), &brTitleText);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.70f, 0.12f), &brGreen);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.15f, 0.15f), &brError);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.15f, 0.15f, 0.15f), &brBtnBg);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.22f, 0.22f), &brBtnHov);
    {
        D2D1_RECT_F titleRc = D2D1::RectF(0, 0, w, (float)TITLEBAR_H);
        g_renderTarget->DrawText(L"SilkWare", 8, g_fmtTitle, titleRc, brTitleText);
        D2D1_RECT_F closeRc = GetCloseButtonRect(hwnd);
        if (g_closeHovered) g_renderTarget->FillRectangle(closeRc, brCloseHov);
        g_renderTarget->DrawText(L"\u00D7", 1, g_fmtCloseX, closeRc, brText);
    }
    if (g_viewMode == ViewMode::Commits) {
        RenderCommitsView(w, h);
    }
    else if (g_viewMode == ViewMode::Settings) {
        RenderSettingsContent(w, h);
    }
    else {
        D2D1_RECT_F ghRc = GetGitHubButtonRect();
        g_renderTarget->FillRectangle(ghRc, g_githubHovered ? brBtnHov : brBtnBg);
        g_renderTarget->DrawText(L"GitHub", 6, g_fmtNavBtn, ghRc, brText);
        D2D1_RECT_F cmRc = GetCommitsButtonRect();
        g_renderTarget->FillRectangle(cmRc, g_commitsHovered ? brBtnHov : brBtnBg);
        g_renderTarget->DrawText(L"Commits", 7, g_fmtNavBtn, cmRc, brText);
        D2D1_RECT_F stRc = GetSettingsButtonRect();
        g_renderTarget->FillRectangle(stRc, g_settingsHovered ? brBtnHov : brBtnBg);
        g_renderTarget->DrawText(L"Settings", 8, g_fmtNavBtn, stRc, brText);
        if (g_state == AppState::Menu) {
            D2D1_RECT_F btnRc = GetLaunchButtonRect(hwnd);
            ID2D1SolidColorBrush* btnBr = brRed;
            if (g_launchPressed) btnBr = brRedPress;
            else if (g_launchHovered) btnBr = brRedHov;
            g_renderTarget->FillRectangle(btnRc, btnBr);
            g_renderTarget->DrawText(L"Launch Garry's Mod", 18, g_fmtButton, btnRc, brText);
        }
        else {
            float cx = w / 2.0f, cy = h / 2.0f;
            ID2D1SolidColorBrush* statusBr = brText;
            if (g_state == AppState::Done) statusBr = brGreen;
            if (g_state == AppState::Error) statusBr = brError;
            float textBottom = cy - SPINNER_RADIUS - 24.0f;
            D2D1_RECT_F textRc = D2D1::RectF(0, textBottom - 50.0f, w, textBottom);
            g_renderTarget->DrawText(g_statusText.c_str(), (UINT32)g_statusText.size(), g_fmtStatus, textRc, statusBr);
            if (g_state == AppState::Launching || g_state == AppState::Injecting) {
                float startRad = g_spinnerAngle * 3.14159265f / 180.0f;
                float endRad = (g_spinnerAngle - SPINNER_ARC_DEG) * 3.14159265f / 180.0f;
                D2D1_POINT_2F startPt = D2D1::Point2F(cx + cosf(startRad) * SPINNER_RADIUS, cy + sinf(startRad) * SPINNER_RADIUS);
                D2D1_POINT_2F endPt = D2D1::Point2F(cx + cosf(endRad) * SPINNER_RADIUS, cy + sinf(endRad) * SPINNER_RADIUS);
                ID2D1PathGeometry* path = nullptr;
                g_d2dFactory->CreatePathGeometry(&path);
                if (path) {
                    ID2D1GeometrySink* sink = nullptr;
                    path->Open(&sink);
                    if (sink) {
                        sink->BeginFigure(startPt, D2D1_FIGURE_BEGIN_HOLLOW);
                        sink->AddArc(D2D1::ArcSegment(endPt, D2D1::SizeF(SPINNER_RADIUS, SPINNER_RADIUS), 0.0f,
                            D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
                        sink->EndFigure(D2D1_FIGURE_END_OPEN);
                        sink->Close(); sink->Release();
                    }
                    ID2D1StrokeStyle* strokeStyle = nullptr;
                    D2D1_STROKE_STYLE_PROPERTIES ssp = D2D1::StrokeStyleProperties();
                    ssp.startCap = D2D1_CAP_STYLE_FLAT; ssp.endCap = D2D1_CAP_STYLE_FLAT; ssp.lineJoin = D2D1_LINE_JOIN_MITER;
                    g_d2dFactory->CreateStrokeStyle(ssp, nullptr, 0, &strokeStyle);
                    g_renderTarget->DrawGeometry(path, brSpinner, SPINNER_THICKNESS, strokeStyle);
                    if (strokeStyle) strokeStyle->Release();
                    path->Release();
                }
            }
        }
    }
    if (g_viewMode == ViewMode::Settings) RenderSettingsDropdown();
    if (brText) brText->Release(); if (brRed) brRed->Release(); if (brRedHov) brRedHov->Release();
    if (brRedPress) brRedPress->Release(); if (brCloseHov) brCloseHov->Release();
    if (brSpinner) brSpinner->Release(); if (brTitleText) brTitleText->Release();
    if (brGreen) brGreen->Release(); if (brError) brError->Release();
    if (brBtnBg) brBtnBg->Release(); if (brBtnHov) brBtnHov->Release();
    HRESULT hr = g_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) DiscardD2DResources();
}

static std::string ExtractJsonString(const std::string& json, size_t startPos, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey, startPos);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + searchKey.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
            switch (json[pos]) {
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            case 'n': result += ' '; break;
            case 'r': break;
            case 't': result += ' '; break;
            default: result += json[pos]; break;
            }
        }
        else { result += json[pos]; }
        pos++;
    }
    return result;
}

static std::vector<CommitInfo> ParseCommitsJson(const std::string& json) {
    std::vector<CommitInfo> commits;
    size_t pos = 0;
    while (pos < json.size()) {
        size_t shaPos = json.find("\"sha\"", pos);
        if (shaPos == std::string::npos) break;
        size_t commitObjPos = json.find("\"commit\"", shaPos);
        if (commitObjPos == std::string::npos) break;
        std::string sha = ExtractJsonString(json, shaPos, "sha");
        size_t messagePos = json.find("\"message\"", commitObjPos);
        std::string message;
        if (messagePos != std::string::npos) message = ExtractJsonString(json, messagePos, "message");
        size_t authorPos = json.find("\"author\"", commitObjPos);
        std::string authorName, date;
        if (authorPos != std::string::npos) {
            authorName = ExtractJsonString(json, authorPos, "name");
            date = ExtractJsonString(json, authorPos, "date");
        }
        if (!sha.empty()) {
            CommitInfo ci;
            ci.sha = Utf8ToWide(sha.substr(0, 7));
            ci.message = Utf8ToWide(message);
            ci.author = Utf8ToWide(authorName);
            ci.date = (date.size() >= 10) ? Utf8ToWide(date.substr(0, 10)) : Utf8ToWide(date);
            commits.push_back(ci);
        }
        size_t nextSha = json.find("\"sha\"", shaPos + 5);
        if (nextSha == std::string::npos) break;
        pos = nextSha;
    }
    return commits;
}

static std::string HttpGet(const char* host, const char* path) {
    HINTERNET hInternet = InternetOpenA("SilkWare/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return "";
    HINTERNET hConnect = InternetConnectA(hInternet, host, INTERNET_DEFAULT_HTTPS_PORT, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }
    DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", path, nullptr, nullptr, nullptr, flags, 0);
    if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return ""; }
    if (!HttpSendRequestA(hRequest, nullptr, 0, nullptr, 0)) {
        InternetCloseHandle(hRequest); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return "";
    }
    std::string response;
    char buffer[4096]; DWORD bytesRead = 0;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = 0; response.append(buffer, bytesRead);
    }
    InternetCloseHandle(hRequest); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet);
    return response;
}

static DWORD WINAPI LoadCommitsThread(LPVOID) {
    g_commitsLoading = true;
    g_commitsError.clear();
    std::string json = HttpGet(GITHUB_API_HOST, GITHUB_API_PATH);
    if (json.empty()) { g_commitsError = L"Failed to fetch commits"; g_commitsLoading = false; InvalidateRect(g_hwnd, nullptr, FALSE); return 1; }
    g_commits = ParseCommitsJson(json);
    if (g_commits.empty()) g_commitsError = L"No commits found";
    g_commitsLoaded = true;
    g_commitsLoading = false;
    InvalidateRect(g_hwnd, nullptr, FALSE);
    return 0;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        DisableRoundedCorners(hwnd);
        TrayCreate(hwnd);
        SetTimer(hwnd, TIMER_RPC, RPC_TIMER_MS, nullptr);
        RpcTick();
        return 0;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(hwnd, &ps); Render(hwnd); EndPaint(hwnd, &ps); return 0; }
    case WM_SIZE:
        if (g_renderTarget) { RECT rc; GetClientRect(hwnd, &rc); g_renderTarget->Resize(D2D1::SizeU(rc.right, rc.bottom)); }
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        if (wp == TIMER_SPINNER) {
            g_spinnerAngle += SPINNER_SPEED;
            if (g_spinnerAngle >= 360.0f) g_spinnerAngle -= 360.0f;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        else if (wp == TIMER_RPC) {
            RpcTick();
        }
        else if (wp == TIMER_GAMEWATCH) {
            CheckGameState();
            RpcTick();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK) { ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); }
        else if (lp == WM_RBUTTONUP) { TrayShowContextMenu(hwnd); }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == IDM_TRAY_SHOW) { ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); }
        else if (LOWORD(wp) == IDM_TRAY_EXIT) { g_reallyQuit = true; DestroyWindow(hwnd); }
        return 0;
    case WM_MOUSEWHEEL:
        if (g_viewMode == ViewMode::Commits) {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            g_commitsScroll -= delta / 2;
            if (g_commitsScroll < 0) g_commitsScroll = 0;
            int maxScroll = (int)(g_commits.size() * 64) - (g_wndH - NAV_BTN_Y - NAV_BTN_H - 32);
            if (maxScroll < 0) maxScroll = 0;
            if (g_commitsScroll > maxScroll) g_commitsScroll = maxScroll;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&tme);
        if (g_dragging) {
            POINT pt; GetCursorPos(&pt); RECT wr; GetWindowRect(hwnd, &wr);
            SetWindowPos(hwnd, nullptr, wr.left + pt.x - g_dragStart.x, wr.top + pt.y - g_dragStart.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            g_dragStart = pt; return 0;
        }
        bool repaint = false;
        bool newCloseHov = HitTest(GetCloseButtonRect(hwnd), mx, my);
        if (newCloseHov != g_closeHovered) { g_closeHovered = newCloseHov; repaint = true; }
        if (g_viewMode == ViewMode::Commits) {
            bool newBackHov = HitTest(GetBackButtonRect(), mx, my);
            if (newBackHov != g_backHovered) { g_backHovered = newBackHov; repaint = true; }
            if (g_githubHovered) { g_githubHovered = false; repaint = true; }
            if (g_commitsHovered) { g_commitsHovered = false; repaint = true; }
            if (g_settingsHovered) { g_settingsHovered = false; repaint = true; }
            if (g_launchHovered) { g_launchHovered = false; repaint = true; }
        }
        else if (g_viewMode == ViewMode::Settings) {
            bool newBackHov = HitTest(GetBackButtonRect(), mx, my);
            if (newBackHov != g_backHovered) { g_backHovered = newBackHov; repaint = true; }
            bool newRpcEn = HitTest(GetRpcEnabledCheckRect(), mx, my);
            if (newRpcEn != g_rpcEnabledHovered) { g_rpcEnabledHovered = newRpcEn; repaint = true; }
            if (g_presetDropdownOpen) {
                int oldIdx = g_presetHoveredIdx; g_presetHoveredIdx = -1;
                for (int i = 0; i < g_presetCount; i++) {
                    if (HitTest(GetPresetDropdownItemRect(i), mx, my)) { g_presetHoveredIdx = i; break; }
                }
                if (g_presetHoveredIdx != oldIdx) repaint = true;
            }
            if (g_githubHovered) { g_githubHovered = false; repaint = true; }
            if (g_commitsHovered) { g_commitsHovered = false; repaint = true; }
            if (g_settingsHovered) { g_settingsHovered = false; repaint = true; }
            if (g_launchHovered) { g_launchHovered = false; repaint = true; }
        }
        else {
            bool newGhHov = HitTest(GetGitHubButtonRect(), mx, my);
            if (newGhHov != g_githubHovered) { g_githubHovered = newGhHov; repaint = true; }
            bool newCmHov = HitTest(GetCommitsButtonRect(), mx, my);
            if (newCmHov != g_commitsHovered) { g_commitsHovered = newCmHov; repaint = true; }
            bool newStHov = HitTest(GetSettingsButtonRect(), mx, my);
            if (newStHov != g_settingsHovered) { g_settingsHovered = newStHov; repaint = true; }
            if (g_backHovered) { g_backHovered = false; repaint = true; }
            if (g_state == AppState::Menu) {
                bool newLaunchHov = HitTest(GetLaunchButtonRect(hwnd), mx, my);
                if (newLaunchHov != g_launchHovered) { g_launchHovered = newLaunchHov; repaint = true; }
            }
        }
        if (repaint) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSELEAVE: {
        bool repaint = g_closeHovered || g_launchHovered || g_githubHovered || g_commitsHovered || g_backHovered || g_settingsHovered || g_rpcEnabledHovered;
        g_closeHovered = g_launchHovered = g_githubHovered = g_commitsHovered = g_backHovered = g_settingsHovered = g_rpcEnabledHovered = false;
        g_presetHoveredIdx = -1;
        if (repaint) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        if (HitTest(GetCloseButtonRect(hwnd), mx, my)) { ShowWindow(hwnd, SW_HIDE); return 0; }
        if (g_viewMode == ViewMode::Commits) {
            if (HitTest(GetBackButtonRect(), mx, my)) {
                g_viewMode = ViewMode::Main; g_commitsScroll = 0; g_backHovered = false;
                InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
        }
        else if (g_viewMode == ViewMode::Settings) {
            if (g_presetDropdownOpen) {
                bool clickedItem = false;
                for (int i = 0; i < g_presetCount; i++) {
                    if (HitTest(GetPresetDropdownItemRect(i), mx, my)) {
                        g_presetDropdownOpen = false; ApplyPreset(i); clickedItem = true; break;
                    }
                }
                if (!clickedItem) { g_presetDropdownOpen = false; InvalidateRect(hwnd, nullptr, FALSE); }
                return 0;
            }
            if (HitTest(GetBackButtonRect(), mx, my)) {
                g_viewMode = ViewMode::Main; g_backHovered = false; g_presetDropdownOpen = false;
                InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
            if (HitTest(GetPresetButtonRect(), mx, my)) {
                g_presetDropdownOpen = !g_presetDropdownOpen; InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
            if (HitTest(GetRpcEnabledCheckRect(), mx, my)) {
                g_rpcEnabled = !g_rpcEnabled; RpcTick(); InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
        }
        else {
            if (HitTest(GetGitHubButtonRect(), mx, my)) { ShellExecuteW(nullptr, L"open", GITHUB_URL, nullptr, nullptr, SW_SHOWNORMAL); return 0; }
            if (HitTest(GetCommitsButtonRect(), mx, my)) {
                g_viewMode = ViewMode::Commits; g_commitsScroll = 0; g_commitsHovered = false;
                if (!g_commitsLoaded && !g_commitsLoading) {
                    HANDLE hThread = CreateThread(nullptr, 0, LoadCommitsThread, nullptr, 0, nullptr);
                    if (hThread) CloseHandle(hThread);
                }
                InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
            if (HitTest(GetSettingsButtonRect(), mx, my)) {
                g_viewMode = ViewMode::Settings; g_settingsHovered = false; g_presetDropdownOpen = false;
                InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
            if (g_state == AppState::Menu && HitTest(GetLaunchButtonRect(hwnd), mx, my)) {
                g_launchPressed = true; SetCapture(hwnd); InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
        }
        if (my < TITLEBAR_H) { g_dragging = true; GetCursorPos(&g_dragStart); SetCapture(hwnd); return 0; }
        return 0;
    }
    case WM_LBUTTONUP: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        if (g_dragging) { g_dragging = false; ReleaseCapture(); return 0; }
        if (g_launchPressed) {
            g_launchPressed = false; ReleaseCapture();
            if (HitTest(GetLaunchButtonRect(hwnd), mx, my)) {
                g_state = AppState::Launching; g_statusText = L"Starting..."; g_launchHovered = false;
                SetTimer(hwnd, TIMER_SPINNER, TIMER_MS, nullptr);
                HANDLE hThread = CreateThread(nullptr, 0, InjectionThread, nullptr, 0, nullptr);
                if (hThread) CloseHandle(hThread);
            }
            InvalidateRect(hwnd, nullptr, FALSE); return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        if (!g_reallyQuit) { ShowWindow(hwnd, SW_HIDE); return 0; }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_SPINNER);
        KillTimer(hwnd, TIMER_RPC);
        KillTimer(hwnd, TIMER_GAMEWATCH);
        DiscordDisconnect();
        TrayRemove();
        DiscardD2DResources();
        PostQuitMessage(0);
        return 0;
    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    g_loaderDir = GetLoaderDirectory();
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2dFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_dwFactory));
    g_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &g_fmtTitle);
    g_fmtTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); g_fmtTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", &g_fmtButton);
    g_fmtButton->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); g_fmtButton->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 28.0f, L"en-us", &g_fmtStatus);
    g_fmtStatus->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); g_fmtStatus->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", &g_fmtCloseX);
    g_fmtCloseX->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); g_fmtCloseX->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &g_fmtNavBtn);
    g_fmtNavBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); g_fmtNavBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &g_fmtCommitMsg);
    g_fmtCommitMsg->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); g_fmtCommitMsg->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    g_fmtCommitMsg->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    g_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &g_fmtCommitMeta);
    g_fmtCommitMeta->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); g_fmtCommitMeta->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    g_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"en-us", &g_fmtSettingsLabel);
    g_fmtSettingsLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); g_fmtSettingsLabel->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    g_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &g_fmtSettingsSmall);
    g_fmtSettingsSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); g_fmtSettingsSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    const wchar_t CLASS_NAME[] = L"SilkWareLoaderClass";
    WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);
    int screenW = GetSystemMetrics(SM_CXSCREEN), screenH = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExW(0, CLASS_NAME, L"SilkWare", WS_POPUP | WS_VISIBLE,
        (screenW - g_wndW) / 2, (screenH - g_wndH) / 2, g_wndW, g_wndH, nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, SW_SHOW); UpdateWindow(g_hwnd);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (g_fmtTitle) g_fmtTitle->Release(); if (g_fmtButton) g_fmtButton->Release();
    if (g_fmtStatus) g_fmtStatus->Release(); if (g_fmtCloseX) g_fmtCloseX->Release();
    if (g_fmtNavBtn) g_fmtNavBtn->Release(); if (g_fmtCommitMsg) g_fmtCommitMsg->Release();
    if (g_fmtCommitMeta) g_fmtCommitMeta->Release();
    if (g_fmtSettingsLabel) g_fmtSettingsLabel->Release();
    if (g_fmtSettingsSmall) g_fmtSettingsSmall->Release();
    if (g_dwFactory) g_dwFactory->Release(); if (g_d2dFactory) g_d2dFactory->Release();
    CoUninitialize();
    return (int)msg.wParam;
}

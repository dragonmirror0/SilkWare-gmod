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
#include <shlobj.h>

#include "resource.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")

static constexpr int WND_W = 600;
static constexpr int WND_H = 400;

static constexpr int   TITLEBAR_H = 32;
static constexpr int   CLOSE_BTN_W = 40;
static constexpr int   LAUNCH_BTN_W = 260;
static constexpr int   LAUNCH_BTN_H = 44;
static constexpr float SPINNER_RADIUS = 55.0f;
static constexpr float SPINNER_THICK = 5.0f;
static constexpr float SPINNER_ARC_DEG = 270.0f;
static constexpr float SPINNER_SPEED = 8.0f;

static constexpr int   NAV_BTN_W = 80;
static constexpr int   NAV_BTN_H = 26;
static constexpr int   NAV_BTN_Y = 38;
static constexpr int   NAV_BTN_X = 10;
static constexpr int   NAV_BTN_GAP = 6;

static constexpr int   RPC_CHECK_Y = 70;
static constexpr int   RPC_CHECK_H = 22;
static constexpr int   RPC_CHECK_X = 10;
static constexpr int   RPC_CHECK_W = 200;

static constexpr UINT_PTR TIMER_SPINNER = 1;
static constexpr UINT_PTR TIMER_RPC = 2;
static constexpr UINT_PTR TIMER_GAMEWATCH = 3;
static constexpr UINT     TIMER_MS = 16;
static constexpr UINT     RPC_TIMER_MS = 15000;
static constexpr UINT     GAMEWATCH_MS = 2000;

static const wchar_t* GITHUB_URL = L"https://github.com/dragonmirror0/SilkWare-gmod";
static const char* GITHUB_API_HOST = "api.github.com";
static const char* GITHUB_API_PATH = "/repos/dragonmirror0/SilkWare-gmod/commits?per_page=50";
static const char* DISCORD_APP_ID = "1479703856534261813";

static constexpr UINT WM_TRAYICON = WM_USER + 1;
static constexpr UINT TRAY_ID = 1;
static constexpr UINT IDM_TRAY_SHOW = 4001;
static constexpr UINT IDM_TRAY_EXIT = 4002;

struct CommitInfo { std::wstring sha, message, author, date; };
static std::vector<CommitInfo> g_commits;
static bool         g_commitsLoaded = false;
static bool         g_commitsLoading = false;
static std::wstring g_commitsError;

enum class AppState { Menu, Launching, Injecting, Done, Error };
enum class ViewMode { Main, Commits };

static AppState  g_state = AppState::Menu;
static ViewMode  g_viewMode = ViewMode::Main;
static float     g_spinnerAngle = 0.0f;
static bool      g_closeHovered = false;
static bool      g_launchHovered = false;
static bool      g_launchPressed = false;
static bool      g_githubHovered = false;
static bool      g_commitsHovered = false;
static bool      g_backHovered = false;
static bool      g_rpcCheckHovered = false;
static bool      g_rpcEnabled = true;
static bool      g_dragging = false;
static POINT     g_dragStart = {};
static HWND      g_hwnd = nullptr;
static int       g_commitsScroll = 0;

static HANDLE     g_discordPipe = INVALID_HANDLE_VALUE;
static bool       g_discordConnected = false;
static std::mutex g_discordMutex;
static int        g_discordNonce = 1;
static ULONGLONG  g_rpcStartTime = 0;

static bool            g_trayCreated = false;
static NOTIFYICONDATAW g_nid = {};
static bool            g_reallyQuit = false;

static std::atomic<DWORD> g_gamePid{ 0 };
static std::atomic<bool>  g_gameInjected{ false };
static std::atomic<bool>  g_gameWasRunning{ false };

static std::wstring g_statusText;
static std::wstring g_extractedDLLPath;

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
static IDWriteTextFormat* g_fmtCheckbox = nullptr;

// ==================== DLL Extraction ====================

static std::wstring GetTempDLLPath() {
    wchar_t tempPath[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath) == 0) {
        return L"";
    }
    std::wstring folder = std::wstring(tempPath) + L"SilkWare\\";
    CreateDirectoryW(folder.c_str(), nullptr);
    return folder + L"SilkWareDLL.dll";
}

static bool ExtractDLLFromResources() {
    HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_SILKWARE_DLL), MAKEINTRESOURCEW(IDR_DLL_TYPE));
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(nullptr, hRes);
    if (!hData) return false;

    DWORD sz = SizeofResource(nullptr, hRes);
    if (!sz) return false;

    const void* data = LockResource(hData);
    if (!data) return false;

    g_extractedDLLPath = GetTempDLLPath();
    if (g_extractedDLLPath.empty()) return false;

    // Проверяем, существует ли уже файл с правильным размером
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(g_extractedDLLPath.c_str(), GetFileExInfoStandard, &fad)) {
        if (fad.nFileSizeLow == sz && fad.nFileSizeHigh == 0) {
            return true; // DLL уже извлечена
        }
    }

    HANDLE hFile = CreateFileW(g_extractedDLLPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, data, sz, &written, nullptr);
    CloseHandle(hFile);

    return ok && written == sz;
}

static void CleanupExtractedDLL() {
    if (!g_extractedDLLPath.empty()) {
        // Пробуем удалить, но не критично если не получится
        DeleteFileW(g_extractedDLLPath.c_str());
    }
}

// ==================== Utility Functions ====================

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
    return w;
}

// ==================== Discord RPC ====================

#pragma pack(push, 1)
struct DiscordHeader { int32_t opcode; int32_t length; };
#pragma pack(pop)

static bool DiscordPipeWrite(int32_t op, const std::string& json) {
    if (g_discordPipe == INVALID_HANDLE_VALUE) return false;
    DiscordHeader h; h.opcode = op; h.length = (int32_t)json.size();
    DWORD w = 0;
    if (!WriteFile(g_discordPipe, &h, sizeof(h), &w, nullptr)) return false;
    if (!WriteFile(g_discordPipe, json.c_str(), (DWORD)json.size(), &w, nullptr)) return false;
    return true;
}

static bool DiscordPipeRead(std::string& out) {
    DiscordHeader h; DWORD r = 0;
    if (!ReadFile(g_discordPipe, &h, sizeof(h), &r, nullptr) || r < sizeof(h)) return false;
    if (h.length <= 0 || h.length > 65536) return false;
    std::string buf(h.length, '\0');
    if (!ReadFile(g_discordPipe, &buf[0], h.length, &r, nullptr)) return false;
    out = buf; return true;
}

static void DiscordConnect() {
    std::lock_guard<std::mutex> lock(g_discordMutex);
    if (g_discordConnected) return;
    for (int i = 0; i < 10; i++) {
        wchar_t pn[64]; wsprintfW(pn, L"\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE p = CreateFileW(pn, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (p != INVALID_HANDLE_VALUE) { g_discordPipe = p; break; }
    }
    if (g_discordPipe == INVALID_HANDLE_VALUE) return;
    std::string hs = "{\"v\":1,\"client_id\":\""; hs += DISCORD_APP_ID; hs += "\"}";
    if (!DiscordPipeWrite(0, hs)) { CloseHandle(g_discordPipe); g_discordPipe = INVALID_HANDLE_VALUE; return; }
    std::string resp;
    if (!DiscordPipeRead(resp)) { CloseHandle(g_discordPipe); g_discordPipe = INVALID_HANDLE_VALUE; return; }
    g_discordConnected = true; g_rpcStartTime = (ULONGLONG)time(nullptr);
}

static void DiscordDisconnect() {
    std::lock_guard<std::mutex> lock(g_discordMutex);
    if (g_discordPipe != INVALID_HANDLE_VALUE) { CloseHandle(g_discordPipe); g_discordPipe = INVALID_HANDLE_VALUE; }
    g_discordConnected = false;
}

static void DiscordClearPresence() {
    std::lock_guard<std::mutex> lock(g_discordMutex);
    if (!g_discordConnected || g_discordPipe == INVALID_HANDLE_VALUE) return;
    char ns[32]; sprintf_s(ns, "%d", g_discordNonce++);
    char ps[16]; sprintf_s(ps, "%lu", GetCurrentProcessId());
    std::string j = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
    j += ps; j += ",\"activity\":null},\"nonce\":\""; j += ns; j += "\"}";
    if (!DiscordPipeWrite(1, j)) { CloseHandle(g_discordPipe); g_discordPipe = INVALID_HANDLE_VALUE; g_discordConnected = false; }
    else { std::string r; DWORD m = PIPE_NOWAIT; SetNamedPipeHandleState(g_discordPipe, &m, nullptr, nullptr); DiscordPipeRead(r); m = PIPE_WAIT; SetNamedPipeHandleState(g_discordPipe, &m, nullptr, nullptr); }
}

static void DiscordUpdatePresence() {
    std::lock_guard<std::mutex> lock(g_discordMutex);
    if (!g_discordConnected || g_discordPipe == INVALID_HANDLE_VALUE) return;
    std::string state, details;
    if (g_gameInjected.load()) { details = "In Game"; state = "Playing Garry's Mod"; }
    else { details = "In Loader"; state = "Selecting options..."; }
    char ns[32]; sprintf_s(ns, "%d", g_discordNonce++);
    char ps[16]; sprintf_s(ps, "%lu", GetCurrentProcessId());
    char ts[32]; sprintf_s(ts, "%llu", g_rpcStartTime);
    std::string j = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
    j += ps; j += ",\"activity\":{\"details\":\"" + details + "\",\"state\":\"" + state + "\",";
    j += "\"timestamps\":{\"start\":"; j += ts; j += "},";
    j += "\"assets\":{\"large_image\":\"silkware_logo\",\"large_text\":\"SilkWare\"}";
    j += "}},\"nonce\":\""; j += ns; j += "\"}";
    if (!DiscordPipeWrite(1, j)) { CloseHandle(g_discordPipe); g_discordPipe = INVALID_HANDLE_VALUE; g_discordConnected = false; }
    else { std::string r; DWORD m = PIPE_NOWAIT; SetNamedPipeHandleState(g_discordPipe, &m, nullptr, nullptr); DiscordPipeRead(r); m = PIPE_WAIT; SetNamedPipeHandleState(g_discordPipe, &m, nullptr, nullptr); }
}

static void RpcTick() {
    if (!g_rpcEnabled) { if (g_discordConnected) { DiscordClearPresence(); DiscordDisconnect(); } return; }
    if (!g_discordConnected) DiscordConnect();
    if (g_discordConnected) DiscordUpdatePresence();
}

// ==================== System Tray ====================

static void TrayCreate(HWND hwnd) {
    if (g_trayCreated) return;
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = hwnd; g_nid.uID = TRAY_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"SilkWare Loader");
    Shell_NotifyIconW(NIM_ADD, &g_nid); g_trayCreated = true;
}

static void TrayRemove() { if (!g_trayCreated) return; Shell_NotifyIconW(NIM_DELETE, &g_nid); g_trayCreated = false; }

static void TrayShowMenu(HWND hwnd) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_TRAY_SHOW, L"Show");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_TRAY_EXIT, L"Exit");
    POINT pt; GetCursorPos(&pt); SetForegroundWindow(hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(m);
}

// ==================== Process Functions ====================

static DWORD FindProcessId(const wchar_t* name) {
    DWORD pid = 0; HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) { do { if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; } } while (Process32NextW(snap, &pe)); }
    CloseHandle(snap); return pid;
}

static bool IsProcessRunning(DWORD pid) {
    if (!pid) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED;
    DWORD ec = 0; BOOL ok = GetExitCodeProcess(h, &ec); CloseHandle(h);
    return !ok || ec == STILL_ACTIVE;
}

static bool HasModule(DWORD pid, const wchar_t* mod) {
    for (int a = 0; a < 5; a++) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) { DWORD e = GetLastError(); if (e == ERROR_BAD_LENGTH || e == ERROR_PARTIAL_COPY || e == ERROR_ACCESS_DENIED) { Sleep(300); continue; } return false; }
        MODULEENTRY32W me; me.dwSize = sizeof(me); bool found = false;
        if (Module32FirstW(snap, &me)) { do { if (_wcsicmp(me.szModule, mod) == 0) { found = true; break; } } while (Module32NextW(snap, &me)); }
        CloseHandle(snap); return found;
    }
    return false;
}

static bool WaitForModule(DWORD pid, const wchar_t* mod, const wchar_t* display, DWORD timeout = 180000) {
    DWORD t0 = GetTickCount();
    g_statusText = std::wstring(L"Waiting for ") + display + L"...";
    InvalidateRect(g_hwnd, nullptr, FALSE);
    while (GetTickCount() - t0 < timeout) {
        if (!IsProcessRunning(pid)) { DWORD fp = FindProcessId(L"gmod.exe"); if (fp && fp != pid) pid = fp; else if (!fp) { g_statusText = L"Error: Game exited!"; return false; } }
        if (HasModule(pid, mod)) return true;
        Sleep(500);
    }
    g_statusText = std::wstring(L"Error: Timeout ") + mod; return false;
}

static bool WaitForGameReady(DWORD& pid) {
    struct M { const wchar_t* n; const wchar_t* d; };
    M mods[] = { {L"tier0.dll",L"tier0"},{L"vstdlib.dll",L"vstdlib"},{L"engine.dll",L"Engine"},{L"vgui2.dll",L"VGUI"},{L"client.dll",L"Client"},{L"lua_shared.dll",L"Lua"} };
    for (auto& m : mods) { DWORD fp = FindProcessId(L"gmod.exe"); if (fp) pid = fp; if (!WaitForModule(pid, m.n, m.d)) return false; }
    return true;
}

static bool InjectDLL(DWORD pid, const wchar_t* dll) {
    HANDLE hp = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hp) return false;
    size_t sz = (wcslen(dll) + 1) * sizeof(wchar_t);
    void* rm = VirtualAllocEx(hp, nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!rm) { CloseHandle(hp); return false; }
    if (!WriteProcessMemory(hp, rm, dll, sz, nullptr)) { VirtualFreeEx(hp, rm, 0, MEM_RELEASE); CloseHandle(hp); return false; }
    FARPROC pLL = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE ht = CreateRemoteThread(hp, nullptr, 0, (LPTHREAD_START_ROUTINE)pLL, rm, 0, nullptr);
    if (!ht) { VirtualFreeEx(hp, rm, 0, MEM_RELEASE); CloseHandle(hp); return false; }
    WaitForSingleObject(ht, 30000);
    DWORD ec = 0; GetExitCodeThread(ht, &ec);
    CloseHandle(ht); VirtualFreeEx(hp, rm, 0, MEM_RELEASE); CloseHandle(hp);
    return ec != 0;
}

static void SetStatus(const std::wstring& t, AppState s) { g_statusText = t; g_state = s; InvalidateRect(g_hwnd, nullptr, FALSE); }

static void ResetToMenu() {
    g_state = AppState::Menu; g_statusText = L"";
    g_gamePid = 0; g_gameInjected = false; g_gameWasRunning = false;
    KillTimer(g_hwnd, TIMER_SPINNER); KillTimer(g_hwnd, TIMER_GAMEWATCH);
    RpcTick(); InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void CheckGameState() {
    DWORD pid = g_gamePid.load();
    if (!pid) { pid = FindProcessId(L"gmod.exe"); if (pid) g_gamePid = pid; }
    if (!pid) { if (g_gameWasRunning.load()) ResetToMenu(); return; }
    if (!IsProcessRunning(pid)) { DWORD fp = FindProcessId(L"gmod.exe"); if (!fp) { if (g_gameWasRunning.load()) ResetToMenu(); return; } g_gamePid = fp; }
    g_gameWasRunning = true;
}

static DWORD WINAPI InjectionThread(LPVOID) {
    // Извлекаем DLL из ресурсов
    if (!ExtractDLLFromResources()) {
        SetStatus(L"Error: Failed to extract DLL!", AppState::Error);
        return 1;
    }

    std::wstring dll = g_extractedDLLPath;
    if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
        SetStatus(L"Error: DLL not found!", AppState::Error);
        return 1;
    }

    DWORD pid = FindProcessId(L"gmod.exe"); bool wasRunning = pid != 0;
    if (!pid) {
        SetStatus(L"Launching Garry's Mod...", AppState::Launching); RpcTick();
        ShellExecuteW(nullptr, L"open", L"steam://rungameid/4000", nullptr, nullptr, SW_SHOWNORMAL);
        SetStatus(L"Waiting for gmod.exe...", AppState::Launching);
        DWORD t0 = GetTickCount();
        while (GetTickCount() - t0 < 120000) { pid = FindProcessId(L"gmod.exe"); if (pid) break; Sleep(1000); }
        if (!pid) { SetStatus(L"Error: Game not started!", AppState::Error); return 1; }
        Sleep(5000); DWORD fp = FindProcessId(L"gmod.exe"); if (fp && fp != pid) pid = fp;
    }
    g_gamePid = pid; g_gameWasRunning = true; RpcTick();
    if (!WaitForGameReady(pid)) { if (g_state != AppState::Error) SetStatus(g_statusText, AppState::Error); return 1; }
    g_gamePid = pid;
    SetStatus(L"Waiting for initialization...", AppState::Launching);
    int ws = wasRunning ? 3 : 10;
    for (int i = 0; i < ws; i++) { DWORD fp = FindProcessId(L"gmod.exe"); if (!fp) { SetStatus(L"Error: Game exited!", AppState::Error); return 1; } if (fp != pid) { pid = fp; g_gamePid = pid; } Sleep(1000); }
    SetStatus(L"Injecting SilkWare...", AppState::Injecting);
    bool ok = false;
    for (int a = 0; a < 3; a++) { DWORD fp = FindProcessId(L"gmod.exe"); if (fp) { pid = fp; g_gamePid = pid; } if (InjectDLL(pid, dll.c_str())) { ok = true; break; } Sleep(2000); }
    if (!ok) { SetStatus(L"Error: Injection failed!", AppState::Error); return 1; }
    g_gameInjected = true; SetStatus(L"SilkWare injected!", AppState::Done); RpcTick();
    SetTimer(g_hwnd, TIMER_GAMEWATCH, GAMEWATCH_MS, nullptr);
    return 0;
}

// ==================== UI Helpers ====================

static D2D1_RECT_F GetCloseButtonRect() {
    return D2D1::RectF((float)(WND_W - CLOSE_BTN_W), 0, (float)WND_W, (float)TITLEBAR_H);
}

static D2D1_RECT_F GetLaunchButtonRect() {
    float cx = (WND_W - LAUNCH_BTN_W) / 2.0f;
    float cy = (WND_H - LAUNCH_BTN_H) / 2.0f + 20.0f;
    return D2D1::RectF(cx, cy, cx + LAUNCH_BTN_W, cy + LAUNCH_BTN_H);
}

static D2D1_RECT_F GetGitHubButtonRect() {
    return D2D1::RectF((float)NAV_BTN_X, (float)NAV_BTN_Y,
        (float)(NAV_BTN_X + NAV_BTN_W), (float)(NAV_BTN_Y + NAV_BTN_H));
}

static D2D1_RECT_F GetCommitsButtonRect() {
    float x = (float)(NAV_BTN_X + NAV_BTN_W + NAV_BTN_GAP);
    return D2D1::RectF(x, (float)NAV_BTN_Y, x + NAV_BTN_W, (float)(NAV_BTN_Y + NAV_BTN_H));
}

static D2D1_RECT_F GetBackButtonRect() {
    return D2D1::RectF((float)NAV_BTN_X, (float)NAV_BTN_Y,
        (float)(NAV_BTN_X + NAV_BTN_W), (float)(NAV_BTN_Y + NAV_BTN_H));
}

static D2D1_RECT_F GetRpcCheckRect() {
    return D2D1::RectF((float)RPC_CHECK_X, (float)RPC_CHECK_Y,
        (float)(RPC_CHECK_X + RPC_CHECK_W), (float)(RPC_CHECK_Y + RPC_CHECK_H));
}

static bool HitTest(D2D1_RECT_F r, int mx, int my) {
    return mx >= r.left && mx <= r.right && my >= r.top && my <= r.bottom;
}

// ==================== Direct2D ====================

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

static void DisableRoundedCorners(HWND hwnd) {
    enum { DWMWA_WINDOW_CORNER_PREFERENCE_VAL = 33, DWMWCP_DONOTROUND = 1 };
    int pref = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE_VAL, &pref, sizeof(pref));
}

static void RenderCheckbox(float x, float y, float w, float h,
    const wchar_t* label, bool checked, bool hovered)
{
    ID2D1SolidColorBrush* brBox = nullptr, * brChk = nullptr, * brTxt = nullptr, * brHov = nullptr;
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.30f, 0.30f, 0.30f), &brBox);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.12f, 0.12f), &brChk);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.86f, 0.86f), &brTxt);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.18f, 0.18f), &brHov);

    float bs = 14.0f, bx = x, by = y + (h - bs) / 2;
    if (hovered) { D2D1_RECT_F bg = D2D1::RectF(x - 4, y, x + w, y + h); g_renderTarget->FillRectangle(bg, brHov); }

    D2D1_RECT_F boxRc = D2D1::RectF(bx, by, bx + bs, by + bs);
    g_renderTarget->DrawRectangle(boxRc, brBox, 1.5f);
    if (checked) { D2D1_RECT_F inner = D2D1::RectF(bx + 3, by + 3, bx + bs - 3, by + bs - 3); g_renderTarget->FillRectangle(inner, brChk); }

    D2D1_RECT_F textRc = D2D1::RectF(bx + bs + 6, y, x + w, y + h);
    g_renderTarget->DrawText(label, (UINT32)wcslen(label), g_fmtCheckbox, textRc, brTxt);

    if (brBox) brBox->Release(); if (brChk) brChk->Release();
    if (brTxt) brTxt->Release(); if (brHov) brHov->Release();
}

static void RenderCommitsView() {
    ID2D1SolidColorBrush* brText = nullptr, * brDim = nullptr, * brBtnBg = nullptr,
        * brBtnHov = nullptr, * brSep = nullptr, * brSha = nullptr, * brErr = nullptr;
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.86f, 0.86f), &brText);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.50f, 0.50f, 0.50f), &brDim);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.15f, 0.15f, 0.15f), &brBtnBg);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.22f, 0.22f), &brBtnHov);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.12f), &brSep);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.12f, 0.12f), &brSha);
    g_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.15f, 0.15f), &brErr);

    D2D1_RECT_F backRc = GetBackButtonRect();
    g_renderTarget->FillRectangle(backRc, g_backHovered ? brBtnHov : brBtnBg);
    g_renderTarget->DrawText(L"\u2190 Back", 6, g_fmtNavBtn, backRc, brText);

    float contentTop = (float)(NAV_BTN_Y + NAV_BTN_H + 12);
    float cl = 16.0f, cr = WND_W - 16.0f;

    if (g_commitsLoading) {
        D2D1_RECT_F lr = D2D1::RectF(0, contentTop, (float)WND_W, contentTop + 40);
        g_renderTarget->DrawText(L"Loading commits...", 18, g_fmtStatus, lr, brDim);
    }
    else if (!g_commitsError.empty() && g_commits.empty()) {
        D2D1_RECT_F er = D2D1::RectF(0, contentTop, (float)WND_W, contentTop + 40);
        g_renderTarget->DrawText(g_commitsError.c_str(), (UINT32)g_commitsError.size(), g_fmtStatus, er, brErr);
    }
    else {
        float itemH = 54.0f;
        float y0 = contentTop - (float)g_commitsScroll;
        for (size_t i = 0; i < g_commits.size(); i++) {
            float top = y0 + i * itemH, bot = top + itemH;
            if (bot < contentTop || top > WND_H) continue;
            if (i > 0) { D2D1_RECT_F sep = D2D1::RectF(cl, top, cr, top + 1); g_renderTarget->FillRectangle(sep, brSep); }

            D2D1_RECT_F shaRc = D2D1::RectF(cl, top + 4, cl + 60, top + 20);
            g_renderTarget->DrawText(g_commits[i].sha.c_str(), (UINT32)g_commits[i].sha.size(), g_fmtCommitMeta, shaRc, brSha);

            D2D1_RECT_F msgRc = D2D1::RectF(cl + 66, top + 4, cr, top + 22);
            std::wstring msg = g_commits[i].message;
            if (msg.size() > 80) msg = msg.substr(0, 80) + L"...";
            g_renderTarget->DrawText(msg.c_str(), (UINT32)msg.size(), g_fmtCommitMsg, msgRc, brText);

            std::wstring meta = g_commits[i].author + L"  \u00B7  " + g_commits[i].date;
            D2D1_RECT_F metaRc = D2D1::RectF(cl + 66, top + 26, cr, top + 42);
            g_renderTarget->DrawText(meta.c_str(), (UINT32)meta.size(), g_fmtCommitMeta, metaRc, brDim);
        }
    }

    if (brText) brText->Release(); if (brDim) brDim->Release(); if (brBtnBg) brBtnBg->Release();
    if (brBtnHov) brBtnHov->Release(); if (brSep) brSep->Release();
    if (brSha) brSha->Release(); if (brErr) brErr->Release();
}

static void Render(HWND hwnd) {
    CreateD2DResources(hwnd);
    if (!g_renderTarget) return;

    float w = (float)WND_W, h = (float)WND_H;
    g_renderTarget->BeginDraw();
    g_renderTarget->Clear(D2D1::ColorF(0.047f, 0.047f, 0.047f));

    ID2D1SolidColorBrush* brText = nullptr, * brRed = nullptr, * brRedHov = nullptr, * brRedPress = nullptr,
        * brCloseHov = nullptr, * brSpinner = nullptr, * brTitleText = nullptr,
        * brGreen = nullptr, * brError = nullptr, * brBtnBg = nullptr, * brBtnHov = nullptr;
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

    D2D1_RECT_F titleRc = D2D1::RectF(0, 0, w, (float)TITLEBAR_H);
    g_renderTarget->DrawText(L"SilkWare", 8, g_fmtTitle, titleRc, brTitleText);
    D2D1_RECT_F closeRc = GetCloseButtonRect();
    if (g_closeHovered) g_renderTarget->FillRectangle(closeRc, brCloseHov);
    g_renderTarget->DrawText(L"\u00D7", 1, g_fmtCloseX, closeRc, brText);

    if (g_viewMode == ViewMode::Commits) {
        RenderCommitsView();
    }
    else {
        D2D1_RECT_F ghRc = GetGitHubButtonRect();
        g_renderTarget->FillRectangle(ghRc, g_githubHovered ? brBtnHov : brBtnBg);
        g_renderTarget->DrawText(L"GitHub", 6, g_fmtNavBtn, ghRc, brText);

        D2D1_RECT_F cmRc = GetCommitsButtonRect();
        g_renderTarget->FillRectangle(cmRc, g_commitsHovered ? brBtnHov : brBtnBg);
        g_renderTarget->DrawText(L"Commits", 7, g_fmtNavBtn, cmRc, brText);

        RenderCheckbox((float)RPC_CHECK_X, (float)RPC_CHECK_Y,
            (float)RPC_CHECK_W, (float)RPC_CHECK_H,
            L"Discord Rich Presence", g_rpcEnabled, g_rpcCheckHovered);

        if (g_state == AppState::Menu) {
            D2D1_RECT_F btnRc = GetLaunchButtonRect();
            ID2D1SolidColorBrush* br = brRed;
            if (g_launchPressed) br = brRedPress;
            else if (g_launchHovered) br = brRedHov;
            g_renderTarget->FillRectangle(btnRc, br);
            g_renderTarget->DrawText(L"Launch Garry's Mod", 18, g_fmtButton, btnRc, brText);
        }
        else {
            float cx = w / 2, cy = h / 2 + 20.0f;

            ID2D1SolidColorBrush* statusBr = brText;
            if (g_state == AppState::Done) statusBr = brGreen;
            if (g_state == AppState::Error) statusBr = brError;

            float textY = cy - SPINNER_RADIUS - 20.0f;
            D2D1_RECT_F textRc = D2D1::RectF(0, textY - 36, w, textY);
            g_renderTarget->DrawText(g_statusText.c_str(), (UINT32)g_statusText.size(), g_fmtStatus, textRc, statusBr);

            if (g_state == AppState::Launching || g_state == AppState::Injecting) {
                float startRad = g_spinnerAngle * 3.14159265f / 180.0f;
                float endRad = (g_spinnerAngle - SPINNER_ARC_DEG) * 3.14159265f / 180.0f;
                D2D1_POINT_2F sp = D2D1::Point2F(cx + cosf(startRad) * SPINNER_RADIUS, cy + sinf(startRad) * SPINNER_RADIUS);
                D2D1_POINT_2F ep = D2D1::Point2F(cx + cosf(endRad) * SPINNER_RADIUS, cy + sinf(endRad) * SPINNER_RADIUS);

                ID2D1PathGeometry* path = nullptr;
                g_d2dFactory->CreatePathGeometry(&path);
                if (path) {
                    ID2D1GeometrySink* sink = nullptr;
                    path->Open(&sink);
                    if (sink) {
                        sink->BeginFigure(sp, D2D1_FIGURE_BEGIN_HOLLOW);
                        sink->AddArc(D2D1::ArcSegment(ep, D2D1::SizeF(SPINNER_RADIUS, SPINNER_RADIUS), 0,
                            D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
                        sink->EndFigure(D2D1_FIGURE_END_OPEN);
                        sink->Close(); sink->Release();
                    }
                    D2D1_STROKE_STYLE_PROPERTIES ssp = D2D1::StrokeStyleProperties();
                    ssp.startCap = D2D1_CAP_STYLE_FLAT; ssp.endCap = D2D1_CAP_STYLE_FLAT;
                    ID2D1StrokeStyle* ss = nullptr;
                    g_d2dFactory->CreateStrokeStyle(ssp, nullptr, 0, &ss);
                    g_renderTarget->DrawGeometry(path, brSpinner, SPINNER_THICK, ss);
                    if (ss) ss->Release();
                    path->Release();
                }
            }
        }
    }

    if (brText) brText->Release(); if (brRed) brRed->Release(); if (brRedHov) brRedHov->Release();
    if (brRedPress) brRedPress->Release(); if (brCloseHov) brCloseHov->Release();
    if (brSpinner) brSpinner->Release(); if (brTitleText) brTitleText->Release();
    if (brGreen) brGreen->Release(); if (brError) brError->Release();
    if (brBtnBg) brBtnBg->Release(); if (brBtnHov) brBtnHov->Release();

    HRESULT hr = g_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) DiscardD2DResources();
}

// ==================== GitHub API ====================

static std::string ExtractJsonString(const std::string& json, size_t start, const std::string& key) {
    std::string sk = "\"" + key + "\"";
    size_t pos = json.find(sk, start);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + sk.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;
    std::string r;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
            switch (json[pos]) { case '"': r += '"'; break; case '\\': r += '\\'; break; case 'n': r += ' '; break; case 'r': break; case 't': r += ' '; break; default: r += json[pos]; }
        }
        else r += json[pos];
        pos++;
    }
    return r;
}

static std::vector<CommitInfo> ParseCommitsJson(const std::string& json) {
    std::vector<CommitInfo> commits; size_t pos = 0;
    while (pos < json.size()) {
        size_t shaPos = json.find("\"sha\"", pos);
        if (shaPos == std::string::npos) break;
        size_t coPos = json.find("\"commit\"", shaPos);
        if (coPos == std::string::npos) break;
        std::string sha = ExtractJsonString(json, shaPos, "sha");
        size_t mp = json.find("\"message\"", coPos);
        std::string message; if (mp != std::string::npos) message = ExtractJsonString(json, mp, "message");
        size_t ap = json.find("\"author\"", coPos);
        std::string author, date;
        if (ap != std::string::npos) { author = ExtractJsonString(json, ap, "name"); date = ExtractJsonString(json, ap, "date"); }
        if (!sha.empty()) {
            CommitInfo ci;
            ci.sha = Utf8ToWide(sha.substr(0, 7)); ci.message = Utf8ToWide(message);
            ci.author = Utf8ToWide(author); ci.date = (date.size() >= 10) ? Utf8ToWide(date.substr(0, 10)) : Utf8ToWide(date);
            commits.push_back(ci);
        }
        size_t ns = json.find("\"sha\"", shaPos + 5);
        if (ns == std::string::npos) break; pos = ns;
    }
    return commits;
}

static std::string HttpGet(const char* host, const char* path) {
    HINTERNET hI = InternetOpenA("SilkWare/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hI) return "";
    HINTERNET hC = InternetConnectA(hI, host, INTERNET_DEFAULT_HTTPS_PORT, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hC) { InternetCloseHandle(hI); return ""; }
    DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
    HINTERNET hR = HttpOpenRequestA(hC, "GET", path, nullptr, nullptr, nullptr, flags, 0);
    if (!hR) { InternetCloseHandle(hC); InternetCloseHandle(hI); return ""; }
    if (!HttpSendRequestA(hR, nullptr, 0, nullptr, 0)) { InternetCloseHandle(hR); InternetCloseHandle(hC); InternetCloseHandle(hI); return ""; }
    std::string resp; char buf[4096]; DWORD br = 0;
    while (InternetReadFile(hR, buf, sizeof(buf) - 1, &br) && br > 0) { buf[br] = 0; resp.append(buf, br); }
    InternetCloseHandle(hR); InternetCloseHandle(hC); InternetCloseHandle(hI);
    return resp;
}

static DWORD WINAPI LoadCommitsThread(LPVOID) {
    g_commitsLoading = true; g_commitsError.clear();
    std::string json = HttpGet(GITHUB_API_HOST, GITHUB_API_PATH);
    if (json.empty()) { g_commitsError = L"Failed to fetch commits"; g_commitsLoading = false; InvalidateRect(g_hwnd, nullptr, FALSE); return 1; }
    g_commits = ParseCommitsJson(json);
    if (g_commits.empty()) g_commitsError = L"No commits found";
    g_commitsLoaded = true; g_commitsLoading = false;
    InvalidateRect(g_hwnd, nullptr, FALSE);
    return 0;
}

// ==================== Window Procedure ====================

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        DisableRoundedCorners(hwnd);
        TrayCreate(hwnd);
        SetTimer(hwnd, TIMER_RPC, RPC_TIMER_MS, nullptr);
        RpcTick();
        return 0;

    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(hwnd, &ps); Render(hwnd); EndPaint(hwnd, &ps); return 0; }
    case WM_ERASEBKGND: return 1;

    case WM_TIMER:
        if (wp == TIMER_SPINNER) { g_spinnerAngle += SPINNER_SPEED; if (g_spinnerAngle >= 360) g_spinnerAngle -= 360; InvalidateRect(hwnd, nullptr, FALSE); }
        else if (wp == TIMER_RPC) RpcTick();
        else if (wp == TIMER_GAMEWATCH) { CheckGameState(); RpcTick(); InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;

    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK) { ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); }
        else if (lp == WM_RBUTTONUP) TrayShowMenu(hwnd);
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
            int maxS = (int)(g_commits.size() * 54) - (WND_H - NAV_BTN_Y - NAV_BTN_H - 28);
            if (maxS < 0) maxS = 0;
            if (g_commitsScroll > maxS) g_commitsScroll = maxS;
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
        bool nc = HitTest(GetCloseButtonRect(), mx, my);
        if (nc != g_closeHovered) { g_closeHovered = nc; repaint = true; }

        if (g_viewMode == ViewMode::Commits) {
            bool nb = HitTest(GetBackButtonRect(), mx, my);
            if (nb != g_backHovered) { g_backHovered = nb; repaint = true; }
            if (g_githubHovered) { g_githubHovered = false; repaint = true; }
            if (g_commitsHovered) { g_commitsHovered = false; repaint = true; }
            if (g_launchHovered) { g_launchHovered = false; repaint = true; }
            if (g_rpcCheckHovered) { g_rpcCheckHovered = false; repaint = true; }
        }
        else {
            bool ng = HitTest(GetGitHubButtonRect(), mx, my);
            if (ng != g_githubHovered) { g_githubHovered = ng; repaint = true; }
            bool ncm = HitTest(GetCommitsButtonRect(), mx, my);
            if (ncm != g_commitsHovered) { g_commitsHovered = ncm; repaint = true; }
            bool nr = HitTest(GetRpcCheckRect(), mx, my);
            if (nr != g_rpcCheckHovered) { g_rpcCheckHovered = nr; repaint = true; }
            if (g_backHovered) { g_backHovered = false; repaint = true; }
            if (g_state == AppState::Menu) {
                bool nl = HitTest(GetLaunchButtonRect(), mx, my);
                if (nl != g_launchHovered) { g_launchHovered = nl; repaint = true; }
            }
        }
        if (repaint) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSELEAVE: {
        bool r = g_closeHovered || g_launchHovered || g_githubHovered || g_commitsHovered || g_backHovered || g_rpcCheckHovered;
        g_closeHovered = g_launchHovered = g_githubHovered = g_commitsHovered = g_backHovered = g_rpcCheckHovered = false;
        if (r) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        if (HitTest(GetCloseButtonRect(), mx, my)) { ShowWindow(hwnd, SW_HIDE); return 0; }

        if (g_viewMode == ViewMode::Commits) {
            if (HitTest(GetBackButtonRect(), mx, my)) {
                g_viewMode = ViewMode::Main; g_commitsScroll = 0; g_backHovered = false;
                InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
        }
        else {
            if (HitTest(GetGitHubButtonRect(), mx, my)) {
                ShellExecuteW(nullptr, L"open", GITHUB_URL, nullptr, nullptr, SW_SHOWNORMAL); return 0;
            }
            if (HitTest(GetCommitsButtonRect(), mx, my)) {
                g_viewMode = ViewMode::Commits; g_commitsScroll = 0; g_commitsHovered = false;
                if (!g_commitsLoaded && !g_commitsLoading) {
                    HANDLE h = CreateThread(nullptr, 0, LoadCommitsThread, nullptr, 0, nullptr);
                    if (h) CloseHandle(h);
                }
                InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
            if (HitTest(GetRpcCheckRect(), mx, my)) {
                g_rpcEnabled = !g_rpcEnabled; RpcTick();
                InvalidateRect(hwnd, nullptr, FALSE); return 0;
            }
            if (g_state == AppState::Menu && HitTest(GetLaunchButtonRect(), mx, my)) {
                g_launchPressed = true; SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE); return 0;
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
            if (HitTest(GetLaunchButtonRect(), mx, my)) {
                g_state = AppState::Launching; g_statusText = L"Starting..."; g_launchHovered = false;
                SetTimer(hwnd, TIMER_SPINNER, TIMER_MS, nullptr);
                HANDLE h = CreateThread(nullptr, 0, InjectionThread, nullptr, 0, nullptr);
                if (h) CloseHandle(h);
            }
            InvalidateRect(hwnd, nullptr, FALSE); return 0;
        }
        return 0;
    }

    case WM_CLOSE:
        if (!g_reallyQuit) { ShowWindow(hwnd, SW_HIDE); return 0; }
        DestroyWindow(hwnd); return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_SPINNER); KillTimer(hwnd, TIMER_RPC); KillTimer(hwnd, TIMER_GAMEWATCH);
        DiscordDisconnect(); TrayRemove(); DiscardD2DResources();
        CleanupExtractedDLL();
        PostQuitMessage(0); return 0;

    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// ==================== Entry Point ====================

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2dFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_dwFactory));

    auto MakeFmt = [](IDWriteTextFormat** fmt, DWRITE_FONT_WEIGHT wt, float sz,
        DWRITE_TEXT_ALIGNMENT ta, DWRITE_PARAGRAPH_ALIGNMENT pa,
        DWRITE_WORD_WRAPPING wrap = DWRITE_WORD_WRAPPING_WRAP) {
            g_dwFactory->CreateTextFormat(L"Segoe UI", nullptr, wt,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sz, L"en-us", fmt);
            if (*fmt) { (*fmt)->SetTextAlignment(ta); (*fmt)->SetParagraphAlignment(pa); (*fmt)->SetWordWrapping(wrap); }
        };

    MakeFmt(&g_fmtTitle, DWRITE_FONT_WEIGHT_BOLD, 17.0f,
        DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    MakeFmt(&g_fmtButton, DWRITE_FONT_WEIGHT_SEMI_BOLD, 16.0f,
        DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    MakeFmt(&g_fmtStatus, DWRITE_FONT_WEIGHT_NORMAL, 18.0f,
        DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    MakeFmt(&g_fmtCloseX, DWRITE_FONT_WEIGHT_NORMAL, 16.0f,
        DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    MakeFmt(&g_fmtNavBtn, DWRITE_FONT_WEIGHT_NORMAL, 12.0f,
        DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    MakeFmt(&g_fmtCommitMsg, DWRITE_FONT_WEIGHT_NORMAL, 12.0f,
        DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_WORD_WRAPPING_NO_WRAP);
    MakeFmt(&g_fmtCommitMeta, DWRITE_FONT_WEIGHT_NORMAL, 10.0f,
        DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    MakeFmt(&g_fmtCheckbox, DWRITE_FONT_WEIGHT_NORMAL, 12.0f,
        DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    const wchar_t CLASS_NAME[] = L"SilkWareLoaderClass";
    WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExW(0, CLASS_NAME, L"SilkWare", WS_POPUP | WS_VISIBLE,
        (sw - WND_W) / 2, (sh - WND_H) / 2, WND_W, WND_H, nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;

    ShowWindow(g_hwnd, SW_SHOW); UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    if (g_fmtTitle) g_fmtTitle->Release();
    if (g_fmtButton) g_fmtButton->Release();
    if (g_fmtStatus) g_fmtStatus->Release();
    if (g_fmtCloseX) g_fmtCloseX->Release();
    if (g_fmtNavBtn) g_fmtNavBtn->Release();
    if (g_fmtCommitMsg) g_fmtCommitMsg->Release();
    if (g_fmtCommitMeta) g_fmtCommitMeta->Release();
    if (g_fmtCheckbox) g_fmtCheckbox->Release();
    if (g_dwFactory) g_dwFactory->Release();
    if (g_d2dFactory) g_d2dFactory->Release();
    CoUninitialize();
    return (int)msg.wParam;
}
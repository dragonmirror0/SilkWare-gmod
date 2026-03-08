#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <atomic>
#include <vector>

#include "resource.h"

static HMODULE g_hModule = nullptr;

typedef int         (*fn_lua_pcall_t)(void* L, int nargs, int nresults, int errfunc);
typedef const char* (*fn_lua_tolstring_t)(void* L, int idx, size_t* len);
typedef void        (*fn_lua_settop_t)(void* L, int idx);
typedef int         (*fn_lua_gettop_t)(void* L);
typedef int         (*fn_lua_type_t)(void* L, int idx);
typedef void        (*fn_lua_getfield_t)(void* L, int idx, const char* k);
typedef int         (*fn_lua_toboolean_t)(void* L, int idx);
typedef int         (*fn_luaL_loadbufferx_t)(void* L, const char* buf, size_t sz, const char* name, const char* mode);

static fn_lua_pcall_t        g_pcall = nullptr;
static fn_lua_tolstring_t    g_tolstring = nullptr;
static fn_lua_settop_t       g_settop = nullptr;
static fn_lua_gettop_t       g_gettop = nullptr;
static fn_lua_type_t         g_type = nullptr;
static fn_lua_getfield_t     g_getfield = nullptr;
static fn_lua_toboolean_t    g_toboolean = nullptr;
static fn_luaL_loadbufferx_t g_loadbufferx = nullptr;

#define LUA_TNIL            0
#define LUA_TBOOLEAN        1
#define LUA_TNUMBER         3
#define LUA_TSTRING         4
#define LUA_TTABLE          5
#define LUA_TFUNCTION       6
#define LUA_GLOBALSINDEX    (-10002)

static std::atomic<bool> g_scriptReady{ false };
static std::atomic<bool> g_hookActive{ false };
static std::atomic<int>  g_hookCallCount{ 0 };
static std::atomic<bool> g_shouldRun{ true };
static std::atomic<bool> g_injectedThisState{ false };
static std::atomic<int>  g_failCount{ 0 };
static std::atomic<DWORD> g_lastFailTime{ 0 };
static std::atomic<void*> g_lastClientState{ nullptr };
static std::atomic<DWORD> g_lastCheckTime{ 0 };
static std::atomic<int>  g_callsSinceInject{ 0 };
static std::atomic<int>  g_readyCounter{ 0 };
static std::atomic<DWORD> g_firstReadyTime{ 0 };
static std::atomic<bool> g_injecting{ false };

static constexpr int MAX_INJECT_ATTEMPTS = 10;
static constexpr DWORD RETRY_DELAY_MS = 5000;
static constexpr DWORD MARK_CHECK_INTERVAL_MS = 5000;
static constexpr int CALLS_BEFORE_MARK_CHECK = 500;
static constexpr int READY_THRESHOLD = 200;
static constexpr DWORD READY_TIME_MS = 3000;

static const char* g_cachedScript = nullptr;
static size_t g_cachedScriptLen = 0;
static char* g_scriptBuffer = nullptr;

struct InstrInfo {
    size_t length;
    bool hasRipRelative;
    size_t ripOffset;
    int32_t ripDisp;
    uint8_t destReg;
};

static InstrInfo AnalyzeInstruction(const uint8_t* code) {
    InstrInfo info = { 0, false, 0, 0, 0 };
    const uint8_t* p = code;
    const uint8_t* start = code;
    bool hasRex = false;
    uint8_t rex = 0;
    bool hasOperandOverride = false;
    while (*p == 0x66 || *p == 0x67 || *p == 0xF0 || *p == 0xF2 || *p == 0xF3 ||
        *p == 0x2E || *p == 0x3E || *p == 0x26 || *p == 0x64 || *p == 0x65 || *p == 0x36) {
        if (*p == 0x66) hasOperandOverride = true;
        p++;
    }
    if (*p >= 0x40 && *p <= 0x4F) { rex = *p; hasRex = true; p++; }
    uint8_t op = *p++;
    auto parseModRM = [&](bool hasImm8 = false, bool hasImm32 = false, bool hasImmSized = false) {
        uint8_t modrm = *p++;
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t reg = (modrm >> 3) & 7;
        uint8_t rm = modrm & 7;
        info.destReg = reg | ((rex & 0x04) ? 8 : 0);
        if (mod != 3 && rm == 4) p++;
        if (mod == 0 && rm == 5) {
            info.hasRipRelative = true;
            info.ripOffset = (size_t)(p - start);
            info.ripDisp = *(int32_t*)p;
            p += 4;
        }
        else if (mod == 1) { p++; }
        else if (mod == 2) { p += 4; }
        if (hasImm8) p++;
        if (hasImm32) p += 4;
        if (hasImmSized) p += hasOperandOverride ? 2 : 4;
        info.length = (size_t)(p - start);
    };
    if (op == 0x90) { info.length = (size_t)(p - start); return info; }
    if (op >= 0x50 && op <= 0x5F) { info.length = (size_t)(p - start); return info; }
    if (op == 0xC3 || op == 0xCC || op == 0xC9) { info.length = (size_t)(p - start); return info; }
    if (op == 0xC2) { p += 2; info.length = (size_t)(p - start); return info; }
    if (op >= 0x91 && op <= 0x97) { info.length = (size_t)(p - start); return info; }
    if (op == 0x99 || op == 0x98) { info.length = (size_t)(p - start); return info; }
    if (op == 0x9C || op == 0x9D || op == 0x9E || op == 0x9F) { info.length = (size_t)(p - start); return info; }
    if (op >= 0xF8 && op <= 0xFD) { info.length = (size_t)(p - start); return info; }
    if (op >= 0xB0 && op <= 0xB7) { p++; info.length = (size_t)(p - start); return info; }
    if (op >= 0xB8 && op <= 0xBF) {
        p += (hasRex && (rex & 0x08)) ? 8 : (hasOperandOverride ? 2 : 4);
        info.length = (size_t)(p - start); return info;
    }
    if (op == 0x88 || op == 0x89 || op == 0x8A || op == 0x8B ||
        op == 0x8C || op == 0x8D || op == 0x8E || op == 0x8F) {
        parseModRM(); return info;
    }
    if ((op & 0xC4) == 0x00 && (op & 0x07) <= 0x03) { parseModRM(); return info; }
    if (op == 0x84 || op == 0x85 || op == 0x86 || op == 0x87) { parseModRM(); return info; }
    if (op == 0x31 || op == 0x33 || op == 0x29 || op == 0x2B ||
        op == 0x01 || op == 0x03 || op == 0x09 || op == 0x0B ||
        op == 0x11 || op == 0x13 || op == 0x19 || op == 0x1B ||
        op == 0x21 || op == 0x23 || op == 0x39 || op == 0x3B) {
        parseModRM(); return info;
    }
    if (op == 0x80 || op == 0x82 || op == 0x83) { parseModRM(true); return info; }
    if (op == 0x81) { parseModRM(false, false, true); return info; }
    if (op == 0xC6) { parseModRM(true); return info; }
    if (op == 0xC7) { parseModRM(false, false, true); return info; }
    if (op == 0xA8) { p++; info.length = (size_t)(p - start); return info; }
    if (op == 0xA9) { p += hasOperandOverride ? 2 : 4; info.length = (size_t)(p - start); return info; }
    if (op == 0xF6) {
        uint8_t modrm = *p; uint8_t reg = (modrm >> 3) & 7;
        if (reg <= 1) parseModRM(true); else parseModRM(); return info;
    }
    if (op == 0xF7) {
        uint8_t modrm = *p; uint8_t reg = (modrm >> 3) & 7;
        if (reg <= 1) parseModRM(false, false, true); else parseModRM(); return info;
    }
    if (op == 0xFE || op == 0xFF) { parseModRM(); return info; }
    if (op == 0xD0 || op == 0xD1 || op == 0xD2 || op == 0xD3) { parseModRM(); return info; }
    if (op == 0xC0 || op == 0xC1) { parseModRM(true); return info; }
    if (op == 0xEB) { p++; info.length = (size_t)(p - start); return info; }
    if (op == 0xE9 || op == 0xE8) { p += 4; info.length = (size_t)(p - start); return info; }
    if (op >= 0x70 && op <= 0x7F) { p++; info.length = (size_t)(p - start); return info; }
    if (op == 0x6A) { p++; info.length = (size_t)(p - start); return info; }
    if (op == 0x68) { p += 4; info.length = (size_t)(p - start); return info; }
    if (op == 0xC8) { p += 3; info.length = (size_t)(p - start); return info; }
    if (op == 0x6B) { parseModRM(true); return info; }
    if (op == 0x69) { parseModRM(false, false, true); return info; }
    if (op == 0x63 && hasRex) { parseModRM(); return info; }
    if (op >= 0xA4 && op <= 0xA7) { info.length = (size_t)(p - start); return info; }
    if (op >= 0xAA && op <= 0xAF) { info.length = (size_t)(p - start); return info; }
    if (op == 0x0F) {
        uint8_t op2 = *p++;
        if (op2 >= 0x80 && op2 <= 0x8F) { p += 4; info.length = (size_t)(p - start); return info; }
        if (op2 >= 0x90 && op2 <= 0x9F) { parseModRM(); return info; }
        if (op2 >= 0x40 && op2 <= 0x4F) { parseModRM(); return info; }
        if (op2 == 0xB6 || op2 == 0xB7 || op2 == 0xBE || op2 == 0xBF || op2 == 0xAF) { parseModRM(); return info; }
        if (op2 == 0xBC || op2 == 0xBD || op2 == 0xA3 || op2 == 0xAB || op2 == 0xB3 || op2 == 0xBB) { parseModRM(); return info; }
        if (op2 == 0xBA || op2 == 0xA4 || op2 == 0xAC) { parseModRM(true); return info; }
        if (op2 == 0xA5 || op2 == 0xAD) { parseModRM(); return info; }
        if (op2 == 0x05 || op2 == 0x07 || op2 == 0xA2) { info.length = (size_t)(p - start); return info; }
        if (op2 == 0x1F || op2 == 0x18) { parseModRM(); return info; }
    }
    return info;
}

static fn_luaL_loadbufferx_t g_origLoadBufferX = nullptr;
static fn_luaL_loadbufferx_t g_loadBufferXTarget = nullptr;
static uint8_t  g_savedPrologue[64] = {};
static void* g_trampolineMem = nullptr;
static size_t   g_prologueSize = 0;

#pragma pack(push, 1)
struct Jmp64Safe {
    uint8_t  jmpRipRel[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    uint64_t target = 0;
};
#pragma pack(pop)

static size_t GenerateAbsoluteLoad(uint8_t* dst, uint8_t destReg, uint64_t absoluteAddr) {
    uint8_t* p = dst;
    uint8_t rex = 0x48; if (destReg >= 8) rex |= 0x01;
    *p++ = rex; *p++ = 0xB8 + (destReg & 7);
    *(uint64_t*)p = absoluteAddr; p += 8;
    rex = 0x48; if (destReg >= 8) { rex |= 0x04; rex |= 0x01; }
    *p++ = rex; *p++ = 0x8B; *p++ = ((destReg & 7) << 3) | (destReg & 7);
    return (size_t)(p - dst);
}

static size_t GenerateAbsoluteLea(uint8_t* dst, uint8_t destReg, uint64_t absoluteAddr) {
    uint8_t* p = dst;
    uint8_t rex = 0x48; if (destReg >= 8) rex |= 0x01;
    *p++ = rex; *p++ = 0xB8 + (destReg & 7);
    *(uint64_t*)p = absoluteAddr; p += 8;
    return (size_t)(p - dst);
}

static int Hooked_luaL_loadbufferx(void* L, const char* buf, size_t sz, const char* name, const char* mode);

static bool InstallHook() {
    HMODULE hLua = GetModuleHandleA("lua_shared.dll");
    if (!hLua) return false;
    g_loadBufferXTarget = (fn_luaL_loadbufferx_t)GetProcAddress(hLua, "luaL_loadbufferx");
    if (!g_loadBufferXTarget) return false;
    const uint8_t* func = (const uint8_t*)g_loadBufferXTarget;
    struct InstrData { InstrInfo info; size_t srcOffset; };
    std::vector<InstrData> instructions;
    size_t totalLen = 0;
    while (totalLen < 14) {
        InstrInfo info = AnalyzeInstruction(func + totalLen);
        if (info.length == 0) return false;
        instructions.push_back({ info, totalLen });
        totalLen += info.length;
    }
    g_prologueSize = totalLen;
    memcpy(g_savedPrologue, func, g_prologueSize);
    g_trampolineMem = VirtualAlloc(nullptr, 512, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_trampolineMem) return false;
    uint8_t* tramp = (uint8_t*)g_trampolineMem;
    size_t trampOffset = 0;
    for (const auto& data : instructions) {
        const uint8_t* srcInstr = func + data.srcOffset;
        if (data.info.hasRipRelative) {
            uintptr_t instrEnd = (uintptr_t)(srcInstr + data.info.length);
            uintptr_t targetAddr = instrEnd + data.info.ripDisp;
            bool isLea = false;
            const uint8_t* pp = srcInstr;
            while (*pp == 0x66 || *pp == 0x67 || *pp == 0xF0 || *pp == 0xF2 || *pp == 0xF3 ||
                *pp == 0x2E || *pp == 0x3E || *pp == 0x26 || *pp == 0x64 || *pp == 0x65 || *pp == 0x36) pp++;
            if (*pp >= 0x40 && *pp <= 0x4F) pp++;
            if (*pp == 0x8D) isLea = true;
            size_t genLen = isLea ?
                GenerateAbsoluteLea(tramp + trampOffset, data.info.destReg, targetAddr) :
                GenerateAbsoluteLoad(tramp + trampOffset, data.info.destReg, targetAddr);
            trampOffset += genLen;
        }
        else {
            memcpy(tramp + trampOffset, srcInstr, data.info.length);
            trampOffset += data.info.length;
        }
    }
    Jmp64Safe jmpBack;
    jmpBack.target = (uint64_t)(func + g_prologueSize);
    memcpy(tramp + trampOffset, &jmpBack, sizeof(jmpBack));
    g_origLoadBufferX = (fn_luaL_loadbufferx_t)g_trampolineMem;
    DWORD oldProtect;
    if (!VirtualProtect((void*)func, g_prologueSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(g_trampolineMem, 0, MEM_RELEASE); g_trampolineMem = nullptr; return false;
    }
    memset((void*)func, 0x90, g_prologueSize);
    Jmp64Safe jmpHook;
    jmpHook.target = (uint64_t)&Hooked_luaL_loadbufferx;
    memcpy((void*)func, &jmpHook, sizeof(jmpHook));
    VirtualProtect((void*)func, g_prologueSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void*)func, g_prologueSize);
    g_hookActive = true;
    return true;
}

static void RemoveHook() {
    if (!g_hookActive.load()) return;
    g_hookActive = false;
    Sleep(100);
    int wc = 0;
    while ((g_hookCallCount.load() > 0 || g_injecting.load()) && wc < 100) { Sleep(10); wc++; }
    const uint8_t* func = (const uint8_t*)g_loadBufferXTarget;
    DWORD oldProtect;
    VirtualProtect((void*)func, g_prologueSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)func, g_savedPrologue, g_prologueSize);
    VirtualProtect((void*)func, g_prologueSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void*)func, g_prologueSize);
    Sleep(100);
    if (g_trampolineMem) { VirtualFree(g_trampolineMem, 0, MEM_RELEASE); g_trampolineMem = nullptr; }
    g_origLoadBufferX = nullptr;
}

static bool SEH_QuickIsClient(void* L) {
    __try {
        int top = g_gettop(L);
        if (top < 0 || top > 50000) return false;
        g_getfield(L, LUA_GLOBALSINDEX, "CLIENT");
        int t = g_type(L, -1);
        bool r = (t == LUA_TBOOLEAN && g_toboolean(L, -1));
        g_settop(L, top);
        return r;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SEH_HasGlobals(void* L) {
    __try {
        int top = g_gettop(L);
        if (top < 0) return false;
        const char* tables[] = { "hook", "surface", "net", "timer", "table", "math", "draw", "vgui", "render", "cam" };
        for (int i = 0; i < 10; i++) {
            g_getfield(L, LUA_GLOBALSINDEX, tables[i]);
            bool has = (g_type(L, -1) == LUA_TTABLE);
            g_settop(L, top);
            if (!has) return false;
        }
        g_getfield(L, LUA_GLOBALSINDEX, "LocalPlayer");
        bool has = (g_type(L, -1) == LUA_TFUNCTION);
        g_settop(L, top);
        if (!has) return false;
        g_getfield(L, LUA_GLOBALSINDEX, "string");
        if (g_type(L, -1) != LUA_TTABLE) { g_settop(L, top); return false; }
        g_getfield(L, -1, "format");
        has = (g_type(L, -1) == LUA_TFUNCTION);
        g_settop(L, top);
        return has;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SEH_IsPlayerReady(void* L) {
    __try {
        int top = g_gettop(L);
        if (top < 0) return false;
        const char* checkCode = "return LocalPlayer ~= nil and LocalPlayer() ~= nil and IsValid(LocalPlayer())";
        int lr = g_origLoadBufferX(L, checkCode, strlen(checkCode), "@SilkWare/check", nullptr);
        if (lr != 0) { g_settop(L, top); return false; }
        int pr = g_pcall(L, 0, 1, 0);
        if (pr != 0) { g_settop(L, top); return false; }
        bool ready = (g_type(L, -1) == LUA_TBOOLEAN && g_toboolean(L, -1));
        g_settop(L, top);
        return ready;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SEH_HasMark(void* L) {
    __try {
        int top = g_gettop(L);
        if (top < 0) return false;
        g_getfield(L, LUA_GLOBALSINDEX, "_SILKWARE_INJECTED");
        int t = g_type(L, -1);
        bool r = (t == LUA_TBOOLEAN && g_toboolean(L, -1));
        g_settop(L, top);
        return r;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static int SEH_ExecuteLua(void* L, const char* code, size_t len, const char* chunkName) {
    __try {
        int top = g_gettop(L);
        if (top < 0) return -1;
        int loadResult = g_origLoadBufferX(L, code, len, chunkName, nullptr);
        if (loadResult != 0) { g_settop(L, top); return -2; }
        int pcr = g_pcall(L, 0, 0, 0);
        if (pcr != 0) { g_settop(L, top); return -3; }
        return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -4; }
}

static int SEH_SetMark(void* L) {
    __try {
        const char* markCode = "_SILKWARE_INJECTED=true";
        int mr = g_origLoadBufferX(L, markCode, 23, "@SilkWare/mark", nullptr);
        if (mr == 0) g_pcall(L, 0, 0, 0);
        return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

static int SEH_ClearMark(void* L) {
    __try {
        const char* code = "_SILKWARE_INJECTED=nil";
        int mr = g_origLoadBufferX(L, code, 22, "@SilkWare/unmark", nullptr);
        if (mr == 0) g_pcall(L, 0, 0, 0);
        return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

static bool QuickIsClient(void* L) {
    if (!L || !g_gettop || !g_getfield || !g_type || !g_settop || !g_toboolean) return false;
    return SEH_QuickIsClient(L);
}

static bool HasGlobals(void* L) {
    if (!L || !g_gettop || !g_getfield || !g_type || !g_settop) return false;
    return SEH_HasGlobals(L);
}

static bool IsPlayerReady(void* L) {
    if (!L || !g_gettop || !g_settop || !g_type || !g_toboolean || !g_origLoadBufferX || !g_pcall) return false;
    return SEH_IsPlayerReady(L);
}

static bool HasMark(void* L) {
    if (!L || !g_gettop || !g_getfield || !g_type || !g_settop || !g_toboolean) return false;
    return SEH_HasMark(L);
}

static void ResetInjectionState() {
    g_injectedThisState = false;
    g_failCount = 0;
    g_lastFailTime = 0;
    g_lastCheckTime = 0;
    g_callsSinceInject = 0;
    g_readyCounter = 0;
    g_firstReadyTime = 0;
}

static bool DoInjection(void* L, const char* scriptData, size_t scriptLen) {
    if (!L || !g_gettop || !g_origLoadBufferX || !g_pcall) return false;
    if (!scriptData || scriptLen == 0) return false;
    if (SEH_SetMark(L) != 0) return false;
    int result = SEH_ExecuteLua(L, scriptData, scriptLen, "@SilkWare/init.lua");
    if (result == 0) {
        g_callsSinceInject = 0;
        g_lastCheckTime = GetTickCount();
        return true;
    }
    SEH_ClearMark(L);
    return false;
}

static int Hooked_luaL_loadbufferx(void* L, const char* buf, size_t sz, const char* name, const char* mode) {
    if (!g_hookActive.load())
        return g_origLoadBufferX(L, buf, sz, name, mode);

    g_hookCallCount++;
    int result = g_origLoadBufferX(L, buf, sz, name, mode);

    if (!g_scriptReady.load() || L == nullptr || g_injecting.load()) {
        g_hookCallCount--;
        return result;
    }

    if (g_injectedThisState.load()) {
        void* prev = g_lastClientState.load();
        if (prev != nullptr && prev != L) {
            if (QuickIsClient(L)) {
                ResetInjectionState();
                g_lastClientState = L;
            }
        }
        if (g_injectedThisState.load() && prev == L) {
            g_callsSinceInject++;
            int calls = g_callsSinceInject.load();
            if (calls > CALLS_BEFORE_MARK_CHECK) {
                DWORD now = GetTickCount();
                if (now - g_lastCheckTime.load() > MARK_CHECK_INTERVAL_MS) {
                    g_lastCheckTime = now;
                    g_callsSinceInject = 0;
                    if (!HasMark(L)) ResetInjectionState();
                }
            }
        }
        g_hookCallCount--;
        return result;
    }

    int fails = g_failCount.load();
    if (fails >= MAX_INJECT_ATTEMPTS) { g_hookCallCount--; return result; }
    if (fails > 0 && (GetTickCount() - g_lastFailTime.load()) < RETRY_DELAY_MS) { g_hookCallCount--; return result; }
    if (!QuickIsClient(L)) { g_hookCallCount--; return result; }

    g_lastClientState = L;

    if (!HasGlobals(L)) {
        g_readyCounter = 0;
        g_firstReadyTime = 0;
        g_hookCallCount--;
        return result;
    }

    int counter = g_readyCounter.load();
    if (counter == 0) g_firstReadyTime = GetTickCount();
    g_readyCounter = counter + 1;

    if (g_readyCounter.load() < READY_THRESHOLD) { g_hookCallCount--; return result; }

    DWORD firstReady = g_firstReadyTime.load();
    if (firstReady == 0 || (GetTickCount() - firstReady) < READY_TIME_MS) { g_hookCallCount--; return result; }

    if (!IsPlayerReady(L)) { g_hookCallCount--; return result; }

    bool expected = false;
    if (!g_injectedThisState.compare_exchange_strong(expected, true)) { g_hookCallCount--; return result; }

    g_injecting = true;
    bool ok = DoInjection(L, g_cachedScript, g_cachedScriptLen);
    if (!ok) {
        g_injectedThisState = false;
        g_failCount++;
        g_lastFailTime = GetTickCount();
        g_readyCounter = 0;
        g_firstReadyTime = 0;
    }
    g_injecting = false;

    g_hookCallCount--;
    return result;
}

static HMODULE WaitForModuleDll(const char* name, int maxSeconds) {
    for (int i = 0; i < maxSeconds * 2; i++) {
        if (!g_shouldRun.load()) return nullptr;
        HMODULE h = GetModuleHandleA(name);
        if (h) return h;
        Sleep(500);
    }
    return nullptr;
}

static bool InitLuaFunctions() {
    HMODULE hLua = GetModuleHandleA("lua_shared.dll");
    if (!hLua) hLua = WaitForModuleDll("lua_shared.dll", 30);
    if (!hLua) return false;
    g_loadbufferx = (fn_luaL_loadbufferx_t)GetProcAddress(hLua, "luaL_loadbufferx");
    g_pcall = (fn_lua_pcall_t)GetProcAddress(hLua, "lua_pcall");
    g_tolstring = (fn_lua_tolstring_t)GetProcAddress(hLua, "lua_tolstring");
    g_settop = (fn_lua_settop_t)GetProcAddress(hLua, "lua_settop");
    g_gettop = (fn_lua_gettop_t)GetProcAddress(hLua, "lua_gettop");
    g_type = (fn_lua_type_t)GetProcAddress(hLua, "lua_type");
    g_getfield = (fn_lua_getfield_t)GetProcAddress(hLua, "lua_getfield");
    g_toboolean = (fn_lua_toboolean_t)GetProcAddress(hLua, "lua_toboolean");
    return g_loadbufferx && g_pcall && g_gettop && g_settop && g_getfield && g_type && g_toboolean && g_tolstring;
}

static DWORD WINAPI MainThread(LPVOID) {
    if (!InitLuaFunctions()) return 1;
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (!hClient) hClient = WaitForModuleDll("client.dll", 120);
    if (!hClient) return 1;
    Sleep(2000);

    HRSRC hRes = FindResourceW(g_hModule, MAKEINTRESOURCEW(IDR_LUA_AUTORUN), MAKEINTRESOURCEW(IDR_LUA_TYPE));
    if (!hRes) return 1;
    HGLOBAL hData = LoadResource(g_hModule, hRes);
    if (!hData) return 1;
    DWORD sz = SizeofResource(g_hModule, hRes);
    if (!sz) return 1;
    const char* data = (const char*)LockResource(hData);
    if (!data) return 1;

    g_scriptBuffer = (char*)VirtualAlloc(nullptr, sz + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_scriptBuffer) return 1;
    memcpy(g_scriptBuffer, data, sz);
    g_scriptBuffer[sz] = 0;
    g_cachedScript = g_scriptBuffer;
    g_cachedScriptLen = sz;
    g_scriptReady = true;

    if (!InstallHook()) return 1;

    while (g_shouldRun.load()) Sleep(1000);
    RemoveHook();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE h = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        g_shouldRun = false;
        if (g_hookActive.load()) RemoveHook();
        if (g_scriptBuffer) { VirtualFree(g_scriptBuffer, 0, MEM_RELEASE); g_scriptBuffer = nullptr; }
    }
    return TRUE;
}

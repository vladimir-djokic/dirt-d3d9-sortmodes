// dirt-d3d9-sortmodes: a minimal Direct3D 9 proxy DLL.
//
// Old games such as Colin McRae: DiRT (2007) and Race Driver: GRID (2008) break
// on displays that report many modes. DiRT builds a sorted list and crashes
// when the driver reports modes out of order (for example an LG TV listing
// 1600x1200 after 2560x1600). This proxy loads the real d3d9.dll
// from the Windows system folder, passes every call straight through, and only
// changes IDirect3D9::GetAdapterModeCount / EnumAdapterModes so that modes are
// returned sorted by width, height and refresh rate.
//
// Optional settings in d3d9_sortmodes.ini next to the DLL:
//   [modes]
//   only_aspect=16:9   ; keep only modes with this exact aspect ratio
//   refresh=60         ; list each resolution once, at the rate closest to this
//   log=1              ; write the returned mode lists to d3d9_sortmodes.log
//
// The refresh and only_aspect options exist because both games store the
// detected mode list in a small fixed memory pool (100 KB in DiRT, 16 KB in
// GRID). A TV reporting hundreds of modes fills it, and the settings that come
// after it are silently lost, which crashes the game. Set refresh to the rate
// saved in the game's settings, or the game falls back to a lower resolution.
//
// Build (32-bit MinGW-w64):
//   g++ -O2 -shared -static -s -o d3d9.dll d3d9_sortmodes.cpp d3d9.def -luuid

#include <windows.h>
#include <initguid.h>  // makes <d3d9.h> define IID_IDirect3D9 here
#include <d3d9.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <vector>

static HMODULE g_self = nullptr;
static HMODULE g_real = nullptr;
static CRITICAL_SECTION g_lock;

struct Settings {
    UINT aspectW = 0, aspectH = 0;  // 0 = no filter
    UINT refresh = 0;               // 0 = keep every refresh rate
    bool log = false;
    wchar_t logPath[MAX_PATH] = L"";
};
static Settings g_cfg;

static void LoadSettings() {
    wchar_t ini[MAX_PATH];
    GetModuleFileNameW(g_self, ini, MAX_PATH);
    wchar_t* slash = wcsrchr(ini, L'\\');
    if (slash) slash[1] = 0;
    lstrcpyW(g_cfg.logPath, ini);
    lstrcatW(g_cfg.logPath, L"d3d9_sortmodes.log");
    lstrcatW(ini, L"d3d9_sortmodes.ini");

    wchar_t aspect[32] = L"";
    GetPrivateProfileStringW(L"modes", L"only_aspect", L"", aspect, 32, ini);
    unsigned w = 0, h = 0;
    if (swscanf(aspect, L"%u:%u", &w, &h) == 2 && w && h) { g_cfg.aspectW = w; g_cfg.aspectH = h; }
    g_cfg.refresh = GetPrivateProfileIntW(L"modes", L"refresh", 0, ini);
    g_cfg.log = GetPrivateProfileIntW(L"modes", L"log", 0, ini) != 0;
}

static void Log(const char* fmt, ...) {
    if (!g_cfg.log) return;
    FILE* f = _wfopen(g_cfg.logPath, L"a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}

static bool LoadReal() {
    if (g_real) return true;
    wchar_t path[MAX_PATH];
    GetSystemDirectoryW(path, MAX_PATH);  // SysWOW64 for a 32-bit process
    lstrcatW(path, L"\\d3d9.dll");
    g_real = LoadLibraryW(path);
    return g_real != nullptr;
}

template <typename T> static T RealProc(const char* name) {
    return LoadReal() ? reinterpret_cast<T>(GetProcAddress(g_real, name)) : nullptr;
}

// Wraps the real IDirect3D9 object; everything except mode enumeration is forwarded.
class SortedD3D9 final : public IDirect3D9 {
public:
    explicit SortedD3D9(IDirect3D9* real) : m_real(real) {}

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** out) override {
        if (!out) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDirect3D9) {
            *out = static_cast<IDirect3D9*>(this);
            AddRef();
            return S_OK;
        }
        return m_real->QueryInterface(riid, out);  // e.g. IDirect3D9Ex: hand out the real object
    }
    STDMETHOD_(ULONG, AddRef)() override { return m_real->AddRef(); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG n = m_real->Release();
        if (n == 0) delete this;
        return n;
    }

    // IDirect3D9 - pass-through
    STDMETHOD(RegisterSoftwareDevice)(void* p) override { return m_real->RegisterSoftwareDevice(p); }
    STDMETHOD_(UINT, GetAdapterCount)() override { return m_real->GetAdapterCount(); }
    STDMETHOD(GetAdapterIdentifier)(UINT a, DWORD f, D3DADAPTER_IDENTIFIER9* id) override { return m_real->GetAdapterIdentifier(a, f, id); }
    STDMETHOD(GetAdapterDisplayMode)(UINT a, D3DDISPLAYMODE* m) override { return m_real->GetAdapterDisplayMode(a, m); }
    STDMETHOD(CheckDeviceType)(UINT a, D3DDEVTYPE t, D3DFORMAT af, D3DFORMAT bf, BOOL w) override { return m_real->CheckDeviceType(a, t, af, bf, w); }
    STDMETHOD(CheckDeviceFormat)(UINT a, D3DDEVTYPE t, D3DFORMAT af, DWORD u, D3DRESOURCETYPE r, D3DFORMAT cf) override { return m_real->CheckDeviceFormat(a, t, af, u, r, cf); }
    STDMETHOD(CheckDeviceMultiSampleType)(UINT a, D3DDEVTYPE t, D3DFORMAT f, BOOL w, D3DMULTISAMPLE_TYPE m, DWORD* q) override { return m_real->CheckDeviceMultiSampleType(a, t, f, w, m, q); }
    STDMETHOD(CheckDepthStencilMatch)(UINT a, D3DDEVTYPE t, D3DFORMAT af, D3DFORMAT rf, D3DFORMAT df) override { return m_real->CheckDepthStencilMatch(a, t, af, rf, df); }
    STDMETHOD(CheckDeviceFormatConversion)(UINT a, D3DDEVTYPE t, D3DFORMAT sf, D3DFORMAT tf) override { return m_real->CheckDeviceFormatConversion(a, t, sf, tf); }
    STDMETHOD(GetDeviceCaps)(UINT a, D3DDEVTYPE t, D3DCAPS9* c) override { return m_real->GetDeviceCaps(a, t, c); }
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT a) override { return m_real->GetAdapterMonitor(a); }
    STDMETHOD(CreateDevice)(UINT a, D3DDEVTYPE t, HWND w, DWORD f, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** d) override {
        return m_real->CreateDevice(a, t, w, f, pp, d);
    }

    // IDirect3D9 - sorted mode enumeration
    STDMETHOD_(UINT, GetAdapterModeCount)(UINT a, D3DFORMAT f) override {
        return static_cast<UINT>(Modes(a, f).size());
    }
    STDMETHOD(EnumAdapterModes)(UINT a, D3DFORMAT f, UINT i, D3DDISPLAYMODE* m) override {
        if (!m) return D3DERR_INVALIDCALL;
        const std::vector<D3DDISPLAYMODE>& list = Modes(a, f);
        if (i >= list.size()) return D3DERR_INVALIDCALL;
        *m = list[i];
        return D3D_OK;
    }

private:
    const std::vector<D3DDISPLAYMODE>& Modes(UINT adapter, D3DFORMAT fmt) {
        unsigned long long key = (static_cast<unsigned long long>(adapter) << 32) | static_cast<DWORD>(fmt);
        EnterCriticalSection(&g_lock);
        auto it = m_cache.find(key);
        if (it == m_cache.end()) {
            std::vector<D3DDISPLAYMODE> list;
            UINT n = m_real->GetAdapterModeCount(adapter, fmt);
            for (UINT i = 0; i < n; ++i) {
                D3DDISPLAYMODE m{};
                if (FAILED(m_real->EnumAdapterModes(adapter, fmt, i, &m))) continue;
                if (g_cfg.aspectW && m.Width * g_cfg.aspectH != m.Height * g_cfg.aspectW) continue;
                list.push_back(m);
            }
            std::stable_sort(list.begin(), list.end(), [](const D3DDISPLAYMODE& x, const D3DDISPLAYMODE& y) {
                if (x.Width != y.Width) return x.Width < y.Width;
                if (x.Height != y.Height) return x.Height < y.Height;
                return x.RefreshRate < y.RefreshRate;
            });
            if (g_cfg.refresh) list = OneRatePerResolution(list, g_cfg.refresh);
            Log("adapter %u format %d: driver reported %u modes, returning %u sorted%s%s\n",
                adapter, static_cast<int>(fmt), n, static_cast<unsigned>(list.size()),
                g_cfg.aspectW ? " (aspect filter on)" : "",
                g_cfg.refresh ? " (one refresh rate per resolution)" : "");
            UINT lastW = 0, lastH = 0;
            for (const D3DDISPLAYMODE& m : list) {
                if (m.Width != lastW || m.Height != lastH) Log("  %ux%u\n", m.Width, m.Height);
                lastW = m.Width; lastH = m.Height;
            }
            it = m_cache.emplace(key, std::move(list)).first;
        }
        LeaveCriticalSection(&g_lock);
        return it->second;
    }

    // Keeps one mode per resolution: the refresh rate closest to `want`, preferring
    // the higher rate on a tie. The input must already be sorted.
    static std::vector<D3DDISPLAYMODE> OneRatePerResolution(const std::vector<D3DDISPLAYMODE>& in, UINT want) {
        std::vector<D3DDISPLAYMODE> out;
        for (const D3DDISPLAYMODE& m : in) {
            if (!out.empty() && out.back().Width == m.Width && out.back().Height == m.Height) {
                UINT cur = out.back().RefreshRate;
                UINT dCur = cur > want ? cur - want : want - cur;
                UINT dNew = m.RefreshRate > want ? m.RefreshRate - want : want - m.RefreshRate;
                if (dNew < dCur || (dNew == dCur && m.RefreshRate > cur)) out.back() = m;
            } else {
                out.push_back(m);
            }
        }
        return out;
    }

    IDirect3D9* m_real;
    std::map<unsigned long long, std::vector<D3DDISPLAYMODE>> m_cache;
};

// ---- exports ---------------------------------------------------------------

extern "C" IDirect3D9* WINAPI Proxy_Direct3DCreate9(UINT sdk) {
    auto create = RealProc<IDirect3D9*(WINAPI*)(UINT)>("Direct3DCreate9");
    if (!create) return nullptr;
    IDirect3D9* real = create(sdk);
    if (!real) return nullptr;
    Log("Direct3DCreate9(%u): wrapping real IDirect3D9\n", sdk);
    return new SortedD3D9(real);
}

// The remaining exports are passed through unchanged.
extern "C" HRESULT WINAPI Proxy_Direct3DCreate9Ex(UINT sdk, IDirect3D9Ex** out) {
    auto f = RealProc<HRESULT(WINAPI*)(UINT, IDirect3D9Ex**)>("Direct3DCreate9Ex");
    return f ? f(sdk, out) : D3DERR_NOTAVAILABLE;
}
extern "C" int WINAPI Proxy_D3DPERF_BeginEvent(D3DCOLOR c, LPCWSTR n) {
    auto f = RealProc<int(WINAPI*)(D3DCOLOR, LPCWSTR)>("D3DPERF_BeginEvent");
    return f ? f(c, n) : 0;
}
extern "C" int WINAPI Proxy_D3DPERF_EndEvent() {
    auto f = RealProc<int(WINAPI*)()>("D3DPERF_EndEvent");
    return f ? f() : 0;
}
extern "C" DWORD WINAPI Proxy_D3DPERF_GetStatus() {
    auto f = RealProc<DWORD(WINAPI*)()>("D3DPERF_GetStatus");
    return f ? f() : 0;
}
extern "C" BOOL WINAPI Proxy_D3DPERF_QueryRepeatFrame() {
    auto f = RealProc<BOOL(WINAPI*)()>("D3DPERF_QueryRepeatFrame");
    return f ? f() : FALSE;
}
extern "C" void WINAPI Proxy_D3DPERF_SetMarker(D3DCOLOR c, LPCWSTR n) {
    auto f = RealProc<void(WINAPI*)(D3DCOLOR, LPCWSTR)>("D3DPERF_SetMarker");
    if (f) f(c, n);
}
extern "C" void WINAPI Proxy_D3DPERF_SetOptions(DWORD o) {
    auto f = RealProc<void(WINAPI*)(DWORD)>("D3DPERF_SetOptions");
    if (f) f(o);
}
extern "C" void WINAPI Proxy_D3DPERF_SetRegion(D3DCOLOR c, LPCWSTR n) {
    auto f = RealProc<void(WINAPI*)(D3DCOLOR, LPCWSTR)>("D3DPERF_SetRegion");
    if (f) f(c, n);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = inst;
        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&g_lock);
        LoadSettings();
    }
    return TRUE;
}

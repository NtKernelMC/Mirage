#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_dx9.h"
#include "../third_party/imgui/backends/imgui_impl_win32.h"
#include "../third_party/ImGuiColorTextEdit/TextEditor.h"
#include "../CrashHandler.h"

// imgui_impl_win32.h intentionally keeps WndProc declaration commented out.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using PresentSignature = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using ResetSignature = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using Direct3DCreate9Signature = IDirect3D9* (WINAPI*)(UINT);
using Direct3DCreate9ExSignature = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
using CreateDeviceSignature = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using CreateDeviceExSignature = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**);

enum class ImGuiLuaCommandType
{
    BeginWindow,
    EndWindow,
    BeginChild,
    EndChild,
    BeginTabBar,
    EndTabBar,
    BeginTabItem,
    EndTabItem,
    SetNextWindowPos,
    SetNextWindowSize,
    SetWindowFontScale,
    Text,
    BulletText,
    Separator,
    Spacing,
    SameLine,
    NewLine,
    Dummy,
    SetCursorPos,
    Button,
    GradientButton,
    SmallButton,
    Checkbox,
    SliderFloat,
    SliderInt,
    InputText,
    InputTextMultiline,
    CodeEditorLua,
    Combo,
    ProgressBar,
    Image,
    PushFont,
    PopFont,
    DrawLine,
    DrawRect,
    DrawRectFilled,
    DrawCircle,
    DrawCircleFilled,
    DrawText,
    DrawImage,
    PushStyleColor,
    PopStyleColor,
    PushStyleVarFloat,
    PushStyleVarVec2,
    PopStyleVar
};

struct ImGuiLuaCommand
{
    ImGuiLuaCommandType type{};
    std::string key;
    std::string label;
    std::string payload;
    float f1 = 0.0f;
    float f2 = 0.0f;
    float f3 = 0.0f;
    float f4 = 0.0f;
    float f5 = 0.0f;
    float f6 = 0.0f;
    int i1 = 0;
    int i2 = 0;
    bool b1 = false;
    bool b2 = false;
    ImU32 color = IM_COL32_WHITE;
    ImU32 color2 = IM_COL32_WHITE;
};

struct ImGuiTextureResource
{
    IDirect3DTexture9* texture = nullptr;
    int width = 0;
    int height = 0;
};

struct ImGuiFontRequest
{
    std::string key;
    std::string path;
    float size = 16.0f;
};

struct ImGuiTextureRequest
{
    std::string key;
    std::string path;
};

class SimpleVTableHook
{
public:
    bool install(void** slot_ptr, void* detour)
    {
        if (!slot_ptr || !detour)
            return false;

        DWORD old = 0;
        if (!VirtualProtect(slot_ptr, sizeof(void*), PAGE_EXECUTE_READWRITE, &old))
            return false;

        slot_ = slot_ptr;
        original_ = *slot_ptr;
        detour_ = detour;
        *slot_ptr = detour;

        DWORD restore = 0;
        VirtualProtect(slot_ptr, sizeof(void*), old, &restore);
        return true;
    }

    void remove()
    {
        if (!slot_ || !original_)
        {
            slot_ = nullptr;
            original_ = nullptr;
            detour_ = nullptr;
            return;
        }

        DWORD old = 0;
        if (VirtualProtect(slot_, sizeof(void*), PAGE_EXECUTE_READWRITE, &old))
        {
            if (*slot_ == detour_)
                *slot_ = original_;
            DWORD restore = 0;
            VirtualProtect(slot_, sizeof(void*), old, &restore);
        }
        slot_ = nullptr;
        original_ = nullptr;
        detour_ = nullptr;
    }

    template <typename T>
    T original() const
    {
        return reinterpret_cast<T>(original_);
    }

private:
    void** slot_ = nullptr;
    void* original_ = nullptr;
    void* detour_ = nullptr;
};

static std::atomic_bool g_imgui_hooks_installed = false;
static std::atomic_bool g_imgui_initialized = false;
static std::atomic_bool g_imgui_render_enabled = true;
static std::atomic_bool g_imgui_lua_render_allowed = false;
static std::atomic_bool g_imgui_capture_input = false;
static std::atomic_bool g_imgui_block_mta_binds = false;
static std::atomic_bool g_imgui_force_cursor = false;
static std::atomic_bool g_imgui_draw_cursor = false;
static std::atomic_int g_imgui_theme_request = -1; // 0 dark, 1 light, 2 classic
static std::string g_imgui_ini_path;

static std::mutex g_imgui_mutex;
static std::mutex g_imgui_hook_state_mutex;
static std::vector<ImGuiLuaCommand> g_imgui_queue;
static std::unordered_map<std::string, bool> g_imgui_bool_state;
static std::unordered_map<std::string, float> g_imgui_float_state;
static std::unordered_map<std::string, int> g_imgui_int_state;
static std::unordered_map<std::string, std::string> g_imgui_string_state;
static std::unordered_map<std::string, bool> g_imgui_pulse_state;
static std::unordered_map<std::string, ImGuiTextureResource> g_imgui_textures;
static std::unordered_map<std::string, ImFont*> g_imgui_fonts;
static std::vector<ImGuiFontRequest> g_imgui_pending_fonts;
static std::vector<ImGuiTextureRequest> g_imgui_pending_textures;

struct ImGuiCodeEditorState
{
    std::unique_ptr<TextEditor> editor;
    std::string synced_text;
};
static std::unordered_map<std::string, ImGuiCodeEditorState> g_imgui_code_editors;

static HWND g_imgui_hwnd = nullptr;
static WNDPROC g_imgui_old_wndproc = nullptr;
static IDirect3DDevice9* g_imgui_device = nullptr;

static SimpleVTableHook g_present_vtbl_hook;
static SimpleVTableHook g_reset_vtbl_hook;
static PresentSignature g_present_original = nullptr;
static ResetSignature g_reset_original = nullptr;

static bool g_cursor_visible_by_imgui = false;
static std::atomic_bool g_imgui_hook_thread_running = false;
static std::atomic_bool g_imgui_hook_thread_stop_requested = false;
static std::atomic_bool g_imgui_client_reload_pending = false;
static std::atomic_bool g_imgui_reload_requested = false;
static std::atomic_uint g_imgui_dx9_hook_fail_count = 0;
static std::atomic<DWORD> g_imgui_dx9_hook_last_fail_log = 0;
static std::atomic_bool g_imgui_d3d_factory_hooks_installed = false;
static HMODULE g_imgui_d3d9_module = nullptr;
static void* g_imgui_direct3d_create9_target = nullptr;
static void* g_imgui_direct3d_create9ex_target = nullptr;
static void* g_imgui_create_device_target = nullptr;
static void* g_imgui_create_device_ex_target = nullptr;
static Direct3DCreate9Signature g_imgui_direct3d_create9_original = nullptr;
static Direct3DCreate9ExSignature g_imgui_direct3d_create9ex_original = nullptr;
static CreateDeviceSignature g_imgui_create_device_original = nullptr;
static CreateDeviceExSignature g_imgui_create_device_ex_original = nullptr;
static void** g_imgui_present_slot = nullptr;
static void** g_imgui_reset_slot = nullptr;

static bool InstallDx9Hooks();
static bool ImGuiInstallD3DFactoryHooks();
static bool ImGuiInstallCreateDeviceHooks(IDirect3D9* direct3d);
static bool ImGuiInstallCreateDeviceExHook(IDirect3D9Ex* direct3d_ex);
static void ImGuiRememberDx9Device(IDirect3DDevice9* device);
static void ImGuiNotifyClientDllReload();
static void ImGuiHandleClientDllReload();
static IDirect3D9* WINAPI ImGuiDirect3DCreate9Hook(UINT sdk_version);
static HRESULT WINAPI ImGuiDirect3DCreate9ExHook(UINT sdk_version, IDirect3D9Ex** direct3d_ex);
static HRESULT STDMETHODCALLTYPE ImGuiCreateDeviceHook(IDirect3D9* direct3d, UINT adapter, D3DDEVTYPE device_type, HWND focus_window,
    DWORD behavior_flags, D3DPRESENT_PARAMETERS* presentation_parameters, IDirect3DDevice9** returned_device);
static HRESULT STDMETHODCALLTYPE ImGuiCreateDeviceExHook(IDirect3D9Ex* direct3d, UINT adapter, D3DDEVTYPE device_type, HWND focus_window,
    DWORD behavior_flags, D3DPRESENT_PARAMETERS* presentation_parameters, D3DDISPLAYMODEEX* fullscreen_display_mode,
    IDirect3DDevice9Ex** returned_device);

static inline bool ImGuiIsLuaRenderAllowed()
{
    return g_imgui_render_enabled && g_imgui_lua_render_allowed;
}

static void ImGuiSetLuaRenderAllowed(bool allowed)
{
    g_imgui_lua_render_allowed = allowed;
    if (allowed)
        return;

    g_imgui_capture_input = false;
    g_imgui_block_mta_binds = false;
    g_imgui_force_cursor = false;
    g_imgui_draw_cursor = false;

    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    g_imgui_queue.clear();
}

static inline bool ImGuiIsInputMessage(UINT msg)
{
    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
        return true;
    default:
        return false;
    }
}

static inline UINT ImGuiGetMenuToggleVk()
{
    return mirage.MenuToggleVk <= 0xFF ? static_cast<UINT>(mirage.MenuToggleVk) : VK_F1;
}

static inline bool ImGuiIsMenuToggleMessage(UINT msg, WPARAM wparam)
{
    if (static_cast<UINT>(wparam) != ImGuiGetMenuToggleVk())
        return false;

    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        return true;
    default:
        return false;
    }
}

static inline bool ImGuiIsInitialKeyDown(UINT msg, LPARAM lparam)
{
    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN)
        return false;

    return (lparam & (static_cast<LPARAM>(1) << 30)) == 0;
}

static inline bool ImGuiIsPassthroughHotkey(UINT msg, WPARAM wparam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        break;
    default:
        return false;
    }

    switch (static_cast<UINT>(wparam))
    {
    case VK_F5:
    case VK_ESCAPE:
        return true;
    default:
        return false;
    }
}

static bool ImGuiHandleRenderToggleHotkey(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (!ImGuiIsMenuToggleMessage(msg, wparam))
        return false;

    if (g_imgui_initialized && ImGui::GetCurrentContext())
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

    if (ImGuiIsInitialKeyDown(msg, lparam))
    {
        const bool allowed = !g_imgui_lua_render_allowed.load();
        ImGuiSetLuaRenderAllowed(allowed);
        LogInFile(LOG_NAME, xorstr_("[LOG] ImGui Lua render %s by VK_%u (0x%02X).\n"), allowed ? "enabled" : "disabled", ImGuiGetMenuToggleVk(), ImGuiGetMenuToggleVk());
    }

    return true;
}

static LRESULT CALLBACK ImGuiWndProcHook(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (ImGuiHandleRenderToggleHotkey(hwnd, msg, wparam, lparam))
        return 1;

    bool should_handle = g_imgui_initialized &&
        (g_imgui_capture_input || g_imgui_force_cursor || g_imgui_block_mta_binds);

    if (should_handle && ImGui::GetCurrentContext())
    {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

        if (ImGuiIsInputMessage(msg))
        {
            bool block = g_imgui_block_mta_binds || g_imgui_force_cursor;
            ImGuiIO& io = ImGui::GetIO();
            if (io.WantCaptureMouse || io.WantCaptureKeyboard)
                block = true;

            if (block && !ImGuiIsPassthroughHotkey(msg, wparam))
                return 1;
        }
    }

    if (g_imgui_old_wndproc)
        return CallWindowProc(g_imgui_old_wndproc, hwnd, msg, wparam, lparam);

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void ImGuiUpdateSystemCursor()
{
    const bool should_show = ImGuiIsLuaRenderAllowed() && (g_imgui_force_cursor || g_imgui_capture_input);
    if (should_show == g_cursor_visible_by_imgui)
        return;

    if (should_show)
    {
        while (ShowCursor(TRUE) < 0) {}
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    else
    {
        while (ShowCursor(FALSE) >= 0) {}
    }

    g_cursor_visible_by_imgui = should_show;
}

static const char* ImGuiResolveIniPath()
{
    if (!g_imgui_ini_path.empty())
        return g_imgui_ini_path.c_str();

    char temp_path[MAX_PATH]{};
    DWORD temp_len = GetTempPathA(MAX_PATH, temp_path);
    std::string base;
    if (temp_len > 0 && temp_len < MAX_PATH)
    {
        base.assign(temp_path, temp_len);
    }
    else
    {
        char local_app_data[MAX_PATH]{};
        DWORD env_len = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, MAX_PATH);
        if (env_len == 0 || env_len >= MAX_PATH)
            return nullptr;
        base.assign(local_app_data, env_len);
        if (!base.empty() && base.back() != '\\' && base.back() != '/')
            base.push_back('\\');
        base += "Temp\\";
    }

    if (!base.empty() && base.back() != '\\' && base.back() != '/')
        base.push_back('\\');

    std::string mirage_dir = base + "Mirage\\";
    CreateDirectoryA(mirage_dir.c_str(), nullptr);

    g_imgui_ini_path = mirage_dir + "imgui.ini";
    return g_imgui_ini_path.c_str();
}

static void ImGuiDrawCustomCursor()
{
    if (!g_imgui_draw_cursor || !ImGuiIsLuaRenderAllowed() || !ImGui::GetCurrentContext())
        return;

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 p = io.MousePos;
    if (p.x < 0.0f || p.y < 0.0f)
        return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImU32 fill = IM_COL32(255, 226, 78, 255);
    const ImU32 outline = IM_COL32(92, 36, 2, 255);

    const ImVec2 a(p.x, p.y);
    const ImVec2 b(p.x + 15.0f, p.y + 5.5f);
    const ImVec2 c(p.x + 5.5f, p.y + 18.0f);
    draw->AddTriangleFilled(a, b, c, fill);
    draw->AddTriangle(a, b, c, outline, 1.3f);
    draw->AddLine(c, ImVec2(p.x + 9.0f, p.y + 25.0f), fill, 2.0f);
    draw->AddLine(c, ImVec2(p.x + 9.0f, p.y + 25.0f), outline, 1.0f);
}

static void ImGuiResetDx9HookFailureState()
{
    const unsigned int failed_attempts = g_imgui_dx9_hook_fail_count.exchange(0);
    g_imgui_dx9_hook_last_fail_log = 0;
    if (failed_attempts > 0)
        LogInFile(LOG_NAME, xorstr_("[LOG] DX9 hook install recovered after %u failed attempts.\n"), failed_attempts);
}

static void ImGuiLogDx9HookFailure(const char* format, ...)
{
    const unsigned int fail_count = g_imgui_dx9_hook_fail_count.fetch_add(1) + 1;
    const DWORD now = GetTickCount();
    const DWORD last = g_imgui_dx9_hook_last_fail_log.load();

    if (fail_count > 1 && (fail_count % 20) != 0 && last != 0 && (now - last) < 3000)
        return;

    char message[512]{};
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);

    g_imgui_dx9_hook_last_fail_log = now;
    LogInFile(LOG_NAME, xorstr_("[WARN] DX9 hook install attempt %u failed: %s\n"), fail_count, message);
}

static bool ImGuiEnsureMinHook(void* target, void* detour, void** original, const char* name)
{
    MH_STATUS status = MH_CreateHook(target, detour, reinterpret_cast<LPVOID*>(original));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED)
    {
        ImGuiLogDx9HookFailure("MH_CreateHook failed for %s (status=%d, target=%p)", name, status, target);
        return false;
    }

    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED)
    {
        ImGuiLogDx9HookFailure("MH_EnableHook failed for %s (status=%d, target=%p)", name, status, target);
        return false;
    }

    return true;
}

static void ImGuiRemoveMinHook(void*& target)
{
    if (!target)
        return;

    MH_DisableHook(target);
    MH_RemoveHook(target);
    target = nullptr;
}

static void ImGuiRememberDx9Device(IDirect3DDevice9* device)
{
    if (!device)
        return;

    void** table = *reinterpret_cast<void***>(device);
    if (!table)
        return;

    const bool changed = g_imgui_device != device;
    g_imgui_device = device;
    g_imgui_present_slot = &table[17];
    g_imgui_reset_slot = &table[16];

    if (changed)
        LogInFile(LOG_NAME, xorstr_("[LOG] DX9 device captured: %p\n"), device);
}

static bool ImGuiInstallCreateDeviceHooks(IDirect3D9* direct3d)
{
    if (!direct3d)
        return false;

    void** table = *reinterpret_cast<void***>(direct3d);
    if (!table)
    {
        ImGuiLogDx9HookFailure("IDirect3D9 vtable is null while installing CreateDevice hook");
        return false;
    }

    std::lock_guard<std::mutex> lock(g_imgui_hook_state_mutex);

    void* create_device_target = table[16];
    if (!create_device_target)
    {
        ImGuiLogDx9HookFailure("CreateDevice slot is null");
        return false;
    }

    if (g_imgui_create_device_target == create_device_target && g_imgui_create_device_original)
        return true;

    if (g_imgui_create_device_target && g_imgui_create_device_target != create_device_target)
    {
        ImGuiRemoveMinHook(g_imgui_create_device_target);
        g_imgui_create_device_original = nullptr;
    }

    if (!ImGuiEnsureMinHook(create_device_target, reinterpret_cast<void*>(&ImGuiCreateDeviceHook),
        reinterpret_cast<void**>(&g_imgui_create_device_original), "IDirect3D9::CreateDevice"))
        return false;

    g_imgui_create_device_target = create_device_target;
    LogInFile(LOG_NAME, xorstr_("[LOG] DX9 CreateDevice hook installed.\n"));
    return true;
}

static bool ImGuiInstallCreateDeviceExHook(IDirect3D9Ex* direct3d_ex)
{
    if (!direct3d_ex)
        return true;

    void** table = *reinterpret_cast<void***>(direct3d_ex);
    if (!table)
    {
        ImGuiLogDx9HookFailure("IDirect3D9Ex vtable is null while installing CreateDeviceEx hook");
        return false;
    }

    std::lock_guard<std::mutex> lock(g_imgui_hook_state_mutex);

    void* create_device_ex_target = table[20];
    if (!create_device_ex_target)
    {
        ImGuiLogDx9HookFailure("CreateDeviceEx slot is null");
        return false;
    }

    if (g_imgui_create_device_ex_target == create_device_ex_target && g_imgui_create_device_ex_original)
        return true;

    if (g_imgui_create_device_ex_target && g_imgui_create_device_ex_target != create_device_ex_target)
    {
        ImGuiRemoveMinHook(g_imgui_create_device_ex_target);
        g_imgui_create_device_ex_original = nullptr;
    }

    if (!ImGuiEnsureMinHook(create_device_ex_target, reinterpret_cast<void*>(&ImGuiCreateDeviceExHook),
        reinterpret_cast<void**>(&g_imgui_create_device_ex_original), "IDirect3D9Ex::CreateDeviceEx"))
        return false;

    g_imgui_create_device_ex_target = create_device_ex_target;
    LogInFile(LOG_NAME, xorstr_("[LOG] DX9 CreateDeviceEx hook installed.\n"));
    return true;
}

static IDirect3D9* WINAPI ImGuiDirect3DCreate9Hook(UINT sdk_version)
{
    IDirect3D9* direct3d = g_imgui_direct3d_create9_original ? g_imgui_direct3d_create9_original(sdk_version) : nullptr;
    if (direct3d)
        ImGuiInstallCreateDeviceHooks(direct3d);
    return direct3d;
}

static HRESULT WINAPI ImGuiDirect3DCreate9ExHook(UINT sdk_version, IDirect3D9Ex** direct3d_ex)
{
    HRESULT result = g_imgui_direct3d_create9ex_original ? g_imgui_direct3d_create9ex_original(sdk_version, direct3d_ex) : D3DERR_INVALIDCALL;
    if (SUCCEEDED(result) && direct3d_ex && *direct3d_ex)
    {
        ImGuiInstallCreateDeviceHooks(*direct3d_ex);
        ImGuiInstallCreateDeviceExHook(*direct3d_ex);
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE ImGuiCreateDeviceHook(IDirect3D9* direct3d, UINT adapter, D3DDEVTYPE device_type, HWND focus_window,
    DWORD behavior_flags, D3DPRESENT_PARAMETERS* presentation_parameters, IDirect3DDevice9** returned_device)
{
    HRESULT result = g_imgui_create_device_original ?
        g_imgui_create_device_original(direct3d, adapter, device_type, focus_window, behavior_flags, presentation_parameters, returned_device) :
        D3DERR_INVALIDCALL;

    if (SUCCEEDED(result) && returned_device && *returned_device)
    {
        ImGuiRememberDx9Device(*returned_device);
        InstallDx9Hooks();
    }

    return result;
}

static HRESULT STDMETHODCALLTYPE ImGuiCreateDeviceExHook(IDirect3D9Ex* direct3d, UINT adapter, D3DDEVTYPE device_type, HWND focus_window,
    DWORD behavior_flags, D3DPRESENT_PARAMETERS* presentation_parameters, D3DDISPLAYMODEEX* fullscreen_display_mode,
    IDirect3DDevice9Ex** returned_device)
{
    HRESULT result = g_imgui_create_device_ex_original ?
        g_imgui_create_device_ex_original(direct3d, adapter, device_type, focus_window, behavior_flags, presentation_parameters, fullscreen_display_mode, returned_device) :
        D3DERR_INVALIDCALL;

    if (SUCCEEDED(result) && returned_device && *returned_device)
    {
        ImGuiRememberDx9Device(*returned_device);
        InstallDx9Hooks();
    }

    return result;
}

static bool ImGuiInstallD3DFactoryHooks()
{
    std::unique_lock<std::mutex> lock(g_imgui_hook_state_mutex);

    if (g_imgui_d3d_factory_hooks_installed)
        return true;

    HMODULE d3d9_module = GetModuleHandleA(xorstr_("d3d9.dll"));
    if (!d3d9_module)
        return false;

    void* direct3d_create9_target = reinterpret_cast<void*>(GetProcAddress(d3d9_module, xorstr_("Direct3DCreate9")));
    if (!direct3d_create9_target)
    {
        ImGuiLogDx9HookFailure("GetProcAddress failed for Direct3DCreate9 (gle=%lu)", GetLastError());
        return false;
    }

    if (!ImGuiEnsureMinHook(direct3d_create9_target, reinterpret_cast<void*>(&ImGuiDirect3DCreate9Hook),
        reinterpret_cast<void**>(&g_imgui_direct3d_create9_original), "Direct3DCreate9"))
        return false;

    g_imgui_direct3d_create9_target = direct3d_create9_target;

    void* direct3d_create9ex_target = reinterpret_cast<void*>(GetProcAddress(d3d9_module, xorstr_("Direct3DCreate9Ex")));
    if (direct3d_create9ex_target)
    {
        if (!ImGuiEnsureMinHook(direct3d_create9ex_target, reinterpret_cast<void*>(&ImGuiDirect3DCreate9ExHook),
            reinterpret_cast<void**>(&g_imgui_direct3d_create9ex_original), "Direct3DCreate9Ex"))
            return false;

        g_imgui_direct3d_create9ex_target = direct3d_create9ex_target;
    }

    g_imgui_d3d9_module = d3d9_module;
    g_imgui_d3d_factory_hooks_installed = true;
    lock.unlock();

    if (g_imgui_direct3d_create9_original)
    {
        IDirect3D9* direct3d = g_imgui_direct3d_create9_original(D3D_SDK_VERSION);
        if (direct3d)
        {
            ImGuiInstallCreateDeviceHooks(direct3d);
            direct3d->Release();
        }
    }

    if (g_imgui_direct3d_create9ex_original)
    {
        IDirect3D9Ex* direct3d_ex = nullptr;
        if (SUCCEEDED(g_imgui_direct3d_create9ex_original(D3D_SDK_VERSION, &direct3d_ex)) && direct3d_ex)
        {
            ImGuiInstallCreateDeviceHooks(direct3d_ex);
            ImGuiInstallCreateDeviceExHook(direct3d_ex);
            direct3d_ex->Release();
        }
    }

    LogInFile(LOG_NAME, xorstr_("[LOG] DX9 factory hooks installed (Direct3DCreate9/Ex).\n"));
    return true;
}

static void** get_function_slot(int vtable_index)
{
    if (g_imgui_device)
    {
        __try
        {
            void** table = *reinterpret_cast<void***>(g_imgui_device);
            if (table)
            {
                if (vtable_index == 17)
                    g_imgui_present_slot = &table[17];
                else if (vtable_index == 16)
                    g_imgui_reset_slot = &table[16];
                return &table[vtable_index];
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    if (vtable_index == 17)
        return g_imgui_present_slot;
    if (vtable_index == 16)
        return g_imgui_reset_slot;
    return nullptr;
}

static std::string ImGuiLuaToUtf8(std::string input)
{
    return input;
}

static ImU32 ImGuiColorFromInts(int r, int g, int b, int a)
{
    r = (r < 0) ? 0 : ((r > 255) ? 255 : r);
    g = (g < 0) ? 0 : ((g > 255) ? 255 : g);
    b = (b < 0) ? 0 : ((b > 255) ? 255 : b);
    a = (a < 0) ? 0 : ((a > 255) ? 255 : a);
    return IM_COL32(r, g, b, a);
}

static ImU32 ImGuiScaleRgb(ImU32 color, float multiplier)
{
    int r = static_cast<int>(color & 0xFFu);
    int g = static_cast<int>((color >> 8u) & 0xFFu);
    int b = static_cast<int>((color >> 16u) & 0xFFu);
    int a = static_cast<int>((color >> 24u) & 0xFFu);

    auto clamp_u8 = [](int value) -> int
    {
        if (value < 0) return 0;
        if (value > 255) return 255;
        return value;
    };

    r = clamp_u8(static_cast<int>(r * multiplier));
    g = clamp_u8(static_cast<int>(g * multiplier));
    b = clamp_u8(static_cast<int>(b * multiplier));

    return IM_COL32(r, g, b, a);
}

static ImDrawList* ImGuiSelectDrawList(bool foreground)
{
    return foreground ? ImGui::GetForegroundDrawList() : ImGui::GetBackgroundDrawList();
}

static std::string ImGuiLuaGetArgString(void* lua_vm, int idx, const std::string& fallback = {}, bool convert_to_utf8 = false)
{
    if (!call_tostring)
        return fallback;
    unsigned int len = 0;
    const char* str = call_tostring(lua_vm, idx, &len);
    if (!str)
        return fallback;
    std::string out(str, len);
    return convert_to_utf8 ? ImGuiLuaToUtf8(out) : out;
}

static float ImGuiLuaGetArgFloat(void* lua_vm, int idx, float fallback = 0.0f)
{
    std::string s = ImGuiLuaGetArgString(lua_vm, idx);
    if (s.empty())
        return fallback;

    char* end = nullptr;
    float value = std::strtof(s.c_str(), &end);
    if (!end || end == s.c_str())
        return fallback;
    return value;
}

static int ImGuiLuaGetArgInt(void* lua_vm, int idx, int fallback = 0)
{
    std::string s = ImGuiLuaGetArgString(lua_vm, idx);
    if (s.empty())
        return fallback;

    char* end = nullptr;
    long value = std::strtol(s.c_str(), &end, 10);
    if (!end || end == s.c_str())
        return fallback;
    return static_cast<int>(value);
}

static bool ImGuiLuaGetArgBool(void* lua_vm, int idx, bool fallback = false)
{
    if (!call_toboolean)
        return fallback;
    return call_toboolean(lua_vm, idx) != 0;
}

static std::string ImGuiLuaNormalizeKey(const std::string& preferred, const std::string& fallback)
{
    return preferred.empty() ? fallback : preferred;
}

static bool ImGuiLuaReadBool(const std::string& key, bool fallback)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    auto it = g_imgui_bool_state.find(key);
    if (it == g_imgui_bool_state.end())
    {
        g_imgui_bool_state[key] = fallback;
        return fallback;
    }
    return it->second;
}

static void ImGuiLuaWriteBool(const std::string& key, bool value)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    g_imgui_bool_state[key] = value;
}

static float ImGuiLuaReadFloat(const std::string& key, float fallback)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    auto it = g_imgui_float_state.find(key);
    if (it == g_imgui_float_state.end())
    {
        g_imgui_float_state[key] = fallback;
        return fallback;
    }
    return it->second;
}

static void ImGuiLuaWriteFloat(const std::string& key, float value)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    g_imgui_float_state[key] = value;
}

static int ImGuiLuaReadInt(const std::string& key, int fallback)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    auto it = g_imgui_int_state.find(key);
    if (it == g_imgui_int_state.end())
    {
        g_imgui_int_state[key] = fallback;
        return fallback;
    }
    return it->second;
}

static void ImGuiLuaWriteInt(const std::string& key, int value)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    g_imgui_int_state[key] = value;
}

static std::string ImGuiLuaReadString(const std::string& key, const std::string& fallback)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    auto it = g_imgui_string_state.find(key);
    if (it == g_imgui_string_state.end())
    {
        g_imgui_string_state[key] = fallback;
        return fallback;
    }
    return it->second;
}

static void ImGuiLuaWriteString(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    g_imgui_string_state[key] = value;
}

static void ImGuiLuaSetPulse(const std::string& key, bool value)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    g_imgui_pulse_state[key] = value;
}

static bool ImGuiLuaTakePulse(const std::string& key)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    auto it = g_imgui_pulse_state.find(key);
    if (it == g_imgui_pulse_state.end())
        return false;
    bool pulse = it->second;
    it->second = false;
    return pulse;
}

static LONG ImGuiHandleCodeEditorException(const char* stage, const char* key, PEXCEPTION_POINTERS ep)
{
    if (ep && ep->ExceptionRecord && ep->ContextRecord)
    {
        char reason[128]{};
        sprintf_s(reason, sizeof(reason), "ImGui Lua editor %s exception", stage ? stage : "runtime");
        CrashHandler::WriteCrashReportRecoverable(reason, ep->ExceptionRecord->ExceptionCode, reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress), *ep->ContextRecord);
    }

    LogInFile(LOG_NAME, xorstr_("[ERROR] ImGui Lua editor exception during %s. Render disabled.\n"), stage ? stage : "runtime");
    ImGuiSetLuaRenderAllowed(false);

    if (key && *key)
        g_imgui_code_editors.erase(key);
    else
        g_imgui_code_editors.clear();

    return EXCEPTION_EXECUTE_HANDLER;
}

static void ImGuiInitLuaCodeEditorUnsafe(ImGuiCodeEditorState* state)
{
    if (!state)
        return;

    state->editor = std::make_unique<TextEditor>();
    state->editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
    state->editor->SetShowWhitespaces(false);
    state->editor->SetTabSize(4);
    state->editor->SetHandleMouseInputs(true);
    state->editor->SetHandleKeyboardInputs(true);
    state->editor->SetImGuiChildIgnored(false);

    auto palette = TextEditor::GetDarkPalette();
    palette[(unsigned)TextEditor::PaletteIndex::Default] = IM_COL32(238, 238, 238, 255);
    palette[(unsigned)TextEditor::PaletteIndex::Keyword] = IM_COL32(255, 208, 64, 255);
    palette[(unsigned)TextEditor::PaletteIndex::Number] = IM_COL32(138, 206, 255, 255);
    palette[(unsigned)TextEditor::PaletteIndex::String] = IM_COL32(146, 228, 128, 255);
    palette[(unsigned)TextEditor::PaletteIndex::CharLiteral] = IM_COL32(146, 228, 128, 255);
    palette[(unsigned)TextEditor::PaletteIndex::Comment] = IM_COL32(168, 116, 124, 255);
    palette[(unsigned)TextEditor::PaletteIndex::MultiLineComment] = IM_COL32(168, 116, 124, 255);
    palette[(unsigned)TextEditor::PaletteIndex::Punctuation] = IM_COL32(236, 176, 186, 255);
    palette[(unsigned)TextEditor::PaletteIndex::Identifier] = IM_COL32(236, 236, 236, 255);
    palette[(unsigned)TextEditor::PaletteIndex::Background] = IM_COL32(3, 3, 3, 245);
    palette[(unsigned)TextEditor::PaletteIndex::Cursor] = IM_COL32(255, 235, 120, 255);
    palette[(unsigned)TextEditor::PaletteIndex::Selection] = IM_COL32(112, 24, 36, 190);
    palette[(unsigned)TextEditor::PaletteIndex::LineNumber] = IM_COL32(182, 84, 96, 220);
    palette[(unsigned)TextEditor::PaletteIndex::CurrentLineFill] = IM_COL32(22, 6, 8, 210);
    palette[(unsigned)TextEditor::PaletteIndex::CurrentLineFillInactive] = IM_COL32(14, 4, 6, 170);
    palette[(unsigned)TextEditor::PaletteIndex::CurrentLineEdge] = IM_COL32(192, 44, 62, 255);
    state->editor->SetPalette(palette);
}

static bool ImGuiInitLuaCodeEditorSafe(ImGuiCodeEditorState* state, const char* key)
{
    __try
    {
        ImGuiInitLuaCodeEditorUnsafe(state);
        return true;
    }
    __except (ImGuiHandleCodeEditorException("init", key, GetExceptionInformation()))
    {
        return false;
    }
}

static void ImGuiLuaCodeEditorSetTextUnsafe(ImGuiCodeEditorState* state, const std::string* text)
{
    if (!state || !state->editor || !text)
        return;

    state->editor->SetText(*text);
    state->synced_text = *text;
}

static bool ImGuiLuaCodeEditorSetTextSafe(ImGuiCodeEditorState* state, const std::string* text, const char* key)
{
    __try
    {
        ImGuiLuaCodeEditorSetTextUnsafe(state, text);
        return true;
    }
    __except (ImGuiHandleCodeEditorException("set text", key, GetExceptionInformation()))
    {
        return false;
    }
}

static void ImGuiLuaCodeEditorRenderUnsafe(ImGuiCodeEditorState* state, const char* label, float width, float height)
{
    if (!state || !state->editor)
        return;

    state->editor->Render(label, ImVec2(width, height), true);
}

static bool ImGuiLuaCodeEditorRenderSafe(ImGuiCodeEditorState* state, const char* label, float width, float height, const char* key)
{
    __try
    {
        ImGuiLuaCodeEditorRenderUnsafe(state, label, width, height);
        return true;
    }
    __except (ImGuiHandleCodeEditorException("render", key, GetExceptionInformation()))
    {
        return false;
    }
}

static void ImGuiLuaCodeEditorGetTextUnsafe(ImGuiCodeEditorState* state, std::string* out)
{
    if (!state || !state->editor || !out)
        return;

    *out = state->editor->GetText();
}

static bool ImGuiLuaCodeEditorGetTextSafe(ImGuiCodeEditorState* state, std::string* out, const char* key)
{
    __try
    {
        ImGuiLuaCodeEditorGetTextUnsafe(state, out);
        return true;
    }
    __except (ImGuiHandleCodeEditorException("get text", key, GetExceptionInformation()))
    {
        return false;
    }
}

static void ImGuiLuaQueueCommand(ImGuiLuaCommand&& cmd)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    g_imgui_queue.emplace_back(std::move(cmd));
}

static std::vector<std::string> ImGuiSplitItems(const std::string& src)
{
    std::vector<std::string> out;
    if (src.empty())
        return out;

    size_t start = 0;
    while (start < src.size())
    {
        size_t sep = src.find('|', start);
        if (sep == std::string::npos)
            sep = src.size();
        out.emplace_back(src.substr(start, sep - start));
        start = sep + 1;
    }
    return out;
}

static void ImGuiReleaseTextureResource(ImGuiTextureResource& resource)
{
    if (resource.texture)
    {
        resource.texture->Release();
        resource.texture = nullptr;
    }
    resource.width = 0;
    resource.height = 0;
}

static bool ImGuiLoadTextureNow(const std::string& key, const std::string& path)
{
    if (!g_imgui_device || key.empty() || path.empty())
        return false;

    IDirect3DTexture9* tex = nullptr;
    HRESULT hr = D3DXCreateTextureFromFileA(g_imgui_device, path.c_str(), &tex);
    if (FAILED(hr) || !tex)
        return false;

    D3DSURFACE_DESC desc{};
    if (FAILED(tex->GetLevelDesc(0, &desc)))
    {
        tex->Release();
        return false;
    }

    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    auto& slot = g_imgui_textures[key];
    ImGuiReleaseTextureResource(slot);
    slot.texture = tex;
    slot.width = static_cast<int>(desc.Width);
    slot.height = static_cast<int>(desc.Height);

    return true;
}

static bool ImGuiQueueTextureLoad(const std::string& key, const std::string& path)
{
    if (key.empty() || path.empty())
        return false;
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    g_imgui_pending_textures.push_back({ key, path });
    return true;
}

static bool ImGuiQueueFontLoad(const std::string& key, const std::string& path, float size)
{
    if (key.empty() || path.empty() || size <= 4.0f)
        return false;
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    g_imgui_pending_fonts.push_back({ key, path, size });
    return true;
}

static void ImGuiProcessPendingResources()
{
    std::vector<ImGuiFontRequest> pending_fonts;
    std::vector<ImGuiTextureRequest> pending_textures;
    {
        std::lock_guard<std::mutex> lock(g_imgui_mutex);
        pending_fonts.swap(g_imgui_pending_fonts);
        pending_textures.swap(g_imgui_pending_textures);
    }

    bool fonts_changed = false;
    if (ImGui::GetCurrentContext())
    {
        ImGuiIO& io = ImGui::GetIO();
        static const ImWchar ukrainian_ranges[] = {
            0x0020, 0x00FF,
            0x0400, 0x052F,
            0
        };

        for (const auto& req : pending_fonts)
        {
            ImFont* font = io.Fonts->AddFontFromFileTTF(req.path.c_str(), req.size, nullptr, ukrainian_ranges);
            if (font)
            {
                std::lock_guard<std::mutex> lock(g_imgui_mutex);
                g_imgui_fonts[req.key] = font;
                fonts_changed = true;
            }
        }

        if (fonts_changed)
        {
            ImGui_ImplDX9_InvalidateDeviceObjects();
            ImGui_ImplDX9_CreateDeviceObjects();
        }
    }

    for (const auto& req : pending_textures)
    {
        ImGuiLoadTextureNow(req.key, req.path);
    }
}

static ImGuiTextureResource ImGuiGetTexture(const std::string& key)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    auto it = g_imgui_textures.find(key);
    if (it == g_imgui_textures.end())
        return {};
    return it->second;
}

static ImFont* ImGuiGetFontByKey(const std::string& key)
{
    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    auto it = g_imgui_fonts.find(key);
    if (it == g_imgui_fonts.end())
        return nullptr;
    return it->second;
}

static void ImGuiExecuteLuaQueue()
{
    std::vector<ImGuiLuaCommand> commands;
    {
        std::lock_guard<std::mutex> lock(g_imgui_mutex);
        commands.swap(g_imgui_queue);
    }

    if (!ImGuiIsLuaRenderAllowed())
        return;

    std::vector<bool> tab_bar_stack;
    std::vector<bool> tab_item_stack;
    int inactive_tab_bar_depth = 0;
    int inactive_tab_item_depth = 0;

    for (const auto& cmd : commands)
    {
        if (inactive_tab_bar_depth > 0)
        {
            if (cmd.type == ImGuiLuaCommandType::BeginTabBar)
            {
                tab_bar_stack.push_back(false);
                ++inactive_tab_bar_depth;
                ImGuiLuaSetPulse(cmd.key + "#visible", false);
                continue;
            }
            if (cmd.type == ImGuiLuaCommandType::EndTabBar)
            {
                if (!tab_bar_stack.empty())
                {
                    bool active = tab_bar_stack.back();
                    tab_bar_stack.pop_back();
                    if (active)
                        ImGui::EndTabBar();
                    else if (inactive_tab_bar_depth > 0)
                        --inactive_tab_bar_depth;
                }
                continue;
            }
            if (cmd.type == ImGuiLuaCommandType::BeginTabItem)
            {
                tab_item_stack.push_back(false);
                ++inactive_tab_item_depth;
                ImGuiLuaSetPulse(cmd.key + "#visible", false);
                continue;
            }
            if (cmd.type == ImGuiLuaCommandType::EndTabItem)
            {
                if (!tab_item_stack.empty())
                {
                    bool active = tab_item_stack.back();
                    tab_item_stack.pop_back();
                    if (active)
                        ImGui::EndTabItem();
                    else if (inactive_tab_item_depth > 0)
                        --inactive_tab_item_depth;
                }
                continue;
            }

            continue;
        }

        if (inactive_tab_item_depth > 0)
        {
            if (cmd.type == ImGuiLuaCommandType::BeginTabItem)
            {
                tab_item_stack.push_back(false);
                ++inactive_tab_item_depth;
                ImGuiLuaSetPulse(cmd.key + "#visible", false);
                continue;
            }
            if (cmd.type == ImGuiLuaCommandType::EndTabItem)
            {
                if (!tab_item_stack.empty())
                {
                    bool active = tab_item_stack.back();
                    tab_item_stack.pop_back();
                    if (active)
                        ImGui::EndTabItem();
                    else if (inactive_tab_item_depth > 0)
                        --inactive_tab_item_depth;
                }
                continue;
            }
            if (cmd.type == ImGuiLuaCommandType::EndTabBar)
            {
                if (!tab_bar_stack.empty())
                {
                    bool active = tab_bar_stack.back();
                    tab_bar_stack.pop_back();
                    if (active)
                        ImGui::EndTabBar();
                    else if (inactive_tab_bar_depth > 0)
                        --inactive_tab_bar_depth;
                }
                continue;
            }

            // Skip all commands while traversing an inactive tab content block.
            continue;
        }

        switch (cmd.type)
        {
        case ImGuiLuaCommandType::BeginWindow:
        {
            bool open = ImGuiLuaReadBool(cmd.key, true);
            bool visible = false;
            if (cmd.b1)
                visible = ImGui::Begin(cmd.label.c_str(), &open, cmd.i1);
            else
                visible = ImGui::Begin(cmd.label.c_str(), nullptr, cmd.i1);
            ImGuiLuaWriteBool(cmd.key, open);
            const ImVec2 wnd_pos = ImGui::GetWindowPos();
            const ImVec2 wnd_size = ImGui::GetWindowSize();
            ImGuiLuaWriteFloat(cmd.key + "#x", wnd_pos.x);
            ImGuiLuaWriteFloat(cmd.key + "#y", wnd_pos.y);
            ImGuiLuaWriteFloat(cmd.key + "#w", wnd_size.x);
            ImGuiLuaWriteFloat(cmd.key + "#h", wnd_size.y);
            ImGuiLuaSetPulse(cmd.key + "#visible", visible);
            break;
        }
        case ImGuiLuaCommandType::EndWindow:
            ImGui::End();
            break;
        case ImGuiLuaCommandType::BeginChild:
        {
            bool active = ImGui::BeginChild(cmd.label.c_str(), ImVec2(cmd.f1, cmd.f2), cmd.b1, cmd.i1);
            ImGuiLuaSetPulse(cmd.key + "#visible", active);
            break;
        }
        case ImGuiLuaCommandType::EndChild:
            ImGui::EndChild();
            break;
        case ImGuiLuaCommandType::BeginTabBar:
        {
            bool active = ImGui::BeginTabBar(cmd.label.c_str(), cmd.i1);
            tab_bar_stack.push_back(active);
            if (!active)
                ++inactive_tab_bar_depth;
            ImGuiLuaSetPulse(cmd.key + "#visible", active);
            break;
        }
        case ImGuiLuaCommandType::EndTabBar:
            if (!tab_bar_stack.empty())
            {
                bool active = tab_bar_stack.back();
                tab_bar_stack.pop_back();
                if (active)
                    ImGui::EndTabBar();
                else if (inactive_tab_bar_depth > 0)
                    --inactive_tab_bar_depth;
            }
            break;
        case ImGuiLuaCommandType::BeginTabItem:
        {
            if (tab_bar_stack.empty() || !tab_bar_stack.back())
            {
                tab_item_stack.push_back(false);
                ++inactive_tab_item_depth;
                ImGuiLuaSetPulse(cmd.key + "#visible", false);
                break;
            }

            bool open = ImGuiLuaReadBool(cmd.key, true);
            bool active = ImGui::BeginTabItem(cmd.label.c_str(), cmd.b1 ? &open : nullptr, cmd.i1);
            tab_item_stack.push_back(active);
            if (!active)
                ++inactive_tab_item_depth;
            ImGuiLuaWriteBool(cmd.key, open);
            ImGuiLuaSetPulse(cmd.key + "#visible", active);
            break;
        }
        case ImGuiLuaCommandType::EndTabItem:
            if (!tab_item_stack.empty())
            {
                bool active = tab_item_stack.back();
                tab_item_stack.pop_back();
                if (active)
                    ImGui::EndTabItem();
                else if (inactive_tab_item_depth > 0)
                    --inactive_tab_item_depth;
            }
            break;
        case ImGuiLuaCommandType::SetNextWindowPos:
            ImGui::SetNextWindowPos(ImVec2(cmd.f1, cmd.f2), cmd.i1);
            break;
        case ImGuiLuaCommandType::SetNextWindowSize:
            ImGui::SetNextWindowSize(ImVec2(cmd.f1, cmd.f2), cmd.i1);
            break;
        case ImGuiLuaCommandType::SetWindowFontScale:
            ImGui::SetWindowFontScale(cmd.f1);
            break;
        case ImGuiLuaCommandType::Text:
            ImGui::TextUnformatted(cmd.label.c_str());
            break;
        case ImGuiLuaCommandType::BulletText:
            ImGui::BulletText("%s", cmd.label.c_str());
            break;
        case ImGuiLuaCommandType::Separator:
            ImGui::Separator();
            break;
        case ImGuiLuaCommandType::Spacing:
            ImGui::Spacing();
            break;
        case ImGuiLuaCommandType::SameLine:
            ImGui::SameLine(cmd.f1, cmd.f2);
            break;
        case ImGuiLuaCommandType::NewLine:
            ImGui::NewLine();
            break;
        case ImGuiLuaCommandType::Dummy:
            ImGui::Dummy(ImVec2(cmd.f1, cmd.f2));
            break;
        case ImGuiLuaCommandType::SetCursorPos:
            ImGui::SetCursorPos(ImVec2(cmd.f1, cmd.f2));
            break;
        case ImGuiLuaCommandType::Button:
        {
            bool clicked = ImGui::Button(cmd.label.c_str(), ImVec2(cmd.f1, cmd.f2));
            ImGuiLuaSetPulse(cmd.key, clicked);
            break;
        }
        case ImGuiLuaCommandType::GradientButton:
        {
            ImVec2 size(cmd.f1, cmd.f2);
            if (size.x <= 0.0f)
                size.x = ImGui::CalcTextSize(cmd.label.c_str()).x + (ImGui::GetStyle().FramePadding.x * 4.0f) + 24.0f;
            if (size.y <= 0.0f)
                size.y = ImGui::GetFrameHeight();

            std::string item_id = cmd.key.empty() ? (cmd.label + "##gradient") : cmd.key;
            bool clicked = ImGui::InvisibleButton(item_id.c_str(), size);
            const bool hovered = ImGui::IsItemHovered();
            const bool held = ImGui::IsItemActive();

            ImU32 left_col = cmd.color;
            ImU32 right_col = cmd.color2;
            if (held)
            {
                left_col = ImGuiScaleRgb(left_col, 0.74f);
                right_col = ImGuiScaleRgb(right_col, 0.74f);
            }
            else if (hovered)
            {
                left_col = ImGuiScaleRgb(left_col, 1.24f);
                right_col = ImGuiScaleRgb(right_col, 1.24f);
            }

            ImVec2 p0 = ImGui::GetItemRectMin();
            ImVec2 p1 = ImGui::GetItemRectMax();
            float rounding = (cmd.f3 > 0.0f) ? cmd.f3 : ImGui::GetStyle().FrameRounding;

            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImU32 top_left = ImGuiScaleRgb(left_col, 1.32f);
            ImU32 top_right = ImGuiScaleRgb(right_col, 1.32f);
            ImU32 bottom_left = ImGuiScaleRgb(left_col, 0.62f);
            ImU32 bottom_right = ImGuiScaleRgb(right_col, 0.62f);
            draw->AddRectFilledMultiColor(p0, p1, top_left, top_right, bottom_right, bottom_left);

            // Carve opposite corners to make button silhouette less "default rectangle".
            const float h = p1.y - p0.y;
            const float cut = (h > 10.0f) ? (h * 0.28f) : 0.0f;
            if (cut > 0.0f)
            {
                ImU32 cut_col = ImGui::GetColorU32(ImGuiCol_WindowBg);
                draw->AddTriangleFilled(p0, ImVec2(p0.x + cut, p0.y), ImVec2(p0.x, p0.y + cut), cut_col);
                draw->AddTriangleFilled(p1, ImVec2(p1.x - cut, p1.y), ImVec2(p1.x, p1.y - cut), cut_col);
            }

            draw->AddRect(p0, p1, ImGuiScaleRgb(top_right, 1.16f), rounding, 0, 1.45f);

            ImVec2 text_size = ImGui::CalcTextSize(cmd.label.c_str());
            ImVec2 text_pos(p0.x + (size.x - text_size.x) * 0.5f, p0.y + (size.y - text_size.y) * 0.5f);
            draw->AddText(ImVec2(text_pos.x + 1.0f, text_pos.y + 1.0f), IM_COL32(18, 8, 8, 235), cmd.label.c_str());
            draw->AddText(text_pos, IM_COL32(255, 225, 95, 255), cmd.label.c_str());

            ImGuiLuaSetPulse(cmd.key, clicked);
            break;
        }
        case ImGuiLuaCommandType::SmallButton:
        {
            bool clicked = ImGui::SmallButton(cmd.label.c_str());
            ImGuiLuaSetPulse(cmd.key, clicked);
            break;
        }
        case ImGuiLuaCommandType::Checkbox:
        {
            bool value = ImGuiLuaReadBool(cmd.key, cmd.b1);
            ImGui::Checkbox(cmd.label.c_str(), &value);
            ImGuiLuaWriteBool(cmd.key, value);
            break;
        }
        case ImGuiLuaCommandType::SliderFloat:
        {
            float value = ImGuiLuaReadFloat(cmd.key, cmd.f3);
            const char* fmt = cmd.payload.empty() ? "%.3f" : cmd.payload.c_str();
            ImGui::SliderFloat(cmd.label.c_str(), &value, cmd.f1, cmd.f2, fmt);
            ImGuiLuaWriteFloat(cmd.key, value);
            break;
        }
        case ImGuiLuaCommandType::SliderInt:
        {
            int value = ImGuiLuaReadInt(cmd.key, cmd.i2);
            const char* fmt = cmd.payload.empty() ? "%d" : cmd.payload.c_str();
            ImGui::SliderInt(cmd.label.c_str(), &value, cmd.i1, static_cast<int>(cmd.f1), fmt);
            ImGuiLuaWriteInt(cmd.key, value);
            break;
        }
        case ImGuiLuaCommandType::InputText:
        {
            std::string current = ImGuiLuaReadString(cmd.key, cmd.payload);
            const int max_len = (cmd.i1 <= 1) ? 256 : cmd.i1;
            std::vector<char> buf(static_cast<size_t>(max_len), '\0');
            std::snprintf(buf.data(), buf.size(), "%s", current.c_str());
            ImGui::InputText(cmd.label.c_str(), buf.data(), buf.size(), cmd.i2);
            ImGuiLuaWriteString(cmd.key, std::string(buf.data()));
            break;
        }
        case ImGuiLuaCommandType::InputTextMultiline:
        {
            std::string current = ImGuiLuaReadString(cmd.key, cmd.payload);
            const int max_len = (cmd.i1 <= 1) ? 4096 : cmd.i1;
            std::vector<char> buf(static_cast<size_t>(max_len), '\0');
            std::snprintf(buf.data(), buf.size(), "%s", current.c_str());
            ImGui::InputTextMultiline(cmd.label.c_str(), buf.data(), buf.size(), ImVec2(cmd.f1, cmd.f2), cmd.i2);
            ImGuiLuaWriteString(cmd.key, std::string(buf.data()));
            break;
        }
        case ImGuiLuaCommandType::CodeEditorLua:
        {
            ImGuiCodeEditorState& state = g_imgui_code_editors[cmd.key];
            if (!state.editor)
            {
                if (!ImGuiInitLuaCodeEditorSafe(&state, cmd.key.c_str()))
                    break;
            }

            std::string wanted = ImGuiLuaReadString(cmd.key, cmd.payload);
            if (wanted != state.synced_text)
            {
                if (!ImGuiLuaCodeEditorSetTextSafe(&state, &wanted, cmd.key.c_str()))
                    break;
            }

            if (!ImGuiLuaCodeEditorRenderSafe(&state, cmd.label.c_str(), cmd.f1, cmd.f2, cmd.key.c_str()))
                break;

            std::string edited;
            if (!ImGuiLuaCodeEditorGetTextSafe(&state, &edited, cmd.key.c_str()))
                break;

            if (edited != state.synced_text)
            {
                state.synced_text = edited;
                ImGuiLuaWriteString(cmd.key, edited);
            }
            break;
        }
        case ImGuiLuaCommandType::Combo:
        {
            std::vector<std::string> items = ImGuiSplitItems(cmd.payload);
            std::vector<const char*> citems;
            citems.reserve(items.size());
            for (const auto& item : items)
                citems.push_back(item.c_str());

            int idx = ImGuiLuaReadInt(cmd.key, cmd.i1);
            if (!citems.empty())
            {
                ImGui::Combo(cmd.label.c_str(), &idx, citems.data(), static_cast<int>(citems.size()));
                ImGuiLuaWriteInt(cmd.key, idx);
            }
            break;
        }
        case ImGuiLuaCommandType::ProgressBar:
            ImGui::ProgressBar(cmd.f1, ImVec2(cmd.f2, cmd.f3), cmd.payload.empty() ? nullptr : cmd.payload.c_str());
            break;
        case ImGuiLuaCommandType::Image:
        {
            ImGuiTextureResource tex = ImGuiGetTexture(cmd.key);
            if (tex.texture)
            {
                float width = (cmd.f1 > 0.0f) ? cmd.f1 : static_cast<float>(tex.width);
                float height = (cmd.f2 > 0.0f) ? cmd.f2 : static_cast<float>(tex.height);
                ImGui::ImageWithBg(ImTextureRef((ImTextureID)(uintptr_t)tex.texture),
                    ImVec2(width, height),
                    ImVec2(cmd.f3, cmd.f4),
                    ImVec2(cmd.f5, cmd.f6),
                    ImVec4(0, 0, 0, 0),
                    ImGui::ColorConvertU32ToFloat4(cmd.color));
            }
            break;
        }
        case ImGuiLuaCommandType::PushFont:
        {
            ImFont* font = ImGuiGetFontByKey(cmd.key);
            if (font)
                ImGui::PushFont(font);
            break;
        }
        case ImGuiLuaCommandType::PopFont:
            ImGui::PopFont();
            break;
        case ImGuiLuaCommandType::DrawLine:
        {
            ImDrawList* draw = ImGuiSelectDrawList(cmd.b1);
            draw->AddLine(ImVec2(cmd.f1, cmd.f2), ImVec2(cmd.f3, cmd.f4), cmd.color, cmd.f5);
            break;
        }
        case ImGuiLuaCommandType::DrawRect:
        {
            ImDrawList* draw = ImGuiSelectDrawList(cmd.b1);
            draw->AddRect(ImVec2(cmd.f1, cmd.f2), ImVec2(cmd.f1 + cmd.f3, cmd.f2 + cmd.f4), cmd.color, cmd.f5, 0, cmd.f6);
            break;
        }
        case ImGuiLuaCommandType::DrawRectFilled:
        {
            ImDrawList* draw = ImGuiSelectDrawList(cmd.b1);
            draw->AddRectFilled(ImVec2(cmd.f1, cmd.f2), ImVec2(cmd.f1 + cmd.f3, cmd.f2 + cmd.f4), cmd.color, cmd.f5);
            break;
        }
        case ImGuiLuaCommandType::DrawCircle:
        {
            ImDrawList* draw = ImGuiSelectDrawList(cmd.b1);
            draw->AddCircle(ImVec2(cmd.f1, cmd.f2), cmd.f3, cmd.color, cmd.i1, cmd.f4);
            break;
        }
        case ImGuiLuaCommandType::DrawCircleFilled:
        {
            ImDrawList* draw = ImGuiSelectDrawList(cmd.b1);
            draw->AddCircleFilled(ImVec2(cmd.f1, cmd.f2), cmd.f3, cmd.color, cmd.i1);
            break;
        }
        case ImGuiLuaCommandType::DrawText:
        {
            ImDrawList* draw = ImGuiSelectDrawList(cmd.b1);
            ImFont* font = ImGuiGetFontByKey(cmd.key);
            if (font)
                draw->AddText(font, cmd.f3, ImVec2(cmd.f1, cmd.f2), cmd.color, cmd.payload.c_str());
            else
                draw->AddText(ImVec2(cmd.f1, cmd.f2), cmd.color, cmd.payload.c_str());
            break;
        }
        case ImGuiLuaCommandType::DrawImage:
        {
            ImGuiTextureResource tex = ImGuiGetTexture(cmd.key);
            if (tex.texture)
            {
                ImDrawList* draw = ImGuiSelectDrawList(cmd.b1);
                draw->AddImage(ImTextureRef((ImTextureID)(uintptr_t)tex.texture),
                    ImVec2(cmd.f1, cmd.f2),
                    ImVec2(cmd.f1 + cmd.f3, cmd.f2 + cmd.f4),
                    ImVec2(cmd.f5, cmd.f6),
                    ImVec2(cmd.i1 / 10000.0f, cmd.i2 / 10000.0f),
                    cmd.color);
            }
            break;
        }
        case ImGuiLuaCommandType::PushStyleColor:
            ImGui::PushStyleColor(static_cast<ImGuiCol>(cmd.i1), cmd.color);
            break;
        case ImGuiLuaCommandType::PopStyleColor:
            ImGui::PopStyleColor((cmd.i1 <= 0) ? 1 : cmd.i1);
            break;
        case ImGuiLuaCommandType::PushStyleVarFloat:
            ImGui::PushStyleVar(static_cast<ImGuiStyleVar>(cmd.i1), cmd.f1);
            break;
        case ImGuiLuaCommandType::PushStyleVarVec2:
            ImGui::PushStyleVar(static_cast<ImGuiStyleVar>(cmd.i1), ImVec2(cmd.f1, cmd.f2));
            break;
        case ImGuiLuaCommandType::PopStyleVar:
            ImGui::PopStyleVar((cmd.i1 <= 0) ? 1 : cmd.i1);
            break;
        default:
            break;
        }
    }
}

static void ImGuiDestroyContextSafe()
{
    if (g_imgui_initialized)
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imgui_initialized = false;
    }
}

static void ImGuiResetRenderContextState()
{
    ImGuiDestroyContextSafe();

    std::lock_guard<std::mutex> lock(g_imgui_mutex);
    for (auto& kv : g_imgui_textures)
        ImGuiReleaseTextureResource(kv.second);
    g_imgui_textures.clear();
    g_imgui_fonts.clear();
    g_imgui_code_editors.clear();
    g_imgui_pending_fonts.clear();
    g_imgui_pending_textures.clear();
    g_imgui_queue.clear();
}

static void ImGuiEnsureWndProcHook(IDirect3DDevice9* device)
{
    if (!device)
        return;

    D3DDEVICE_CREATION_PARAMETERS params{};
    if (FAILED(device->GetCreationParameters(&params)))
        return;

    HWND hwnd = params.hFocusWindow;
    if (!hwnd)
        return;

    WNDPROC current_wndproc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
    if (g_imgui_hwnd == hwnd && g_imgui_old_wndproc && current_wndproc == &ImGuiWndProcHook)
        return;

    if (g_imgui_hwnd && g_imgui_old_wndproc)
    {
        SetWindowLongPtr(g_imgui_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_imgui_old_wndproc));
    }

    g_imgui_hwnd = hwnd;
    g_imgui_old_wndproc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(g_imgui_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ImGuiWndProcHook)));
}

static bool ImGuiInitialize(IDirect3DDevice9* device)
{
    if (!device)
        return false;
    if (g_imgui_initialized)
        return true;

    ImGuiEnsureWndProcHook(device);
    if (!g_imgui_hwnd)
        return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = false;
    io.IniFilename = ImGuiResolveIniPath();
    io.LogFilename = nullptr;
    io.MouseDrawCursor = false;
    ImGui::StyleColorsDark();

    static const ImWchar ukrainian_ranges[] = {
        0x0020, 0x00FF,
        0x0400, 0x052F,
        0
    };
    ImFont* default_font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 16.0f, nullptr, ukrainian_ranges);
    if (!default_font)
        default_font = io.Fonts->AddFontDefault();
    {
        std::lock_guard<std::mutex> lock(g_imgui_mutex);
        g_imgui_fonts[xorstr_("default")] = default_font;
    }

    if (!ImGui_ImplWin32_Init(g_imgui_hwnd))
    {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplDX9_Init(device))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    g_imgui_device = device;
    g_imgui_initialized = true;
    LogInFile(LOG_NAME, xorstr_("[LOG] ImGui initialized (DX9 + Win32).\n"));
    return true;
}

static HRESULT __stdcall ImGuiResetHook(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params)
{
    ImGuiRememberDx9Device(device);

    if (g_imgui_initialized)
        ImGui_ImplDX9_InvalidateDeviceObjects();

    HRESULT result = g_reset_original ? g_reset_original(device, params) : D3D_OK;

    if (SUCCEEDED(result) && g_imgui_initialized)
        ImGui_ImplDX9_CreateDeviceObjects();

    return result;
}

static HRESULT __stdcall ImGuiPresentHook(IDirect3DDevice9* device, const RECT* src_rect, const RECT* dst_rect, HWND dst_wnd, const RGNDATA* dirty_region)
{
    ImGuiRememberDx9Device(device);

    if (g_imgui_reload_requested.exchange(false))
    {
        ImGuiResetRenderContextState();
        g_imgui_device = device;
        LogInFile(LOG_NAME, xorstr_("[LOG] ImGui render context reset after client.dll reload.\n"));
    }

    if (!g_imgui_initialized)
    {
        ImGuiInitialize(device);
    }

    if (g_imgui_initialized)
    {
        ImGuiEnsureWndProcHook(device);

        ImGuiUpdateSystemCursor();

        int theme_request = g_imgui_theme_request.exchange(-1);
        if (theme_request == 0) ImGui::StyleColorsDark();
        else if (theme_request == 1) ImGui::StyleColorsLight();
        else if (theme_request == 2) ImGui::StyleColorsClassic();

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = false;

        ImGuiProcessPendingResources();
        ImGuiExecuteLuaQueue();
        ImGuiDrawCustomCursor();

        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    if (g_present_original)
        return g_present_original(device, src_rect, dst_rect, dst_wnd, dirty_region);

    return D3D_OK;
}

static bool ImGuiAreDx9HooksAlive()
{
    void** present_slot = get_function_slot(17);
    void** reset_slot = get_function_slot(16);
    if (!present_slot || !reset_slot)
        return false;
    if (!g_present_original || !g_reset_original)
        return false;

    return (*present_slot == reinterpret_cast<void*>(&ImGuiPresentHook)) &&
        (*reset_slot == reinterpret_cast<void*>(&ImGuiResetHook));
}

static bool ImGuiIsWndProcHookAlive()
{
    if (!g_imgui_hwnd || !g_imgui_old_wndproc)
        return false;

    return reinterpret_cast<WNDPROC>(GetWindowLongPtr(g_imgui_hwnd, GWLP_WNDPROC)) == &ImGuiWndProcHook;
}

static bool InstallDx9Hooks()
{
    std::lock_guard<std::mutex> lock(g_imgui_hook_state_mutex);

    const bool was_marked_installed = g_imgui_hooks_installed.load();
    if (was_marked_installed && ImGuiAreDx9HooksAlive())
        return true;

    if (was_marked_installed)
    {
        LogInFile(LOG_NAME, xorstr_("[WARN] DX9 vtable hooks look overwritten, attempting reinstall.\n"));
        g_present_vtbl_hook.remove();
        g_reset_vtbl_hook.remove();
        g_present_original = nullptr;
        g_reset_original = nullptr;
        g_imgui_hooks_installed = false;
    }

    void** present_slot = get_function_slot(17);
    void** reset_slot = get_function_slot(16);
    if (!present_slot || !reset_slot)
        return false;

    if (!g_present_vtbl_hook.install(present_slot, reinterpret_cast<void*>(&ImGuiPresentHook)))
    {
        ImGuiLogDx9HookFailure("failed to patch Present slot %p (gle=%lu)", present_slot, GetLastError());
        return false;
    }
    g_present_original = g_present_vtbl_hook.original<PresentSignature>();

    if (!g_reset_vtbl_hook.install(reset_slot, reinterpret_cast<void*>(&ImGuiResetHook)))
    {
        g_present_vtbl_hook.remove();
        g_present_original = nullptr;
        ImGuiLogDx9HookFailure("failed to patch Reset slot %p (gle=%lu)", reset_slot, GetLastError());
        return false;
    }
    g_reset_original = g_reset_vtbl_hook.original<ResetSignature>();

    g_imgui_hooks_installed = true;
    ImGuiResetDx9HookFailureState();
    if (was_marked_installed)
        LogInFile(LOG_NAME, xorstr_("[LOG] DX9 vtable hooks reinstalled (Present/Reset).\n"));
    else
        LogInFile(LOG_NAME, xorstr_("[LOG] DX9 vtable hooks installed (Present/Reset).\n"));
    return true;
}

static DWORD WINAPI ImGuiHookInstallThread(LPVOID)
{
    while (!g_imgui_hook_thread_stop_requested)
    {
        if (g_imgui_client_reload_pending.exchange(false))
            ImGuiHandleClientDllReload();

        if (!g_imgui_d3d_factory_hooks_installed)
            ImGuiInstallD3DFactoryHooks();

        if (!ImGuiAreDx9HooksAlive())
        {
            InstallDx9Hooks();
        }

        if (g_imgui_device && !ImGuiIsWndProcHookAlive())
        {
            ImGuiEnsureWndProcHook(g_imgui_device);
            if (ImGuiIsWndProcHookAlive())
                LogInFile(LOG_NAME, xorstr_("[LOG] ImGui WndProc hook restored.\n"));
        }

        Sleep(750);
    }
    g_imgui_hook_thread_running = false;
    return 0;
}

static void StartImGuiHookingAsync()
{
    g_imgui_hook_thread_stop_requested = false;
    if (g_imgui_hook_thread_running.exchange(true))
        return;

    HANDLE h_thread = CreateThread(nullptr, 0, ImGuiHookInstallThread, nullptr, 0, nullptr);
    if (h_thread)
        CloseHandle(h_thread);
    else
        g_imgui_hook_thread_running = false;
}

static void ImGuiNotifyClientDllReload()
{
    g_imgui_client_reload_pending = true;
    StartImGuiHookingAsync();
}

static void ImGuiHandleClientDllReload()
{
    bool had_device = false;

    {
        std::lock_guard<std::mutex> lock(g_imgui_hook_state_mutex);
        had_device = g_imgui_device != nullptr;
        g_present_vtbl_hook.remove();
        g_reset_vtbl_hook.remove();
        g_present_original = nullptr;
        g_reset_original = nullptr;
        g_imgui_hooks_installed = false;
        g_imgui_present_slot = nullptr;
        g_imgui_reset_slot = nullptr;
    }
    g_imgui_reload_requested = true;

    if (had_device)
        LogInFile(LOG_NAME, xorstr_("[WARN] client.dll reload detected, refreshing ImGui DX9 hooks.\n"));
    else
        LogInFile(LOG_NAME, xorstr_("[LOG] client.dll reload detected before DX9 device capture.\n"));

    if (!had_device || !InstallDx9Hooks())
        StartImGuiHookingAsync();

    if (g_imgui_device && !ImGuiIsWndProcHookAlive())
    {
        ImGuiEnsureWndProcHook(g_imgui_device);
        if (ImGuiIsWndProcHookAlive())
            LogInFile(LOG_NAME, xorstr_("[LOG] ImGui WndProc hook restored after client.dll reload.\n"));
    }
}

static void ImGuiEnsureHooksForLuaInvoke()
{
    if (g_imgui_initialized && !ImGui::GetCurrentContext())
    {
        g_imgui_initialized = false;
        LogInFile(LOG_NAME, xorstr_("[WARN] ImGui context disappeared, waiting for reinit.\n"));
    }

    if (!g_imgui_d3d_factory_hooks_installed)
        ImGuiInstallD3DFactoryHooks();

    if (!ImGuiAreDx9HooksAlive())
    {
        if (!InstallDx9Hooks())
            StartImGuiHookingAsync();
    }

    if (g_imgui_device && !ImGuiIsWndProcHookAlive())
    {
        ImGuiEnsureWndProcHook(g_imgui_device);
        if (ImGuiIsWndProcHookAlive())
            LogInFile(LOG_NAME, xorstr_("[LOG] ImGui WndProc hook restored.\n"));
    }
}

static void ShutdownImGuiHooking()
{
    g_imgui_hook_thread_stop_requested = true;
    g_imgui_hook_thread_running = false;
    g_present_vtbl_hook.remove();
    g_reset_vtbl_hook.remove();
    g_present_original = nullptr;
    g_reset_original = nullptr;
    g_imgui_hooks_installed = false;
    g_imgui_present_slot = nullptr;
    g_imgui_reset_slot = nullptr;
    g_imgui_device = nullptr;

    ImGuiRemoveMinHook(g_imgui_create_device_ex_target);
    ImGuiRemoveMinHook(g_imgui_create_device_target);
    ImGuiRemoveMinHook(g_imgui_direct3d_create9ex_target);
    ImGuiRemoveMinHook(g_imgui_direct3d_create9_target);
    g_imgui_direct3d_create9_original = nullptr;
    g_imgui_direct3d_create9ex_original = nullptr;
    g_imgui_create_device_original = nullptr;
    g_imgui_create_device_ex_original = nullptr;
    g_imgui_d3d_factory_hooks_installed = false;
    g_imgui_d3d9_module = nullptr;

    if (g_imgui_hwnd && g_imgui_old_wndproc)
    {
        SetWindowLongPtr(g_imgui_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_imgui_old_wndproc));
        g_imgui_old_wndproc = nullptr;
    }
    g_imgui_hwnd = nullptr;

    {
        std::lock_guard<std::mutex> lock(g_imgui_mutex);
        for (auto& kv : g_imgui_textures)
            ImGuiReleaseTextureResource(kv.second);
        g_imgui_textures.clear();
        g_imgui_fonts.clear();
        g_imgui_code_editors.clear();
        g_imgui_pending_fonts.clear();
        g_imgui_pending_textures.clear();
    }

    if (g_cursor_visible_by_imgui)
    {
        while (ShowCursor(FALSE) >= 0) {}
        g_cursor_visible_by_imgui = false;
    }

    g_imgui_dx9_hook_fail_count = 0;
    g_imgui_dx9_hook_last_fail_log = 0;
    ImGuiSetLuaRenderAllowed(false);
    ImGuiDestroyContextSafe();
}

static bool HandleImGuiInvoke(void* lua_vm, const std::string& func_name)
{
    const bool is_render_allowed_query = findStringIC(func_name, xorstr_("isimguirenderallowed"));
    if (!is_render_allowed_query && !findStringIC(func_name, xorstr_("imgui")))
        return false;

    call_lua_remove(lua_vm, 1);
    ImGuiEnsureHooksForLuaInvoke();

    if (is_render_allowed_query || findStringIC(func_name, xorstr_("imgui.isrenderallowed")))
    {
        call_pushboolean(lua_vm, ImGuiIsLuaRenderAllowed() ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.isready")))
    {
        call_pushboolean(lua_vm, g_imgui_initialized ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.enable")) || findStringIC(func_name, xorstr_("imgui.setvisible")))
    {
        g_imgui_render_enabled = ImGuiLuaGetArgBool(lua_vm, 1, true);
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.isvisible")))
    {
        call_pushboolean(lua_vm, ImGuiIsLuaRenderAllowed() ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.captureinput")) || findStringIC(func_name, xorstr_("imgui.setinput")))
    {
        g_imgui_capture_input = ImGuiLuaGetArgBool(lua_vm, 1, true);
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.blockbinds")))
    {
        g_imgui_block_mta_binds = ImGuiLuaGetArgBool(lua_vm, 1, true);
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.forcecursor")))
    {
        g_imgui_force_cursor = ImGuiLuaGetArgBool(lua_vm, 1, true);
        if (g_imgui_force_cursor)
            g_imgui_block_mta_binds = true;
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.drawcursor")))
    {
        g_imgui_draw_cursor = ImGuiLuaGetArgBool(lua_vm, 1, true);
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.clear")))
    {
        std::lock_guard<std::mutex> lock(g_imgui_mutex);
        g_imgui_queue.clear();
        call_pushboolean(lua_vm, true);
        return true;
    }

    if (findStringIC(func_name, xorstr_("imgui.themedark")))
    {
        g_imgui_theme_request = 0;
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.themelight")))
    {
        g_imgui_theme_request = 1;
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.themeclassic")))
    {
        g_imgui_theme_request = 2;
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.setstylerounding")))
    {
        if (ImGui::GetCurrentContext())
        {
            ImGuiStyle& st = ImGui::GetStyle();
            st.WindowRounding = ImGuiLuaGetArgFloat(lua_vm, 1, st.WindowRounding);
            st.FrameRounding = ImGuiLuaGetArgFloat(lua_vm, 2, st.FrameRounding);
            st.GrabRounding = ImGuiLuaGetArgFloat(lua_vm, 3, st.GrabRounding);
            st.ScrollbarRounding = ImGuiLuaGetArgFloat(lua_vm, 4, st.ScrollbarRounding);
        }
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.pushstylecolor")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::PushStyleColor;
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 1, 0);
        cmd.color = ImGuiColorFromInts(
            ImGuiLuaGetArgInt(lua_vm, 2, 255),
            ImGuiLuaGetArgInt(lua_vm, 3, 255),
            ImGuiLuaGetArgInt(lua_vm, 4, 255),
            ImGuiLuaGetArgInt(lua_vm, 5, 255));
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.popstylecolor")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::PopStyleColor;
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 1, 1);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.pushstylevarfloat")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::PushStyleVarFloat;
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 1, 0);
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.pushstylevarvec2")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::PushStyleVarVec2;
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 1, 0);
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.popstylevar")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::PopStyleVar;
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 1, 1);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }

    if (findStringIC(func_name, xorstr_("imgui.loadfont")))
    {
        std::string path = ImGuiLuaGetArgString(lua_vm, 1, {});
        float size = ImGuiLuaGetArgFloat(lua_vm, 2, 16.0f);
        std::string key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 3, {}), path);
        bool ok = ImGuiQueueFontLoad(key, path, size);
        call_pushboolean(lua_vm, ok ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.pushfont")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::PushFont;
        cmd.key = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("default"));
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.popfont")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::PopFont;
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.loadimage")))
    {
        std::string path = ImGuiLuaGetArgString(lua_vm, 1, {});
        std::string key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 2, {}), path);
        bool ok = ImGuiQueueTextureLoad(key, path);
        call_pushboolean(lua_vm, ok ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.unloadimage")))
    {
        std::string key = ImGuiLuaGetArgString(lua_vm, 1, {});
        bool ok = false;
        {
            std::lock_guard<std::mutex> lock(g_imgui_mutex);
            auto it = g_imgui_textures.find(key);
            if (it != g_imgui_textures.end())
            {
                ImGuiReleaseTextureResource(it->second);
                g_imgui_textures.erase(it);
                ok = true;
            }
        }
        call_pushboolean(lua_vm, ok ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.image")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::Image;
        cmd.key = ImGuiLuaGetArgString(lua_vm, 1, {});
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 4, 0.0f);
        cmd.f4 = ImGuiLuaGetArgFloat(lua_vm, 5, 0.0f);
        cmd.f5 = ImGuiLuaGetArgFloat(lua_vm, 6, 1.0f);
        cmd.f6 = ImGuiLuaGetArgFloat(lua_vm, 7, 1.0f);
        cmd.color = ImGuiColorFromInts(
            ImGuiLuaGetArgInt(lua_vm, 8, 255),
            ImGuiLuaGetArgInt(lua_vm, 9, 255),
            ImGuiLuaGetArgInt(lua_vm, 10, 255),
            ImGuiLuaGetArgInt(lua_vm, 11, 255));
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }

    if (findStringIC(func_name, xorstr_("imgui.beginchild")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::BeginChild;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("child"), true);
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 4, false);
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 5, 0);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 6, {}, true), cmd.label);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.endchild")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::EndChild;
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.begintabbar")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::BeginTabBar;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("tabbar"), true);
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 2, 0);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 3, {}, true), cmd.label);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.endtabbar")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::EndTabBar;
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.begintabitem")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::BeginTabItem;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("tab"), true);
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 2, 0);
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 3, true);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 4, {}, true), cmd.label);
        bool open_state = ImGuiLuaReadBool(cmd.key, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, open_state ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.endtabitem")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::EndTabItem;
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.setnextwindowpos")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::SetNextWindowPos;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 3, 0);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.setnextwindowsize")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::SetNextWindowSize;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 3, 0);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.setwindowfontscale")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::SetWindowFontScale;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 1.0f);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.begin")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::BeginWindow;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("Mirage"), true);
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 2, true);
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 3, 0);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 4, {}, true), cmd.label);
        bool open_state = ImGuiLuaReadBool(cmd.key, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, open_state ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.end")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::EndWindow;
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.bullettext")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::BulletText;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, {}, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.text")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::Text;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, {}, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.separator")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::Separator;
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.spacing")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::Spacing;
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.sameline")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::SameLine;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, -1.0f);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.newline")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::NewLine;
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.dummy")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::Dummy;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 1.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, 1.0f);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.setcursorpos")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::SetCursorPos;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.smallbutton")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::SmallButton;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("SmallButton"), true);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 2, {}, true), cmd.label);
        bool clicked = ImGuiLuaTakePulse(cmd.key);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, clicked ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.button")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::Button;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("Button"), true);
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 4, {}, true), cmd.label);
        bool clicked = ImGuiLuaTakePulse(cmd.key);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, clicked ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.gradientbutton")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::GradientButton;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("Button"), true);
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.color = ImGuiColorFromInts(
            ImGuiLuaGetArgInt(lua_vm, 4, 90),
            ImGuiLuaGetArgInt(lua_vm, 5, 24),
            ImGuiLuaGetArgInt(lua_vm, 6, 34),
            ImGuiLuaGetArgInt(lua_vm, 7, 245));
        cmd.color2 = ImGuiColorFromInts(
            ImGuiLuaGetArgInt(lua_vm, 8, 150),
            ImGuiLuaGetArgInt(lua_vm, 9, 32),
            ImGuiLuaGetArgInt(lua_vm, 10, 46),
            ImGuiLuaGetArgInt(lua_vm, 11, 255));
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 12, 9.0f);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 13, {}, true), cmd.label);
        bool clicked = ImGuiLuaTakePulse(cmd.key);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, clicked ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.checkbox")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::Checkbox;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("Checkbox"), true);
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 2, false);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 3, {}, true), cmd.label);
        bool value = ImGuiLuaReadBool(cmd.key, cmd.b1);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, value ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.sliderfloat")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::SliderFloat;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("SliderFloat"), true);
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 3, 1.0f);
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 4, 0.0f);
        cmd.payload = ImGuiLuaGetArgString(lua_vm, 5, xorstr_("%.3f"), true);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 6, {}, true), cmd.label);
        float value = ImGuiLuaReadFloat(cmd.key, cmd.f3);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushnumber(lua_vm, static_cast<long double>(value));
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.sliderint")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::SliderInt;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("SliderInt"), true);
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 2, 0);
        cmd.f1 = static_cast<float>(ImGuiLuaGetArgInt(lua_vm, 3, 100));
        cmd.i2 = ImGuiLuaGetArgInt(lua_vm, 4, 0);
        cmd.payload = ImGuiLuaGetArgString(lua_vm, 5, xorstr_("%d"), true);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 6, {}, true), cmd.label);
        int value = ImGuiLuaReadInt(cmd.key, cmd.i2);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushnumber(lua_vm, static_cast<long double>(value));
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.inputtext")) && !findStringIC(func_name, xorstr_("multiline")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::InputText;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("Input"), true);
        cmd.payload = ImGuiLuaGetArgString(lua_vm, 2, {}, true);
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 3, 256);
        cmd.i2 = ImGuiLuaGetArgInt(lua_vm, 4, 0);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 5, {}, true), cmd.label);
        std::string value = ImGuiLuaReadString(cmd.key, cmd.payload);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushstring(lua_vm, value.c_str());
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.inputtextmultiline")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::InputTextMultiline;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("##input"), true);
        cmd.payload = ImGuiLuaGetArgString(lua_vm, 2, {}, true);
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 3, 8192);
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 4, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 5, 0.0f);
        cmd.i2 = ImGuiLuaGetArgInt(lua_vm, 6, 0);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 7, {}, true), cmd.label);
        std::string value = ImGuiLuaReadString(cmd.key, cmd.payload);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushstring(lua_vm, value.c_str());
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.codeeditorlua")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::CodeEditorLua;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("##lua_editor"), true);
        cmd.payload = ImGuiLuaGetArgString(lua_vm, 2, {}, true);
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 4, 0.0f);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 5, {}, true), cmd.label);
        std::string value = ImGuiLuaReadString(cmd.key, cmd.payload);
        cmd.payload = value;
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushstring(lua_vm, value.c_str());
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.combo")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::Combo;
        cmd.label = ImGuiLuaGetArgString(lua_vm, 1, xorstr_("Combo"), true);
        cmd.payload = ImGuiLuaGetArgString(lua_vm, 2, {}, true);
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 3, 0);
        cmd.key = ImGuiLuaNormalizeKey(ImGuiLuaGetArgString(lua_vm, 4, {}, true), cmd.label);
        int value = ImGuiLuaReadInt(cmd.key, cmd.i1);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushnumber(lua_vm, static_cast<long double>(value));
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.progressbar")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::ProgressBar;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, -FLT_MIN);
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.payload = ImGuiLuaGetArgString(lua_vm, 4, {}, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }

    if (findStringIC(func_name, xorstr_("imgui.drawline")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::DrawLine;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.f4 = ImGuiLuaGetArgFloat(lua_vm, 4, 0.0f);
        cmd.color = ImGuiColorFromInts(ImGuiLuaGetArgInt(lua_vm, 5, 255), ImGuiLuaGetArgInt(lua_vm, 6, 255), ImGuiLuaGetArgInt(lua_vm, 7, 255), ImGuiLuaGetArgInt(lua_vm, 8, 255));
        cmd.f5 = ImGuiLuaGetArgFloat(lua_vm, 9, 1.0f);
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 10, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.drawrectfilled")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::DrawRectFilled;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.f4 = ImGuiLuaGetArgFloat(lua_vm, 4, 0.0f);
        cmd.color = ImGuiColorFromInts(ImGuiLuaGetArgInt(lua_vm, 5, 255), ImGuiLuaGetArgInt(lua_vm, 6, 255), ImGuiLuaGetArgInt(lua_vm, 7, 255), ImGuiLuaGetArgInt(lua_vm, 8, 255));
        cmd.f5 = ImGuiLuaGetArgFloat(lua_vm, 9, 0.0f);
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 10, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.drawrect")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::DrawRect;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.f4 = ImGuiLuaGetArgFloat(lua_vm, 4, 0.0f);
        cmd.color = ImGuiColorFromInts(ImGuiLuaGetArgInt(lua_vm, 5, 255), ImGuiLuaGetArgInt(lua_vm, 6, 255), ImGuiLuaGetArgInt(lua_vm, 7, 255), ImGuiLuaGetArgInt(lua_vm, 8, 255));
        cmd.f5 = ImGuiLuaGetArgFloat(lua_vm, 9, 0.0f);
        cmd.f6 = ImGuiLuaGetArgFloat(lua_vm, 10, 1.0f);
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 11, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.drawcirclefilled")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::DrawCircleFilled;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.color = ImGuiColorFromInts(ImGuiLuaGetArgInt(lua_vm, 4, 255), ImGuiLuaGetArgInt(lua_vm, 5, 255), ImGuiLuaGetArgInt(lua_vm, 6, 255), ImGuiLuaGetArgInt(lua_vm, 7, 255));
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 8, 0);
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 9, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.drawcircle")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::DrawCircle;
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 1, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.color = ImGuiColorFromInts(ImGuiLuaGetArgInt(lua_vm, 4, 255), ImGuiLuaGetArgInt(lua_vm, 5, 255), ImGuiLuaGetArgInt(lua_vm, 6, 255), ImGuiLuaGetArgInt(lua_vm, 7, 255));
        cmd.i1 = ImGuiLuaGetArgInt(lua_vm, 8, 0);
        cmd.f4 = ImGuiLuaGetArgFloat(lua_vm, 9, 1.0f);
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 10, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.drawtext")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::DrawText;
        cmd.payload = ImGuiLuaGetArgString(lua_vm, 1, {}, true);
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.color = ImGuiColorFromInts(ImGuiLuaGetArgInt(lua_vm, 4, 255), ImGuiLuaGetArgInt(lua_vm, 5, 255), ImGuiLuaGetArgInt(lua_vm, 6, 255), ImGuiLuaGetArgInt(lua_vm, 7, 255));
        cmd.key = ImGuiLuaGetArgString(lua_vm, 8, xorstr_("default"));
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 9, 16.0f);
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 10, true);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.drawimage")))
    {
        ImGuiLuaCommand cmd{};
        cmd.type = ImGuiLuaCommandType::DrawImage;
        cmd.key = ImGuiLuaGetArgString(lua_vm, 1, {});
        cmd.f1 = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        cmd.f2 = ImGuiLuaGetArgFloat(lua_vm, 3, 0.0f);
        cmd.f3 = ImGuiLuaGetArgFloat(lua_vm, 4, 0.0f);
        cmd.f4 = ImGuiLuaGetArgFloat(lua_vm, 5, 0.0f);
        cmd.f5 = ImGuiLuaGetArgFloat(lua_vm, 6, 0.0f);
        cmd.f6 = ImGuiLuaGetArgFloat(lua_vm, 7, 0.0f);
        float u1 = ImGuiLuaGetArgFloat(lua_vm, 8, 1.0f);
        float v1 = ImGuiLuaGetArgFloat(lua_vm, 9, 1.0f);
        cmd.i1 = static_cast<int>(u1 * 10000.0f);
        cmd.i2 = static_cast<int>(v1 * 10000.0f);
        cmd.color = ImGuiColorFromInts(ImGuiLuaGetArgInt(lua_vm, 10, 255), ImGuiLuaGetArgInt(lua_vm, 11, 255), ImGuiLuaGetArgInt(lua_vm, 12, 255), ImGuiLuaGetArgInt(lua_vm, 13, 255));
        cmd.b1 = ImGuiLuaGetArgBool(lua_vm, 14, false);
        ImGuiLuaQueueCommand(std::move(cmd));
        call_pushboolean(lua_vm, true);
        return true;
    }

    if (findStringIC(func_name, xorstr_("imgui.getbool")))
    {
        std::string key = ImGuiLuaGetArgString(lua_vm, 1, {});
        bool fallback = ImGuiLuaGetArgBool(lua_vm, 2, false);
        bool value = ImGuiLuaReadBool(key, fallback);
        call_pushboolean(lua_vm, value ? 1 : 0);
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.getfloat")))
    {
        std::string key = ImGuiLuaGetArgString(lua_vm, 1, {});
        float fallback = ImGuiLuaGetArgFloat(lua_vm, 2, 0.0f);
        float value = ImGuiLuaReadFloat(key, fallback);
        call_pushnumber(lua_vm, static_cast<long double>(value));
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.getint")))
    {
        std::string key = ImGuiLuaGetArgString(lua_vm, 1, {});
        int fallback = ImGuiLuaGetArgInt(lua_vm, 2, 0);
        int value = ImGuiLuaReadInt(key, fallback);
        call_pushnumber(lua_vm, static_cast<long double>(value));
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.getstring")))
    {
        std::string key = ImGuiLuaGetArgString(lua_vm, 1, {});
        std::string fallback = ImGuiLuaGetArgString(lua_vm, 2, {}, true);
        std::string value = ImGuiLuaReadString(key, fallback);
        call_pushstring(lua_vm, value.c_str());
        return true;
    }
    if (findStringIC(func_name, xorstr_("imgui.setstring")))
    {
        std::string key = ImGuiLuaGetArgString(lua_vm, 1, {});
        std::string value = ImGuiLuaGetArgString(lua_vm, 2, {}, true);
        ImGuiLuaWriteString(key, value);
        call_pushboolean(lua_vm, true);
        return true;
    }

    call_pushboolean(lua_vm, false);
    return true;
}

#include "overlay.h"
#include "menu_surface_dx11.h"
#include "keyboard_click.h"
#include "brand_icon.h"
#include "krux_menu_motion.h"

#include <Windows.h>
#include <shellapi.h>
#include "bridge/Jvm.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <string>
#include <thread>
#include <mmsystem.h>

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"
#include "framework/framework.h"
#include "krux_module_routes.h"
#include "krux_classic_focus.h"
#include "modules/ModuleManager.hpp"
#include "modules/render/BlockEsp.hpp"
#include "modules/render/CrossHair.hpp"
#include "hooks/Input.hpp"
#include "ui/Client.hpp"
#include "ui/Hud.hpp"
#include "ui/Theme.hpp"
#include "ui/ClassicPreferences.hpp"
#include "core/CloudConfigs.hpp"
#include "core/ModuleUsage.hpp"
#include "core/MenuMusic.hpp"
#include "core/Log.hpp"
#include "modules/client/Webhooks.hpp"
#include "core/BaseAlerts.hpp"
#include "krux-menu-assets/PrimordialMontserratMedium.hpp"
#include "krux-menu-assets/PrimordialMontserratBold.hpp"
#include "krux-menu-assets/Montserrat-Regular.h"
#include "krux_minecraft_imgui_font.h"
#include "assets/AssetStore.hpp"

#include <unordered_map>
#include <variant>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

constexpr int kPanelWidth = 780;
constexpr int kPanelHeight = 540;
constexpr int kHotkeyId = 1;
constexpr UINT kMenuKeyMessage = WM_APP + 1;
constexpr UINT kMenuCharMessage = WM_APP + 2;
constexpr UINT kMenuClickMessage = WM_APP + 3;
constexpr UINT kMenuScrollMessage = WM_APP + 4;

std::atomic_bool* g_stop = nullptr;
ID3D11Device* g_device = nullptr;
struct BlockTileTexture { ID3D11ShaderResourceView* view = nullptr; unsigned revision = 0; };
std::unordered_map<std::string, BlockTileTexture> g_blockTileTextures;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_renderTarget = nullptr;
HWND g_overlayWindow = nullptr;
HWND g_gameWindow = nullptr;
ImFont* g_primordialMedium = nullptr;
ImFont* g_pixelMenuFont = nullptr;
ImFont* g_ricoMenuFont = nullptr;
krux::fonts::MinecraftImGuiAtlas g_minecraftImGui;
ImFont* g_primordialBold = nullptr;
ImFont* g_primordialIcons = nullptr;
ImFont* g_primordialLogIcons = nullptr;
std::atomic_bool g_menuVisible{ false };
std::atomic_bool g_menuInputActive{ false };
std::atomic<HWND> g_menuInputWindow{ nullptr };
int g_activeTab = 0;
int g_selectedModuleIndex = 0;
char g_moduleSearch[64]{};
bool g_overlayInputCaptured = false;
bool g_draggingMenu = false;
bool g_overlayPositionInitialized = false;
POINT g_dragStartCursor{};
POINT g_dragStartWindow{};
int g_overlayOffsetX = 0;
int g_overlayOffsetY = 0;
int g_menuPage = 1;
bool g_hudEditing = false;
bool g_interfaceLoaded = false;
std::atomic_bool g_reloadInterface{false};
bool g_menuMusic=true,g_musicLoop=true,g_moduleNotices=true,g_noticeProgress=true;
float g_musicVolume=.22f,g_musicFade=1.5f,g_noticeDuration=3,g_noticeScale=1,g_noticeOpacity=.95f;
int g_noticeStyle=0,g_noticePosition=4;
bool g_preferencesSaved = false;
cold::config::Document g_savedPreferences;
bool g_clickSound = true;
float g_textScale = 1.0f;
bool g_matchHudTheme = true;
bool g_hurtCamera = false;
cold::Color g_menuTextColour=cold::Color::rgb(0xBABABA);
constexpr int g_menuStyle = 1;
bool g_desktopMenu=false,g_resetClassicLayout=false;
krux::classic::Focus g_classicFocus;
bool g_dimInactivePanels=true;
float g_menuBackdrop=.32f;
float g_classicInactiveBrightness=.42f;
float g_cornerRadius = 8.0f;
float g_moduleRowHeight = 22.0f;
float g_classicColumnWidth=196.f,g_classicHeaderHeight=24.f,g_menuPanelOpacity=1.f,g_menuOutlineOpacity=1.f;
cold::Color g_bodyColour=cold::Color::rgb(0x1D1D1D);
cold::Color g_headerColour=cold::Color::rgb(0x272727);
ImU32 MenuBody() {return ImGui::ColorConvertFloat4ToU32({g_bodyColour.r,g_bodyColour.g,g_bodyColour.b,g_menuPanelOpacity});}
ImU32 MenuHeader() {return ImGui::ColorConvertFloat4ToU32({g_headerColour.r,g_headerColour.g,g_headerColour.b,g_menuPanelOpacity});}
cold::modules::Module* g_bindingModule = nullptr;
cold::modules::Setting* g_bindingSetting = nullptr;

void PlayMenuClick() {
    if (g_clickSound) PlayKeyboardClick();
}

ImU32 MenuAccent(int alpha = 255) {
    const cold::Color& colour = cold::ui::theme().accent;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(colour.r, colour.g, colour.b, static_cast<float>(alpha) / 255.0f));
}

ImU32 MenuOutline(int alpha=255) {return MenuAccent(int(std::round(alpha*g_menuOutlineOpacity)));}

void SyncNativeHudNoticeScale() {
    static bool wasMigrated=false;
    const auto& config=cold::modules::modules().config();
    const bool migrated=config.getBool("interface","nativeHudSizingV1",false);
    if(migrated&&!wasMigrated)g_noticeScale=std::clamp(config.getNumber("interface","noticeScale",1),.5f,3.f);
    wasMigrated=migrated;
}
void SaveInterface() {
    SyncNativeHudNoticeScale();
    if(g_reloadInterface.load())return;
    auto& manager = cold::modules::modules();
    auto& config = manager.config();
    config.setBool("interface", "clickSound", g_clickSound);
    config.setBool("interface","menuMusic",g_menuMusic);config.setBool("interface","musicLoop",g_musicLoop);
    config.setNumber("interface","musicVolume",g_musicVolume,2);config.setNumber("interface","musicFade",g_musicFade,1);
    config.setBool("interface","moduleNotices",g_moduleNotices);config.setBool("interface","noticeProgress",g_noticeProgress);
    config.setInt("interface","noticeStyle",g_noticeStyle);config.setInt("interface","noticePosition",g_noticePosition);
    config.setNumber("interface","noticeDuration",g_noticeDuration,1);config.setNumber("interface","noticeScale",g_noticeScale,2);config.setNumber("interface","noticeOpacity",g_noticeOpacity,2);
    config.setNumber("interface", "textScale", g_textScale, 2);
    config.setBool("interface", "matchHud", g_matchHudTheme);
    config.setBool("interface","hurtCamera",g_hurtCamera);
    config.setNumber("interface", "menuTextR", g_menuTextColour.r, 3);
    config.setNumber("interface", "menuTextG", g_menuTextColour.g, 3);
    config.setNumber("interface", "menuTextB", g_menuTextColour.b, 3);
    config.setInt("interface", "menuStyle", g_menuStyle);
    config.setBool("interface","classicDimOthers",g_dimInactivePanels);
    config.setNumber("interface","menuBackdrop",g_menuBackdrop,2);
    config.setNumber("interface","classicInactiveBrightness",g_classicInactiveBrightness,2);
    config.setNumber("interface", "cornerRadius",g_cornerRadius,1);
    config.setNumber("interface", "moduleRowHeight",g_moduleRowHeight,0);
    config.setNumber("interface","classicColumnWidth",g_classicColumnWidth,0);
    config.setNumber("interface","classicHeaderHeight",g_classicHeaderHeight,0);
    config.setNumber("interface","menuPanelOpacity",g_menuPanelOpacity,2);
    config.setNumber("interface","menuOutlineOpacity",g_menuOutlineOpacity,2);
    for(int i=0;i<3;++i) {config.setNumber("interface","body"+std::to_string(i),i==0?g_bodyColour.r:i==1?g_bodyColour.g:g_bodyColour.b,3);config.setNumber("interface","header"+std::to_string(i),i==0?g_headerColour.r:i==1?g_headerColour.g:g_headerColour.b,3);}
    config.setBool("interface","menuPositionSet",g_overlayPositionInitialized);
    config.setInt("interface", "offsetX", g_overlayOffsetX);
    config.setInt("interface", "offsetY", g_overlayOffsetY);
    const auto& accent = cold::ui::theme().accent;
    config.setNumber("interface", "accentR", accent.r, 3);
    config.setNumber("interface", "accentG", accent.g, 3);
    config.setNumber("interface", "accentB", accent.b, 3);
    if (cold::ui::client().hasHud()) cold::ui::client().hud().saveLayout(config);
    const auto snapshot = manager.snapshotConfig();
    if ((!g_preferencesSaved || snapshot != g_savedPreferences)
        && snapshot.save(cold::ui::client().directory() + L"\\krux.lol.cfg")) {
        g_savedPreferences = snapshot;
        g_preferencesSaved = true;
    }
}

c_window g_menu("##krux_menu",
    ImVec2(static_cast<float>(kPanelWidth), static_cast<float>(kPanelHeight)),
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

class c_module_selector;

struct ModuleControl {
    cold::modules::Module* module{};
    c_module_selector* selector{};
    c_checkbox* enabled{};
    c_keybind* key{};
    std::unique_ptr<c_binding> binding;
    int syncedKey{};
};

class c_module_selector final : public c_element {
    std::string label_;
    int* selected_{};
    int index_{};
    bool enabled_{};

public:
    c_module_selector(std::string label, int* selected, int index)
        : label_(std::move(label)), selected_(selected), index_(index) {
        set_label(label_);
    }

    void set_enabled(bool enabled) { enabled_ = enabled; }

    bool render(float dpi_scale) override {
        const bool selected = *selected_ == index_;
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 size(available.x, 25.0f * dpi_scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 3.0f) * dpi_scale);
        ImGui::PushStyleColor(ImGuiCol_Header, selected
            ? ImVec4(0.19f, 0.17f, 0.22f, 0.96f)
            : ImVec4(0.08f, 0.075f, 0.09f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.17f, 0.15f, 0.20f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.22f, 0.20f, 0.26f, 1.0f));
        const bool pressed = ImGui::Selectable(label_.c_str(), selected,
            ImGuiSelectableFlags_None, size);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        if (selected) {
            draw->AddRectFilled(ImVec2(rowMin.x, rowMin.y + 2.0f * dpi_scale),
                ImVec2(rowMin.x + 2.0f * dpi_scale, rowMax.y - 2.0f * dpi_scale),
                IM_COL32(130, 125, 150, 255));
        }
        if (enabled_) {
            draw->AddCircleFilled(ImVec2(rowMax.x - 8.0f * dpi_scale,
                (rowMin.y + rowMax.y) * 0.5f), 2.5f * dpi_scale,
                IM_COL32(174, 163, 188, 255));
        }
        return pressed;
    }
};

struct SettingControl {
    cold::modules::Setting* setting{};
    c_checkbox* boolean{};
    c_combo* choice{};
    c_slider_float* number{};
    c_keybind* key{};
    std::unique_ptr<c_binding> binding;
    int syncedKey{};
    c_colorpicker* colour{};
};

std::vector<ModuleControl> g_moduleControls;
std::vector<SettingControl> g_settingControls;
std::unordered_map<std::string, bool> g_pendingEnabled;
bool g_menuBuilt = false;

int GlfwToVirtualKey(int key) {
    if ((key >= 32 && key <= 57) || (key >= 65 && key <= 90)) return key;
    if (key >= 290 && key <= 314) return VK_F1 + key - 290;
    if (key >= 320 && key <= 329) return VK_NUMPAD0 + key - 320;
    switch (key) {
    case 256: return VK_ESCAPE;
    case 257: return VK_RETURN;
    case 258: return VK_TAB;
    case 259: return VK_BACK;
    case 260: return VK_INSERT;
    case 261: return VK_DELETE;
    case 262: return VK_RIGHT;
    case 263: return VK_LEFT;
    case 264: return VK_DOWN;
    case 265: return VK_UP;
    case 266: return VK_PRIOR;
    case 267: return VK_NEXT;
    case 268: return VK_HOME;
    case 269: return VK_END;
    case 280: return VK_CAPITAL;
    case 281: return VK_SCROLL;
    case 282: return VK_NUMLOCK;
    case 283: return VK_SNAPSHOT;
    case 284: return VK_PAUSE;
    case 340: return VK_LSHIFT;
    case 341: return VK_LCONTROL;
    case 342: return VK_LMENU;
    case 343: return VK_LWIN;
    case 344: return VK_RSHIFT;
    case 345: return VK_RCONTROL;
    case 346: return VK_RMENU;
    case 347: return VK_RWIN;
    case 39: return VK_OEM_7;
    case 44: return VK_OEM_COMMA;
    case 45: return VK_OEM_MINUS;
    case 46: return VK_OEM_PERIOD;
    case 47: return VK_OEM_2;
    case 59: return VK_OEM_1;
    case 61: return VK_OEM_PLUS;
    case 91: return VK_OEM_4;
    case 92: return VK_OEM_5;
    case 93: return VK_OEM_6;
    case 96: return VK_OEM_3;
    default: return 0;
    }
}

int VirtualToGlfwKey(int key) {
    if ((key >= 32 && key <= 57) || (key >= 65 && key <= 90)) return key;
    if (key >= VK_F1 && key <= VK_F24) return 290 + key - VK_F1;
    if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) return 320 + key - VK_NUMPAD0;
    switch (key) {
    case VK_ESCAPE: return 256;
    case VK_RETURN: return 257;
    case VK_TAB: return 258;
    case VK_BACK: return 259;
    case VK_INSERT: return 260;
    case VK_DELETE: return 261;
    case VK_RIGHT: return 262;
    case VK_LEFT: return 263;
    case VK_DOWN: return 264;
    case VK_UP: return 265;
    case VK_PRIOR: return 266;
    case VK_NEXT: return 267;
    case VK_HOME: return 268;
    case VK_END: return 269;
    case VK_CAPITAL: return 280;
    case VK_SCROLL: return 281;
    case VK_NUMLOCK: return 282;
    case VK_SNAPSHOT: return 283;
    case VK_PAUSE: return 284;
    case VK_LSHIFT: return 340;
    case VK_LCONTROL: return 341;
    case VK_LMENU: return 342;
    case VK_LWIN: return 343;
    case VK_RSHIFT: return 344;
    case VK_RCONTROL: return 345;
    case VK_RMENU: return 346;
    case VK_RWIN: return 347;
    case VK_OEM_7: return 39;
    case VK_OEM_COMMA: return 44;
    case VK_OEM_MINUS: return 45;
    case VK_OEM_PERIOD: return 46;
    case VK_OEM_2: return 47;
    case VK_OEM_1: return 59;
    case VK_OEM_PLUS: return 61;
    case VK_OEM_4: return 91;
    case VK_OEM_5: return 92;
    case VK_OEM_6: return 93;
    case VK_OEM_3: return 96;
    default: return 0;
    }
}

void CreateRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTarget);
        backBuffer->Release();
    }
}

void DestroyRenderTarget() {
    if (g_renderTarget) {
        g_renderTarget->Release();
        g_renderTarget = nullptr;
    }
}

bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.Flags = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL selected{};
    const HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
        D3D11_SDK_VERSION, &desc, &g_swapChain, &g_device, &selected, &g_context);
    if (FAILED(hr)) return false;
    IDXGIFactory* factory = nullptr;
    if (SUCCEEDED(g_swapChain->GetParent(IID_PPV_ARGS(&factory)))) {
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
        factory->Release();
    }
    IDXGIDevice1* dxgiDevice = nullptr;
    if (SUCCEEDED(g_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) {
        dxgiDevice->SetMaximumFrameLatency(1);
        dxgiDevice->Release();
    }
    CreateRenderTarget();
    return g_renderTarget != nullptr;
}

void CleanupDeviceD3D() {
    for (auto& [id, texture] : g_blockTileTextures) if (texture.view) texture.view->Release();
    g_blockTileTextures.clear();
    DestroyRenderTarget();
    if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
}

BOOL CALLBACK FindGameWindowCallback(HWND hwnd, LPARAM parameter) {
    auto* best = reinterpret_cast<HWND*>(parameter);
    if (hwnd == g_overlayWindow || !IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid != GetCurrentProcessId()) return TRUE;

    RECT client{};
    if (!GetClientRect(hwnd, &client)) return TRUE;
    const LONG width = client.right - client.left;
    const LONG height = client.bottom - client.top;
    if (width < 500 || height < 300) return TRUE;

    if (!*best) {
        *best = hwnd;
    } else {
        RECT bestClient{};
        GetClientRect(*best, &bestClient);
        const LONGLONG area = static_cast<LONGLONG>(width) * height;
        const LONGLONG bestArea = static_cast<LONGLONG>(bestClient.right - bestClient.left)
            * (bestClient.bottom - bestClient.top);
        if (area > bestArea) *best = hwnd;
    }
    return TRUE;
}

HWND FindGameWindow() {
    HWND result = nullptr;
    EnumWindows(FindGameWindowCallback, reinterpret_cast<LPARAM>(&result));
    return result;
}

bool GetGameClientBounds(POINT* origin, int* width, int* height) {
    if (!g_gameWindow || !IsWindow(g_gameWindow)) return false;
    static POINT lastOrigin{};static int lastWidth=0,lastHeight=0;
    if(IsIconic(g_gameWindow)) {
        if(lastWidth<400||lastHeight<260)return false;
        *origin=lastOrigin;*width=lastWidth;*height=lastHeight;return true;
    }
    RECT client{};
    if (!GetClientRect(g_gameWindow, &client)) return false;
    POINT clientOrigin{ 0, 0 };
    if (!ClientToScreen(g_gameWindow, &clientOrigin)) return false;
    *origin = clientOrigin;
    *width = client.right - client.left;
    *height = client.bottom - client.top;
    if(*width<400||*height<260)return false;
    lastOrigin=*origin;lastWidth=*width;lastHeight=*height;return true;
}

void CaptureOverlayInput() {
    if (!g_overlayWindow || !g_menuVisible.load() || !krux::menu::surfaceActive.load()) return;
    const HWND foreground = GetForegroundWindow();
    const bool active = foreground == g_gameWindow;
    const bool wasActive = g_menuInputActive.exchange(active);
    if (active && !wasActive && ImGui::GetCurrentContext()) ImGui::GetIO().AddFocusEvent(true);
    // Activating a second HWND makes GLFW minimize an exclusive fullscreen window.
    // Keyboard events arrive through the existing game callbacks instead.
    g_overlayInputCaptured = active;
}

void ReleaseOverlayInput() {
    const bool wasActive = g_menuInputActive.exchange(false) || g_overlayInputCaptured;
    if (wasActive && ImGui::GetCurrentContext()) ImGui::GetIO().AddFocusEvent(false);
    if (g_overlayWindow && GetCapture() == g_overlayWindow) ReleaseCapture();
    g_overlayInputCaptured = false;
    krux::menu::surfaceActive.store(false);
    g_draggingMenu = false;
}

void MoveOverlayTo(int screenX, int screenY) {
    POINT gameOrigin{};
    int gameWidth = 0;
    int gameHeight = 0;
    if (!GetGameClientBounds(&gameOrigin, &gameWidth, &gameHeight)) return;

    RECT bounds{};
    if (!GetWindowRect(g_overlayWindow, &bounds)) return;
    const int maxX = gameOrigin.x + gameWidth - (bounds.right-bounds.left);
    const int maxY = gameOrigin.y + gameHeight - (bounds.bottom-bounds.top);
    screenX = std::clamp<int>(screenX, static_cast<int>(gameOrigin.x), std::max<int>(gameOrigin.x, maxX));
    screenY = std::clamp<int>(screenY, static_cast<int>(gameOrigin.y), std::max<int>(gameOrigin.y, maxY));
    g_overlayOffsetX = screenX - gameOrigin.x;
    g_overlayOffsetY = screenY - gameOrigin.y;

    RECT current{};
    if (GetWindowRect(g_overlayWindow, &current) && current.left == screenX && current.top == screenY) return;
    SetWindowPos(g_overlayWindow, nullptr, screenX, screenY, 0, 0,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
}

#include "krux_menu_window.inl"

#include "krux_classic_style.inl"

void SetTabVisibility(c_child* child, int tab) {
    child->set_visibility_callback([tab]() { return g_activeTab == tab; });
}

void AddSettingControl(c_child* child, cold::modules::Module* module,
    cold::modules::Setting* setting) {
    using Type = cold::modules::Setting::Type;
    SettingControl control{};
    control.setting = setting;
    const std::string label = setting->name() + "##" + module->name();
    switch (setting->type()) {
    case Type::Boolean:
        control.boolean = child->insert_element<c_checkbox>(label);
        control.boolean->set_has_tooltip(true);
        control.boolean->set_tooltip(setting->description());
        control.boolean->set_visibility_callback([setting]() { return setting->visible(); });
        control.boolean->set_interaction_callback([setting, widget = control.boolean]() {
            std::lock_guard lock(cold::modules::runtimeMutex());
            setting->setBoolean(widget->get_value());
        });
        break;
    case Type::Choice: {
        control.choice = child->insert_element<c_combo>(label, setting->choices());
        control.choice->set_has_tooltip(true);
        control.choice->set_tooltip(setting->description());
        control.choice->set_visibility_callback([setting]() { return setting->visible(); });
        control.choice->set_interaction_callback([setting, widget = control.choice]() {
            std::lock_guard lock(cold::modules::runtimeMutex());
            setting->setChoice(widget->get_value());
        });
        break;
    }
    case Type::Number: {
        char format[16]{};
        std::snprintf(format, sizeof(format), "%%.%df", setting->decimals());
        control.number = child->insert_element<c_slider_float>(label,
            setting->minimum(), setting->maximum(), format);
        control.number->set_has_tooltip(true);
        control.number->set_tooltip(setting->description());
        control.number->set_visibility_callback([setting]() { return setting->visible(); });
        control.number->set_interaction_callback([setting, widget = control.number]() {
            std::lock_guard lock(cold::modules::runtimeMutex());
            setting->setNumber(widget->get_value());
        });
        break;
    }
    case Type::Key:
        control.syncedKey = setting->key();
        control.binding = std::make_unique<c_binding>(label, GlfwToVirtualKey(setting->key()),
            keybind_interaction_type::TOGGLE);
        control.key = child->insert_element<c_keybind>(setting->name(), control.binding.get(),
            keybind_display_type::WITHOUT_LABEL);
        control.key->set_has_tooltip(true);
        control.key->set_tooltip(setting->description());
        control.key->set_visibility_callback([setting]() { return setting->visible(); });
        break;
    case Type::Colour:
        control.colour = child->insert_element<c_colorpicker>(label);
        control.colour->set_has_tooltip(true);
        control.colour->set_tooltip(setting->description());
        control.colour->set_visibility_callback([setting]() { return setting->visible(); });
        control.colour->set_interaction_callback([setting, widget = control.colour]() {
            std::lock_guard lock(cold::modules::runtimeMutex());
            const c_color value = widget->get_value();
            setting->setColour(cold::Color{ value.r, value.g, value.b, value.a });
        });
        break;
    }
    g_settingControls.push_back(std::move(control));
    (void)module;
}

int TabForCategory(cold::modules::Category category) {
    using Category = cold::modules::Category;
    switch (category) {
    case Category::Combat:
    case Category::Movement:
    case Category::Player: return 0;
    case Category::Render:
    case Category::Cosmetics:
    case Category::World: return 1;
    case Category::Client: return 2;
    case Category::Scripts: return 4;
    case Category::Donut: return 5;
    default: return 0;
    }
}

bool ModuleMatchesSearch(const std::string& name) {
    if (g_moduleSearch[0] == '\0') return true;
    std::string candidate = name;
    std::string query = g_moduleSearch;
    const auto lowercase = [](unsigned char value) { return static_cast<char>(std::tolower(value)); };
    std::transform(candidate.begin(), candidate.end(), candidate.begin(), lowercase);
    std::transform(query.begin(), query.end(), query.begin(), lowercase);
    return candidate.find(query) != std::string::npos;
}

bool TabHasSearchMatch(int tab) {
    return std::any_of(g_moduleControls.begin(), g_moduleControls.end(), [tab](const ModuleControl& control) {
        return TabForCategory(control.module->category()) == tab
            && ModuleMatchesSearch(control.module->name());
    });
}

int PrimordialDockForCategory(cold::modules::Category category) {
    using Category = cold::modules::Category;
    switch (category) {
    case Category::Combat: return 0;
    case Category::Movement: return 1;
    case Category::Render:
    case Category::World: return 2;
    case Category::Player: return 3;
    case Category::Client: return 4;
    case Category::Scripts: return 4;
    case Category::Donut: return 5;
    case Category::Cosmetics: return 6;
    default: return 0;
    }
}

void OpenLocalMap(bool stop=false) {
    auto* env=cold::bridge::Jvm::instance().env();
    const auto url=cold::bridge::utilitySnapshot(env,610,stop?2:1);
    if(stop)return;
    if(url.starts_with("http://127.0.0.1:")){const std::wstring wide(url.begin(),url.end());ShellExecuteW(nullptr,L"open",wide.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
    else if(cold::ui::client().hasHud())cold::ui::client().hud().notify("Local map could not start",4,cold::ui::theme().accent);
}
float PrimordialControlWidth();
#include "krux_menu_widgets.inl"

std::string ModuleKeyName(int key) {
    if (key == 0) return "none";
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) return std::string(1, char(key));
    if (key >= 290 && key <= 314) return "F" + std::to_string(key - 289);
    if (key >= 320 && key <= 329) return "Num " + std::to_string(key - 320);
    switch (key) {
    case 32: return "Space";
    case 256: return "Esc";
    case 257: return "Enter";
    case 258: return "Tab";
    case 259: return "Backspace";
    case 260: return "Insert";
    case 261: return "Delete";
    case 262: return "Right";
    case 263: return "Left";
    case 264: return "Down";
    case 265: return "Up";
    case 266: return "PgUp";
    case 267: return "PgDn";
    case 268: return "Home";
    case 269: return "End";
    case 280: return "Caps";
    case 281: return "Scroll";
    case 282: return "NumLock";
    case 283: return "PrtSc";
    case 284: return "Pause";
    case 330: return "Num .";
    case 331: return "Num /";
    case 332: return "Num *";
    case 333: return "Num -";
    case 334: return "Num +";
    case 335: return "Num Enter";
    case 336: return "Num =";
    case 340: return "LShift";
    case 341: return "LCtrl";
    case 342: return "LAlt";
    case 343: return "LWin";
    case 344: return "RShift";
    case 345: return "RCtrl";
    case 346: return "RAlt";
    case 347: return "RWin";
    case 348: return "Menu";
    default: break;
    }
    if (key > 32 && key < 127) return std::string(1, char(key));
    return "code " + std::to_string(key);
}

void RenderBinding(const char* label, int key, cold::modules::Module* module, cold::modules::Setting* setting, float width = 0, bool inlineLabel = false) {
    const bool listening = (module && g_bindingModule == module) || (setting && g_bindingSetting == setting);
    const std::string caption = std::string(listening ? "press a key" : ModuleKeyName(key)) + "##binding-key";
    ImGui::PushID(module ? static_cast<void*>(module) : static_cast<void*>(setting));
    ImGui::TextDisabled("%s", label);
    if (inlineLabel) ImGui::SameLine(0, 8);
    const float buttonWidth = inlineLabel ? width - ImGui::CalcTextSize(label).x - 8 : width > 0 ? width : PrimordialControlWidth();
    if (ImGui::Button(caption.c_str(), ImVec2(buttonWidth, 24.0f))) {
        g_bindingModule = module;
        g_bindingSetting = setting;
        PlayMenuClick();
    }
    ImGui::PopID();
}

void RenderModuleActivation(cold::modules::Module& module, float width, bool showLabel = true) {
    if (!module.key()) return;
    ImGui::PushID(&module);
    if (showLabel) ImGui::TextDisabled("Activation mode");
    const float gap = 6.0f;
    const float half = (width - gap) * 0.5f;
    for (int index = 0; index < 2; ++index) {
        const bool hold = index == 1;
        const bool selected = module.holdBind() == hold;
        ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(0.20f, 0.18f, 0.19f, 1) : ImVec4(0.10f, 0.10f, 0.10f, 1));
        ImGui::PushStyleColor(ImGuiCol_Border, selected ? ImGui::ColorConvertU32ToFloat4(MenuAccent(130)) : ImVec4(0.21f, 0.21f, 0.21f, 1));
        if (ImGui::Button(hold ? "Hold" : "Toggle", ImVec2(half, 24))) {
            module.setHoldBind(hold);
            PlayMenuClick();
        }
        ImGui::PopStyleColor(2);
        if (!hold) ImGui::SameLine(0, gap);
    }
    ImGui::PopID();
}

void RenderModuleBindingPopup(cold::modules::Module& module) {
    static cold::modules::Module* popupCapture = nullptr;
    ImGui::PushID(&module);
    PushKruxPopupStyle();
    // Explicit padding overrides the borderless main window without inheriting its narrow rail width.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    const float width = (std::max)(196.0f, ImGui::CalcTextSize(module.name().c_str()).x + 20.0f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0), ImVec2(width, FLT_MAX));
    if (ImGui::BeginPopupContextItem("##module-binding")) {
        if (ImGui::IsWindowAppearing()) {
            g_bindingModule = nullptr;
            g_bindingSetting = nullptr;
        }
        ImGui::TextUnformatted(module.name().c_str());
        ImGui::Separator();
        const float contentWidth = ImGui::GetContentRegionAvail().x;
        RenderBinding("Key", module.key(), &module, nullptr, contentWidth, true);
        if (g_bindingModule == &module) popupCapture = &module;
        RenderModuleActivation(module, contentWidth, false);
        ImGui::Separator();
        if (ImGui::Button("Clear binding", ImVec2(contentWidth, 22))) {
            module.setKey(0);
            if (g_bindingModule == &module) g_bindingModule = nullptr;
            PlayMenuClick();
        }
        if (g_bindingModule == &module) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Press a key to assign. Escape or Delete removes the binding.");
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    } else if (popupCapture == &module) {
        // A dismissed popup must not keep consuming the next key typed into the menu.
        if (g_bindingModule == &module) g_bindingModule = nullptr;
        popupCapture = nullptr;
    }
    ImGui::PopStyleVar(3);
    PopKruxPopupStyle();
    ImGui::PopID();
}

void RenderPrimordialSetting(cold::modules::Setting& setting, bool colourAlpha = true) {
    using Type = cold::modules::Setting::Type;
    if (!setting.visible()) return;
    ImGui::PushID(&setting);
    switch (setting.type()) {
    case Type::Boolean: {
        bool value = setting.booleanValue();
        if (DrawPrimordialToggle("##toggle", setting.name().c_str(), value)) setting.setBoolean(value);
        break;
    }
    case Type::Number: {
        float value = setting.number();
        if (DrawPrimordialValue("##value", setting.name().c_str(), value, setting.minimum(), setting.maximum(), setting.decimals(), setting.defaultNumber())) setting.setNumber(value);
        break;
    }
    case Type::Choice: {
        int choice = setting.choice();
        if (DrawPrimordialChoice("##choice", setting.name().c_str(), setting.choices(), choice)) setting.setChoice(choice);
        break;
    }
    case Type::Key:
        RenderBinding(setting.name().c_str(), setting.key(), nullptr, &setting);
        break;
    case Type::Text: {
        std::array<char,2049> text{};
        std::copy_n(setting.text().data(), (std::min)(setting.text().size(), text.size()-1), text.data());
        ImGui::TextDisabled("%s", setting.name().c_str());
        ImGui::SetNextItemWidth(PrimordialControlWidth());
        if(ImGui::InputText("##text",text.data(),text.size(),setting.secret()?ImGuiInputTextFlags_Password:ImGuiInputTextFlags_None)) setting.setText(text.data());
        break;
    }
    case Type::Colour: {
        const cold::Color colour = setting.colour();
        float value[4]{ colour.r, colour.g, colour.b, colour.a };
        if (DrawPrimordialColour("##colour-setting", setting.name().c_str(), value, colourAlpha))
            setting.setColour(cold::Color{ value[0], value[1], value[2], colourAlpha ? value[3] : 1.f });
        break;
    }
    }
    ImGui::PopID();
}

void DrawKruxMark(ImDrawList* draw, const ImVec2& center, ImU32 colour) {
    for (const auto& polygon : krux::brand::mark) {
        ImVec2 points[4];
        for (int index = 0; index < 4; ++index)
            points[index] = center + ImVec2(polygon[index].x, polygon[index].y) * 18.0f;
        draw->AddConvexPolyFilled(points, 4, colour);
    }
}

#include "krux_inline_colours.inl"

void RenderBlockSelector(cold::modules::Module& module, const ImVec2& origin, float footer) {
    static char query[64]{};
    const ImVec2 position = origin + ImVec2(361, 70), extent(402, footer - 86);
    DrawPrimordialPanel("Blocks", position, extent);
    auto& catalogue = static_cast<cold::modules::BlockEsp&>(module);
    const auto& blocks = catalogue.blocks();
    const auto selectedCount = std::count_if(blocks.begin(), blocks.end(), [](const auto& block) { return block.selected->booleanValue(); });
    const std::string countLabel = std::to_string(selectedCount) + " / " + std::to_string(blocks.size());
    ImGui::GetWindowDrawList()->AddText(position + ImVec2(extent.x - 13 - ImGui::CalcTextSize(countLabel.c_str()).x, 9),
        IM_COL32(143,139,142,255), countLabel.c_str());
    ImGui::SetCursorScreenPos(position + ImVec2(13, 42));
    ImGui::SetNextItemWidth(376);
    ImGui::InputTextWithHint("##block-search", "Search names or registry IDs...", query, sizeof(query));
    std::string needle = query;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char value) { return char(std::tolower(value)); });
    std::vector<std::size_t> filtered;
    for (std::size_t index = 0; index < blocks.size(); ++index)
        if (needle.empty() || blocks[index].search.find(needle) != std::string::npos) filtered.push_back(index);
    ImGui::SetCursorScreenPos(position + ImVec2(13, 77));
    ImGui::BeginChild("##block-tiles", ImVec2(376, extent.y - 147), false, ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar);
    ImGuiListClipper clipper;
    clipper.Begin(int((filtered.size() + 1) / 2), 54);
    while (clipper.Step()) for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        for (int column = 0; column < 2; ++column) {
            const auto at = std::size_t(row * 2 + column);
            if (at >= filtered.size()) break;
            const auto index = filtered[at]; const auto& block = blocks[index];
            catalogue.requestIcon(index);
            ImGui::SetCursorPos(ImVec2(4 + column * 177.0f, row * 54.0f + 3));
            ImGui::PushID(block.id.c_str());
            const ImVec2 tile = ImGui::GetCursorScreenPos(), size(169,46);
            ImGui::InvisibleButton("##block",size);
            if (ImGui::IsItemClicked()) { block.selected->setBoolean(!block.selected->booleanValue()); PlayMenuClick(); }
            const bool checked = block.selected->booleanValue(), hovered = ImGui::IsItemHovered();
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(tile,tile+size,hovered ? MenuHeader() : MenuBody(),5);
            draw->AddRect(tile,tile+size,checked ? MenuAccent(140) : IM_COL32(51,51,51,255),5);
            auto& icon = g_blockTileTextures[block.id];
            if (icon.revision != block.revision) {
                if (icon.view) { icon.view->Release(); icon.view = nullptr; }
                icon.revision = block.revision;
                if (!block.pixels.empty() && g_device) {
                    D3D11_TEXTURE2D_DESC description{};
                    description.Width = description.Height = 32; description.MipLevels = description.ArraySize = 1;
                    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM; description.SampleDesc.Count = 1;
                    description.Usage = D3D11_USAGE_IMMUTABLE; description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    D3D11_SUBRESOURCE_DATA data{block.pixels.data(),32*4,0};
                    ID3D11Texture2D* texture = nullptr;
                    if (SUCCEEDED(g_device->CreateTexture2D(&description,&data,&texture))) {
                        g_device->CreateShaderResourceView(texture,nullptr,&icon.view); texture->Release();
                    }
                }
            }
            if (icon.view) draw->AddImage(reinterpret_cast<ImTextureID>(icon.view),tile+ImVec2(6,7),tile+ImVec2(38,39));
            else draw->AddText(tile+ImVec2(18,16),IM_COL32(100,100,100,255),block.hasIcon ? "..." : "-");
            draw->PushClipRect(tile+ImVec2(43,4),tile+ImVec2(145,42),true);
            draw->AddText(tile+ImVec2(43,8),checked ? IM_COL32(230,225,228,255) : IM_COL32(166,162,165,255),block.name.c_str());
            const auto path = block.id.substr(block.id.find(':')+1);
            draw->AddText(ImGui::GetFont(),9,tile+ImVec2(43,26),IM_COL32(112,109,111,255),path.c_str());
            draw->PopClipRect();
            const ImVec2 check = tile+ImVec2(150,17);
            draw->AddRectFilled(check,check+ImVec2(12,12),checked ? MenuAccent() : IM_COL32(20,20,20,255),2);
            if (checked) {
                draw->AddLine(check+ImVec2(2,6),check+ImVec2(5,9),IM_COL32(29,29,29,255),1.5f);
                draw->AddLine(check+ImVec2(5,9),check+ImVec2(10,3),IM_COL32(29,29,29,255),1.5f);
            }
            if (hovered) DrawPrimordialTooltip(block.name.c_str(),block.id.c_str(),block.hasIcon?"":"Technical block; no inventory item icon",g_textScale);
            ImGui::PopID();
        }
    }
    if (blocks.empty()) ImGui::TextDisabled("Loading Minecraft block registry...");
    else if (filtered.empty()) ImGui::TextDisabled("No matching blocks.");
    ImGui::EndChild();
    auto* colourMode = module.setting("Colour mode");
    auto* highlight = module.setting("Highlight colour");
    if (!colourMode || !highlight) return;
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 palette = position + ImVec2(13, extent.y - 54);
    draw->AddLine(palette - ImVec2(0,7), palette + ImVec2(376,-7), IM_COL32(56,53,55,255));
    draw->AddText(palette, IM_COL32(166,162,165,255), "Highlight colour");
    constexpr unsigned int colours[] = {0xC19AA4,0x627CB6,0x44BFD6,0x43BB75,0xE0BA47,0xC5845E,0xE75052,0x8D65BD};
    for (int index = 0; index < int(std::size(colours)); ++index) {
        ImGui::PushID(index);
        const ImVec2 swatch = palette + ImVec2(index * 28.0f, 20);
        ImGui::SetCursorScreenPos(swatch);
        const auto colour = cold::Color::rgb(colours[index]);
        if (ImGui::InvisibleButton("##highlight-swatch", ImVec2(20,18))) {
            highlight->setColour(colour); colourMode->setChoice(2); PlayMenuClick();
        }
        draw->AddRectFilled(swatch, swatch + ImVec2(20,18), ImGui::ColorConvertFloat4ToU32(ImVec4(colour.r,colour.g,colour.b,1)), 3);
        const auto chosen = highlight->colour();
        const bool active = colourMode->choice() == 2 && std::abs(chosen.r-colour.r)<.005f && std::abs(chosen.g-colour.g)<.005f && std::abs(chosen.b-colour.b)<.005f;
        if (active) draw->AddRect(swatch-ImVec2(2,2),swatch+ImVec2(22,20),MenuAccent(),4,0,1);
        if (ImGui::IsItemHovered()) DrawPrimordialTooltip("Selected block colour","Use this colour for all selected blocks","",g_textScale);
        ImGui::PopID();
    }
}
void RenderCrosshairPattern(cold::modules::CrossHair& module) {
    ImGui::Spacing(); ImGui::Separator(); ImGui::TextUnformatted("Pattern editor");
    ImGui::TextDisabled("Left draws, right erases");
    const ImVec2 origin=ImGui::GetCursorScreenPos();
    const float cell=PrimordialControlWidth()/21;
    auto* draw=ImGui::GetWindowDrawList();
    for(int y=0;y<21;++y) for(int x=0;x<21;++x) {
        ImGui::PushID(y*21+x); const ImVec2 position=origin+ImVec2(x*cell,y*cell);
        ImGui::SetCursorScreenPos(position);ImGui::InvisibleButton("##pixel",ImVec2(cell,cell));
        if(ImGui::IsItemHovered()) {
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) module.pixel(x,y,true);
            if(ImGui::IsMouseDown(ImGuiMouseButton_Right)) module.pixel(x,y,false);
        }
        draw->AddRectFilled(position,position+ImVec2(cell-1,cell-1),module.pixel(x,y)?MenuAccent():IM_COL32(43,43,43,255));
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(origin+ImVec2(0,21*cell+8));
    if(ImGui::Button("Reset pattern",ImVec2(PrimordialControlWidth(),23))) {module.resetPattern();PlayMenuClick();}
    if(ImGui::Button("Clear pattern",ImVec2(PrimordialControlWidth(),23))) {module.resetPattern(true);PlayMenuClick();}
}

void SetHudEditing(bool enabled) {
    g_hudEditing = enabled;
    if(!enabled&&g_menuPage==2)g_menuPage=1;
    if (auto* hudModule = cold::modules::modules().find("HUD")) {
        if (enabled) cold::modules::modules().queueEnabled("HUD", true);
    }
    cold::ui::client().hud().setLayoutEditor(enabled);
    LONG_PTR style = GetWindowLongPtrW(g_overlayWindow, GWL_EXSTYLE);
    SetWindowLongPtrW(g_overlayWindow,GWL_EXSTYLE,style|WS_EX_LAYERED);
    SetLayeredWindowAttributes(g_overlayWindow,RGB(1,2,3),255,LWA_COLORKEY);
    SetWindowRgn(g_overlayWindow,nullptr,TRUE);
    if(!enabled)SaveInterface();
}

void RenderClassicToolbar();
#include "krux_hud_editor.inl"

bool ClassicCanvas() { return true; }
bool FullCanvas() { return true; }

#include "krux_classic_toolbar.inl"

void RenderClassicPreferenceSettings(bool mainMenu);
void RenderClassicColumns() {
    auto& manager=cold::modules::modules();
    static std::unordered_map<std::string,bool> folded;
    static std::unordered_map<std::string,float> categoryBrightness,moduleBrightness;
    static std::unordered_map<std::string,float> contentHeights;
    struct ScrollMotion {float position=0,velocity=0,lastApplied=0;bool initialized=false;};
    static std::unordered_map<std::string,ScrollMotion> scrollMotion;
    g_sliderWheelConsumed=false;
    if(g_resetClassicLayout){manager.config().clearSection("classic.layout");folded.clear();contentHeights.clear();g_classicFocus.clear();g_resetClassicLayout=false;}
    const char* categories[]={"Combat","Movement","Visuals","Player","Misc","Exploits","Others"};
    constexpr int categoryCount=int(std::size(categories));
    const auto screen=ImGui::GetIO().DisplaySize;
    if(manager.config().getInt("interface","centeredClassicLayout",0)<2) {
        manager.config().clearSection("classic.layout");
        manager.config().setInt("interface","centeredClassicLayout",2);
    }
    const float columnCanvas=screen.x-16;
    const float width=std::min(g_classicColumnWidth,std::max(80.f,columnCanvas));
    const float headerHeight=std::max(g_classicHeaderHeight,ImGui::GetFont()->RasterSize(ImGui::GetFontSize()*g_textScale)+8.f);
    const int perRow=std::max(1,std::min(categoryCount,int((columnCanvas+8.01f)/(width+8))));
    const int rowCount=(categoryCount+perRow-1)/perRow;
    const float rowSpace=std::max(90.f,(screen.y-68)/rowCount);
    const float rowHeight=g_moduleRowHeight*g_textScale;
    for(int category=0;category<categoryCount;++category) {
        std::vector<cold::modules::Module*> entries;
        for(const auto& entry:manager.all()) {
            auto* module=entry.get();
            if(module->name()=="HUD"||module->name()=="TargetHud"||module->name()=="RegionMap"||module->name()=="Admins") continue;
            if(krux::catalog::classicLocation(module->name(),module->category())==category&&ModuleMatchesSearch(module->name())) entries.push_back(module);
        }
        const std::string title=std::string(categories[category])+"##classic-category-flat-v4";
        if(!folded.contains(title))folded[title]=manager.config().getBool("classic.layout",std::string(categories[category])+"Collapsed",false);
        const float content=contentHeights.count(title)?contentHeights[title]:(entries.size()+(category==6?2:0))*rowHeight;
        auto& saved=manager.config();const std::string positionKey=categories[category];
        const ImVec2 desired{saved.getNumber("classic.layout",positionKey+"X",8+(category%perRow)*(width+8)),saved.getNumber("classic.layout",positionKey+"Y",60+(category/perRow)*rowSpace)};
        // Settings grow below the saved header; changing content never relocates the category.
        const ImVec2 anchor{std::clamp(desired.x,0.f,std::max(0.f,screen.x-width)),std::clamp(desired.y,52.f,std::max(52.f,screen.y-headerHeight-44.f))};
        const float expandedHeight=std::min({std::max(68.f,g_classicFocus.category==category?screen.y-anchor.y-8:rowSpace-8),std::max(headerHeight+18,screen.y-anchor.y-8),headerHeight+4+content});
        float height=folded[title]?headerHeight:expandedHeight;
        ImGui::SetNextWindowPos(anchor,ImGuiCond_Always);
        ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize,{0,0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{1,0});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,0});
        ImGui::Begin(title.c_str(),nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowFontScale(g_textScale);
        const auto pos=ImGui::GetWindowPos();auto* draw=ImGui::GetWindowDrawList();
        const float fade=1-std::exp(-18*ImGui::GetIO().DeltaTime);
        if(!categoryBrightness.contains(title))categoryBrightness[title]=1;
        float& categoryShade=categoryBrightness[title];
        categoryShade+=((g_dimInactivePanels&&g_classicFocus.active()&&g_classicFocus.category!=category?g_classicInactiveBrightness:1.f)-categoryShade)*fade;
        draw->AddRectFilled(pos,pos+ImVec2(width,height),MenuBody());
        const auto header=cold::Color::mix(g_headerColour,cold::ui::theme().accent,.4f);
        draw->AddRectFilled(pos,pos+ImVec2(width,headerHeight),ImGui::ColorConvertFloat4ToU32({header.r,header.g,header.b,g_menuPanelOpacity}));
        draw->PushClipRect(pos,pos+ImVec2(width,height),false);
        draw->AddRect(pos+ImVec2(.5f,.5f),pos+ImVec2(width-.5f,height-.5f),MenuOutline(180));
        draw->PopClipRect();
        draw->AddLine(pos+ImVec2(1,headerHeight),pos+ImVec2(width-1,headerHeight),MenuOutline(150));
        draw->AddText(pos+ImVec2(6,(headerHeight-ImGui::GetFontSize())*.5f),IM_COL32(238,234,237,255),categories[category]);
        draw->AddText(pos+ImVec2(width-13,(headerHeight-ImGui::GetFontSize())*.5f),IM_COL32(218,212,216,255),folded[title]?"+":"-");
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("##category-header",{width,headerHeight});
        if(ImGui::IsItemClicked(ImGuiMouseButton_Right)||(ImGui::IsItemClicked(ImGuiMouseButton_Left)&&ImGui::GetIO().MousePos.x>=pos.x+width-22)) {
            folded[title]=!folded[title];height=folded[title]?headerHeight:expandedHeight;
            saved.setBool("classic.layout",positionKey+"Collapsed",folded[title]);
            if(folded[title]&&g_classicFocus.category==category)g_classicFocus.clear();
            ImGui::SetWindowSize({width,height});
        }
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const auto delta=pos+ImGui::GetIO().MouseDelta;
            const ImVec2 moved{std::clamp(delta.x,0.f,std::max(0.f,screen.x-width)),std::clamp(delta.y,52.f,std::max(52.f,screen.y-headerHeight-44.f))};
            ImGui::SetWindowPos(moved);
            saved.setNumber("classic.layout",positionKey+"X",moved.x,1);saved.setNumber("classic.layout",positionKey+"Y",moved.y,1);
        }
        if(!folded[title]) {
            ImGui::SetCursorScreenPos(pos+ImVec2(1,headerHeight+1));
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,0.f);
            ImGui::BeginChild("##entries",{width-2,height-headerHeight-2},false,ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
            auto& motion=scrollMotion[title];
            auto* scrollWindow=ImGui::GetCurrentWindow();
            const float limit=ImGui::GetScrollMaxY();
            const float scroll=ImGui::GetScrollY();
            if(!motion.initialized||std::abs(scroll-motion.lastApplied)>1.f){motion.position=scroll;motion.velocity=0;motion.initialized=true;}
            if(limit<=0){motion.position=motion.velocity=0;}
            else {
                const float dt=std::min(ImGui::GetIO().DeltaTime,.05f)/4;
                for(int step=0;step<4;++step){
                    const float boundary=std::clamp(motion.position,0.f,limit);
                    motion.velocity*=std::exp(-10*dt);
                    if(motion.position!=boundary){motion.velocity+=(boundary-motion.position)*220*dt;motion.velocity*=std::exp(-22*dt);}
                    motion.position=std::clamp(motion.position+motion.velocity*dt,-28.f,limit+28.f);
                }
                const float boundary=std::clamp(motion.position,0.f,limit);
                if(std::abs(motion.position-boundary)<.03f&&std::abs(motion.velocity)<.3f){motion.position=boundary;motion.velocity=0;}
            }
            // Move both hit boxes and vertices together; never animate only the painted content.
            const float scrollOffset=scroll-motion.position;
            const float previousMax=scrollWindow->DC.CursorMaxPos.y;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY()+scrollOffset);
            draw=ImGui::GetWindowDrawList();
            const float start=ImGui::GetCursorPosY();
            if(category==6)for(const char* name:{"ClickGUI","Main Menu"}) {
                if(!ModuleMatchesSearch(name))continue;
                ImGui::PushID(name);const auto at=ImGui::GetCursorScreenPos();const float available=ImGui::GetContentRegionAvail().x;
                bool open=g_classicFocus.expanded(name);
                const bool clicked=ImGui::InvisibleButton("##preferences",{available,rowHeight});const bool hovered=ImGui::IsItemHovered();
                draw=ImGui::GetWindowDrawList();
                if(open||hovered)draw->AddRectFilled(at,at+ImVec2(available,rowHeight),MenuAccent(open?70:35));
                draw->AddRect(at+ImVec2(1,1),at+ImVec2(available-1,rowHeight-1),MenuOutline(hovered?150:80));
                draw->PushClipRect(at,at+ImVec2(available-18,rowHeight),true);
                draw->AddText(at+ImVec2(5,(rowHeight-ImGui::GetFontSize())*.5f),IM_COL32(225,218,224,255),name);draw->PopClipRect();
                draw->AddText(at+ImVec2(available-12,(rowHeight-ImGui::GetFontSize())*.5f),MenuAccent(),open?"-":"+");
                if(clicked||ImGui::IsItemClicked(ImGuiMouseButton_Right)){open=g_classicFocus.toggle(name,category);PlayMenuClick();}
                if(open){ImGui::Dummy({0,7});ImGui::Indent(8);ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+PrimordialControlWidth());ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,5});RenderClassicPreferenceSettings(std::string_view(name)=="Main Menu");ImGui::PopStyleVar();ImGui::PopTextWrapPos();ImGui::Unindent(8);ImGui::Dummy({0,4});}
                ImGui::PopID();
            }
            for(auto* module:entries) {
                ImGui::PushID(module);const auto at=ImGui::GetCursorScreenPos();
                const float available=ImGui::GetContentRegionAvail().x;
                const auto binding=module->key()?ModuleKeyName(module->key()):std::string{};
                bool open=g_classicFocus.expanded(module->name());
                const int firstVertex=draw->VtxBuffer.Size;
                const bool clicked=ImGui::InvisibleButton("##module",{available,rowHeight});
                const bool hovered=ImGui::IsItemHovered();
                static std::unordered_map<std::string,float> hoverPhases;
                float& phase=hoverPhases[module->name()];
                phase=std::clamp(phase+(hovered?1.f:-1.f)*ImGui::GetIO().DeltaTime/.12f,0.f,1.f);
                const float highlight=std::max(open?.36f:module->enabled()?.32f:0.f,krux::menumotion::cubicOut(phase)*.18f);
                const auto fill=cold::Color::mix(g_bodyColour,cold::ui::theme().accent,highlight);
                draw=ImGui::GetWindowDrawList();
                if(highlight>0)draw->AddRectFilled(at,at+ImVec2(available,rowHeight),ImGui::ColorConvertFloat4ToU32({fill.r,fill.g,fill.b,g_menuPanelOpacity}));
                draw->AddRect(at+ImVec2(1,1),at+ImVec2(available-1,rowHeight-1),hovered?MenuOutline(140):module->enabled()?MenuOutline(80):IM_COL32(40,38,40,int(180*g_menuOutlineOpacity)));
                const float bindingSize=ImGui::GetFont()->RasterSize(ImGui::GetFontSize()*.68f);
                const float bindingWidth=binding.empty()?0:std::min(available*.3f,ImGui::GetFont()->CalcTextSizeA(bindingSize,FLT_MAX,0,binding.c_str()).x);
                const float textRight=at.x+available-27-bindingWidth-(binding.empty()?0:4);
                draw->PushClipRect(at,at+ImVec2(std::max(10.f,textRight-at.x),rowHeight),true);
                draw->AddText(at+ImVec2(5,(rowHeight-ImGui::GetFontSize())*.5f),module->enabled()?IM_COL32(242,236,241,255):ImGui::ColorConvertFloat4ToU32({g_menuTextColour.r,g_menuTextColour.g,g_menuTextColour.b,1}),module->name().c_str());
                draw->PopClipRect();
                if(!binding.empty()){draw->PushClipRect({textRight+3,at.y},{at.x+available-27,at.y+rowHeight},true);draw->AddText(ImGui::GetFont(),bindingSize,{textRight+3,at.y+(rowHeight-bindingSize)*.5f},MenuAccent(220),binding.c_str());draw->PopClipRect();}
                draw->AddCircleFilled(at+ImVec2(available-22,rowHeight*.5f),1.8f,module->enabled()?MenuAccent():IM_COL32(75,70,76,255));
                draw->AddText(at+ImVec2(available-12,(rowHeight-ImGui::GetFontSize())*.5f),IM_COL32(213,205,214,255),open?"-":"+");
                if(clicked) {if(ImGui::GetIO().MousePos.x>=at.x+available-17){open=g_classicFocus.toggle(module->name(),category);}else manager.queueEnabled(module->name(),!module->enabled());PlayMenuClick();}
                if(ImGui::IsItemClicked(ImGuiMouseButton_Right)) {open=g_classicFocus.toggle(module->name(),category);PlayMenuClick();}
                if(hovered)DrawPrimordialTooltip(module->name().c_str(),module->description().c_str(),"Right click to expand settings",g_textScale);
                ImGui::SetCursorScreenPos(at+ImVec2(0,rowHeight));
                if(open) {
                    ImGui::Dummy({0,7});
                    ImGui::Indent(8);
                    const auto settingsTop=ImGui::GetCursorScreenPos();
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+PrimordialControlWidth());
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,5});
                    RenderBinding("Key",module->key(),module,nullptr);
                    RenderModuleActivation(*module,PrimordialControlWidth());
                    if(module->name()=="ElytraFly")ImGui::TextDisabled("Target: %s",module->suffix().c_str());
                    if(module->name()=="BlockEsp") {
                        if(ImGui::Button("Choose blocks",{PrimordialControlWidth(),24})) ImGui::OpenPopup("Block picker");
                        const auto display=ImGui::GetIO().DisplaySize;
                        const ImVec2 pickerSize{std::min(430.f,display.x-16),std::min(490.f,display.y-16)};
                        ImGui::SetNextWindowSize(pickerSize,ImGuiCond_Always);ImGui::SetNextWindowPos((display-pickerSize)*.5f,ImGuiCond_Always);
                        if(ImGui::BeginPopupModal("Block picker",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings)) {
                            const auto picker=ImGui::GetWindowPos();RenderBlockSelector(*module,picker+ImVec2(-349,-32),pickerSize.y-28);
                            ImGui::SetCursorScreenPos(picker+ImVec2(12,pickerSize.y-28));if(ImGui::Button("Done",{pickerSize.x-24,22})) ImGui::CloseCurrentPopup();ImGui::EndPopup();
                        }
                    }
                    const auto* outlineStyle=module->name()=="Shader"?module->setting("Outline style"):nullptr;
                    RenderModuleSettings(*module,!outlineStyle||outlineStyle->choice()!=0);
                    if(module->name()=="Webhooks") {
                        auto& hooks=*static_cast<cold::modules::Webhooks*>(module);hooks.sync();
                        ImGui::TextWrapped("Tests send a sample embed only after your click. They contain no real coordinates or player details.");
                        for(unsigned profile=0;profile<4;profile++){ImGui::PushID(int(profile));const std::string label="Test Profile "+std::to_string(profile+1);if(ImGui::Button(label.c_str(),{PrimordialControlWidth(),24}))hooks.testProfile(profile);ImGui::TextWrapped("%s",hooks.testStatus(profile).c_str());ImGui::PopID();}
                    }
                    if(module->name()=="CrossHair") RenderCrosshairPattern(*static_cast<cold::modules::CrossHair*>(module));
                    ImGui::Spacing();
                    draw=ImGui::GetWindowDrawList();
                    draw->AddRect(settingsTop-ImVec2(5,2),{at.x+available-2,ImGui::GetCursorScreenPos().y+1},MenuOutline(100));
                    ImGui::PopStyleVar();
                    ImGui::PopTextWrapPos();
                    ImGui::Unindent(8);
                    ImGui::Dummy({0,4});
                }
                if(!moduleBrightness.contains(module->name()))moduleBrightness[module->name()]=1;
                float& shade=moduleBrightness[module->name()];
                shade+=((g_dimInactivePanels&&g_classicFocus.active()&&!g_classicFocus.expanded(module->name())?g_classicInactiveBrightness:1.f)-shade)*fade;
                if(g_classicFocus.category==category||!g_classicFocus.active())krux::classic::dim(draw,firstVertex,shade);
                ImGui::PopID();
            }
            contentHeights[title]=ImGui::GetCursorPosY()-start;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY()-scrollOffset);
            scrollWindow->DC.CursorMaxPos.y=std::max(previousMax,scrollWindow->DC.CursorMaxPos.y-scrollOffset);
            if(ImGui::IsWindowHovered()&&!g_sliderWheelConsumed&&limit>0)motion.velocity-=ImGui::GetIO().MouseWheel*400;
            if(scrollWindow->ScrollTarget.y<FLT_MAX){motion.initialized=false;motion.velocity=0;}
            else {motion.lastApplied=std::clamp(motion.position,0.f,limit);ImGui::SetScrollY(motion.lastApplied);}
            if(g_classicFocus.category!=category&&g_classicFocus.active())krux::classic::dim(draw,0,categoryShade);
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        krux::classic::dim(ImGui::GetWindowDrawList(),0,categoryShade);
        ImGui::End();
        ImGui::PopStyleVar(3);
    }
}

#include "krux_cloud_config_ui.inl"

#include "krux_classic_pages.inl"

void RenderPrimordialMenu() {
    auto& manager = cold::modules::modules();
    if(g_reloadInterface.exchange(false))g_interfaceLoaded=false;
    if (!g_interfaceLoaded) {
        cold::ui::migrateClassicPreferences(manager.config());
        const auto& config = manager.config();
        g_menuMusic=config.getBool("interface","menuMusic",true);g_musicLoop=config.getBool("interface","musicLoop",true);
        g_musicVolume=std::clamp(config.getNumber("interface","musicVolume",.22f),0.f,1.f);g_musicFade=std::clamp(config.getNumber("interface","musicFade",1.5f),0.f,5.f);
        g_moduleNotices=config.getBool("interface","moduleNotices",true);g_noticeProgress=config.getBool("interface","noticeProgress",true);
        g_noticeStyle=std::clamp(config.getInt("interface","noticeStyle",0),0,1);g_noticePosition=std::clamp(config.getInt("interface","noticePosition",4),0,5);
        if(config.getInt("interface","noticeLayoutRevision",0)<1){g_noticePosition=4;manager.config().setInt("interface","noticePosition",4);manager.config().setInt("interface","noticeLayoutRevision",1);}
        g_noticeDuration=std::clamp(config.getNumber("interface","noticeDuration",3),1.f,10.f);g_noticeScale=std::clamp(config.getNumber("interface","noticeScale",1),.5f,3.f);g_noticeOpacity=std::clamp(config.getNumber("interface","noticeOpacity",.95f),0.f,1.f);
        g_dimInactivePanels=config.getBool("interface","classicDimOthers",true);
        g_menuBackdrop=std::clamp(config.getNumber("interface","menuBackdrop",.32f),0.f,.85f);
        g_classicInactiveBrightness=std::clamp(config.getNumber("interface","classicInactiveBrightness",.42f),.15f,.8f);
        manager.config().setInt("interface","menuStyle",g_menuStyle);
        g_matchHudTheme=config.getBool("interface","matchHud",true);
        if(config.getInt("interface","hudThemeRevision",0)<1){g_matchHudTheme=true;manager.config().setBool("interface","matchHud",true);manager.config().setInt("interface","hudThemeRevision",1);}
        g_hurtCamera=config.getBool("interface","hurtCamera",false);
        g_menuTextColour={config.getNumber("interface","menuTextR",.73f),config.getNumber("interface","menuTextG",.73f),config.getNumber("interface","menuTextB",.73f),1};
        if(config.getInt("interface","classicRevision",0)<4) {
            manager.config().setInt("interface","classicRevision",4);
            if(config.getNumber("interface","moduleRowHeight",26)==26)manager.config().setNumber("interface","moduleRowHeight",22,0);
        }
        g_clickSound = config.getBool("interface", "clickSound", true);
        g_textScale = std::clamp(config.getNumber("interface", "textScale", 1.0f), 0.75f, 2.0f);
        g_cornerRadius=std::clamp(config.getNumber("interface","cornerRadius",8),0.0f,16.0f);
        g_moduleRowHeight=std::clamp(config.getNumber("interface","moduleRowHeight",22),18.0f,34.0f);
        g_classicColumnWidth=std::clamp(config.getNumber("interface","classicColumnWidth",196),140.f,300.f);
        g_classicHeaderHeight=std::clamp(config.getNumber("interface","classicHeaderHeight",24),18.f,36.f);
        g_menuPanelOpacity=std::clamp(config.getNumber("interface","menuPanelOpacity",1),.25f,1.f);
        g_menuOutlineOpacity=std::clamp(config.getNumber("interface","menuOutlineOpacity",1),0.f,1.f);
        g_bodyColour={config.getNumber("interface","body0",.114f),config.getNumber("interface","body1",.114f),config.getNumber("interface","body2",.114f),1};
        g_headerColour={config.getNumber("interface","header0",.153f),config.getNumber("interface","header1",.153f),config.getNumber("interface","header2",.153f),1};
        if(config.getInt("interface","classicPaletteRevision",0)<1) {
            if(std::abs(g_bodyColour.r-.114f)<.005f&&std::abs(g_bodyColour.g-.114f)<.005f&&std::abs(g_bodyColour.b-.114f)<.005f)g_bodyColour=cold::Color::rgb(0x101010);
            if(std::abs(g_headerColour.r-.153f)<.005f&&std::abs(g_headerColour.g-.153f)<.005f&&std::abs(g_headerColour.b-.153f)<.005f)g_headerColour=cold::Color::rgb(0x151515);
            manager.config().setInt("interface","classicPaletteRevision",1);
        }
        const bool hasCoordinates=!config.get("interface","offsetX","").empty()&&!config.get("interface","offsetY","").empty();
        g_overlayPositionInitialized=config.getBool("interface","menuPositionSet",hasCoordinates&&(config.getInt("interface","offsetX",0)!=0||config.getInt("interface","offsetY",0)!=0));
        if(g_overlayPositionInitialized){g_overlayOffsetX=config.getInt("interface","offsetX",0);g_overlayOffsetY=config.getInt("interface","offsetY",0);}
        manager.config().setInt("interface","menuPlacementRevision",2);
        cold::ui::setAccent(cold::Color{config.getNumber("interface", "accentR", 0.757f),
            config.getNumber("interface", "accentG", 0.604f), config.getNumber("interface", "accentB", 0.643f), 1.0f});
        g_interfaceLoaded = true;
    }
    SyncNativeHudNoticeScale();
    static bool styleApplied=false;
    if(!styleApplied){ApplyStyle();styleApplied=true;}
    cold::ui::setSurfaces(g_bodyColour,g_headerColour);
    if(ClassicCanvas()&&g_gameWindow) {
        RECT gameBounds{};
        if(GetClientRect(g_gameWindow,&gameBounds)&&(std::abs(ImGui::GetIO().DisplaySize.x-gameBounds.right)>1||std::abs(ImGui::GetIO().DisplaySize.y-gameBounds.bottom)>1))return;
    }
    auto& menuStyle=ImGui::GetStyle();menuStyle.WindowRounding=0.f;menuStyle.ChildRounding=0.f;menuStyle.PopupRounding=0.f;
    menuStyle.Colors[ImGuiCol_WindowBg]={g_bodyColour.r,g_bodyColour.g,g_bodyColour.b,g_menuPanelOpacity};
    menuStyle.Colors[ImGuiCol_Text]={g_menuTextColour.r,g_menuTextColour.g,g_menuTextColour.b,1};
    menuStyle.Colors[ImGuiCol_FrameBg]={g_bodyColour.r*.6f,g_bodyColour.g*.6f,g_bodyColour.b*.6f,1};
    static bool canvasApplied=false;
    if(g_overlayWindow&&!canvasApplied) {
        SetWindowLongPtrW(g_overlayWindow,GWL_EXSTYLE,GetWindowLongPtrW(g_overlayWindow,GWL_EXSTYLE)|WS_EX_LAYERED);
        SetLayeredWindowAttributes(g_overlayWindow,RGB(1,2,3),255,LWA_COLORKEY);SetWindowRgn(g_overlayWindow,nullptr,TRUE);
        canvasApplied=true;
    }
    const auto& currentAccent = cold::ui::theme().accent;
    menuStyle.Colors[ImGuiCol_TitleBg]=menuStyle.Colors[ImGuiCol_TitleBgActive]={g_headerColour.r,g_headerColour.g,g_headerColour.b,1};
    menuStyle.Colors[ImGuiCol_TitleBgCollapsed]=menuStyle.Colors[ImGuiCol_TitleBg];
    menuStyle.Colors[ImGuiCol_PopupBg]={g_bodyColour.r,g_bodyColour.g,g_bodyColour.b,1};
    menuStyle.Colors[ImGuiCol_ResizeGrip]={currentAccent.r,currentAccent.g,currentAccent.b,.15f};
    menuStyle.Colors[ImGuiCol_ResizeGripHovered]={currentAccent.r,currentAccent.g,currentAccent.b,.5f};
    menuStyle.Colors[ImGuiCol_ResizeGripActive]={currentAccent.r,currentAccent.g,currentAccent.b,.8f};
    ImGui::GetStyle().Colors[ImGuiCol_Separator] = ImVec4(.22f, .22f, .22f, 1);
    ImGui::GetStyle().Colors[ImGuiCol_SeparatorHovered] = ImVec4(currentAccent.r, currentAccent.g, currentAccent.b, .65f);
    ImGui::GetStyle().Colors[ImGuiCol_SeparatorActive] = ImVec4(currentAccent.r, currentAccent.g, currentAccent.b, 1);
    ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = ImVec4(currentAccent.r, currentAccent.g, currentAccent.b, 1);
    ImGui::GetStyle().Colors[ImGuiCol_ScrollbarGrab] = ImVec4(currentAccent.r, currentAccent.g, currentAccent.b, 0.65f);
    ImGui::GetStyle().Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(currentAccent.r, currentAccent.g, currentAccent.b, 0.85f);
    ImGui::GetStyle().Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(currentAccent.r, currentAccent.g, currentAccent.b, 1);
    if(g_menuPage==2&&!g_hudEditing)SetHudEditing(true);
    krux::menu::backdropOpacity.store(g_hudEditing?0.f:g_menuBackdrop);
    if(g_hudEditing) {RenderHudEditor();return;}
    RenderClassicToolbar();
    if(g_menuPage==0)g_menuPage=1;
    if(g_menuPage==1)RenderClassicColumns();else RenderClassicUtilityPage();
    RenderClassicNavigation();
}

void AddModuleDetail(c_child* child, cold::modules::Module* module, ModuleControl& control) {
    child->insert_element<c_label>(module->description());

    control.module = module;
    control.syncedKey = module->key();
    control.enabled = child->insert_element<c_checkbox>("Enabled##" + module->name());
    control.enabled->set_value(module->enabled());
    control.enabled->set_has_tooltip(true);
    control.enabled->set_tooltip("Turns this module on or off.");
    control.enabled->set_interaction_callback([module, enabled = control.enabled]() {
        std::lock_guard controlLock(cold::modules::runtimeMutex());
        const bool requested = enabled->get_value();
        g_pendingEnabled[module->name()] = requested;
        cold::modules::modules().queueEnabled(module->name(), requested);
    });

    if (module->supportsToggleKey()) {
        const std::string bindLabel = "Key##module-bind-" + module->name();
        control.binding = std::make_unique<c_binding>(bindLabel,
            GlfwToVirtualKey(module->key()), keybind_interaction_type::TOGGLE);
        control.key = child->insert_element<c_keybind>("Toggle key", control.binding.get(),
            keybind_display_type::WITHOUT_LABEL);
        control.key->set_has_tooltip(true);
        control.key->set_tooltip("Click to assign a key that toggles this module.");
    }

    std::string visibleGroup;
    bool hasSettings = false;
    for (const auto& setting : module->settings()) {
        hasSettings = true;
        const std::string group = setting->parentName().empty()
            ? "General settings"
            : "Options for " + setting->parentName();
        if (group != visibleGroup) {
            auto* divider = child->insert_element<c_label_divider>(group);
            if (!setting->parentName().empty()) {
                divider->set_visibility_callback([setting = setting.get()] {
                    return setting->visible();
                });
            }
            visibleGroup = group;
        }
        AddSettingControl(child, module, setting.get());
    }
    if (!hasSettings)
        child->insert_element<c_label>("This module has no configurable options.");
}

void AddEmptyPanel(c_child* child, const char* message) {
    child->insert_element<c_label>(message);
}

void SyncModuleControls() {
    std::lock_guard lock(cold::modules::runtimeMutex());
    if (g_selectedModuleIndex < 0 || static_cast<std::size_t>(g_selectedModuleIndex) >= g_moduleControls.size()
        || TabForCategory(g_moduleControls[static_cast<std::size_t>(g_selectedModuleIndex)].module->category()) != g_activeTab
        || !ModuleMatchesSearch(g_moduleControls[static_cast<std::size_t>(g_selectedModuleIndex)].module->name())) {
        for (std::size_t i = 0; i < g_moduleControls.size(); ++i) {
            if (TabForCategory(g_moduleControls[i].module->category()) == g_activeTab
                && ModuleMatchesSearch(g_moduleControls[i].module->name())) {
                g_selectedModuleIndex = static_cast<int>(i);
                break;
            }
        }
    }
    for (std::size_t i = 0; i < g_moduleControls.size(); ++i) {
        ModuleControl& control = g_moduleControls[i];
        const std::string& name = control.module->name();
        if (control.selector) {
            control.selector->set_enabled(control.module->enabled());
        }
        if (control.binding) {
            const int boundKey = VirtualToGlfwKey(control.binding->get_bound_key());
            if (boundKey != control.syncedKey) {
                control.module->setKey(boundKey);
                control.syncedKey = boundKey;
            }
        }
        const auto pending = g_pendingEnabled.find(name);
        if (pending != g_pendingEnabled.end()) {
            if (control.module->enabled() == pending->second)
                g_pendingEnabled.erase(pending);
            else {
                control.enabled->set_value(pending->second);
                continue;
            }
        }
        control.enabled->set_value(control.module->enabled());
    }
    for (SettingControl& control : g_settingControls) {
        auto* setting = control.setting;
        if (control.boolean) control.boolean->set_value(setting->booleanValue());
        if (control.choice) control.choice->set_value(setting->choice());
        if (control.number) control.number->set_value(setting->number());
        if (control.key) {
            const int boundKey = VirtualToGlfwKey(control.binding->get_bound_key());
            if (boundKey != control.syncedKey) {
                setting->setKey(boundKey);
                control.syncedKey = boundKey;
            }
        }
        if (control.colour) {
            const cold::Color value = setting->colour();
            control.colour->set_value(c_color(value.r, value.g, value.b, value.a));
        }
    }
}

void SetupMenu() {
    auto addPanel = [](const char* title, int tab, ImVec2 position, ImVec2 size) {
        auto* child = g_menu.insert_child(title, position, size);
        child->set_visibility_callback([tab]() { return g_activeTab == tab; });
        return child;
    };
    constexpr float bodyTop = 68.0f;
    constexpr float bodyHeight = 510.0f;
    auto* moduleList = addPanel("modules", 0, ImVec2(18.0f, bodyTop), ImVec2(220.0f, bodyHeight));
    auto* visualList = addPanel("modules", 1, ImVec2(18.0f, bodyTop), ImVec2(220.0f, bodyHeight));
    auto* clientList = addPanel("modules", 2, ImVec2(18.0f, bodyTop), ImVec2(220.0f, bodyHeight));
    auto* scriptList = addPanel("modules", 4, ImVec2(18.0f, bodyTop), ImVec2(220.0f, bodyHeight));
    auto* configList = addPanel("profiles", 3, ImVec2(18.0f, bodyTop), ImVec2(340.0f, bodyHeight));
    auto* configInfo = addPanel("storage", 3, ImVec2(378.0f, bodyTop), ImVec2(480.0f, bodyHeight));

    AddEmptyPanel(configList, "Local profiles");
    AddEmptyPanel(configInfo, "Profiles are stored in the injector folder.");

    std::lock_guard lock(cold::modules::runtimeMutex());
    cold::modules::Category previousCategory = cold::modules::Category::Count;
    for (const auto& entry : cold::modules::modules().all()) {
        cold::modules::Module* module = entry.get();
        const int tab = TabForCategory(module->category());
        c_child* list = tab == 0 ? moduleList : tab == 1 ? visualList
            : tab == 2 ? clientList : scriptList;
        if (module->category() != previousCategory) {
            auto* divider = list->insert_element<c_label_divider>(cold::modules::categoryName(module->category()));
            const auto category = module->category();
            divider->set_visibility_callback([tab, category]() {
                return g_activeTab == tab && std::any_of(g_moduleControls.begin(), g_moduleControls.end(),
                    [category](const ModuleControl& control) {
                        return control.module->category() == category
                            && ModuleMatchesSearch(control.module->name());
                    });
            });
            previousCategory = module->category();
        }

        ModuleControl control{};
        control.module = module;
        const std::size_t index = g_moduleControls.size();
        control.selector = list->insert_element<c_module_selector>(module->name()
            + "##module-select-" + std::to_string(index), &g_selectedModuleIndex,
            static_cast<int>(index));
        control.selector->set_visibility_callback([tab, module]() {
            return g_activeTab == tab && ModuleMatchesSearch(module->name());
        });
        control.selector->set_interaction_callback([index]() {
            g_selectedModuleIndex = static_cast<int>(index);
        });

        auto* detail = g_menu.insert_child(module->name(),
            ImVec2(252.0f, bodyTop), ImVec2(606.0f, bodyHeight));
        detail->set_visibility_callback([index, tab, module]() {
            return g_activeTab == tab && g_selectedModuleIndex == static_cast<int>(index)
                && ModuleMatchesSearch(module->name());
        });
        AddModuleDetail(detail, module, control);
        g_moduleControls.push_back(std::move(control));
    }

    for (int tab = 0; tab < 5; ++tab) {
        if (tab == 3) continue;
        auto* noResults = g_menu.insert_child("search-empty-" + std::to_string(tab),
            ImVec2(252.0f, bodyTop), ImVec2(606.0f, bodyHeight));
        noResults->set_visibility_callback([tab]() {
            return g_activeTab == tab && !TabHasSearchMatch(tab);
        });
        noResults->insert_element<c_label>("No matching modules. Clear the search to browse this section.");
    }

    auto* combatTab = g_menu.insert_element<c_click_label>("combat", 0, &g_activeTab);
    combatTab->set_custom_position(ImVec2(164.0f, 12.0f));
    auto* visualsTab = g_menu.insert_element<c_click_label>("visuals", 1, &g_activeTab);
    visualsTab->set_same_line(true);
    auto* miscTab = g_menu.insert_element<c_click_label>("misc", 2, &g_activeTab);
    miscTab->set_same_line(true);
    auto* configsTab = g_menu.insert_element<c_click_label>("configs", 3, &g_activeTab);
    configsTab->set_same_line(true);
    auto* scriptsTab = g_menu.insert_element<c_click_label>("scripts", 4, &g_activeTab);
    scriptsTab->set_same_line(true);

    g_menu.insert_attachment([](const ImVec2& position, const ImVec2& size, ImDrawList* draw) {
        const float alpha = ImGui::GetStyle().Alpha;
        const ImVec2 origin = position + ImVec2(1.0f, 1.0f);
        const ImVec2 extent = size - ImVec2(2.0f, 2.0f);
        const float footerTop = extent.y - 29.0f;
        draw->AddRectFilled(origin, origin + extent, IM_COL32(23, 23, 23, 255 * alpha), 8.0f);
        constexpr float checkerSize = 4.0f;
        const ImVec2 checkerMin = origin + ImVec2(0.0f, 56.0f);
        const ImVec2 checkerMax = origin + ImVec2(extent.x, footerTop);
        for (float y = checkerMin.y; y < checkerMax.y; y += checkerSize) {
            for (float x = checkerMin.x; x < checkerMax.x; x += checkerSize) {
                const int cellX = static_cast<int>((x - checkerMin.x) / checkerSize);
                const int cellY = static_cast<int>((y - checkerMin.y) / checkerSize);
                if (((cellX + cellY) & 1) == 0)
                    draw->AddRectFilled(ImVec2(x, y), ImVec2((std::min)(x + checkerSize, checkerMax.x), (std::min)(y + checkerSize, checkerMax.y)), IM_COL32(255, 255, 255, 5 * alpha));
            }
        }
        const ImVec2 topRight = origin + ImVec2(extent.x, 56.0f);
        draw->AddRectFilled(origin, topRight, IM_COL32(41, 39, 46, 255 * alpha), 0.0f,
            ImDrawFlags_RoundCornersTop);
        draw->AddRectFilled(ImVec2(origin.x, topRight.y - 1.0f),
            ImVec2(origin.x + extent.x, topRight.y + 1.0f), IM_COL32(193, 154, 164, 220 * alpha));

        ImGui::PushFont(g_primordialBold ? g_primordialBold : framework::bold_font);
        const char* brand = "krux.lol";
        const ImVec2 textSize = ImGui::CalcTextSize(brand);
        draw->AddText(ImVec2(origin.x + 16.0f, origin.y + (56.0f - textSize.y) / 2.0f),
            IM_COL32(230, 230, 230, 255 * alpha), brand);
        ImGui::PopFont();

        const ImVec2 footerMin(origin.x, origin.y + footerTop);
        const ImVec2 footerMax(origin.x + extent.x, origin.y + extent.y);
        draw->AddRectFilled(footerMin, footerMax, IM_COL32(41, 39, 46, 255 * alpha), 0.0f,
            ImDrawFlags_RoundCornersBottom);
        draw->AddRectFilled(footerMin, ImVec2(footerMax.x, footerMin.y + 1.0f),
            IM_COL32(193, 154, 164, 220 * alpha));
        draw->AddText(ImVec2(footerMin.x + 14.0f, footerMin.y + 9.0f),
            IM_COL32(157, 157, 157, 240 * alpha), "krux.lol  /  INSERT to hide");

        ImGui::SetCursorPos(ImVec2(extent.x - 220.0f, 15.0f));
        ImGui::SetNextItemWidth(194.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.11f, 0.13f, 1.0f));
        ImGui::InputTextWithHint("##krux_module_search", "Search modules", g_moduleSearch,
            sizeof(g_moduleSearch));
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    });

    g_menu.set_start_render_callback([]() {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    });
}

void LoadOriginalMenuAssets() {
    ImGui::CreateShadowTexture(g_device);
    framework::g_scaled_fonts.emplace_back<scaled_font_t>({ "C:\\Windows\\Fonts\\Verdanab.ttf", 14.0f, ImGuiFreeTypeBuilderFlags_ForceAutoHint });
    framework::g_scaled_fonts.emplace_back<scaled_font_t>({ "C:\\Windows\\Fonts\\Tahoma.ttf", 13.0f, ImGuiFreeTypeBuilderFlags_ForceAutoHint });
    framework::g_scaled_fonts.emplace_back<scaled_font_t>({ "C:\\Windows\\Fonts\\Tahoma.ttf", 12.0f, static_cast<ImGuiFreeTypeBuilderFlags>(ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting) });
    framework::g_scaled_fonts.emplace_back<scaled_font_t>({ "C:\\Windows\\Fonts\\Tahomabd.ttf", 12.0f, static_cast<ImGuiFreeTypeBuilderFlags>(ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting) });
    framework::g_scaled_fonts.emplace_back<scaled_font_t>({ "C:\\Windows\\Fonts\\verdanaz.ttf", 48.0f, ImGuiFreeTypeBuilderFlags_ForceAutoHint });
    framework::g_scaled_fonts.emplace_back<scaled_font_t>({ "C:\\Windows\\Fonts\\Verdana.ttf", 12.0f, static_cast<ImGuiFreeTypeBuilderFlags>(ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting) });
    if (!framework::build_fonts()) {
        framework::bold_font = ImGui::GetIO().Fonts->Fonts[0];
        ImGui::GetIO().FontDefault = ImGui::GetIO().Fonts->Fonts[0];
    }
    ImGuiIO& io = ImGui::GetIO();
    static const std::string layoutPath=[] {wchar_t folder[MAX_PATH]{};GetEnvironmentVariableW(L"APPDATA",folder,MAX_PATH);const auto path=std::filesystem::path(folder)/L"krux.lol";std::filesystem::create_directories(path);return (path/L"minecraft-menu.ini").string();}();
    io.IniFilename=layoutPath.empty()?nullptr:layoutPath.c_str();
    ImFontConfig fontConfig{};
    fontConfig.FontDataOwnedByAtlas = false;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;
    static const ImWchar glyphRanges[] = { 0x0020, 0x00FF, 0x0400, 0x04FF, 0 };
    const auto smooth=cold::assets::store().font(cold::assets::FontAsset::InterMedium);
    const auto bold=cold::assets::store().font(cold::assets::FontAsset::InterSemiBold);
    const auto rico=cold::assets::store().font(cold::assets::FontAsset::RicoPixel);
    g_primordialMedium = smooth.data?io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(smooth.data),int(smooth.size),13.f,&fontConfig,glyphRanges):nullptr;
    g_primordialBold = bold.data?io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(bold.data),int(bold.size),13.f,&fontConfig,glyphRanges):nullptr;
    ImFontConfig ricoConfig{};ricoConfig.FontDataOwnedByAtlas=false;
    g_ricoMenuFont=rico.data?io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(rico.data),int(rico.size),10.f,&ricoConfig):nullptr;
    g_minecraftImGui.prepare(*io.Fonts);
    g_pixelMenuFont=g_minecraftImGui.font;
    static const ImWchar iconRanges[] = { 0x0020, 0x00FF, 0 };
    g_primordialIcons = io.Fonts->AddFontFromMemoryTTF(Icons,
        static_cast<int>(sizeof(Icons)), 20.0f, &fontConfig, iconRanges);
    g_primordialLogIcons = io.Fonts->AddFontFromMemoryTTF(IconFontLog,
        static_cast<int>(sizeof(IconFontLog)), 17.0f, &fontConfig, iconRanges);
    if (g_primordialMedium && g_primordialBold && g_primordialIcons && g_primordialLogIcons && io.Fonts->Build()) {
        g_minecraftImGui.populate(*io.Fonts);
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ImGui_ImplDX11_CreateDeviceObjects();
        io.FontDefault = g_primordialMedium;
        framework::bold_font = g_primordialBold;
    } else {
        g_primordialMedium = nullptr;
        g_primordialBold = nullptr;
        g_primordialIcons = nullptr;
        g_primordialLogIcons = nullptr;
    }
}

LRESULT WINAPI WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == kMenuKeyMessage) {
        if (!KruxMenuWantsInput()) return 0;
        const int virtualKey = LOWORD(wParam);
        const UINT keyMessage = lParam == cold::hooks::kRelease ? WM_KEYUP : WM_KEYDOWN;
        WindowProc(hwnd, keyMessage, virtualKey,
            static_cast<LPARAM>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC)) << 16);
        if (ImGui::GetCurrentContext()) {
            const int modifiers = HIWORD(wParam);
            ImGuiIO& io = ImGui::GetIO();
            io.AddKeyEvent(ImGuiMod_Shift, (modifiers & 1) != 0);
            io.AddKeyEvent(ImGuiMod_Ctrl, (modifiers & 2) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (modifiers & 4) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (modifiers & 8) != 0);
        }
        return 0;
    }
    if(msg==kMenuScrollMessage) {
        if(KruxMenuWantsInput()&&ImGui::GetCurrentContext())
            ImGui::GetIO().AddMouseWheelEvent(float(int(wParam))/1000.f,float(int(lParam))/1000.f);
        return 0;
    }
    if (msg == kMenuCharMessage) {
        if (KruxMenuWantsInput() && ImGui::GetCurrentContext()) ImGui::GetIO().AddInputCharacter(static_cast<unsigned int>(wParam));
        return 0;
    }
    static bool escapeHeld=false;
    const bool escapeDown=(msg==WM_KEYDOWN||msg==WM_SYSKEYDOWN)&&wParam==VK_ESCAPE;
    const bool escapePress=escapeDown&&!escapeHeld;
    if(escapeDown)escapeHeld=true;
    if((msg==WM_KEYUP||msg==WM_SYSKEYUP)&&wParam==VK_ESCAPE)escapeHeld=false;
    if(msg==WM_KILLFOCUS||(msg==WM_ACTIVATEAPP&&!wParam))escapeHeld=false;
    if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && (g_bindingModule || g_bindingSetting)) {
        std::lock_guard lock(cold::modules::runtimeMutex());
        int virtualKey = static_cast<int>(wParam);
        if (virtualKey == VK_SHIFT) virtualKey = MapVirtualKeyW((lParam >> 16) & 0xff, MAPVK_VSC_TO_VK_EX);
        if (virtualKey == VK_CONTROL) virtualKey = (lParam & (1 << 24)) ? VK_RCONTROL : VK_LCONTROL;
        if (virtualKey == VK_MENU) virtualKey = (lParam & (1 << 24)) ? VK_RMENU : VK_LMENU;
        const int key = (virtualKey == VK_ESCAPE || virtualKey == VK_DELETE) ? 0 : VirtualToGlfwKey(virtualKey);
        if (g_bindingModule) g_bindingModule->setKey(key);
        if (g_bindingSetting) g_bindingSetting->setKey(key);
        g_bindingModule = nullptr;
        g_bindingSetting = nullptr;
        PlayMenuClick();
        return 0;
    }
    if(escapePress&&g_hudEditing&&ImGui::GetCurrentContext()
        &&!ImGui::GetIO().WantTextInput&&GImGui->OpenPopupStack.Size==0) {
        std::lock_guard lock(cold::modules::runtimeMutex());
        SetHudEditing(false);
        PlayMenuClick();
        return 0;
    }
    if (msg == WM_HOTKEY && wParam == kHotkeyId) {
        const HWND foreground = GetForegroundWindow();
        if (foreground != g_gameWindow && foreground != hwnd) return 0;
        if (g_hudEditing) {
            std::lock_guard lock(cold::modules::runtimeMutex());
            SetHudEditing(false);
            return 0;
        }
        const bool visible = !g_menuVisible.load();
        g_menuVisible.store(visible);
        if (visible) {
            cold::modules::modules().queueEnabled("Radar", false);
            SyncOverlayToGame();
            CaptureOverlayInput();
        } else {
            ReleaseOverlayInput();
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    }

    if (msg == kMenuClickMessage) { PlayMenuClick(); return 0; }
    if (msg == WM_LBUTTONDOWN) {
        const int mouseX = static_cast<short>(LOWORD(lParam));
        const int mouseY = static_cast<short>(HIWORD(lParam));
        const bool onHeader = mouseY >= 1 && mouseY < 54 && mouseX < kPanelWidth - 190;
        const bool onDragArea = !FullCanvas() && onHeader;
        if (g_menuVisible && onDragArea) {
            g_draggingMenu = true;
            GetCursorPos(&g_dragStartCursor);
            RECT windowRect{};
            GetWindowRect(hwnd, &windowRect);
            g_dragStartWindow = POINT{ windowRect.left, windowRect.top };
            SetCapture(hwnd);
            g_overlayInputCaptured = true;
            return 0;
        }
    }

    if (msg == WM_MOUSEMOVE && g_draggingMenu && (wParam & MK_LBUTTON)) {
        // Consume the message here; one cursor sample per rendered frame prevents a queue of window moves.
        return 0;
    }

    if (msg == WM_LBUTTONUP && g_draggingMenu) {
        g_draggingMenu = false;
        if (GetCapture() == hwnd) ReleaseCapture();
        return 0;
    }

    if (msg == WM_ACTIVATEAPP && !wParam) {
        ReleaseOverlayInput();
        if (ImGui::GetCurrentContext()) ImGui::GetIO().ClearInputKeys();
        ShowWindow(hwnd, SW_HIDE);
    }

    if (msg == WM_CANCELMODE) ReleaseOverlayInput();
    if (msg == WM_CAPTURECHANGED) g_draggingMenu = false;

    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return TRUE;

    switch (msg) {
    case WM_SIZE:
        if (g_device && wParam != SIZE_MINIMIZED) {
            // DXGI cannot resize a back buffer while its view is still bound to the context.
            g_context->OMSetRenderTargets(0, nullptr, nullptr);
            DestroyRenderTarget();
            const HRESULT resized=g_swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            if(FAILED(resized))COLD_ERROR("menu","Surface resize failed: 0x%08lx",static_cast<unsigned long>(resized));
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        if (g_stop) g_stop->store(true);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

extern "C" const unsigned char* KruxMenuFont(unsigned int* size) {
    static const std::vector<unsigned char> font=[] {
        wchar_t windows[MAX_PATH]{};if(!GetWindowsDirectoryW(windows,MAX_PATH))return std::vector<unsigned char>{};
        std::ifstream input(std::filesystem::path(windows)/L"Fonts"/L"tahoma.ttf",std::ios::binary);
        return std::vector<unsigned char>(std::istreambuf_iterator<char>(input),{});
    }();
    if(!font.empty()){*size=static_cast<unsigned>(font.size());return font.data();}
    *size = static_cast<unsigned int>(sizeof(Intermedium));
    return Intermedium;
}
extern "C" void KruxPlayMenuClick() {
    if (HWND window = g_menuInputWindow.load()) PostMessageW(window, kMenuClickMessage, 0, 0);
}

extern "C" void KruxSetMenuVisible(bool visible) {
    g_menuVisible.store(visible);
    if (!visible) {g_menuInputActive.store(false);krux::menu::surfaceActive.store(false);}
}
extern "C" void KruxReloadInterface() {g_reloadInterface.store(true);}

extern "C" bool KruxMenuWantsInput() {
    return g_menuVisible.load() && g_menuInputActive.load();
}

extern "C" void KruxForwardMenuKey(int key, int action, int modifiers) {
    const HWND window = g_menuInputWindow.load();
    const int virtualKey = GlfwToVirtualKey(key);
    if (window && virtualKey && KruxMenuWantsInput())
        PostMessageW(window, kMenuKeyMessage, MAKEWPARAM(virtualKey, modifiers), action);
}

extern "C" void KruxForwardMenuScroll(double x,double y) {
    if(const auto window=g_menuInputWindow.load();window&&KruxMenuWantsInput())
        PostMessageW(window,kMenuScrollMessage,WPARAM(int(x*1000)),LPARAM(int(y*1000)));
}

extern "C" void KruxForwardMenuChar(unsigned int codepoint) {
    const HWND window = g_menuInputWindow.load();
    if (window && KruxMenuWantsInput()) PostMessageW(window, kMenuCharMessage, codepoint, 0);
}

void RunOverlay(std::atomic_bool& stopRequested) {
    g_stop = &stopRequested;
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitializeCom = SUCCEEDED(comResult);
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"UtilityTestAgentWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    // D3D owns its surface; a class DC is incompatible with layered HUD windows.
    wc.style = 0;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.hIcon = CreateKruxIcon(32);
    wc.hIconSm = CreateKruxIcon(16);
    RegisterClassExW(&wc);

    g_overlayWindow = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, className,
        L"", WS_POPUP, 0, 0, kPanelWidth, kPanelHeight,
        nullptr, nullptr, instance, nullptr);
    if (!g_overlayWindow) {
        UnregisterClassW(className, instance);
        if (wc.hIcon) DestroyIcon(wc.hIcon);
        if (wc.hIconSm) DestroyIcon(wc.hIconSm);
        if (uninitializeCom) CoUninitialize();
        return;
    }
    if (!CreateDeviceD3D(g_overlayWindow)) {
        CleanupDeviceD3D();
        DestroyWindow(g_overlayWindow);
        UnregisterClassW(className, instance);
        if (wc.hIcon) DestroyIcon(wc.hIcon);
        if (wc.hIconSm) DestroyIcon(wc.hIconSm);
        if (uninitializeCom) CoUninitialize();
        return;
    }
    // Clip the real overlay surface as well as its ImGui paint, preventing DXGI clear pixels at rounded corners.
    SetWindowRgn(g_overlayWindow, CreateRoundRectRgn(0, 0, kPanelWidth + 1, kPanelHeight + 1, std::max(1,int(g_cornerRadius*2)), std::max(1,int(g_cornerRadius*2))), TRUE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;
    ApplyStyle();
    ImGui_ImplWin32_Init(g_overlayWindow);
    ImGui_ImplDX11_Init(g_device, g_context);
    g_menuInputWindow.store(g_overlayWindow);
    krux::menu::SurfaceReadback menuReadback;
    bool menuAssetsLoaded=false;
    bool presentationLogged=false;
    HRESULT lastPresentation=S_OK;

    MSG msg{};
    DWORD lastSync = 0;
    DWORD lastPreferenceCheck = 0;
    bool lastRequestedVisibility = g_menuVisible.load();
    bool lastEditorState = FullCanvas();
    bool insertWasHeld=(GetAsyncKeyState(VK_INSERT)&0x8000)!=0;
    timeBeginPeriod(1);
    constexpr auto frameBudget = std::chrono::microseconds(8333);
    while (!stopRequested.load()) {
        const auto frameStart = std::chrono::steady_clock::now();
        const bool insertHeld=(GetAsyncKeyState(VK_INSERT)&0x8000)!=0;
        if(insertHeld&&!insertWasHeld&&g_gameWindow&&GetForegroundWindow()==g_gameWindow)
            PostMessageW(g_overlayWindow,WM_HOTKEY,kHotkeyId,0);
        insertWasHeld=insertHeld;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) stopRequested.store(true);
        }
        if (stopRequested.load()) break;

        if (g_draggingMenu) {
            if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
                g_draggingMenu = false;
                if (GetCapture() == g_overlayWindow) ReleaseCapture();
            } else {
                POINT cursor{};
                if (GetCursorPos(&cursor)) MoveOverlayTo(g_dragStartWindow.x + cursor.x - g_dragStartCursor.x,
                    g_dragStartWindow.y + cursor.y - g_dragStartCursor.y);
            }
        }

        const DWORD now = GetTickCount();
        const bool requestedVisibility = g_menuVisible.load();
        if (now - lastSync > 100 || requestedVisibility != lastRequestedVisibility || lastEditorState != FullCanvas()) {
            SyncOverlayToGame();
            lastSync = now;
            lastRequestedVisibility = requestedVisibility;
            lastEditorState = FullCanvas();
        }
        if (g_menuBuilt && g_interfaceLoaded && !g_reloadInterface.load() && now - lastPreferenceCheck >= 1000
            && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
            std::lock_guard lock(cold::modules::runtimeMutex());
            SaveInterface();
            lastPreferenceCheck = now;
        }
        if (!g_menuVisible || !krux::menu::surfaceActive.load()) {
            g_bindingModule = nullptr;
            g_bindingSetting = nullptr;
            Sleep(8);
            continue;
        }

        if(!menuAssetsLoaded){LoadOriginalMenuAssets();menuAssetsLoaded=true;}
        if(g_desktopMenu)ImGui::GetIO().ConfigFlags&=~ImGuiConfigFlags_NoMouseCursorChange;
        else ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        // The menu HWND is hidden; it cannot restore a cursor hidden by GLFW.
        // Composite a pointer only when Windows is not already displaying one.
        CURSORINFO systemCursor{sizeof(CURSORINFO)};
        const bool systemCursorVisible=GetCursorInfo(&systemCursor)&&(systemCursor.flags&CURSOR_SHOWING);
        ImGui::GetIO().MouseDrawCursor=!g_desktopMenu&&KruxMenuWantsInput()&&!systemCursorVisible;
        if (KruxMenuWantsInput()) {
            ImGuiIO& io = ImGui::GetIO();
            // Color-keyed editor pixels pass native mouse messages through to Minecraft.
            // Polling keeps a drag alive across transparent parts of the full game canvas.
            POINT pointer{};
            if (GetCursorPos(&pointer) && ScreenToClient(g_overlayWindow, &pointer))
                io.AddMousePosEvent(static_cast<float>(pointer.x), static_cast<float>(pointer.y));
            constexpr int mouseKeys[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
            for (int button = 0; button < 5; ++button)
                io.AddMouseButtonEvent(button, (GetAsyncKeyState(mouseKeys[button]) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Ctrl, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Super, ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0);
        }
        if(!g_pixelMenuFont) {
            static auto nextFontAttempt=std::chrono::steady_clock::now();
            const auto now=std::chrono::steady_clock::now();
            if(now>=nextFontAttempt){nextFontAttempt=now+std::chrono::seconds(1);
                if(g_minecraftImGui.prepare(*ImGui::GetIO().Fonts)&&ImGui::GetIO().Fonts->Build()){
                    g_minecraftImGui.populate(*ImGui::GetIO().Fonts);g_pixelMenuFont=g_minecraftImGui.font;
                    ImGui_ImplDX11_InvalidateDeviceObjects();ImGui_ImplDX11_CreateDeviceObjects();
                }
            }
        }
        ImGui::NewFrame();
        // The surface HWND stays hidden, so header dragging also comes from polled input.
        if(!FullCanvas()&&KruxMenuWantsInput()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const auto pointer=ImGui::GetIO().MousePos;
            if(pointer.x>=0&&pointer.x<ImGui::GetIO().DisplaySize.x&&pointer.y>=0&&pointer.y<30) {
                g_draggingMenu=true;GetCursorPos(&g_dragStartCursor);
                RECT bounds{};GetWindowRect(g_overlayWindow,&bounds);g_dragStartWindow={bounds.left,bounds.top};
            }
        }
        {
            std::lock_guard lock(cold::modules::runtimeMutex());
            if (!g_menuBuilt && KruxModulesReady()) {
                g_menuBuilt = true;
            }
            const int menuFont=krux::fonts::preference(cold::modules::modules().config(),krux::fonts::Domain::ClickGui);
            if(menuFont==1&&g_pixelMenuFont)ImGui::GetIO().FontDefault=g_pixelMenuFont;else if(menuFont==2&&g_ricoMenuFont)ImGui::GetIO().FontDefault=g_ricoMenuFont;else if(g_primordialMedium)ImGui::GetIO().FontDefault=g_primordialMedium;
            if (g_menuBuilt) {RenderPrimordialMenu();if(g_interfaceLoaded&&ImGui::IsMouseReleased(ImGuiMouseButton_Left))SaveInterface();}
        }
        if (GImGui->OpenPopupStack.Size > 0 && GImGui->OpenPopupStack.back().Window)
            ImGui::BringWindowToDisplayFront(GImGui->OpenPopupStack.back().Window);
        ImGui::Render();

        const bool desktopCanvas=g_desktopMenu&&FullCanvas();
        const float clearColor[4] = {desktopCanvas?1.f/255:0.f,desktopCanvas?2.f/255:0.f,desktopCanvas?3.f/255:0.f,desktopCanvas?1.f:0.f};
        g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        g_context->ClearRenderTargetView(g_renderTarget, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        ID3D11Texture2D* source=nullptr;
        HRESULT presentation=g_swapChain->GetBuffer(0,IID_PPV_ARGS(&source));
        POINT origin{};int canvasWidth=0,canvasHeight=0;
        if(SUCCEEDED(presentation)&&!g_desktopMenu&&GetGameClientBounds(&origin,&canvasWidth,&canvasHeight)) {
            if(!menuReadback.capture(g_device,g_context,source,FullCanvas()?0:g_overlayOffsetX,
                FullCanvas()?0:g_overlayOffsetY,canvasWidth,canvasHeight))presentation=E_FAIL;
        }
        if(source)source->Release();
        if(g_desktopMenu&&SUCCEEDED(presentation))presentation=g_swapChain->Present(0,0);
        if(FAILED(presentation)&&presentation!=lastPresentation)
            COLD_ERROR("menu","DXGI presentation failed: 0x%08lx",static_cast<unsigned long>(presentation));
        if(!presentationLogged&&SUCCEEDED(presentation)&&ImGui::GetDrawData()->TotalVtxCount>0) {
            COLD_INFO("menu","native surface %dx%d, style %d, vertices %d, presentation 0x%08lx",
                int(ImGui::GetIO().DisplaySize.x),int(ImGui::GetIO().DisplaySize.y),g_menuStyle,
                ImGui::GetDrawData()->TotalVtxCount,static_cast<unsigned long>(presentation));
            presentationLogged=true;
        }
        lastPresentation=presentation;

        const auto frameDeadline = frameStart + (g_desktopMenu?std::chrono::microseconds(16666):frameBudget);
        while (std::chrono::steady_clock::now() < frameDeadline) {
            const auto remaining = frameDeadline - std::chrono::steady_clock::now();
            if (remaining > std::chrono::milliseconds(1)) Sleep(1);
            else SwitchToThread();
        }
    }

    timeEndPeriod(1);
    if (g_menuBuilt && g_interfaceLoaded && !g_reloadInterface.load()) {
        std::lock_guard lock(cold::modules::runtimeMutex());
        SaveInterface();
    }
    g_menuInputActive.store(false);
    g_menuInputWindow.store(nullptr);
    menuReadback.reset();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_overlayWindow);
    UnregisterClassW(className, instance);
    if (wc.hIcon) DestroyIcon(wc.hIcon);
    if (wc.hIconSm) DestroyIcon(wc.hIconSm);
    g_overlayWindow = nullptr;
    g_stop = nullptr;
    if (uninitializeCom) CoUninitialize();
}
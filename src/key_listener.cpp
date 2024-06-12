#include "./key_listener.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winuser.h>
#include <unordered_map>
#include <vector>

namespace util 
{

static std::unordered_map<key_code, std::vector<keyboard_callback>> g_callbacks = {};

HHOOK g_kb_hook;

// https://docs.microsoft.com/en-us/previous-versions/windows/desktop/legacy/ms644985(v=vs.85)
// https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-kbdllhookstruct?redirectedfrom=MSDN
LRESULT WINAPI global_keyboard_handler(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0) {
        return CallNextHookEx(g_kb_hook, code, wParam, lParam);
    }
    KBDLLHOOKSTRUCT* info = (KBDLLHOOKSTRUCT*)(lParam);
    DWORD virtual_key = info->vkCode;
    WPARAM key_type = wParam;
    if (g_callbacks.find(virtual_key) != g_callbacks.end()) {
        for (auto &callback: g_callbacks.at(virtual_key)) {
            callback(key_type);
        }
    }
    return CallNextHookEx(g_kb_hook, code, wParam, lParam);
};

void init_global_keyboard_listener() {
    g_kb_hook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)(global_keyboard_handler), NULL, 0);
}

void attach_global_keyboard_listener(key_code key, keyboard_callback callback) {
    g_callbacks[key].push_back(callback);
}


}

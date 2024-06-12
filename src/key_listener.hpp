#pragma once

#include <functional>

namespace util 
{

using event_code = unsigned long long;
using key_code = unsigned long;
using keyboard_callback = std::function<void(event_code)>;
void init_global_keyboard_listener();
void attach_global_keyboard_listener(key_code key, keyboard_callback callback);

}

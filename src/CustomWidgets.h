#pragma once
#include "imgui.h"
#include <string>

namespace CustomWidgets {
bool InputTextMultiline(const char *label, std::string *str,
                        const ImVec2 &size = ImVec2(0, 0),
                        ImGuiInputTextFlags flags = 0);
}

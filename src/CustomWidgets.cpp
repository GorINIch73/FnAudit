
#include "CustomWidgets.h"
#include "imgui.h"
#include "imgui_internal.h" // For ImTextCharFromUtf8
#include <string>
#include <vector>
#include <algorithm>

namespace CustomWidgets {

namespace { // Anonymous namespace for helper
struct InputTextCallback_UserData {
        std::string *Str;
        ImGuiInputTextCallback ChainCallback;
        void *ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData *data) {
    InputTextCallback_UserData *user_data =
        (InputTextCallback_UserData *)data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string *str = user_data->Str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = (char *)str->data();
    } else if (user_data->ChainCallback) {
        data->UserData = user_data->ChainCallbackUserData;
        return user_data->ChainCallback(data);
    }
    return 0;
}
} // namespace

bool InputText(const char *label, std::string *str, ImGuiInputTextFlags flags) {
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = nullptr;
    cb_user_data.ChainCallbackUserData = nullptr;
    return ImGui::InputText(label, (char *)str->c_str(), str->capacity() + 1,
                            flags, InputTextCallback, &cb_user_data);
}

bool InputTextMultiline(const char *label, std::string *str, const ImVec2 &size,
                        ImGuiInputTextFlags flags) {
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = nullptr;
    cb_user_data.ChainCallbackUserData = nullptr;

    return ImGui::InputTextMultiline(label, (char *)str->c_str(),
                                     str->capacity() + 1, size, flags,
                                     InputTextCallback, &cb_user_data);
}

// Implementation based on user's explicit request for character-level wrapping
bool InputTextMultilineWithWrap(const char *label, std::string *str,
                                const ImVec2 &size,
                                ImGuiInputTextFlags flags) {
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    
    bool changed = false;
    float width = (size.x <= 0) ? ImGui::GetContentRegionAvail().x : size.x;

    // Create a temporary string with manual character-level wrapping
    std::string wrapped_text;
    wrapped_text.reserve(str->size()); 
    float current_line_width = 0.0f;
    const char* p_text = str->c_str();
    const char* text_end = p_text + str->size();

    while (p_text < text_end) {
        const char* p_char_start = p_text;
        unsigned int c = 0;
        int char_len = ImTextCharFromUtf8(&c, p_text, text_end);
        p_text += char_len;
        if (c == 0) break;

        float char_width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, p_char_start, p_text).x;

        if (*p_char_start != '\n' && *p_char_start != '\r' && current_line_width + char_width > width && width > 0) {
            wrapped_text.push_back('\n');
            current_line_width = 0.0f;
        }
        
        wrapped_text.append(p_char_start, char_len);

        if (*p_char_start == '\n' || *p_char_start == '\r') {
            current_line_width = 0.0f;
        } else {
            current_line_width += char_width;
        }
    }

    // Use the existing callback system to manage the buffer for the wrapped text
    flags |= ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_NoHorizontalScroll;
    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = &wrapped_text;
    cb_user_data.ChainCallback = nullptr;
    cb_user_data.ChainCallbackUserData = nullptr;

    ImVec2 multiline_size = ImVec2(width, (size.y > 0) ? size.y : ImGui::GetTextLineHeight() * 4);
    
    changed = ImGui::InputTextMultiline(label, (char*)wrapped_text.data(), wrapped_text.capacity() + 1,
                                  multiline_size, flags, InputTextCallback, &cb_user_data);

    if (changed) {
        // Per user's example, strip all newlines from the result.
        // This means the field cannot contain intentional newlines.
        wrapped_text.erase(std::remove_if(wrapped_text.begin(), wrapped_text.end(), 
            [](char c){ return c == '\n' || c == '\r'; }), wrapped_text.end());
        *str = wrapped_text;
    }

    return changed;
}

} // namespace CustomWidgets

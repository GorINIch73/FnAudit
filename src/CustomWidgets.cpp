
#include "CustomWidgets.h"
#include "imgui.h"
#include <string>

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

} // namespace CustomWidgets

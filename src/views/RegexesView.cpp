#include "RegexesView.h"
#include "../IconsFontAwesome6.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <algorithm>
#include <cstring>

RegexesView::RegexesView()
    : selectedRegexIndex(-1), isAdding(false) {
    memset(filterText, 0, sizeof(filterText));
}

void RegexesView::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
    RefreshData();
}

void RegexesView::SetPdfReporter(PdfReporter* reporter) {
    // Not used in this view
}

void RegexesView::RefreshData() {
    if (dbManager) {
        regexes = dbManager->getRegexes();
        selectedRegexIndex = -1;
    }
}

const char* RegexesView::GetTitle() {
    return "Справочник 'Регулярные выражения'";
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> RegexesView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Имя", "Паттерн"};
    std::vector<std::vector<std::string>> rows;
    for (const auto& entry : regexes) {
        rows.push_back({std::to_string(entry.id), entry.name, entry.pattern});
    }
    return {headers, rows};
}

void RegexesView::Render() {
    if (!IsVisible) {
        return;
    }

    if (!ImGui::Begin(GetTitle(), &IsVisible)) {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowAppearing()) {
        RefreshData();
    }

    // --- Control Panel ---
    if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
        isAdding = true;
        selectedRegexIndex = -1;
        selectedRegex = Regex{-1, "", ""};
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
        if (!isAdding && selectedRegexIndex != -1 && dbManager) {
            dbManager->deleteRegex(regexes[selectedRegexIndex].id);
            RefreshData();
            selectedRegex = Regex{};
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Сохранить")) {
        if (dbManager) {
            if (isAdding) {
                dbManager->addRegex(selectedRegex);
                isAdding = false;
            } else if (selectedRegex.id != -1) {
                dbManager->updateRegex(selectedRegex);
            }
            RefreshData();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
        RefreshData();
    }

    ImGui::Separator();
    ImGui::InputText("Фильтр по имени", filterText, sizeof(filterText));
    
    // --- Regex List ---
    float list_width = ImGui::GetContentRegionAvail().x * 0.4f;
    ImGui::BeginChild("RegexList", ImVec2(list_width, 0), true);
    if (ImGui::BeginTable("regex_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Имя");
        ImGui::TableHeadersRow();

        for (int i = 0; i < regexes.size(); ++i) {
            if (filterText[0] != '\0' && strcasestr(regexes[i].name.c_str(), filterText) == nullptr) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool is_selected = (selectedRegexIndex == i);
            char label[128];
            sprintf(label, "%d##%d", regexes[i].id, i);
            if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedRegexIndex = i;
                selectedRegex = regexes[i];
                isAdding = false;
            }
             ImGui::TableNextColumn();
            ImGui::Text("%s", regexes[i].name.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // --- Editor ---
    ImGui::BeginChild("RegexEditor", ImVec2(0, 0), true);
    if (selectedRegexIndex != -1 || isAdding) {
        if (isAdding) {
            ImGui::Text("Добавление нового выражения");
        } else {
            ImGui::Text("Редактирование выражения ID: %d", selectedRegex.id);
        }
        
        ImGui::InputText("Имя", &selectedRegex.name);
        ImGui::InputTextMultiline("Паттерн", &selectedRegex.pattern, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8));

    } else {
        ImGui::Text("Выберите выражение для редактирования или добавьте новое.");
    }
    ImGui::EndChild();

    ImGui::End();
}

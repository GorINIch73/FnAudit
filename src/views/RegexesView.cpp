#include "RegexesView.h"
#include "../CustomWidgets.h"
#include "../IconsFontAwesome6.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <algorithm>
#include <cstring>

RegexesView::RegexesView()
    : selectedRegexIndex(-1),
      isAdding(false),
      isDirty(false) {
    Title = "Справочник 'Регулярные выражения'";
    memset(filterText, 0, sizeof(filterText));
}

void RegexesView::SetUIManager(UIManager *manager) { uiManager = manager; }

void RegexesView::SetDatabaseManager(DatabaseManager *manager) {
    dbManager = manager;
    RefreshData();
}

void RegexesView::SetPdfReporter(PdfReporter *reporter) {
    // Not used in this view
}

void RegexesView::RefreshData() {
    if (dbManager) {
        regexes = dbManager->getRegexes();
        selectedRegexIndex = -1;
    }
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
RegexesView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Имя", "Паттерн"};
    std::vector<std::vector<std::string>> rows;
    for (const auto &entry : regexes) {
        rows.push_back({std::to_string(entry.id), entry.name, entry.pattern});
    }
    return {headers, rows};
}

void RegexesView::OnDeactivate() { SaveChanges(); }

void RegexesView::SaveChanges() {
    if (!isDirty) {
        return;
    }

    if (dbManager) {
        if (isAdding) {
            dbManager->addRegex(selectedRegex);
            isAdding = false;
        } else if (selectedRegex.id != -1) {
            dbManager->updateRegex(selectedRegex);
        }

        // Note: We don't call RefreshData() here to avoid invalidating
        // indices during iteration. Just update selectedRegex with the new data
        auto it =
            std::find_if(regexes.begin(), regexes.end(), [&](const Regex &r) {
                return r.id == selectedRegex.id;
            });
        if (it != regexes.end()) {
            selectedRegex = *it;
        }
    }

    isDirty = false;
}

void RegexesView::Render() {
    if (!IsVisible) {
        if (isDirty) {
            SaveChanges();
        }
        return;
    }

    if (ImGui::Begin(GetTitle(), &IsVisible)) {

        if (ImGui::IsWindowAppearing()) {
            RefreshData();
        }

        // --- Control Panel ---
        if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
            SaveChanges();
            isAdding = true;
            selectedRegexIndex = -1;
            selectedRegex = Regex{-1, "", ""};
            originalRegex = selectedRegex;
            isDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
            if (!isAdding && selectedRegexIndex != -1) {
                regex_id_to_delete = regexes[selectedRegexIndex].id;
                show_delete_popup = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
            SaveChanges();
            RefreshData();
        }

        // if (show_delete_popup) {
        // ImGui::OpenPopup("Подтверждение удаления##Regex");
        // }
        if (CustomWidgets::ConfirmationModal(
                "Подтверждение удаления регекс", "Подтверждение удаления",
                "Вы уверены, что хотите удалить это выражение?\nЭто действие "
                "нельзя отменить.",
                "Да", "Нет", show_delete_popup)) {
            if (dbManager && regex_id_to_delete != -1) {
                dbManager->deleteRegex(regex_id_to_delete);
                RefreshData();
                selectedRegex = Regex{};
                originalRegex = Regex{};
            }
            regex_id_to_delete = -1;
        }

        ImGui::Separator();
        ImGui::InputText("Фильтр по имени", filterText, sizeof(filterText));
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK "##clear_filter_regex")) {
            filterText[0] = '\0';
        }

        // --- Regex List ---
        float list_width = ImGui::GetContentRegionAvail().x * 0.4f;
        ImGui::BeginChild("RegexList", ImVec2(list_width, 0), true);
        if (ImGui::BeginTable("regex_table", 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("ID");
            ImGui::TableSetupColumn("Имя");
            ImGui::TableHeadersRow();

            bool need_to_break = false;
            for (int i = 0; i < (int)regexes.size() && !need_to_break; i++) {
                if (filterText[0] != '\0' &&
                    strcasestr(regexes[i].name.c_str(), filterText) ==
                        nullptr) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                int regex_id = regexes[i].id;
                auto original_it = std::find_if(
                    regexes.begin(), regexes.end(),
                    [&](const Regex &r) {
                        return r.id == regex_id;
                    });
                int original_index =
                    (original_it == regexes.end())
                        ? -1
                        : std::distance(regexes.begin(), original_it);

                bool is_selected = (selectedRegexIndex == original_index);
                char label[128];
                sprintf(label, "%d##%d", regexes[i].id, i);
                if (ImGui::Selectable(label, is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    if (selectedRegexIndex != original_index &&
                        original_index != -1) {
                        SaveChanges();
                        // Now refresh the data
                        RefreshData();
                        // Re-find the regex by ID
                        auto new_it = std::find_if(
                            regexes.begin(), regexes.end(),
                            [&](const Regex &r) {
                                return r.id == regex_id;
                            });
                        int new_index =
                            (new_it == regexes.end())
                                ? -1
                                : std::distance(regexes.begin(), new_it);
                        if (new_index != -1 && new_index < (int)regexes.size()) {
                            selectedRegexIndex = new_index;
                            selectedRegex = regexes[new_index];
                            originalRegex = regexes[new_index];
                            isAdding = false;
                            isDirty = false;
                        }
                        need_to_break = true;
                    }
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
        if ((selectedRegexIndex != -1 && selectedRegexIndex < (int)regexes.size()) || isAdding) {
            if (isAdding) {
                ImGui::Text("Добавление нового выражения");
            } else {
                ImGui::Text("Редактирование выражения ID: %d",
                            selectedRegex.id);
            }

            if (ImGui::InputText("Имя", &selectedRegex.name)) {
                isDirty = true;
            }
            if (ImGui::InputTextMultiline(
                    "Паттерн", &selectedRegex.pattern,
                    ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8))) {
                isDirty = true;
            }

        } else {
            ImGui::Text(
                "Выберите выражение для редактирования или добавьте новое.");
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

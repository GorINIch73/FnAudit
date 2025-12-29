#include "KosguView.h"
#include <iostream>
#include <cstring>
#include "../IconsFontAwesome6.h"
#include <algorithm>

KosguView::KosguView()
    : dbManager(nullptr), pdfReporter(nullptr), selectedKosguIndex(-1), showEditModal(false), isAdding(false) {
    memset(filterText, 0, sizeof(filterText));
}

void KosguView::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
}

void KosguView::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
}

void KosguView::RefreshData() {
    if (dbManager) {
        kosguEntries = dbManager->getKosguEntries();
        selectedKosguIndex = -1;
    }
}

const char* KosguView::GetTitle() {
    return "Справочник КОСГУ";
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> KosguView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Код", "Наименование"};
    std::vector<std::vector<std::string>> rows;
    for (const auto& entry : kosguEntries) {
        rows.push_back({std::to_string(entry.id), entry.code, entry.name});
    }
    return {headers, rows};
}

// Вспомогательная функция для сортировки
static void SortKosgu(std::vector<Kosgu>& kosguEntries, const ImGuiTableSortSpecs* sort_specs) {
    std::sort(kosguEntries.begin(), kosguEntries.end(), [&](const Kosgu& a, const Kosgu& b) {
        for (int i = 0; i < sort_specs->SpecsCount; i++) {
            const ImGuiTableColumnSortSpecs* column_spec = &sort_specs->Specs[i];
            int delta = 0;
            switch (column_spec->ColumnIndex) {
                case 0: delta = (a.id < b.id) ? -1 : (a.id > b.id) ? 1 : 0; break;
                case 1: delta = a.code.compare(b.code); break;
                case 2: delta = a.name.compare(b.name); break;
                default: break;
            }
            if (delta != 0) {
                return (column_spec->SortDirection == ImGuiSortDirection_Ascending) ? (delta < 0) : (delta > 0);
            }
        }
        return false;
    });
}


void KosguView::Render() {
    if (!IsVisible) {
        return;
    }

    if (!ImGui::Begin(GetTitle(), &IsVisible)) {
        ImGui::End();
        return;
    }

    if (dbManager && kosguEntries.empty()) {
        RefreshData();
    }

    // Панель управления
    if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
        isAdding = true;
        selectedKosguIndex = -1;
        selectedKosgu = Kosgu{-1, "", ""};
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
        if (!isAdding && selectedKosguIndex != -1 && dbManager) {
            dbManager->deleteKosguEntry(kosguEntries[selectedKosguIndex].id);
            RefreshData();
            selectedKosgu = Kosgu{};
        }
    }
     ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Сохранить")) {
        if (dbManager) {
            if (isAdding) {
                dbManager->addKosguEntry(selectedKosgu);
                isAdding = false;
            } else if (selectedKosgu.id != -1) {
                dbManager->updateKosguEntry(selectedKosgu);
            }
            RefreshData();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
        RefreshData();
    }

    ImGui::Separator();

    ImGui::InputText("Фильтр по наименованию", filterText, sizeof(filterText));

    const float editorHeight = ImGui::GetTextLineHeightWithSpacing() * 8;
    
    ImGui::BeginChild("KosguList", ImVec2(0, -editorHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("kosgu_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
        ImGui::TableSetupColumn("Код", ImGuiTableColumnFlags_DefaultSort, 0.0f, 1);
        ImGui::TableSetupColumn("Наименование", 0, 0.0f, 2);
        ImGui::TableHeadersRow();

        // Сортировка
        if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsDirty) {
                SortKosgu(kosguEntries, sort_specs);
                sort_specs->SpecsDirty = false;
            }
        }

        for (int i = 0; i < kosguEntries.size(); ++i) {
            if (filterText[0] != '\0' && strcasestr(kosguEntries[i].name.c_str(), filterText) == nullptr) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            bool is_selected = (selectedKosguIndex == i);
            char label[256];
            sprintf(label, "%d##%d", kosguEntries[i].id, i);
            if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedKosguIndex = i;
                selectedKosgu = kosguEntries[i];
                isAdding = false;
            }
            if (is_selected) {
                 ImGui::SetItemDefaultFocus();
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", kosguEntries[i].code.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", kosguEntries[i].name.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::Separator();

    ImGui::BeginChild("KosguEditor");
    if (selectedKosguIndex != -1 || isAdding) {
        if (isAdding) {
            ImGui::Text("Добавление новой записи КОСГУ");
        } else {
            ImGui::Text("Редактирование КОСГУ ID: %d", selectedKosgu.id);
        }
        char codeBuf[256];
        char nameBuf[256];

        snprintf(codeBuf, sizeof(codeBuf), "%s", selectedKosgu.code.c_str());
        snprintf(nameBuf, sizeof(nameBuf), "%s", selectedKosgu.name.c_str());

        if (ImGui::InputText("Код", codeBuf, sizeof(codeBuf))) {
            selectedKosgu.code = codeBuf;
        }
        if (ImGui::InputText("Наименование", nameBuf, sizeof(nameBuf))) {
            selectedKosgu.name = nameBuf;
        }
    } else {
        ImGui::Text("Выберите запись для редактирования или добавьте новую.");
    }
    ImGui::EndChild();

    ImGui::End();
}


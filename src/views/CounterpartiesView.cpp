#include "CounterpartiesView.h"
#include <iostream>
#include <cstring>
#include "../IconsFontAwesome6.h"
#include <algorithm>

CounterpartiesView::CounterpartiesView()
    : dbManager(nullptr), pdfReporter(nullptr), selectedCounterpartyIndex(-1), showEditModal(false), isAdding(false) {
    memset(filterText, 0, sizeof(filterText));
}

void CounterpartiesView::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
}

void CounterpartiesView::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
}

void CounterpartiesView::RefreshData() {
    if (dbManager) {
        counterparties = dbManager->getCounterparties();
        selectedCounterpartyIndex = -1;
    }
}

// Вспомогательная функция для сортировки
static void SortCounterparties(std::vector<Counterparty>& counterparties, const ImGuiTableSortSpecs* sort_specs) {
    std::sort(counterparties.begin(), counterparties.end(), [&](const Counterparty& a, const Counterparty& b) {
        for (int i = 0; i < sort_specs->SpecsCount; i++) {
            const ImGuiTableColumnSortSpecs* column_spec = &sort_specs->Specs[i];
            int delta = 0;
            switch (column_spec->ColumnIndex) {
                case 0: delta = (a.id < b.id) ? -1 : (a.id > b.id) ? 1 : 0; break;
                case 1: delta = a.name.compare(b.name); break;
                case 2: delta = a.inn.compare(b.inn); break;
                default: break;
            }
            if (delta != 0) {
                return (column_spec->SortDirection == ImGuiSortDirection_Ascending) ? (delta < 0) : (delta > 0);
            }
        }
        return false;
    });
}


void CounterpartiesView::Render() {
    if (!IsVisible) {
        return;
    }

    if (!ImGui::Begin("Справочник 'Контрагенты'", &IsVisible)) {
        ImGui::End();
        return;
    }

    if (dbManager && counterparties.empty()) {
        RefreshData();
    }

    // Панель управления
    if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
        isAdding = true;
        selectedCounterpartyIndex = -1; // Сброс выделения
        selectedCounterparty = Counterparty{-1, "", ""};
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
        if (!isAdding && selectedCounterpartyIndex != -1 && dbManager) {
            dbManager->deleteCounterparty(counterparties[selectedCounterpartyIndex].id);
            RefreshData();
            selectedCounterparty = Counterparty{};
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Сохранить")) {
         if (dbManager) {
            if (isAdding) {
                dbManager->addCounterparty(selectedCounterparty);
                isAdding = false;
            } else if(selectedCounterparty.id != -1) {
                dbManager->updateCounterparty(selectedCounterparty);
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

    const float editorHeight = ImGui::GetTextLineHeightWithSpacing() * 8;

    // Таблица со списком
    ImGui::BeginChild("CounterpartiesList", ImVec2(0, -editorHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("counterparties_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
        ImGui::TableSetupColumn("Наименование", ImGuiTableColumnFlags_DefaultSort, 0.0f, 1);
        ImGui::TableSetupColumn("ИНН", 0, 0.0f, 2);
        ImGui::TableHeadersRow();

        // Сортировка
        if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsDirty) {
                SortCounterparties(counterparties, sort_specs);
                sort_specs->SpecsDirty = false;
            }
        }

        for (int i = 0; i < counterparties.size(); ++i) {
            if (filterText[0] != '\0' && strcasestr(counterparties[i].name.c_str(), filterText) == nullptr) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            bool is_selected = (selectedCounterpartyIndex == i);
            char label[256];
            sprintf(label, "%d##%d", counterparties[i].id, i);
            if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedCounterpartyIndex = i;
                selectedCounterparty = counterparties[i];
                isAdding = false;
            }
            if (is_selected) {
                 ImGui::SetItemDefaultFocus();
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", counterparties[i].name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", counterparties[i].inn.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Редактор
    ImGui::BeginChild("CounterpartyEditor");
    if (selectedCounterpartyIndex != -1 || isAdding) {
        if (isAdding) {
            ImGui::Text("Добавление нового контрагента");
        } else {
            ImGui::Text("Редактирование контрагента ID: %d", selectedCounterparty.id);
        }

        char nameBuf[256];
        char innBuf[256];
        
        snprintf(nameBuf, sizeof(nameBuf), "%s", selectedCounterparty.name.c_str());
        snprintf(innBuf, sizeof(innBuf), "%s", selectedCounterparty.inn.c_str());

        if (ImGui::InputText("Наименование", nameBuf, sizeof(nameBuf))) {
            selectedCounterparty.name = nameBuf;
        }
        if (ImGui::InputText("ИНН", innBuf, sizeof(innBuf))) {
            selectedCounterparty.inn = innBuf;
        }
    } else {
        ImGui::Text("Выберите контрагента для редактирования или добавьте нового.");
    }
    ImGui::EndChild();

    ImGui::End();
}

#include "CounterpartiesView.h"
#include <iostream>
#include <cstring>

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
    if (ImGui::Button("Добавить")) {
        isAdding = true;
        selectedCounterpartyIndex = -1; // Сброс выделения
        selectedCounterparty = Counterparty{-1, "", ""};
    }
    ImGui::SameLine();
    if (ImGui::Button("Удалить")) {
        if (!isAdding && selectedCounterpartyIndex != -1 && dbManager) {
            dbManager->deleteCounterparty(counterparties[selectedCounterpartyIndex].id);
            RefreshData();
            selectedCounterparty = Counterparty{};
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Сохранить")) {
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
    if (ImGui::Button("Обновить")) {
        RefreshData();
    }

    ImGui::Separator();

    ImGui::InputText("Фильтр по имени", filterText, sizeof(filterText));

    const float editorHeight = ImGui::GetTextLineHeightWithSpacing() * 8;

    // Таблица со списком
    ImGui::BeginChild("CounterpartiesList", ImVec2(0, -editorHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("counterparties_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Наименование");
        ImGui::TableSetupColumn("ИНН");
        ImGui::TableHeadersRow();

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

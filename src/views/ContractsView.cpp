#include "ContractsView.h"
#include <iostream>
#include <cstring>
#include "../IconsFontAwesome6.h"
#include <algorithm>

ContractsView::ContractsView()
    : dbManager(nullptr), pdfReporter(nullptr), selectedContractIndex(-1), showEditModal(false), isAdding(false) {
    memset(filterText, 0, sizeof(filterText));
}

void ContractsView::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
}

void ContractsView::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
}

void ContractsView::RefreshData() {
    if (dbManager) {
        contracts = dbManager->getContracts();
        selectedContractIndex = -1;
    }
}

void ContractsView::RefreshDropdownData() {
    if (dbManager) {
        counterpartiesForDropdown = dbManager->getCounterparties();
    }
}

// Вспомогательная функция для сортировки
static void SortContracts(std::vector<Contract>& contracts, const ImGuiTableSortSpecs* sort_specs) {
    std::sort(contracts.begin(), contracts.end(), [&](const Contract& a, const Contract& b) {
        for (int i = 0; i < sort_specs->SpecsCount; i++) {
            const ImGuiTableColumnSortSpecs* column_spec = &sort_specs->Specs[i];
            int delta = 0;
            switch (column_spec->ColumnIndex) {
                case 0: delta = (a.id < b.id) ? -1 : (a.id > b.id) ? 1 : 0; break;
                case 1: delta = a.number.compare(b.number); break;
                case 2: delta = a.date.compare(b.date); break;
                case 3: delta = (a.counterparty_id < b.counterparty_id) ? -1 : (a.counterparty_id > b.counterparty_id) ? 1 : 0; break;
                default: break;
            }
            if (delta != 0) {
                return (column_spec->SortDirection == ImGuiSortDirection_Ascending) ? (delta < 0) : (delta > 0);
            }
        }
        return false;
    });
}

void ContractsView::Render() {
    if (!IsVisible) {
        return;
    }

    if (!ImGui::Begin("Справочник 'Договоры'", &IsVisible)) {
        ImGui::End();
        return;
    }

    if (dbManager && contracts.empty()) {
        RefreshData();
        RefreshDropdownData();
    }

    // Панель управления
    if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
        isAdding = true;
        selectedContractIndex = -1;
        selectedContract = Contract{-1, "", "", -1};
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
        if (!isAdding && selectedContractIndex != -1 && dbManager) {
            dbManager->deleteContract(contracts[selectedContractIndex].id);
            RefreshData();
            selectedContract = Contract{};
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Сохранить")) {
        if (dbManager) {
            if (isAdding) {
                dbManager->addContract(selectedContract);
                isAdding = false;
            } else if (selectedContract.id != -1) {
                dbManager->updateContract(selectedContract);
            }
            RefreshData();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
        RefreshData();
        RefreshDropdownData();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_PDF " Экспорт в PDF")) {
        if (pdfReporter && dbManager && dbManager->is_open()) {
            std::vector<std::string> columns = {"ID", "Номер", "Дата", "Контрагент ID"};
            std::vector<std::vector<std::string>> rows;
            for (const auto& entry : contracts) {
                rows.push_back({std::to_string(entry.id), entry.number, entry.date, std::to_string(entry.counterparty_id)});
            }
            pdfReporter->generatePdfFromTable("contracts_report.pdf", "Справочник 'Договоры'", columns, rows);
        } else {
            std::cerr << "Cannot export to PDF: No database open or PdfReporter not set." << std::endl;
        }
    }

    ImGui::Separator();

    ImGui::InputText("Фильтр по номеру", filterText, sizeof(filterText));

    const float editorHeight = ImGui::GetTextLineHeightWithSpacing() * 8;

    // Таблица со списком
    ImGui::BeginChild("ContractsList", ImVec2(0, -editorHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("contracts_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
        ImGui::TableSetupColumn("Номер", 0, 0.0f, 1);
        ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_DefaultSort, 0.0f, 2);
        ImGui::TableSetupColumn("Контрагент", 0, 0.0f, 3);
        ImGui::TableHeadersRow();

        // Сортировка
        if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsDirty) {
                SortContracts(contracts, sort_specs);
                sort_specs->SpecsDirty = false;
            }
        }

        auto getCounterpartyName = [&](int id) -> const char* {
            for (const auto& cp : counterpartiesForDropdown) {
                if (cp.id == id) {
                    return cp.name.c_str();
                }
            }
            return "N/A";
        };

        for (int i = 0; i < contracts.size(); ++i) {
             if (filterText[0] != '\0' && strcasestr(contracts[i].number.c_str(), filterText) == nullptr) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            bool is_selected = (selectedContractIndex == i);
            char label[256];
            sprintf(label, "%d##%d", contracts[i].id, i);
            if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedContractIndex = i;
                selectedContract = contracts[i];
                isAdding = false;
            }
            if (is_selected) {
                 ImGui::SetItemDefaultFocus();
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", contracts[i].number.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", contracts[i].date.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", getCounterpartyName(contracts[i].counterparty_id));
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::Separator();
    
    // Редактор
    ImGui::BeginChild("ContractEditor");
    if (selectedContractIndex != -1 || isAdding) {
        if (isAdding) {
            ImGui::Text("Добавление нового договора");
        } else {
            ImGui::Text("Редактирование договора ID: %d", selectedContract.id);
        }
        
        char numberBuf[256];
        char dateBuf[12];
        
        snprintf(numberBuf, sizeof(numberBuf), "%s", selectedContract.number.c_str());
        snprintf(dateBuf, sizeof(dateBuf), "%s", selectedContract.date.c_str());

        if (ImGui::InputText("Номер", numberBuf, sizeof(numberBuf))) {
            selectedContract.number = numberBuf;
        }
        if (ImGui::InputText("Дата", dateBuf, sizeof(dateBuf))) {
            selectedContract.date = dateBuf;
        }
        
        if (!counterpartiesForDropdown.empty()) {
            auto getCounterpartyName = [&](int id) -> const char* {
                for (const auto& cp : counterpartiesForDropdown) {
                    if (cp.id == id) {
                        return cp.name.c_str();
                    }
                }
                return "N/A";
            };
            const char* currentCounterpartyName = getCounterpartyName(selectedContract.counterparty_id);
            
            if (ImGui::BeginCombo("Контрагент", currentCounterpartyName)) {
                for (const auto& cp : counterpartiesForDropdown) {
                    bool isSelected = (cp.id == selectedContract.counterparty_id);
                    if (ImGui::Selectable(cp.name.c_str(), isSelected)) {
                        selectedContract.counterparty_id = cp.id;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }
    } else {
        ImGui::Text("Выберите договор для редактирования или добавьте новый.");
    }
    ImGui::EndChild();

    ImGui::End();
}

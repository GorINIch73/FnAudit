#include "ContractsView.h"
#include "../CustomWidgets.h"
#include <iostream>
#include <cstring>
#include "../IconsFontAwesome6.h"
#include <algorithm>

ContractsView::ContractsView()
    : selectedContractIndex(-1), showEditModal(false), isAdding(false) {
    memset(filterText, 0, sizeof(filterText));
    memset(counterpartyFilter, 0, sizeof(counterpartyFilter));
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

const char* ContractsView::GetTitle() {
    return "Справочник 'Договоры'";
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> ContractsView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Номер", "Дата", "Контрагент"};
    std::vector<std::vector<std::string>> rows;
    for (const auto& entry : contracts) {
        std::string counterpartyName = "N/A";
        for (const auto& cp : counterpartiesForDropdown) {
            if (cp.id == entry.counterparty_id) {
                counterpartyName = cp.name;
                break;
            }
        }
        rows.push_back({std::to_string(entry.id), entry.number, entry.date, counterpartyName});
    }
    return {headers, rows};
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

    if (!ImGui::Begin(GetTitle(), &IsVisible)) {
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

    ImGui::Separator();

    ImGui::InputText("Фильтр по номеру", filterText, sizeof(filterText));

    // Таблица со списком
    ImGui::BeginChild("ContractsList", ImVec2(0, list_view_height), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("contracts_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
        ImGui::TableSetupColumn("Номер", 0, 0.0f, 1);
        ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_DefaultSort, 0.0f, 2);
        ImGui::TableSetupColumn("Контрагент", 0, 0.0f, 3);
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsDirty) {
                SortContracts(contracts, sort_specs);
                sort_specs->SpecsDirty = false;
            }
        }

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
                if(dbManager) {
                    payment_info = dbManager->getPaymentInfoForContract(selectedContract.id);
                }
            }
            if (is_selected) {
                 ImGui::SetItemDefaultFocus();
            }
            
            ImGui::TableNextColumn();
            ImGui::Text("%s", contracts[i].number.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", contracts[i].date.c_str());
            ImGui::TableNextColumn();
            const char* counterpartyName = "N/A";
            for (const auto& cp : counterpartiesForDropdown) {
                if (cp.id == contracts[i].counterparty_id) {
                    counterpartyName = cp.name.c_str();
                    break;
                }
            }
            ImGui::Text("%s", counterpartyName);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::InvisibleButton("h_splitter", ImVec2(-1, 8.0f));
    if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS); }
    if (ImGui::IsItemActive()) {
        list_view_height += ImGui::GetIO().MouseDelta.y;
        if(list_view_height < 50.0f) list_view_height = 50.0f;
    }
    
    // Редактор
    if (selectedContractIndex != -1 || isAdding) {
        ImGui::BeginChild("ContractEditor", ImVec2(editor_width, 0), true);

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
            std::vector<CustomWidgets::ComboItem> counterpartyItems;
            for (const auto& cp : counterpartiesForDropdown) {
                counterpartyItems.push_back({cp.id, cp.name});
            }
            CustomWidgets::ComboWithFilter("Контрагент", selectedContract.counterparty_id, counterpartyItems, counterpartyFilter, sizeof(counterpartyFilter), 0);
        }
        ImGui::EndChild(); 
        ImGui::SameLine();

        ImGui::InvisibleButton("v_splitter", ImVec2(8.0f, -1));
        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW); }
        if (ImGui::IsItemActive()) {
            editor_width += ImGui::GetIO().MouseDelta.x;
            if(editor_width < 100.0f) editor_width = 100.0f;
        }
        ImGui::SameLine();

        ImGui::BeginChild("PaymentDetails", ImVec2(0, 0), true);
        ImGui::Text("Расшифровки платежей:");
        if (ImGui::BeginTable("payment_details_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Дата");
            ImGui::TableSetupColumn("Номер док.");
            ImGui::TableSetupColumn("Сумма");
            ImGui::TableSetupColumn("Назначение");
            ImGui::TableHeadersRow();

            for (const auto& info : payment_info) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", info.date.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", info.doc_number.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", info.amount);
                ImGui::TableNextColumn();
                ImGui::Text("%s", info.description.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::EndChild(); 
    } else {
        ImGui::BeginChild("BottomPane", ImVec2(0,0), true);
        ImGui::Text("Выберите договор для редактирования или добавьте новый.");
        ImGui::EndChild();
    }

    ImGui::End();
}

                

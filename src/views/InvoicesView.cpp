#include "InvoicesView.h"
#include "../CustomWidgets.h"
#include <iostream>
#include <cstring>
#include "../IconsFontAwesome6.h"
#include <algorithm>

InvoicesView::InvoicesView()
    : selectedInvoiceIndex(-1), showEditModal(false), isAdding(false) {
    memset(filterText, 0, sizeof(filterText));
    memset(contractFilter, 0, sizeof(contractFilter));
}

void InvoicesView::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
}

void InvoicesView::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
}

void InvoicesView::RefreshData() {
    if (dbManager) {
        invoices = dbManager->getInvoices();
        selectedInvoiceIndex = -1;
    }
}

void InvoicesView::RefreshDropdownData() {
    if (dbManager) {
        contractsForDropdown = dbManager->getContracts();
    }
}

const char* InvoicesView::GetTitle() {
    return "Справочник 'Накладные'";
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> InvoicesView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Номер", "Дата", "Контракт"};
    std::vector<std::vector<std::string>> rows;
    for (const auto& entry : invoices) {
        std::string contractNumber = "N/A";
        for (const auto& c : contractsForDropdown) {
            if (c.id == entry.contract_id) {
                contractNumber = c.number;
                break;
            }
        }
        rows.push_back({std::to_string(entry.id), entry.number, entry.date, contractNumber});
    }
    return {headers, rows};
}

// Вспомогательная функция для сортировки
static void SortInvoices(std::vector<Invoice>& invoices, const ImGuiTableSortSpecs* sort_specs) {
    std::sort(invoices.begin(), invoices.end(), [&](const Invoice& a, const Invoice& b) {
        for (int i = 0; i < sort_specs->SpecsCount; i++) {
            const ImGuiTableColumnSortSpecs* column_spec = &sort_specs->Specs[i];
            int delta = 0;
            switch (column_spec->ColumnIndex) {
                case 0: delta = (a.id < b.id) ? -1 : (a.id > b.id) ? 1 : 0; break;
                case 1: delta = a.number.compare(b.number); break;
                case 2: delta = a.date.compare(b.date); break;
                case 3: delta = (a.contract_id < b.contract_id) ? -1 : (a.contract_id > b.contract_id) ? 1 : 0; break;
                default: break;
            }
            if (delta != 0) {
                return (column_spec->SortDirection == ImGuiSortDirection_Ascending) ? (delta < 0) : (delta > 0);
            }
        }
        return false;
    });
}

void InvoicesView::Render() {
    if (!IsVisible) {
        return;
    }

    if (!ImGui::Begin(GetTitle(), &IsVisible)) {
        ImGui::End();
        return;
    }

    if (dbManager && invoices.empty()) {
        RefreshData();
        RefreshDropdownData();
    }

    // Панель управления
    if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
        isAdding = true;
        selectedInvoiceIndex = -1;
        selectedInvoice = Invoice{-1, "", "", -1};
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
        if (!isAdding && selectedInvoiceIndex != -1 && dbManager) {
            dbManager->deleteInvoice(invoices[selectedInvoiceIndex].id);
            RefreshData();
            selectedInvoice = Invoice{};
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Сохранить")) {
        if (dbManager) {
            if (isAdding) {
                dbManager->addInvoice(selectedInvoice);
                isAdding = false;
            } else if (selectedInvoice.id != -1) {
                dbManager->updateInvoice(selectedInvoice);
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
    ImGui::BeginChild("InvoicesList", ImVec2(0, list_view_height), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("invoices_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
        ImGui::TableSetupColumn("Номер", 0, 0.0f, 1);
        ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_DefaultSort, 0.0f, 2);
        ImGui::TableSetupColumn("Контракт", 0, 0.0f, 3);
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsDirty) {
                SortInvoices(invoices, sort_specs);
                sort_specs->SpecsDirty = false;
            }
        }

        for (int i = 0; i < invoices.size(); ++i) {
            if (filterText[0] != '\0' && strcasestr(invoices[i].number.c_str(), filterText) == nullptr) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            bool is_selected = (selectedInvoiceIndex == i);
            char label[256];
            sprintf(label, "%d##%d", invoices[i].id, i);
            if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedInvoiceIndex = i;
                selectedInvoice = invoices[i];
                isAdding = false;
                if(dbManager) {
                    payment_info = dbManager->getPaymentInfoForInvoice(selectedInvoice.id);
                }
            }
            if (is_selected) {
                 ImGui::SetItemDefaultFocus();
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", invoices[i].number.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", invoices[i].date.c_str());
            ImGui::TableNextColumn();
            const char* contractNumber = "N/A";
            for (const auto& c : contractsForDropdown) {
                if (c.id == invoices[i].contract_id) {
                    contractNumber = c.number.c_str();
                    break;
                }
            }
            ImGui::Text("%s", contractNumber);
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
    if (selectedInvoiceIndex != -1 || isAdding) {
        ImGui::BeginChild("InvoiceEditor", ImVec2(editor_width, 0), true);

        if (isAdding) {
            ImGui::Text("Добавление новой накладной");
        } else {
            ImGui::Text("Редактирование накладной ID: %d", selectedInvoice.id);
        }
        
        char numberBuf[256];
        char dateBuf[12];
        
        snprintf(numberBuf, sizeof(numberBuf), "%s", selectedInvoice.number.c_str());
        snprintf(dateBuf, sizeof(dateBuf), "%s", selectedInvoice.date.c_str());

        if (ImGui::InputText("Номер", numberBuf, sizeof(numberBuf))) {
            selectedInvoice.number = numberBuf;
        }
        if (ImGui::InputText("Дата", dateBuf, sizeof(dateBuf))) {
            selectedInvoice.date = dateBuf;
        }
        
        if (!contractsForDropdown.empty()) {
            std::vector<CustomWidgets::ComboItem> contractItems;
            for (const auto& c : contractsForDropdown) {
                contractItems.push_back({c.id, c.number});
            }
            CustomWidgets::ComboWithFilter("Контракт", selectedInvoice.contract_id, contractItems, contractFilter, sizeof(contractFilter), 0);
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
        ImGui::Text("Выберите накладную для редактирования или добавьте новую.");
        ImGui::EndChild();
    }

    ImGui::End();
}



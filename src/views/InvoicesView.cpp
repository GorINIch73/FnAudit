#include "InvoicesView.h"
#include <iostream>
#include <cstring>

InvoicesView::InvoicesView()
    : dbManager(nullptr), pdfReporter(nullptr), selectedInvoiceIndex(-1), showEditModal(false), isAdding(false) {
    memset(filterText, 0, sizeof(filterText));
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

void InvoicesView::Render() {
    if (!IsVisible) {
        return;
    }

    if (!ImGui::Begin("Справочник 'Накладные'", &IsVisible)) {
        ImGui::End();
        return;
    }

    if (dbManager && invoices.empty()) {
        RefreshData();
        RefreshDropdownData();
    }

    // Панель управления
    if (ImGui::Button("Добавить")) {
        isAdding = true;
        selectedInvoiceIndex = -1;
        selectedInvoice = Invoice{-1, "", "", -1};
    }
    ImGui::SameLine();
    if (ImGui::Button("Удалить")) {
        if (!isAdding && selectedInvoiceIndex != -1 && dbManager) {
            dbManager->deleteInvoice(invoices[selectedInvoiceIndex].id);
            RefreshData();
            selectedInvoice = Invoice{};
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Сохранить")) {
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
    if (ImGui::Button("Обновить")) {
        RefreshData();
        RefreshDropdownData();
    }
    ImGui::SameLine();
    if (ImGui::Button("Экспорт в PDF")) {
        if (pdfReporter && dbManager && dbManager->is_open()) {
            std::vector<std::string> columns = {"ID", "Номер", "Дата", "Контракт ID"};
            std::vector<std::vector<std::string>> rows;
            for (const auto& entry : invoices) {
                rows.push_back({std::to_string(entry.id), entry.number, entry.date, std::to_string(entry.contract_id)});
            }
            pdfReporter->generatePdfFromTable("invoices_report.pdf", "Справочник 'Накладные'", columns, rows);
        } else {
            std::cerr << "Cannot export to PDF: No database open or PdfReporter not set." << std::endl;
        }
    }

    ImGui::Separator();

    ImGui::InputText("Фильтр по номеру", filterText, sizeof(filterText));
    
    const float editorHeight = ImGui::GetTextLineHeightWithSpacing() * 8;

    // Таблица со списком
    ImGui::BeginChild("InvoicesList", ImVec2(0, -editorHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("invoices_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Номер");
        ImGui::TableSetupColumn("Дата");
        ImGui::TableSetupColumn("Контракт");
        ImGui::TableHeadersRow();

        auto getContractNumber = [&](int id) -> const char* {
            for (const auto& c : contractsForDropdown) {
                if (c.id == id) {
                    return c.number.c_str();
                }
            }
            return "N/A";
        };

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
            }
            if (is_selected) {
                 ImGui::SetItemDefaultFocus();
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", invoices[i].number.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", invoices[i].date.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", getContractNumber(invoices[i].contract_id));
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Редактор
    ImGui::BeginChild("InvoiceEditor");
    if (selectedInvoiceIndex != -1 || isAdding) {
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
            auto getContractNumber = [&](int id) -> const char* {
                for (const auto& c : contractsForDropdown) {
                    if (c.id == id) {
                        return c.number.c_str();
                    }
                }
                return "N/A";
            };
            const char* currentContractNumber = getContractNumber(selectedInvoice.contract_id);

            if (ImGui::BeginCombo("Контракт", currentContractNumber)) {
                for (const auto& c : contractsForDropdown) {
                    bool isSelected = (c.id == selectedInvoice.contract_id);
                    if (ImGui::Selectable(c.number.c_str(), isSelected)) {
                        selectedInvoice.contract_id = c.id;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }
    } else {
        ImGui::Text("Выберите накладную для редактирования или добавьте новую.");
    }
    ImGui::EndChild();

    ImGui::End();
}

#include "InvoicesView.h"
#include "../CustomWidgets.h"
#include "../IconsFontAwesome6.h"
#include <algorithm>
#include <cstring>
#include <iostream>

InvoicesView::InvoicesView()
    : selectedInvoiceIndex(-1),
      showEditModal(false),
      isAdding(false),
      isDirty(false) {
    Title = "Справочник 'Накладные'";
    memset(filterText, 0, sizeof(filterText));
    memset(contractFilter, 0, sizeof(contractFilter));
}

void InvoicesView::SetDatabaseManager(DatabaseManager *manager) {
    dbManager = manager;
}

void InvoicesView::SetPdfReporter(PdfReporter *reporter) {
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


std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
InvoicesView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Номер", "Дата", "Контракт", "Сумма"};
    std::vector<std::vector<std::string>> rows;
    for (const auto &entry : invoices) {
        std::string contractNumber = "N/A";
        for (const auto &c : contractsForDropdown) {
            if (c.id == entry.contract_id) {
                contractNumber = c.number;
                break;
            }
        }
        rows.push_back({std::to_string(entry.id), entry.number, entry.date,
                        contractNumber, std::to_string(entry.total_amount)});
    }
    return {headers, rows};
}

void InvoicesView::OnDeactivate() { SaveChanges(); }

void InvoicesView::SaveChanges() {
    if (!isDirty) {
        return;
    }

    if (dbManager) {
        if (isAdding) {
            dbManager->addInvoice(selectedInvoice);
            isAdding = false;
        } else if (selectedInvoice.id != -1) {
            dbManager->updateInvoice(selectedInvoice);
        }

        std::string current_number = selectedInvoice.number;
        std::string current_date = selectedInvoice.date;
        RefreshData();

        auto it = std::find_if(
            invoices.begin(), invoices.end(), [&](const Invoice &i) {
                return i.number == current_number && i.date == current_date;
            });

        if (it != invoices.end()) {
            selectedInvoiceIndex = std::distance(invoices.begin(), it);
            selectedInvoice = *it;
        } else {
            selectedInvoiceIndex = -1;
        }
    }

    isDirty = false;
}

// Вспомогательная функция для сортировки
static void SortInvoices(std::vector<Invoice> &invoices,
                         const ImGuiTableSortSpecs *sort_specs) {
    std::sort(invoices.begin(), invoices.end(),
              [&](const Invoice &a, const Invoice &b) {
                  for (int i = 0; i < sort_specs->SpecsCount; i++) {
                      const ImGuiTableColumnSortSpecs *column_spec =
                          &sort_specs->Specs[i];
                      int delta = 0;
                      switch (column_spec->ColumnIndex) {
                      case 0:
                          delta = (a.id < b.id) ? -1 : (a.id > b.id) ? 1 : 0;
                          break;
                      case 1:
                          delta = a.number.compare(b.number);
                          break;
                      case 2:
                          delta = a.date.compare(b.date);
                          break;
                      case 3:
                          delta = (a.contract_id < b.contract_id)   ? -1
                                  : (a.contract_id > b.contract_id) ? 1
                                                                    : 0;
                          break;
                      case 4:
                            delta = (a.total_amount < b.total_amount)   ? -1
                                    : (a.total_amount > b.total_amount) ? 1
                                                                        : 0;
                            break;
                      default:
                          break;
                      }
                      if (delta != 0) {
                          return (column_spec->SortDirection ==
                                  ImGuiSortDirection_Ascending)
                                     ? (delta < 0)
                                     : (delta > 0);
                      }
                  }
                  return false;
              });
}

void InvoicesView::Render() {
    if (!IsVisible) {
        if (isDirty) {
            SaveChanges();
        }
        return;
    }

    if (ImGui::Begin(GetTitle(), &IsVisible)) {

        if (dbManager && invoices.empty()) {
            RefreshData();
            RefreshDropdownData();
        }

        // Панель управления
        if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
            SaveChanges();
            isAdding = true;
            selectedInvoiceIndex = -1;
            selectedInvoice = Invoice{-1, "", "", -1};
            originalInvoice = selectedInvoice;
            isDirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
            SaveChanges();
            if (!isAdding && selectedInvoiceIndex != -1 && dbManager) {
                dbManager->deleteInvoice(invoices[selectedInvoiceIndex].id);
                RefreshData();
                selectedInvoice = Invoice{};
                originalInvoice = Invoice{};
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
            SaveChanges();
            RefreshData();
            RefreshDropdownData();
        }

        ImGui::Separator();

        ImGui::InputText("Фильтр по номеру", filterText, sizeof(filterText));
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK "##clear_filter_invoice")) {
            filterText[0] = '\0';
        }

        // Таблица со списком
        ImGui::BeginChild("InvoicesList", ImVec2(0, list_view_height), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("invoices_table", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
            ImGui::TableSetupColumn("Номер", 0, 0.0f, 1);
            ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_DefaultSort,
                                    0.0f, 2);
            ImGui::TableSetupColumn("Контракт", 0, 0.0f, 3);
            ImGui::TableSetupColumn("Сумма", 0, 0.0f, 4);
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty) {
                    SortInvoices(invoices, sort_specs);
                    sort_specs->SpecsDirty = false;
                }
            }

            for (int i = 0; i < invoices.size(); ++i) {
                if (filterText[0] != '\0' &&
                    strcasestr(invoices[i].number.c_str(), filterText) ==
                        nullptr) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                bool is_selected = (selectedInvoiceIndex == i);
                char label[256];
                sprintf(label, "%d##%d", invoices[i].id, i);
                if (ImGui::Selectable(label, is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    if (selectedInvoiceIndex != i) {
                        SaveChanges();
                        selectedInvoiceIndex = i;
                        selectedInvoice = invoices[i];
                        originalInvoice = invoices[i];
                        isAdding = false;
                        isDirty = false;
                        if (dbManager) {
                            payment_info = dbManager->getPaymentInfoForInvoice(
                                selectedInvoice.id);
                        }
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
                const char *contractNumber = "N/A";
                for (const auto &c : contractsForDropdown) {
                    if (c.id == invoices[i].contract_id) {
                        contractNumber = c.number.c_str();
                        break;
                    }
                }
                ImGui::Text("%s", contractNumber);
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", invoices[i].total_amount);
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        CustomWidgets::HorizontalSplitter("h_splitter", &list_view_height);

        // Редактор
        if (selectedInvoiceIndex != -1 || isAdding) {
            ImGui::BeginChild("InvoiceEditor", ImVec2(editor_width, 0), true);

            if (isAdding) {
                ImGui::Text("Добавление новой накладной");
            } else {
                ImGui::Text("Редактирование накладной ID: %d",
                            selectedInvoice.id);
            }

            char numberBuf[256];
            char dateBuf[12];

            snprintf(numberBuf, sizeof(numberBuf), "%s",
                     selectedInvoice.number.c_str());
            snprintf(dateBuf, sizeof(dateBuf), "%s",
                     selectedInvoice.date.c_str());

            if (ImGui::InputText("Номер", numberBuf, sizeof(numberBuf))) {
                selectedInvoice.number = numberBuf;
                isDirty = true;
            }
            if (ImGui::InputText("Дата", dateBuf, sizeof(dateBuf))) {
                selectedInvoice.date = dateBuf;
                isDirty = true;
            }

            if (!contractsForDropdown.empty()) {
                std::vector<CustomWidgets::ComboItem> contractItems;
                for (const auto &c : contractsForDropdown) {
                    contractItems.push_back({c.id, c.number});
                }
                if (CustomWidgets::ComboWithFilter(
                        "Контракт", selectedInvoice.contract_id, contractItems,
                        contractFilter, sizeof(contractFilter), 0)) {
                    isDirty = true;
                }
            }
            ImGui::EndChild();
            ImGui::SameLine();

            CustomWidgets::VerticalSplitter("v_splitter", &editor_width);

            ImGui::SameLine();

            ImGui::BeginChild("PaymentDetails", ImVec2(0, 0), true);
            ImGui::Text("Расшифровки платежей:");
            if (ImGui::BeginTable(
                    "payment_details_table", 4,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX |
                        ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Дата");
                ImGui::TableSetupColumn("Номер док.");
                ImGui::TableSetupColumn("Сумма");
                ImGui::TableSetupColumn("Назначение");
                ImGui::TableHeadersRow();

                for (const auto &info : payment_info) {
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
            ImGui::BeginChild("BottomPane", ImVec2(0, 0), true);
            ImGui::Text(
                "Выберите накладную для редактирования или добавьте новую.");
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

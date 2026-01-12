#include "PaymentsView.h"
#include "../Contract.h"
#include "../IconsFontAwesome6.h"
#include "../Invoice.h"
#include "CustomWidgets.h"
#include <algorithm> // для std::sort
#include <cstring>   // Для strcasestr и memset
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

PaymentsView::PaymentsView()
    : selectedPaymentIndex(-1),
      isAdding(false),
      isDirty(false),
      selectedDetailIndex(-1),
      isAddingDetail(false),
      isDetailDirty(false) {
    Title = "Справочник 'Банк' (Платежи)";
    memset(filterText, 0, sizeof(filterText)); // Инициализация filterText
    memset(counterpartyFilter, 0, sizeof(counterpartyFilter));
    memset(kosguFilter, 0, sizeof(kosguFilter));
    memset(contractFilter, 0, sizeof(contractFilter));
    memset(invoiceFilter, 0, sizeof(invoiceFilter));
}

void PaymentsView::SetDatabaseManager(DatabaseManager *manager) {
    dbManager = manager;
}

void PaymentsView::SetPdfReporter(PdfReporter *reporter) {
    pdfReporter = reporter;
}

void PaymentsView::RefreshData() {
    if (dbManager) {
        payments = dbManager->getPayments();
        selectedPaymentIndex = -1;
        paymentDetails.clear();
        selectedDetailIndex = -1;
    }
}

void PaymentsView::RefreshDropdownData() {
    if (dbManager) {
        counterpartiesForDropdown = dbManager->getCounterparties();
        kosguForDropdown = dbManager->getKosguEntries();
        contractsForDropdown = dbManager->getContracts();
        invoicesForDropdown = dbManager->getInvoices();
    }
}


std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
PaymentsView::GetDataAsStrings() {
    std::vector<std::string> headers = {"Дата",  "Номер",      "Тип",
                                        "Сумма", "Получатель", "Назначение"};
    std::vector<std::vector<std::string>> rows; // Declared here

    for (const auto &p : payments) {
        rows.push_back({p.date, p.doc_number, p.type, std::to_string(p.amount),
                        p.recipient, p.description});
    }
    return {headers, rows};
}

void PaymentsView::OnDeactivate() {
    SaveChanges();
    SaveDetailChanges();
}

void PaymentsView::SaveChanges() {
    if (!isDirty)
        return;

    if (dbManager) {
        selectedPayment.description = descriptionBuffer;
        if (isAdding) {
            dbManager->addPayment(selectedPayment);
        } else if (selectedPayment.id != -1) {
            dbManager->updatePayment(selectedPayment);
        }

        std::string current_doc_number = selectedPayment.doc_number;
        RefreshData();
        auto it = std::find_if(payments.begin(), payments.end(),
                               [&](const Payment &p) {
                                   return p.doc_number == current_doc_number;
                               });
        if (it != payments.end()) {
            selectedPaymentIndex = std::distance(payments.begin(), it);
            selectedPayment = *it;
            originalPayment = *it;
            descriptionBuffer = selectedPayment.description;
            if (dbManager) {
                paymentDetails =
                    dbManager->getPaymentDetails(selectedPayment.id);
            }
        }
    }
    isAdding = false;
    isDirty = false;
}

void PaymentsView::SaveDetailChanges() {
    if (!isDetailDirty)
        return;

    if (dbManager && selectedPayment.id != -1) {
        if (isAddingDetail) {
            dbManager->addPaymentDetail(selectedDetail);
        } else if (selectedDetail.id != -1) {
            dbManager->updatePaymentDetail(selectedDetail);
        }

        int old_detail_id = selectedDetail.id;
        paymentDetails = dbManager->getPaymentDetails(selectedPayment.id);

        auto it = std::find_if(
            paymentDetails.begin(), paymentDetails.end(),
            [&](const PaymentDetail &d) { return d.id == old_detail_id; });
        if (it != paymentDetails.end()) {
            selectedDetailIndex = std::distance(paymentDetails.begin(), it);
            selectedDetail = *it;
            originalDetail = *it;
        } else {
            selectedDetailIndex = -1;
        }
    }

    isAddingDetail = false;
    isDetailDirty = false;
}

// Вспомогательная функция для сортировки
static void SortPayments(std::vector<Payment> &payments,
                         const ImGuiTableSortSpecs *sort_specs) {
    std::sort(payments.begin(), payments.end(),
              [&](const Payment &a, const Payment &b) {
                  for (int i = 0; i < sort_specs->SpecsCount; i++) {
                      const ImGuiTableColumnSortSpecs *column_spec =
                          &sort_specs->Specs[i];
                      int delta = 0;
                      switch (column_spec->ColumnIndex) {
                      case 0:
                          delta = a.date.compare(b.date);
                          break;
                      case 1:
                          delta = a.doc_number.compare(b.doc_number);
                          break;
                      case 2:
                          delta = (a.amount < b.amount)   ? -1
                                  : (a.amount > b.amount) ? 1
                                                          : 0;
                          break;
                      case 3:
                          delta = a.description.compare(b.description);
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

void PaymentsView::Render() {
    if (!IsVisible) {
        if (isDirty) {
            SaveChanges();
            SaveDetailChanges();
        }
        return;
    }

    if (ImGui::Begin(GetTitle(), &IsVisible)) {

        if (dbManager && payments.empty()) {
            RefreshData();
            RefreshDropdownData();
        }

        // --- Панель управления ---
        if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
            SaveChanges();
            SaveDetailChanges();

            isAdding = true;
            selectedPaymentIndex = -1;
            selectedPayment = Payment{};
            selectedPayment.type = "expense";

            auto t = std::time(nullptr);
            auto tm = *std::localtime(&t);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%d");
            selectedPayment.date = oss.str();

            originalPayment = selectedPayment;
            descriptionBuffer.clear();
            paymentDetails.clear();
            isDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
            SaveChanges();
            SaveDetailChanges();
            if (!isAdding && selectedPaymentIndex != -1 && dbManager) {
                dbManager->deletePayment(payments[selectedPaymentIndex].id);
                RefreshData();
                selectedPayment = Payment{};
                originalPayment = Payment{};
                descriptionBuffer.clear();
                paymentDetails.clear();
                isDirty = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
            SaveChanges();
            SaveDetailChanges();
            RefreshData();
            RefreshDropdownData();
        }

        ImGui::Separator();

        ImGui::InputText("Общий фильтр", filterText, sizeof(filterText));

        // --- Список платежей ---
        ImGui::BeginChild("PaymentsList", ImVec2(0, list_view_height), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("payments_table", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable |
                                  ImGuiTableFlags_ScrollX)) {
            ImGui::TableSetupColumn(
                "Дата",
                ImGuiTableColumnFlags_DefaultSort |
                    ImGuiTableColumnFlags_PreferSortDescending,
                0.0f, 0);
            ImGui::TableSetupColumn("Номер", 0, 0.0f, 1);
            ImGui::TableSetupColumn("Сумма", 0, 0.0f, 2);
            ImGui::TableSetupColumn(
                "Назначение", ImGuiTableColumnFlags_WidthFixed, 600.0f, 3);
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty) {
                    SortPayments(payments, sort_specs);
                    sort_specs->SpecsDirty = false;
                }
            }

            std::vector<Payment> filtered_payments;
            if (filterText[0] != '\0') {
                for (const auto& p : payments) {
                    bool match_found = false;
                    if (strcasestr(p.date.c_str(), filterText) != nullptr) {
                        match_found = true;
                    }
                    if (!match_found && strcasestr(p.doc_number.c_str(), filterText) != nullptr) {
                        match_found = true;
                    }
                    if (!match_found && strcasestr(p.description.c_str(), filterText) != nullptr) {
                        match_found = true;
                    }
                    if (!match_found && strcasestr(p.recipient.c_str(), filterText) != nullptr) {
                        match_found = true;
                    }
                    if (!match_found) {
                        char amount_str[32];
                        snprintf(amount_str, sizeof(amount_str), "%.2f", p.amount);
                        if (strcasestr(amount_str, filterText) != nullptr) {
                            match_found = true;
                        }
                    }

                    if (match_found) {
                        filtered_payments.push_back(p);
                    }
                }
            } else {
                filtered_payments = payments;
            }

            ImGuiListClipper clipper;
            clipper.Begin(filtered_payments.size());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto& payment = filtered_payments[i];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    // Find the original index in the main 'payments' vector
                    int original_index = -1;
                    for(int j = 0; j < payments.size(); ++j) {
                        if(payments[j].id == payment.id) {
                            original_index = j;
                            break;
                        }
                    }

                    bool is_selected = (selectedPaymentIndex == original_index);
                    char label[128];
                    sprintf(label, "%s##%d", payment.date.c_str(), payment.id);
                    if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                        if (selectedPaymentIndex != original_index) {
                            SaveChanges();
                            SaveDetailChanges();

                            selectedPaymentIndex = original_index;
                            selectedPayment = payments[original_index];
                            originalPayment = payments[original_index];
                            descriptionBuffer = selectedPayment.description;
                            if (dbManager) {
                                paymentDetails = dbManager->getPaymentDetails(selectedPayment.id);
                            }
                            isAdding = false;
                            isDirty = false;
                            selectedDetailIndex = -1;
                            isDetailDirty = false;
                        }
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", payment.doc_number.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", payment.amount);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", payment.description.c_str());
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        CustomWidgets::HorizontalSplitter("h_splitter", &list_view_height);

        // --- Редактор платежей и расшифровок ---
        ImGui::BeginChild("Editors", ImVec2(0, 0), false);

        ImGui::BeginChild("PaymentEditor", ImVec2(editor_width, 0), true);
        if (selectedPaymentIndex != -1 || isAdding) {
            if (isAdding) {
                ImGui::Text("Добавление нового платежа");
            } else {
                ImGui::Text("Редактирование платежа ID: %d",
                            selectedPayment.id);
            }

            char dateBuf[12];
            snprintf(dateBuf, sizeof(dateBuf), "%s",
                     selectedPayment.date.c_str());
            if (ImGui::InputText("Дата", dateBuf, sizeof(dateBuf))) {
                selectedPayment.date = dateBuf;
                isDirty = true;
            }

            char docNumBuf[256];
            snprintf(docNumBuf, sizeof(docNumBuf), "%s",
                     selectedPayment.doc_number.c_str());
            if (ImGui::InputText("Номер док.", docNumBuf, sizeof(docNumBuf))) {
                selectedPayment.doc_number = docNumBuf;
                isDirty = true;
            }

            char typeBuf[32];
            snprintf(typeBuf, sizeof(typeBuf), "%s",
                     selectedPayment.type.c_str());

            const char *paymentTypes[] = {"income", "expense"};
            int currentTypeIndex = -1;
            for (int i = 0; i < IM_ARRAYSIZE(paymentTypes); i++) {
                if (strcmp(selectedPayment.type.c_str(), paymentTypes[i]) ==
                    0) {
                    currentTypeIndex = i;
                    break;
                }
            }

            if (ImGui::BeginCombo("Тип", currentTypeIndex >= 0
                                             ? paymentTypes[currentTypeIndex]
                                             : "")) {
                for (int i = 0; i < IM_ARRAYSIZE(paymentTypes); i++) {
                    const bool is_selected = (currentTypeIndex == i);
                    if (ImGui::Selectable(paymentTypes[i], is_selected)) {
                        selectedPayment.type = paymentTypes[i];
                        currentTypeIndex = i;
                        isDirty = true;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::InputDouble("Сумма", &selectedPayment.amount)) {
                isDirty = true;
            }

            char recipientBuf[256];
            snprintf(recipientBuf, sizeof(recipientBuf), "%s",
                     selectedPayment.recipient.c_str());
            if (ImGui::InputText("Получатель", recipientBuf,
                                 sizeof(recipientBuf))) {
                selectedPayment.recipient = recipientBuf;
                isDirty = true;
            }

            if (CustomWidgets::InputTextMultilineWithWrap(
                    "Назначение", &descriptionBuffer,
                    ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8))) {
                isDirty = true;
            }

            if (!counterpartiesForDropdown.empty()) {
                std::vector<CustomWidgets::ComboItem> counterpartyItems;
                for (const auto &cp : counterpartiesForDropdown) {
                    counterpartyItems.push_back({cp.id, cp.name});
                }
                if (CustomWidgets::ComboWithFilter(
                        "Контрагент", selectedPayment.counterparty_id,
                        counterpartyItems, counterpartyFilter,
                        sizeof(counterpartyFilter), 0)) {
                    isDirty = true;
                }
            }
        } else {
            ImGui::Text("Выберите платеж для редактирования.");
        }
        ImGui::EndChild();

        ImGui::SameLine();

        CustomWidgets::VerticalSplitter("v_splitter", &editor_width);
        
        ImGui::SameLine();

        // --- Расшифровка платежа ---
        ImGui::BeginChild("PaymentDetailsContainer", ImVec2(0, 0), true);
        ImGui::Text("Расшифровка платежа");

        if (selectedPaymentIndex != -1) {
            if (ImGui::Button(ICON_FA_PLUS " Добавить деталь")) {
                SaveDetailChanges();
                isAddingDetail = true;
                selectedDetailIndex = -1;
                selectedDetail =
                    PaymentDetail{-1, selectedPayment.id, -1, -1, -1, 0.0};
                originalDetail = selectedDetail;
                isDetailDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_TRASH " Удалить деталь") &&
                selectedDetailIndex != -1 && dbManager) {
                SaveDetailChanges();
                dbManager->deletePaymentDetail(
                    paymentDetails[selectedDetailIndex].id);
                paymentDetails = dbManager->getPaymentDetails(
                    selectedPayment.id); // Refresh details
                selectedDetailIndex = -1;
                isDetailDirty = false;
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить детали") &&
                dbManager) {
                SaveDetailChanges();
                paymentDetails = dbManager->getPaymentDetails(
                    selectedPayment.id); // Refresh details
                selectedDetailIndex = -1;
                isAddingDetail = false;
                isDetailDirty = false;
            }

            ImGui::BeginChild(
                "PaymentDetailsList",
                ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 8), true,
                ImGuiWindowFlags_HorizontalScrollbar);
            if (ImGui::BeginTable("payment_details_table", 4,
                                  ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Сумма");
                ImGui::TableSetupColumn("КОСГУ");
                ImGui::TableSetupColumn("Договор");
                ImGui::TableSetupColumn("Накладная");
                ImGui::TableHeadersRow();

                for (int i = 0; i < paymentDetails.size(); ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    bool is_detail_selected = (selectedDetailIndex == i);
                    char detail_label[128];
                    sprintf(detail_label, "%.2f##detail_%d",
                            paymentDetails[i].amount, paymentDetails[i].id);
                    if (ImGui::Selectable(
                            detail_label, is_detail_selected,
                            ImGuiSelectableFlags_SpanAllColumns)) {
                        if (selectedDetailIndex != i) {
                            SaveDetailChanges();
                            selectedDetailIndex = i;
                            selectedDetail = paymentDetails[i];
                            originalDetail = paymentDetails[i];
                            isAddingDetail = false;
                            isDetailDirty = false;
                        }
                    }
                    if (is_detail_selected) {
                        ImGui::SetItemDefaultFocus();
                    }

                    ImGui::TableNextColumn();
                    const char *kosguCode = "N/A";
                    for (const auto &k : kosguForDropdown) {
                        if (k.id == paymentDetails[i].kosgu_id) {
                            kosguCode = k.code.c_str();
                            break;
                        }
                    }
                    ImGui::Text("%s", kosguCode);

                    ImGui::TableNextColumn();
                    const char *contractNumber = "N/A";
                    for (const auto &c : contractsForDropdown) {
                        if (c.id == paymentDetails[i].contract_id) {
                            contractNumber = c.number.c_str();
                            break;
                        }
                    }
                    ImGui::Text("%s", contractNumber);

                    ImGui::TableNextColumn();
                    const char *invoiceNumber = "N/A";
                    for (const auto &inv : invoicesForDropdown) {
                        if (inv.id == paymentDetails[i].invoice_id) {
                            invoiceNumber = inv.number.c_str();
                            break;
                        }
                    }
                    ImGui::Text("%s", invoiceNumber);
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();

            if (isAddingDetail || selectedDetailIndex != -1) {
                ImGui::Separator();
                ImGui::Text(isAddingDetail ? "Добавить новую расшифровку"
                                           : "Редактировать расшифровку ID: %d",
                            selectedDetail.id);
                if (ImGui::InputDouble("Сумма##detail",
                                       &selectedDetail.amount)) {
                    isDetailDirty = true;
                }

                // Dropdown for KOSGU
                std::vector<CustomWidgets::ComboItem> kosguItems;
                for (const auto &k : kosguForDropdown) {
                    kosguItems.push_back({k.id, k.code});
                }
                if (CustomWidgets::ComboWithFilter(
                        "КОСГУ##detail", selectedDetail.kosgu_id, kosguItems,
                        kosguFilter, sizeof(kosguFilter), 0)) {
                    isDetailDirty = true;
                }

                // Dropdown for Contract
                std::vector<CustomWidgets::ComboItem> contractItems;
                for (const auto &c : contractsForDropdown) {
                    contractItems.push_back({c.id, c.number});
                }
                if (CustomWidgets::ComboWithFilter(
                        "Договор##detail", selectedDetail.contract_id,
                        contractItems, contractFilter, sizeof(contractFilter),
                        0)) {
                    isDetailDirty = true;
                }

                // Dropdown for Invoice
                std::vector<CustomWidgets::ComboItem> invoiceItems;
                for (const auto &i : invoicesForDropdown) {
                    invoiceItems.push_back({i.id, i.number});
                }
                if (CustomWidgets::ComboWithFilter("Накладная##detail",
                                                   selectedDetail.invoice_id,
                                                   invoiceItems, invoiceFilter,
                                                   sizeof(invoiceFilter), 0)) {
                    isDetailDirty = true;
                }
            }
        } else {
            ImGui::Text("Выберите платеж для просмотра расшифровок.");
        }
        ImGui::EndChild();
        ImGui::EndChild();
    }
    ImGui::End();
}

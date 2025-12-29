#include "PaymentsView.h"
#include <iostream>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstring> // Для strcasestr и memset
#include "../IconsFontAwesome6.h"
#include <algorithm> // для std::sort

PaymentsView::PaymentsView()
    : dbManager(nullptr), pdfReporter(nullptr), selectedPaymentIndex(-1), isAdding(false) {
    memset(filterText, 0, sizeof(filterText)); // Инициализация filterText
}

void PaymentsView::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
}

void PaymentsView::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
}

void PaymentsView::RefreshData() {
    if (dbManager) {
        payments = dbManager->getPayments();
        selectedPaymentIndex = -1;
    }
}

void PaymentsView::RefreshDropdownData() {
    if (dbManager) {
        counterpartiesForDropdown = dbManager->getCounterparties();
        kosguForDropdown = dbManager->getKosguEntries();
    }
}

const char* PaymentsView::GetTitle() {
    return "Справочник 'Банк' (Платежи)";
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> PaymentsView::GetDataAsStrings() {
    std::vector<std::string> headers = {"Дата", "Номер", "Тип", "Сумма", "Плательщик", "Получатель", "Назначение"};
    std::vector<std::vector<std::string>> rows;
    for (const auto& p : payments) {
        rows.push_back({
            p.date,
            p.doc_number,
            p.type,
            std::to_string(p.amount),
            p.payer,
            p.recipient,
            p.description
        });
    }
    return {headers, rows};
}

// Вспомогательная функция для сортировки
static void SortPayments(std::vector<Payment>& payments, const ImGuiTableSortSpecs* sort_specs) {
    std::sort(payments.begin(), payments.end(), [&](const Payment& a, const Payment& b) {
        for (int i = 0; i < sort_specs->SpecsCount; i++) {
            const ImGuiTableColumnSortSpecs* column_spec = &sort_specs->Specs[i];
            int delta = 0;
            switch (column_spec->ColumnIndex) {
                case 0: delta = a.date.compare(b.date); break;
                case 1: delta = a.doc_number.compare(b.doc_number); break;
                case 2: delta = (a.amount < b.amount) ? -1 : (a.amount > b.amount) ? 1 : 0; break;
                case 3: delta = a.description.compare(b.description); break;
                default: break;
            }
            if (delta != 0) {
                return (column_spec->SortDirection == ImGuiSortDirection_Ascending) ? (delta < 0) : (delta > 0);
            }
        }
        return false;
    });
}

void PaymentsView::Render() {
    if (!IsVisible) {
        return;
    }

    if (!ImGui::Begin(GetTitle(), &IsVisible)) {
        ImGui::End();
        return;
    }

    if (dbManager && payments.empty()) {
        RefreshData();
        RefreshDropdownData();
    }

    // --- Панель управления ---
    if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
        isAdding = true;
        selectedPaymentIndex = -1;
        selectedPayment = Payment{};
        selectedPayment.type = "expense"; 

        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d");
        selectedPayment.date = oss.str();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
        if (!isAdding && selectedPaymentIndex != -1 && dbManager) {
            dbManager->deletePayment(payments[selectedPaymentIndex].id);
            RefreshData();
            selectedPayment = Payment{};
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Сохранить")) {
        if (dbManager) {
            if (isAdding) {
                if (dbManager->addPayment(selectedPayment)) {
                    isAdding = false;
                    RefreshData();
                }
            } else if (selectedPaymentIndex != -1) {
                dbManager->updatePayment(selectedPayment);
                RefreshData();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
        RefreshData();
        RefreshDropdownData();
    }
    
    ImGui::Separator();

    ImGui::InputText("Фильтр по назначению", filterText, sizeof(filterText)); // Поле ввода фильтра
    
    const float editorHeight = ImGui::GetTextLineHeightWithSpacing() * 15; // Примерная высота редактора

    // --- Список платежей ---
    ImGui::BeginChild("PaymentsList", ImVec2(0, -editorHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("payments_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, 0);
        ImGui::TableSetupColumn("Номер", 0, 0.0f, 1);
        ImGui::TableSetupColumn("Сумма", 0, 0.0f, 2);
        ImGui::TableSetupColumn("Назначение", 0, 0.0f, 3);
        ImGui::TableHeadersRow();

        // Сортировка
        if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsDirty) {
                SortPayments(payments, sort_specs);
                sort_specs->SpecsDirty = false;
            }
        }

        for (int i = 0; i < payments.size(); ++i) {
            if (filterText[0] != '\0' && strcasestr(payments[i].description.c_str(), filterText) == nullptr) {
                continue; 
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            
            bool is_selected = (selectedPaymentIndex == i);
            char label[128];
            sprintf(label, "%s##%d", payments[i].date.c_str(), payments[i].id);
            if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedPaymentIndex = i;
                selectedPayment = payments[i];
                isAdding = false;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", payments[i].doc_number.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", payments[i].amount);
            ImGui::TableNextColumn();
            ImGui::Text("%s", payments[i].description.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::Separator();

    // --- Редактор платежей ---
    ImGui::BeginChild("PaymentEditor", ImVec2(0, 0), true);
    if (selectedPaymentIndex != -1 || isAdding) {
        if (isAdding) {
            ImGui::Text("Добавление нового платежа");
        } else {
            ImGui::Text("Редактирование платежа ID: %d", selectedPayment.id);
        }
        
        char dateBuf[12];
        snprintf(dateBuf, sizeof(dateBuf), "%s", selectedPayment.date.c_str());
        if (ImGui::InputText("Дата", dateBuf, sizeof(dateBuf))) {
            selectedPayment.date = dateBuf;
        }

        char docNumBuf[256];
        snprintf(docNumBuf, sizeof(docNumBuf), "%s", selectedPayment.doc_number.c_str());
        if (ImGui::InputText("Номер док.", docNumBuf, sizeof(docNumBuf))) {
            selectedPayment.doc_number = docNumBuf;
        }

        char typeBuf[32];
        snprintf(typeBuf, sizeof(typeBuf), "%s", selectedPayment.type.c_str());
        
        const char* paymentTypes[] = {"income", "expense"};
        int currentTypeIndex = -1;
        for (int i = 0; i < IM_ARRAYSIZE(paymentTypes); i++) {
            if (strcmp(selectedPayment.type.c_str(), paymentTypes[i]) == 0) {
                currentTypeIndex = i;
                break;
            }
        }

        if (ImGui::BeginCombo("Тип", currentTypeIndex >= 0 ? paymentTypes[currentTypeIndex] : "")) {
            for (int i = 0; i < IM_ARRAYSIZE(paymentTypes); i++) {
                const bool is_selected = (currentTypeIndex == i);
                if (ImGui::Selectable(paymentTypes[i], is_selected)) {
                    selectedPayment.type = paymentTypes[i];
                    currentTypeIndex = i;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::InputDouble("Сумма", &selectedPayment.amount)) {}

        char payerBuf[256];
        snprintf(payerBuf, sizeof(payerBuf), "%s", selectedPayment.payer.c_str());
        if (ImGui::InputText("Плательщик", payerBuf, sizeof(payerBuf))) {
            selectedPayment.payer = payerBuf;
        }

        char recipientBuf[256];
        snprintf(recipientBuf, sizeof(recipientBuf), "%s", selectedPayment.recipient.c_str());
        if (ImGui::InputText("Получатель", recipientBuf, sizeof(recipientBuf))) {
            selectedPayment.recipient = recipientBuf;
        }

        char descBuf[1024];
        snprintf(descBuf, sizeof(descBuf), "%s", selectedPayment.description.c_str());
        if (ImGui::InputTextMultiline("Назначение", descBuf, sizeof(descBuf), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4))) {
            selectedPayment.description = descBuf;
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
            const char* currentCounterpartyName = getCounterpartyName(selectedPayment.counterparty_id);
            if (ImGui::BeginCombo("Контрагент", currentCounterpartyName)) {
                for (const auto& cp : counterpartiesForDropdown) {
                    bool isSelected = (cp.id == selectedPayment.counterparty_id);
                    if (ImGui::Selectable(cp.name.c_str(), isSelected)) {
                        selectedPayment.counterparty_id = cp.id;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (!kosguForDropdown.empty()) {
            auto getKosguCode = [&](int id) -> const char* {
                for (const auto& k : kosguForDropdown) {
                    if (k.id == id) {
                        return k.code.c_str();
                    }
                }
                return "N/A";
            };
            const char* currentKosguCode = getKosguCode(selectedPayment.kosgu_id);
            if (ImGui::BeginCombo("КОСГУ", currentKosguCode)) {
                for (const auto& k : kosguForDropdown) {
                    bool isSelected = (k.id == selectedPayment.kosgu_id);
                    if (ImGui::Selectable(k.code.c_str(), isSelected)) {
                        selectedPayment.kosgu_id = k.id;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

    } else {
        ImGui::Text("Выберите платеж для редактирования или добавьте новый.");
    }
    ImGui::EndChild();
    
    ImGui::End();
}


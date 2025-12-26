#include "UIManager.h"
#include <iostream>
#include <string> // Added for std::string functions
#include "PdfReporter.h" // Add PdfReporter header
#include <ctime>
#include <iomanip>
#include <sstream>

UIManager::UIManager()
    : ShowKosguWindow(false), ShowSqlQueryWindow(false), ShowPaymentsWindow(false), ShowCounterpartiesWindow(false), ShowContractsWindow(false),
      dbManager(nullptr), selectedKosguIndex(-1), showEditKosguModal(false), isAddingKosgu(false),
      selectedPaymentIndex(-1), isAddingPayment(false),
      selectedCounterpartyIndex(-1), showEditCounterpartyModal(false), isAddingCounterparty(false),
      selectedContractIndex(-1), showEditContractModal(false), isAddingContract(false) {
    // Конструктор
    memset(queryInputBuffer, 0, sizeof(queryInputBuffer));
}

void UIManager::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
}

void UIManager::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
}

void UIManager::Render() {
    if (ShowKosguWindow) {
        // Если окно открывается впервые или мы попросили обновить данные
        if (dbManager && kosguEntries.empty()) {
            RefreshKosguData();
        }
        RenderKosguWindow();
    }
    if (ShowSqlQueryWindow) {
        RenderSqlQueryWindow();
    }
    if (ShowPaymentsWindow) {
        if (dbManager && payments.empty()) {
            RefreshPaymentsData();
        }
        RenderPaymentsWindow();
    }
    if (ShowCounterpartiesWindow) {
        if (dbManager && counterparties.empty()) {
            RefreshCounterpartiesData();
        }
        RenderCounterpartiesWindow();
    }
    if (ShowContractsWindow) { // Added this call
        if (dbManager && contracts.empty()) {
            RefreshContractsData();
        }
        RenderContractsWindow();
    }
}

// ... existing KOSGU methods ...

void UIManager::RefreshPaymentsData() {
    if (dbManager) {
        payments = dbManager->getPayments();
        selectedPaymentIndex = -1;
    }
}

void UIManager::RenderPaymentsWindow() {
    if (!ImGui::Begin("Справочник 'Банк' (Платежи)", &ShowPaymentsWindow)) {
        ImGui::End();
        return;
    }

    // --- Панель управления ---
    if (ImGui::Button("Добавить")) {
        isAddingPayment = true;
        selectedPaymentIndex = -1;
        selectedPayment = Payment{}; // New empty payment using default initializers
        selectedPayment.type = "expense"; // Default to 'expense' to satisfy CHECK constraint

        // Set current date as default
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d");
        selectedPayment.date = oss.str();
    }
    ImGui::SameLine();
    if (ImGui::Button("Удалить")) {
        if (!isAddingPayment && selectedPaymentIndex != -1 && dbManager) {
            dbManager->deletePayment(payments[selectedPaymentIndex].id);
            RefreshPaymentsData();
            selectedPayment = Payment{}; // Clear selection
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Сохранить")) {
        if (dbManager) {
            if (isAddingPayment) {
                std::cout << "Attempting to save new payment..." << std::endl;
                bool success = dbManager->addPayment(selectedPayment);
                std::cout << "Save success: " << (success ? "true" : "false") << std::endl;
                if (success) {
                    isAddingPayment = false;
                    RefreshPaymentsData();
                }
            } else if (selectedPaymentIndex != -1) {
                dbManager->updatePayment(selectedPayment);
                RefreshPaymentsData();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Обновить")) {
        RefreshPaymentsData();
    }
    ImGui::SameLine();
    if (ImGui::Button("Экспорт в PDF")) {
        if (pdfReporter && dbManager && dbManager->is_open()) {
            std::vector<std::string> columns = {"Дата", "Номер", "Тип", "Сумма", "Плательщик", "Получатель", "Назначение"};
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
            pdfReporter->generatePdfFromTable("payments_report.pdf", "Справочник 'Банк' (Платежи)", columns, rows);
        } else {
            std::cerr << "Cannot export to PDF: No database open or PdfReporter not set." << std::endl;
        }
    }
    
    ImGui::Separator();

    // --- Основной макет: список слева, редактор справа ---
    ImGui::Columns(2, "PaymentsLayout", true);
    
    // --- Левая панель: Список платежей ---
    ImGui::BeginChild("PaymentsList");
    if (ImGui::BeginTable("payments_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Дата");
        ImGui::TableSetupColumn("Номер");
        ImGui::TableSetupColumn("Сумма");
        ImGui::TableSetupColumn("Назначение");
        ImGui::TableHeadersRow();

        for (int i = 0; i < payments.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            
            bool is_selected = (selectedPaymentIndex == i);
            if (ImGui::Selectable(payments[i].date.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedPaymentIndex = i;
                selectedPayment = payments[i];
                isAddingPayment = false; // If user selects an item, no longer adding
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

    ImGui::NextColumn();

    // --- Правая панель: Редактирование платежа ---
    ImGui::BeginChild("PaymentEditor");
    if (selectedPaymentIndex != -1 || isAddingPayment) {
        if (isAddingPayment) {
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
        if (ImGui::InputText("Тип", typeBuf, sizeof(typeBuf))) {
            selectedPayment.type = typeBuf;
        }

        if (ImGui::InputDouble("Сумма", &selectedPayment.amount)) {
            // value updated
        }

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
        
        // TODO: Add fields for counterparty_id and kosgu_id (e.g., using a combo box)
    } else {
        ImGui::Text("Выберите платеж для редактирования или добавьте новый.");
    }
    ImGui::EndChild();

    ImGui::Columns(1);
    
    ImGui::End();
}

void UIManager::RefreshKosguData() {
    if (dbManager) {
        kosguEntries = dbManager->getKosguEntries();
        selectedKosguIndex = -1;
    }
}

void UIManager::RenderKosguWindow() {
    if (!ImGui::Begin("Справочник КОСГУ", &ShowKosguWindow)) {
        ImGui::End();
        return;
    }

    // Панель управления
    if (ImGui::Button("Добавить")) {
        isAddingKosgu = true;
        selectedKosgu = Kosgu{-1, "", ""}; // Очищаем для новой записи
        showEditKosguModal = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Изменить")) {
        if (selectedKosguIndex != -1) {
            isAddingKosgu = false;
            selectedKosgu = kosguEntries[selectedKosguIndex];
            showEditKosguModal = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Удалить")) {
        if (selectedKosguIndex != -1 && dbManager) {
            dbManager->deleteKosguEntry(kosguEntries[selectedKosguIndex].id);
            RefreshKosguData();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Обновить")) {
        RefreshKosguData();
    }
    ImGui::SameLine();
    if (ImGui::Button("Экспорт в PDF")) {
        if (pdfReporter && dbManager && dbManager->is_open()) {
            // Use current KOSGU entries for the report
            // For simplicity, we just pass the current view's data
            std::vector<std::string> columns = {"ID", "Код", "Наименование"};
            std::vector<std::vector<std::string>> rows;
            for (const auto& entry : kosguEntries) {
                rows.push_back({std::to_string(entry.id), entry.code, entry.name});
            }
            pdfReporter->generatePdfFromTable("kosgu_report.pdf", "Справочник КОСГУ", columns, rows);
        } else {
            std::cerr << "Cannot export to PDF: No database open or PdfReporter not set." << std::endl;
        }
    }

    // Таблица со списком
    if (ImGui::BeginTable("kosgu_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Код");
        ImGui::TableSetupColumn("Наименование");
        ImGui::TableHeadersRow();

        for (int i = 0; i < kosguEntries.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            bool is_selected = (selectedKosguIndex == i);
            if (ImGui::Selectable(std::to_string(kosguEntries[i].id).c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedKosguIndex = i;
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

    // Модальное окно для редактирования/добавления
    if (showEditKosguModal) {
        ImGui::OpenPopup("EditKosgu");
    }

    if (ImGui::BeginPopupModal("EditKosgu", &showEditKosguModal)) {
        char codeBuf[256];
        char nameBuf[256];
        if (isAddingKosgu) {
            snprintf(codeBuf, sizeof(codeBuf), "%s", "");
            snprintf(nameBuf, sizeof(nameBuf), "%s", "");
        } else {
            snprintf(codeBuf, sizeof(codeBuf), "%s", selectedKosgu.code.c_str());
            snprintf(nameBuf, sizeof(nameBuf), "%s", selectedKosgu.name.c_str());
        }


        ImGui::InputText("Код", codeBuf, sizeof(codeBuf));
        ImGui::InputText("Наименование", nameBuf, sizeof(nameBuf));

        if (ImGui::Button("Сохранить")) {
            if (dbManager) {
                selectedKosgu.code = codeBuf;
                selectedKosgu.name = nameBuf;
                if (isAddingKosgu) {
                    dbManager->addKosguEntry(selectedKosgu);
                } else {
                    dbManager->updateKosguEntry(selectedKosgu);
                }
                RefreshKosguData();
            }
            showEditKosguModal = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) {
            showEditKosguModal = false;
        }

        ImGui::EndPopup();
    }


    ImGui::End();
}

void UIManager::RenderSqlQueryWindow() {
    if (!ImGui::Begin("SQL Запрос", &ShowSqlQueryWindow)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Введите SQL запрос:");
    ImGui::InputTextMultiline("##SQLQueryInput", queryInputBuffer, sizeof(queryInputBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8));

    if (ImGui::Button("Выполнить")) {
        if (dbManager && dbManager->is_open()) {
            dbManager->executeSelect(queryInputBuffer, queryResult.columns, queryResult.rows);
        } else {
            // Show error message: No database open
            queryResult.columns.clear();
            queryResult.rows.clear();
            std::cerr << "No database open to execute SQL query." << std::endl;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Экспорт в PDF")) {
        if (pdfReporter && dbManager && dbManager->is_open() && !queryResult.columns.empty()) {
            pdfReporter->generatePdfFromTable("sql_query_report.pdf", "Результаты SQL Запроса", queryResult.columns, queryResult.rows);
        } else {
            std::cerr << "Cannot export to PDF: No database open, PdfReporter not set, or no query results." << std::endl;
        }
    }

    ImGui::Separator();
    ImGui::Text("Результат:");

    if (!queryResult.columns.empty()) {
        if (ImGui::BeginTable("sql_query_result", queryResult.columns.size(), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            for (const auto& col : queryResult.columns) {
                ImGui::TableSetupColumn(col.c_str());
            }
            ImGui::TableHeadersRow();

            for (const auto& row : queryResult.rows) {
                ImGui::TableNextRow();
                for (const auto& cell : row) {
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", cell.c_str());
                }
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::Text("Нет результатов или запрос не был выполнен.");
    }

    ImGui::End();
}

void UIManager::RefreshCounterpartiesData() {
    if (dbManager) {
        counterparties = dbManager->getCounterparties();
        selectedCounterpartyIndex = -1;
    }
}

void UIManager::RenderCounterpartiesWindow() {
    if (!ImGui::Begin("Справочник 'Контрагенты'", &ShowCounterpartiesWindow)) {
        ImGui::End();
        return;
    }

    // Панель управления
    if (ImGui::Button("Добавить")) {
        isAddingCounterparty = true;
        selectedCounterparty = Counterparty{-1, "", ""}; // Clear for new record
        showEditCounterpartyModal = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Изменить")) {
        if (selectedCounterpartyIndex != -1) {
            isAddingCounterparty = false;
            selectedCounterparty = counterparties[selectedCounterpartyIndex];
            showEditCounterpartyModal = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Удалить")) {
        if (selectedCounterpartyIndex != -1 && dbManager) {
            dbManager->deleteCounterparty(counterparties[selectedCounterpartyIndex].id);
            RefreshCounterpartiesData();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Обновить")) {
        RefreshCounterpartiesData();
    }

    // Таблица со списком
    if (ImGui::BeginTable("counterparties_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Наименование");
        ImGui::TableSetupColumn("ИНН");
        ImGui::TableHeadersRow();

        for (int i = 0; i < counterparties.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            bool is_selected = (selectedCounterpartyIndex == i);
            if (ImGui::Selectable(std::to_string(counterparties[i].id).c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedCounterpartyIndex = i;
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

    // Модальное окно для редактирования/добавления
    if (showEditCounterpartyModal) {
        ImGui::OpenPopup("EditCounterparty");
    }

    if (ImGui::BeginPopupModal("EditCounterparty", &showEditCounterpartyModal)) {
        char nameBuf[256];
        char innBuf[256];
        
        // Initialize buffers from selectedCounterparty
        snprintf(nameBuf, sizeof(nameBuf), "%s", selectedCounterparty.name.c_str());
        snprintf(innBuf, sizeof(innBuf), "%s", selectedCounterparty.inn.c_str());

        // ImGui::InputText will modify nameBuf/innBuf directly
        // If the value changes, update selectedCounterparty
        if (ImGui::InputText("Наименование", nameBuf, sizeof(nameBuf))) {
            selectedCounterparty.name = nameBuf;
        }
        if (ImGui::InputText("ИНН", innBuf, sizeof(innBuf))) {
            selectedCounterparty.inn = innBuf;
        }

        if (ImGui::Button("Сохранить")) {
            if (dbManager) {
                // No need to copy from nameBuf/innBuf here, selectedCounterparty is already updated
                if (isAddingCounterparty) {
                    dbManager->addCounterparty(selectedCounterparty);
                } else {
                    dbManager->updateCounterparty(selectedCounterparty);
                }
                RefreshCounterpartiesData();
            }
            showEditCounterpartyModal = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) {
            showEditCounterpartyModal = false;
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

void UIManager::RefreshContractsData() {
    if (dbManager) {
        contracts = dbManager->getContracts();
        selectedContractIndex = -1;
    }
}

void UIManager::RenderContractsWindow() {
    if (!ImGui::Begin("Справочник 'Договоры'", &ShowContractsWindow)) {
        ImGui::End();
        return;
    }
    // Панель управления
    if (ImGui::Button("Добавить")) {
        isAddingContract = true;
        selectedContract = Contract{-1, "", "", -1}; // Очищаем для новой записи
        showEditContractModal = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Изменить")) {
        if (selectedContractIndex != -1) {
            isAddingContract = false;
            selectedContract = contracts[selectedContractIndex];
            showEditContractModal = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Удалить")) {
        if (selectedContractIndex != -1 && dbManager) {
            dbManager->deleteContract(contracts[selectedContractIndex].id);
            RefreshContractsData();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Обновить")) {
        RefreshContractsData();
    }
    ImGui::SameLine();
    if (ImGui::Button("Экспорт в PDF")) {
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

    // Таблица со списком
    if (ImGui::BeginTable("contracts_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Номер");
        ImGui::TableSetupColumn("Дата");
        ImGui::TableSetupColumn("Контрагент ID"); // Placeholder, will show name later
        ImGui::TableHeadersRow();

        for (int i = 0; i < contracts.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            bool is_selected = (selectedContractIndex == i);
            if (ImGui::Selectable(std::to_string(contracts[i].id).c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedContractIndex = i;
            }
            if (is_selected) {
                 ImGui::SetItemDefaultFocus();
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", contracts[i].number.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", contracts[i].date.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", contracts[i].counterparty_id);
        }
        ImGui::EndTable();
    }

    // Модальное окно для редактирования/добавления
    if (showEditContractModal) {
        ImGui::OpenPopup("EditContract");
    }

    if (ImGui::BeginPopupModal("EditContract", &showEditContractModal)) {
        char numberBuf[256];
        char dateBuf[12];
        
        // Initialize buffers from selectedContract
        snprintf(numberBuf, sizeof(numberBuf), "%s", selectedContract.number.c_str());
        snprintf(dateBuf, sizeof(dateBuf), "%s", selectedContract.date.c_str());

        if (ImGui::InputText("Номер", numberBuf, sizeof(numberBuf))) {
            selectedContract.number = numberBuf;
        }
        if (ImGui::InputText("Дата", dateBuf, sizeof(dateBuf))) {
            selectedContract.date = dateBuf;
        }
        // TODO: Add dropdown for counterparty_id

        if (ImGui::Button("Сохранить")) {
            if (dbManager) {
                if (isAddingContract) {
                    dbManager->addContract(selectedContract);
                } else {
                    dbManager->updateContract(selectedContract);
                }
                RefreshContractsData();
            }
            showEditContractModal = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) {
            showEditContractModal = false;
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}
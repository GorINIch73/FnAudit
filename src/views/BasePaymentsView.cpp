#include "BasePaymentsView.h"
#include "../CustomWidgets.h"
#include "../IconsFontAwesome6.h"
#include "../UIManager.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

BasePaymentsView::BasePaymentsView()
    : selectedDocIndex(-1),
      showEditModal(false),
      isAdding(false),
      isDirty(false),
      cancel_group_operation(false) {
    Title = "Документы Основания";
    memset(filterText, 0, sizeof(filterText));
    memset(counterpartyFilter, 0, sizeof(counterpartyFilter));
    memset(doc_search_buffer, 0, sizeof(doc_search_buffer));
    memset(groupKosguFilter, 0, sizeof(groupKosguFilter));
}

void BasePaymentsView::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
}

void BasePaymentsView::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
}

void BasePaymentsView::SetUIManager(UIManager* manager) {
    uiManager = manager;
}

void BasePaymentsView::RefreshData() {
    if (dbManager) {
        documents = dbManager->getBasePaymentDocuments();
        UpdateFilteredDocuments();
    }
}

void BasePaymentsView::RefreshDropdownData() {
    if (dbManager) {
        counterpartiesForDropdown = dbManager->getCounterparties();
        contractsForDropdown = dbManager->getContracts();
        kosguForDropdown = dbManager->getKosguEntries();
    }
}

void BasePaymentsView::UpdateFilteredDocuments() {
    m_filtered_documents.clear();
    for (const auto& doc : documents) {
        bool matches_filter = true;

        // Текстовый фильтр
        if (filterText[0] != '\0') {
            std::string search = filterText;
            std::transform(search.begin(), search.end(), search.begin(), ::tolower);
            std::string doc_num = doc.number;
            std::transform(doc_num.begin(), doc_num.end(), doc_num.begin(), ::tolower);
            std::string doc_name = doc.document_name;
            std::transform(doc_name.begin(), doc_name.end(), doc_name.begin(), ::tolower);

            if (doc_num.find(search) == std::string::npos &&
                doc_name.find(search) == std::string::npos &&
                doc.note.find(search) == std::string::npos) {
                matches_filter = false;
            }
        }

        // Фильтр по типу
        if (matches_filter) {
            switch (doc_filter_index) {
                case 1: // Для сверки
                    if (!doc.is_for_checking) matches_filter = false;
                    break;
                case 2: // Сверенные
                    if (!doc.is_checked) matches_filter = false;
                    break;
                case 3: // Не сверенные
                    if (doc.is_checked) matches_filter = false;
                    break;
            }
        }

        if (matches_filter) {
            m_filtered_documents.push_back(doc);
        }
    }
}

void BasePaymentsView::SaveChanges() {
    if (isDirty && dbManager) {
        if (selectedDoc.id != -1) {
            dbManager->updateBasePaymentDocument(selectedDoc);
        }
        isDirty = false;
    }
    if (isDetailDirty && dbManager && selectedDetail.id != -1) {
        dbManager->updateBasePaymentDocumentDetail(selectedDetail);
        isDetailDirty = false;
    }
}

void BasePaymentsView::OnDeactivate() {
    SaveChanges();
    IsVisible = false;
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
BasePaymentsView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Дата", "Номер", "Наименование", "Контрагент", "Договор", "Сумма", "Для сверки", "Сверено"};
    std::vector<std::vector<std::string>> data;
    for (const auto& doc : documents) {
        std::string cp_name;
        for (const auto& cp : counterpartiesForDropdown) {
            if (cp.id == doc.counterparty_id) {
                cp_name = cp.name;
                break;
            }
        }
        std::string contract_num;
        for (const auto& c : contractsForDropdown) {
            if (c.id == doc.contract_id) {
                contract_num = c.number;
                break;
            }
        }
        data.push_back({
            std::to_string(doc.id),
            doc.date,
            doc.number,
            doc.document_name,
            cp_name,
            contract_num,
            std::to_string(doc.total_amount),
            doc.is_for_checking ? "Да" : "",
            doc.is_checked ? "Да" : ""
        });
    }
    return {headers, data};
}

void BasePaymentsView::Render() {
    if (!IsVisible) return;

    RefreshData();
    RefreshDropdownData();

    ImGui::Begin(Title.c_str(), &IsVisible);

    // --- Панель фильтров ---
    ImGui::Text("Фильтры:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##DocSearch", doc_search_buffer, sizeof(doc_search_buffer))) {
        strncpy(filterText, doc_search_buffer, sizeof(filterText) - 1);
        UpdateFilteredDocuments();
    }
    ImGui::SameLine();
    const char* filter_items[] = {"Все", "Для сверки", "Сверенные", "Не сверенные"};
    if (ImGui::Combo("##DocFilter", &doc_filter_index, filter_items, IM_ARRAYSIZE(filter_items))) {
        UpdateFilteredDocuments();
    }

    // --- Групповые операции ---
    if (ImGui::CollapsingHeader("Групповые операции")) {
        if (ImGui::Button("Добавить КОСГУ")) {
            if (!m_filtered_documents.empty() && current_operation == NONE) {
                groupKosguId = -1;
                memset(groupKosguFilter, 0, sizeof(groupKosguFilter));
                show_group_operation_confirmation_popup = true;
                on_group_operation_confirm = [&]() {
                    items_to_process = m_filtered_documents;
                    processed_items = 0;
                    current_operation = ADD_KOSGU;
                };
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Очистить 'Для сверки'")) {
            if (!m_filtered_documents.empty() && current_operation == NONE) {
                items_to_process = m_filtered_documents;
                processed_items = 0;
                current_operation = UNSET_FOR_CHECKING;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Отметить 'Для сверки'")) {
            if (!m_filtered_documents.empty() && current_operation == NONE) {
                items_to_process = m_filtered_documents;
                processed_items = 0;
                current_operation = SET_FOR_CHECKING;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Удалить расшифровки")) {
            if (!m_filtered_documents.empty() && current_operation == NONE) {
                items_to_process = m_filtered_documents;
                processed_items = 0;
                current_operation = DELETE_DETAILS;
            }
        }
    }

    // --- Таблица документов ---
    ImGui::Separator();
    ImGui::Text("Документы основания (%zu):", m_filtered_documents.size());

    if (ImGui::BeginTable("basePaymentsTable", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY |
                          ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Номер");
        ImGui::TableSetupColumn("Наименование");
        ImGui::TableSetupColumn("Контрагент");
        ImGui::TableSetupColumn("Сумма", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Для сверки", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_filtered_documents.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                const auto& doc = m_filtered_documents[row];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", doc.id);
                ImGui::TableNextColumn();
                ImGui::Text("%s", doc.date.c_str());
                ImGui::TableNextColumn();
                if (ImGui::Selectable(doc.number.c_str(), row == selectedDocIndex,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    SaveChanges();
                    selectedDocIndex = row;
                    selectedDoc = doc;
                    originalDoc = doc;
                    docDetails = dbManager ? dbManager->getBasePaymentDocumentDetails(doc.id) : std::vector<BasePaymentDocumentDetail>();
                }
                ImGui::TableNextColumn();
                ImGui::Text("%s", doc.document_name.c_str());
                ImGui::TableNextColumn();
                for (const auto& cp : counterpartiesForDropdown) {
                    if (cp.id == doc.counterparty_id) {
                        ImGui::Text("%s", cp.name.c_str());
                        break;
                    }
                }
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", doc.total_amount);
                ImGui::TableNextColumn();
                ImGui::Text(doc.is_for_checking ? ICON_FA_CHECK : "");
            }
        }
        ImGui::EndTable();
    }

    // --- Редактор и расшифровки ---
    if (selectedDocIndex >= 0 && selectedDocIndex < m_filtered_documents.size()) {
        ImGui::Separator();
        ImGui::BeginChild("EditorRegion", ImVec2(0, 0), true);

        // Левая часть - редактор
        ImGui::BeginChild("DocEditor", ImVec2(editor_width, 0), true);
        ImGui::Text("Редактор документа:");

        if (CustomWidgets::InputText("Дата", &selectedDoc.date)) isDirty = true;
        if (CustomWidgets::InputText("Номер", &selectedDoc.number)) isDirty = true;
        if (CustomWidgets::InputText("Наименование", &selectedDoc.document_name)) isDirty = true;

        // Контрагент
        {
            std::vector<CustomWidgets::ComboItem> items;
            items.push_back({-1, "Не выбрано"});
            for (const auto& cp : counterpartiesForDropdown) {
                items.push_back({cp.id, cp.name});
            }
            char cpFilter[128] = {0};
            if (CustomWidgets::ComboWithFilter("Контрагент", selectedDoc.counterparty_id, items,
                                               cpFilter, sizeof(cpFilter), 0)) {
                isDirty = true;
            }
        }

        // Договор
        {
            std::vector<CustomWidgets::ComboItem> items;
            items.push_back({-1, "Не выбрано"});
            for (const auto& c : contractsForDropdown) {
                items.push_back({c.id, c.number + " " + c.date});
            }
            char contractFilter[128] = {0};
            if (CustomWidgets::ComboWithFilter("Договор", selectedDoc.contract_id, items,
                                               contractFilter, sizeof(contractFilter), 0)) {
                isDirty = true;
            }
        }

        if (CustomWidgets::InputTextMultilineWithWrap("Примечание", &selectedDoc.note,
                                                      ImVec2(-1, ImGui::GetTextLineHeight() * 3))) {
            isDirty = true;
        }

        if (ImGui::Checkbox("Для сверки", &selectedDoc.is_for_checking)) isDirty = true;
        if (ImGui::Checkbox("Сверено", &selectedDoc.is_checked)) isDirty = true;

        ImGui::EndChild();

        CustomWidgets::VerticalSplitter("v_splitter_bpd", &editor_width);

        // Правая часть - расшифровки
        ImGui::SameLine();
        ImGui::BeginChild("DetailsView", ImVec2(0, 0), true);
        ImGui::Text("Расшифровки документа:");

        if (ImGui::Button(ICON_FA_PLUS " Добавить расшифровку")) {
            showDetailAddPopup = true;
        }

        if (ImGui::BeginTable("detailsTable", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Содержание");
            ImGui::TableSetupColumn("Дебет");
            ImGui::TableSetupColumn("Кредит");
            ImGui::TableSetupColumn("КОСГУ");
            ImGui::TableSetupColumn("Сумма", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < docDetails.size(); i++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", docDetails[i].operation_content.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", docDetails[i].debit_account.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", docDetails[i].credit_account.c_str());
                ImGui::TableNextColumn();
                for (const auto& k : kosguForDropdown) {
                    if (k.id == docDetails[i].kosgu_id) {
                        ImGui::Text("%s", k.code.c_str());
                        break;
                    }
                }
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", docDetails[i].amount);
            }
            ImGui::EndTable();
        }

        ImGui::EndChild();
        ImGui::EndChild();
    }

    ImGui::End();

    // Обработка групповых операций
    if (current_operation != NONE) {
        ProcessGroupOperation();
    }
}

void BasePaymentsView::ProcessGroupOperation() {
    if (!dbManager || current_operation == NONE || items_to_process.empty()) {
        current_operation = NONE;
        return;
    }

    const int items_per_frame = 20;
    int processed_in_frame = 0;

    while (processed_items < items_to_process.size() &&
           processed_in_frame < items_per_frame) {
        const auto& doc = items_to_process[processed_items];

        switch (current_operation) {
        case SET_FOR_CHECKING: {
            BasePaymentDocument updated = doc;
            updated.is_for_checking = true;
            dbManager->updateBasePaymentDocument(updated);
            break;
        }
        case UNSET_FOR_CHECKING: {
            BasePaymentDocument updated = doc;
            updated.is_for_checking = false;
            dbManager->updateBasePaymentDocument(updated);
            break;
        }
        case SET_CHECKED: {
            BasePaymentDocument updated = doc;
            updated.is_checked = true;
            dbManager->updateBasePaymentDocument(updated);
            break;
        }
        case UNSET_CHECKED: {
            BasePaymentDocument updated = doc;
            updated.is_checked = false;
            dbManager->updateBasePaymentDocument(updated);
            break;
        }
        case DELETE_DETAILS: {
            dbManager->deleteAllBasePaymentDocumentDetails(doc.id);
            break;
        }
        default:
            break;
        }

        processed_items++;
        processed_in_frame++;
    }

    if (processed_items >= items_to_process.size()) {
        current_operation = NONE;
        processed_items = 0;
        items_to_process.clear();
        RefreshData();
    }
}

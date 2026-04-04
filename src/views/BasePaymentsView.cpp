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

void BasePaymentsView::SetDatabaseManager(DatabaseManager *manager) {
    dbManager = manager;
}

void BasePaymentsView::SetPdfReporter(PdfReporter *reporter) {
    pdfReporter = reporter;
}

void BasePaymentsView::SetUIManager(UIManager *manager) { uiManager = manager; }

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
    for (const auto &doc : documents) {
        bool matches_filter = true;

        // Текстовый фильтр
        if (filterText[0] != '\0') {
            std::string search = filterText;
            std::transform(search.begin(), search.end(), search.begin(),
                           ::tolower);
            std::string doc_num = doc.number;
            std::transform(doc_num.begin(), doc_num.end(), doc_num.begin(),
                           ::tolower);
            std::string doc_name = doc.document_name;
            std::transform(doc_name.begin(), doc_name.end(), doc_name.begin(),
                           ::tolower);

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
                if (!doc.is_for_checking)
                    matches_filter = false;
                break;
            case 2: // Сверенные
                if (!doc.is_checked)
                    matches_filter = false;
                break;
            case 3: // Не сверенные
                if (doc.is_checked)
                    matches_filter = false;
                break;
            }
        }

        if (matches_filter) {
            m_filtered_documents.push_back(doc);
        }
    }
}

void BasePaymentsView::SortDocuments(const ImGuiTableSortSpecs* sort_specs) {
    if (!sort_specs || sort_specs->SpecsCount == 0) return;

    const ImGuiTableColumnSortSpecs* specs = &sort_specs->Specs[0];
    std::sort(m_filtered_documents.begin(), m_filtered_documents.end(),
        [specs](const BasePaymentDocument& a, const BasePaymentDocument& b) {
            bool result = false;
            switch (specs->ColumnUserID) {
                case 0: // ID
                    result = a.id < b.id; break;
                case 1: // Дата
                    result = a.date < b.date; break;
                case 2: // Номер
                    result = a.number < b.number; break;
                case 3: // Наименование
                    result = a.document_name < b.document_name; break;
                case 4: // Контрагент (по ID)
                    result = a.counterparty_id < b.counterparty_id; break;
                case 5: // Сумма
                    result = a.total_amount < b.total_amount; break;
                case 6: // Для сверки
                    result = a.is_for_checking < b.is_for_checking; break;
                default: break;
            }
            return specs->SortDirection == ImGuiSortDirection_Ascending ? result : !result;
        });
}

void BasePaymentsView::SaveChanges() {
    if (isDirty && dbManager) {
        if (selectedDoc.id != -1) {
            dbManager->updateBasePaymentDocument(selectedDoc);
            // Обновляем локальные массивы напрямую, чтобы не сбрасывать сортировку
            for (auto& doc : documents) {
                if (doc.id == selectedDoc.id) { doc = selectedDoc; break; }
            }
            for (auto& doc : m_filtered_documents) {
                if (doc.id == selectedDoc.id) { doc = selectedDoc; break; }
            }
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
void BasePaymentsView::ForceSave() {
    SaveChanges();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
BasePaymentsView::GetDataAsStrings() {
    std::vector<std::string> headers = {
        "ID",      "Дата",  "Номер",      "Наименование", "Контрагент",
        "Договор", "Сумма", "Для сверки", "Сверено"};
    std::vector<std::vector<std::string>> data;
    for (const auto &doc : documents) {
        std::string cp_name;
        for (const auto &cp : counterpartiesForDropdown) {
            if (cp.id == doc.counterparty_id) {
                cp_name = cp.name;
                break;
            }
        }
        std::string contract_num;
        for (const auto &c : contractsForDropdown) {
            if (c.id == doc.contract_id) {
                contract_num = c.number;
                break;
            }
        }
        data.push_back(
            {std::to_string(doc.id), doc.date, doc.number, doc.document_name,
             cp_name, contract_num, std::to_string(doc.total_amount),
             doc.is_for_checking ? "Да" : "", doc.is_checked ? "Да" : ""});
    }
    return {headers, data};
}

void BasePaymentsView::Render() {
    if (!IsVisible)
        return;

    // Обновляем данные только если они помечены как "грязные"
    if (m_dataDirty) {
        RefreshData();
        RefreshDropdownData();
        m_dataDirty = false;
    }

    ImGui::Begin(Title.c_str(), &IsVisible);

    // --- Панель фильтров ---
    ImGui::Text("Фильтры:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##DocSearch", doc_search_buffer,
                         sizeof(doc_search_buffer))) {
        strncpy(filterText, doc_search_buffer, sizeof(filterText) - 1);
        SaveChanges();
        UpdateFilteredDocuments();
    }
    ImGui::SameLine();
    const char *filter_items[] = {"Все", "Для сверки", "Сверенные",
                                  "Не сверенные"};
    if (ImGui::Combo("##DocFilter", &doc_filter_index, filter_items,
                     IM_ARRAYSIZE(filter_items))) {
        SaveChanges();
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

    // --- Таблица документов (верхняя часть) ---
    ImGui::Separator();
    ImGui::Text("Документы основания (%zu):", m_filtered_documents.size());

    ImGui::BeginChild("DocumentsListRegion", ImVec2(0, split_pos), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("basePaymentsTable", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 40.0f, 0);
        ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_WidthFixed, 90.0f, 1);
        ImGui::TableSetupColumn("Номер", 0, 0, 2);
        ImGui::TableSetupColumn("Наименование", 0, 0, 3);
        ImGui::TableSetupColumn("Контрагент", 0, 0, 4);
        ImGui::TableSetupColumn("Сумма", ImGuiTableColumnFlags_WidthFixed, 120.0f, 5);
        ImGui::TableSetupColumn("Для сверки", ImGuiTableColumnFlags_WidthFixed, 80.0f, 6);
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsDirty) {
                SortDocuments(sort_specs);
                sort_specs->SpecsDirty = false;
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_filtered_documents.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd;
                 row++) {
                const auto &doc = m_filtered_documents[row];
                ImGui::PushID(doc.id); // Уникальный ID для каждой строки
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", doc.id);
                ImGui::TableNextColumn();
                ImGui::Text("%s", doc.date.c_str());
                ImGui::TableNextColumn();
                if (ImGui::Selectable(doc.number.c_str(),
                                      row == selectedDocIndex,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    SaveChanges();
                    selectedDocIndex = row;
                    selectedDoc = doc;
                    originalDoc = doc;
                    docDetails =
                        dbManager
                            ? dbManager->getBasePaymentDocumentDetails(doc.id)
                            : std::vector<BasePaymentDocumentDetail>();
                }
                ImGui::TableNextColumn();
                ImGui::Text("%s", doc.document_name.c_str());
                ImGui::TableNextColumn();
                for (const auto &cp : counterpartiesForDropdown) {
                    if (cp.id == doc.counterparty_id) {
                        ImGui::Text("%s", cp.name.c_str());
                        break;
                    }
                }
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", doc.total_amount);
                ImGui::TableNextColumn();
                ImGui::Text(doc.is_for_checking ? ICON_FA_CHECK : "");
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild(); // DocumentsListRegion

    // Сплиттер между списком и редактором
    CustomWidgets::HorizontalSplitter("h_splitter_bpd", &split_pos, 100.0f);

    // --- Редактор и расшифровки (нижняя часть) ---
    if (selectedDocIndex >= 0 &&
        selectedDocIndex < m_filtered_documents.size()) {
        ImGui::BeginChild("EditorRegion", ImVec2(0, 0), true);

        // Левая часть - редактор
        ImGui::BeginChild("DocEditor", ImVec2(editor_width, 0), true);
        ImGui::Text("Редактор документа:");

        if (CustomWidgets::InputText("Дата", &selectedDoc.date))
            isDirty = true;
        if (CustomWidgets::InputText("Номер", &selectedDoc.number))
            isDirty = true;
        if (CustomWidgets::InputText("Наименование",
                                     &selectedDoc.document_name))
            isDirty = true;

        // Контрагент
        {
            std::vector<CustomWidgets::ComboItem> items;
            items.push_back({-1, "Не выбрано"});
            for (const auto &cp : counterpartiesForDropdown) {
                items.push_back({cp.id, cp.name});
            }
            char cpFilter[128] = {0};
            if (CustomWidgets::ComboWithFilter(
                    "Контрагент", selectedDoc.counterparty_id, items, cpFilter,
                    sizeof(cpFilter), 0)) {
                isDirty = true;
            }
        }

        // Договор
        {
            std::vector<CustomWidgets::ComboItem> items;
            items.push_back({-1, "Не выбрано"});
            for (const auto &c : contractsForDropdown) {
                items.push_back({c.id, c.number + " " + c.date});
            }
            char contractFilter[128] = {0};
            if (CustomWidgets::ComboWithFilter(
                    "Договор", selectedDoc.contract_id, items, contractFilter,
                    sizeof(contractFilter), 0)) {
                isDirty = true;
            }
        }

        if (CustomWidgets::InputTextMultilineWithWrap(
                "Примечание", &selectedDoc.note,
                ImVec2(-1, ImGui::GetTextLineHeight() * 3))) {
            isDirty = true;
        }

        if (ImGui::Checkbox("Для сверки", &selectedDoc.is_for_checking))
            isDirty = true;
        if (ImGui::Checkbox("Сверено", &selectedDoc.is_checked))
            isDirty = true;

        ImGui::EndChild();

        ImGui::SameLine();
        CustomWidgets::VerticalSplitter("v_splitter_bpd", &editor_width);
        ImGui::SameLine();

        // Правая часть - расшифровки
        ImGui::BeginChild("DetailsView", ImVec2(0, 0), true);
        ImGui::Text("Расшифровки документа (всего: %zu):", docDetails.size());

        if (ImGui::Button(ICON_FA_PLUS " Добавить расшифровку")) {
            showDetailAddPopup = true;
        }

        if (!docDetails.empty()) {
            if (ImGui::BeginTable("detailsTable", 5,
                                  ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_ScrollX |
                                      ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Содержание");
                ImGui::TableSetupColumn("Дебет");
                ImGui::TableSetupColumn("Кредит");
                ImGui::TableSetupColumn("КОСГУ");
                ImGui::TableSetupColumn(
                    "Сумма", ImGuiTableColumnFlags_WidthFixed, 100.0f);
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
                    for (const auto &k : kosguForDropdown) {
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
        } else {
            ImGui::Text("Нет расшифровок для этого документа.");
        }

        ImGui::EndChild(); // DetailsView
        ImGui::EndChild(); // EditorRegion
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
        const auto &doc = items_to_process[processed_items];

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
        m_dataDirty = true;
        RefreshData();
    }
}

#include "CounterpartiesView.h"
#include "../CustomWidgets.h"
#include "../IconsFontAwesome6.h"
#include "../UIManager.h"
#include <algorithm>
#include <cstring>
#include <iostream>

CounterpartiesView::CounterpartiesView()
    : showEditModal(false),
      isAdding(false),
      isDirty(false) {
    Title = "Справочник 'Контрагенты'";
    memset(filterText, 0, sizeof(filterText));
}

void CounterpartiesView::SetUIManager(UIManager *manager) {
    uiManager = manager;
}

void CounterpartiesView::SetDatabaseManager(DatabaseManager *manager) {
    dbManager = manager;
}

void CounterpartiesView::SetPdfReporter(PdfReporter *reporter) {
    pdfReporter = reporter;
}

void CounterpartiesView::RefreshData() {
    if (dbManager) {
        counterparties = dbManager->getCounterparties();

        auto all_payment_info = dbManager->getAllCounterpartyPaymentInfo();
        m_counterparty_details_map.clear();
        for (const auto &info : all_payment_info) {
            m_counterparty_details_map[info.counterparty_id].push_back(info);
        }

        UpdateFilteredCounterparties();
        // Сбрасываем выделение, если выбранный контрагент больше не существует
        if (selectedCounterparty.id != -1) {
            auto it = std::find_if(counterparties.begin(), counterparties.end(),
                                   [&](const Counterparty &c) {
                                       return c.id == selectedCounterparty.id;
                                   });
            if (it == counterparties.end()) {
                selectedCounterparty = Counterparty{};
            }
        }
    }
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
CounterpartiesView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Наименование", "ИНН",
                                        "Необязательный договор", "Сумма"};
    std::vector<std::vector<std::string>> rows;
    for (const auto &entry : counterparties) {
        rows.push_back({std::to_string(entry.id), entry.name, entry.inn,
                        (entry.is_contract_optional ? "Да" : "Нет"),
                        std::to_string(entry.total_amount)});
    }
    return {headers, rows};
}

void CounterpartiesView::OnDeactivate() { SaveChanges(); }

void CounterpartiesView::SaveChanges() {
    if (!isDirty) {
        return;
    }

    if (dbManager) {
        int saved_id = selectedCounterparty.id;
        
        if (isAdding) {
            dbManager->addCounterparty(selectedCounterparty);
            isAdding = false;
        } else if (selectedCounterparty.id != -1) {
            dbManager->updateCounterparty(selectedCounterparty);
        }

        // После сохранения обновляем данные и находим сохранённый элемент по ID
        RefreshData();

        // Находим контрагента по ID после обновления
        auto it = std::find_if(counterparties.begin(), counterparties.end(),
                               [&](const Counterparty &c) {
                                   return c.id == saved_id || 
                                          (saved_id == -1 && c.name == selectedCounterparty.name);
                               });

        if (it != counterparties.end()) {
            selectedCounterparty = *it;
        }
    }

    isDirty = false;
}

// Вспомогательная функция для сортировки
static void SortCounterparties(std::vector<Counterparty> &counterparties,
                               const ImGuiTableSortSpecs *sort_specs) {
    std::sort(
        counterparties.begin(), counterparties.end(),
        [&](const Counterparty &a, const Counterparty &b) {
            for (int i = 0; i < sort_specs->SpecsCount; i++) {
                const ImGuiTableColumnSortSpecs *column_spec =
                    &sort_specs->Specs[i];
                int delta = 0;
                switch (column_spec->ColumnIndex) {
                case 0:
                    delta = (a.id < b.id) ? -1 : (a.id > b.id) ? 1 : 0;
                    break;
                case 1:
                    delta = a.name.compare(b.name);
                    break;
                case 2:
                    delta = a.inn.compare(b.inn);
                    break;
                case 3: // is_contract_optional
                    delta =
                        (a.is_contract_optional < b.is_contract_optional)   ? -1
                        : (a.is_contract_optional > b.is_contract_optional) ? 1
                                                                            : 0;
                    break;
                case 4: // total_amount
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

void CounterpartiesView::UpdateFilteredCounterparties() {
    m_filtered_counterparties.clear();

    if (filterText[0] == '\0') {
        m_filtered_counterparties = counterparties;
        return;
    }

    for (const auto &counterparty : counterparties) {
        // Check counterparty name
        if (strcasestr(counterparty.name.c_str(), filterText) != nullptr) {
            m_filtered_counterparties.push_back(counterparty);
            continue; // Already added, no need to check payments
        }

        // Check payment descriptions and KOSGU codes
        auto it = m_counterparty_details_map.find(counterparty.id);
        if (it != m_counterparty_details_map.end()) {
            for (const auto &detail : it->second) {
                if (strcasestr(detail.description.c_str(), filterText) !=
                    nullptr ||
                    strcasestr(detail.kosgu_code.c_str(), filterText) != nullptr) {
                    m_filtered_counterparties.push_back(counterparty);
                    break; // Found a match, move to the next counterparty
                }
            }
        }
    }
}

void CounterpartiesView::Render() {
    if (!IsVisible) {
        if (isDirty) {
            SaveChanges();
        }
        return;
    }

    if (ImGui::Begin(GetTitle(), &IsVisible)) {
        if (dbManager && counterparties.empty()) {
            RefreshData();
        }

        // Панель управления
        if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
            SaveChanges();
            isAdding = true;
            selectedCounterparty = Counterparty{-1, "", ""};
            originalCounterparty = selectedCounterparty;
            isDirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
            if (!isAdding && selectedCounterparty.id != -1) {
                counterparty_id_to_delete = selectedCounterparty.id;
                show_delete_popup = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
            SaveChanges();
            RefreshData();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_LIST " Отчет по расшифровкам")) {
            if (!isAdding && selectedCounterparty.id != -1 && dbManager) {
                std::string query = "SELECT p.date AS 'Дата', p.doc_number AS 'Номер док.', "
                                    "pd.amount AS 'Сумма', k.code AS 'КОСГУ', p.description AS 'Назначение' "
                                    "FROM PaymentDetails pd "
                                    "JOIN Payments p ON pd.payment_id = p.id "
                                    "LEFT JOIN KOSGU k ON pd.kosgu_id = k.id "
                                    "WHERE p.counterparty_id = " + std::to_string(selectedCounterparty.id) +
                                    (filterText[0] != '\0' ? " AND (LOWER(p.date) LIKE LOWER('%" + std::string(filterText) + "%') "
                                    "OR LOWER(p.doc_number) LIKE LOWER('%" + std::string(filterText) + "%') "
                                    "OR LOWER(pd.amount) LIKE LOWER('%" + std::string(filterText) + "%') "
                                    "OR LOWER(k.code) LIKE LOWER('%" + std::string(filterText) + "%') "
                                    "OR LOWER(p.description) LIKE LOWER('%" + std::string(filterText) + "%'))" : "") +
                                    ";";
                if (uiManager) {
                    uiManager->CreateSpecialQueryView("Отчет по расшифровкам контрагента " + selectedCounterparty.name, query);
                }
            }
        }

        if (CustomWidgets::ConfirmationModal(
                "Подтверждение удаления контрагенты", "Подтверждение удаления",
                "Вы уверены, что хотите удалить этого контрагента?\nЭто "
                "действие нельзя отменить.",
                "Да", "Нет", show_delete_popup)) {
            std::cout << "удаление";
            if (dbManager && counterparty_id_to_delete != -1) {
                dbManager->deleteCounterparty(counterparty_id_to_delete);
                RefreshData();
                selectedCounterparty = Counterparty{};
                originalCounterparty = Counterparty{};
            }
            counterparty_id_to_delete = -1;
        }

        ImGui::Separator();

        bool filter_changed = false;
        if (ImGui::InputText("Фильтр по имени", filterText,
                             sizeof(filterText))) {
            filter_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK "##clear_filter_counterparty")) {
            if (filterText[0] != '\0') {
                filterText[0] = '\0';
                filter_changed = true;
            }
        }

        if (filter_changed) {
            UpdateFilteredCounterparties();
        }

        // Таблица со списком
        ImGui::BeginChild("CounterpartiesList", ImVec2(0, list_view_height),
                          true, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("counterparties_table", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable |
                                  ImGuiTableFlags_ScrollX)) {
            ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
            ImGui::TableSetupColumn("Наименование",
                                    ImGuiTableColumnFlags_DefaultSort, 0.0f, 1);
            ImGui::TableSetupColumn("ИНН", 0, 0.0f, 2);
            ImGui::TableSetupColumn("Необязательный договор", 0, 0.0f, 3);
            ImGui::TableSetupColumn("Сумма", 0, 0.0f, 4);
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty) {
                    SortCounterparties(m_filtered_counterparties, sort_specs);
                    sort_specs->SpecsDirty = false;
                }
            }

            // Используем ID для отслеживания выделения вместо индекса
            static int selectedCounterpartyId = -1;

            // Для таблиц с ImGuiListClipper нужно использовать другой подход
            const int items_count = (int)m_filtered_counterparties.size();
            const float items_height = ImGui::GetTextLineHeightWithSpacing();
            
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < items_count; i++) {
                const Counterparty &cp = m_filtered_counterparties[i];
                bool is_selected = (selectedCounterpartyId == cp.id);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                char label[256];
                sprintf(label, "%d##%d", cp.id, i);
                if (ImGui::Selectable(
                        label, is_selected,
                        ImGuiSelectableFlags_SpanAllColumns)) {
                    if (selectedCounterpartyId != cp.id) {
                        SaveChanges();
                        selectedCounterpartyId = cp.id;
                        selectedCounterparty = cp;
                        originalCounterparty = cp;
                        isAdding = false;
                        isDirty = false;
                        if (dbManager) {
                            payment_info =
                                dbManager->getPaymentInfoForCounterparty(
                                    selectedCounterparty.id);
                        }
                    }
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }

                ImGui::TableNextColumn();
                ImGui::Text("%s", cp.name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", cp.inn.c_str());
                ImGui::TableNextColumn();
                ImGui::Text(
                    "%s", cp.is_contract_optional
                              ? "Да"
                              : "Нет");
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", cp.total_amount);
            }
            ImGui::PopStyleVar();
            ImGui::EndTable();
        }
        ImGui::EndChild();

        CustomWidgets::HorizontalSplitter("h_splitter", &list_view_height);

        // Редактор
        if (selectedCounterparty.id != -1 || isAdding) {
            ImGui::BeginChild("CounterpartyEditor", ImVec2(editor_width, 0),
                              true);

            if (isAdding) {
                ImGui::Text("Добавление нового контрагента");
            } else {
                ImGui::Text("Редактирование контрагента ID: %d",
                            selectedCounterparty.id);
            }

            char innBuf[256];

            snprintf(innBuf, sizeof(innBuf), "%s",
                     selectedCounterparty.inn.c_str());

            if (CustomWidgets::InputTextMultilineWithWrap(
                    "Наименование", &selectedCounterparty.name,
                    ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4))) {
                isDirty = true;
            }
            if (ImGui::InputText("ИНН", innBuf, sizeof(innBuf))) {
                selectedCounterparty.inn = innBuf;
                isDirty = true;
            }
            bool is_contract_optional_checkbox =
                selectedCounterparty.is_contract_optional;
            if (ImGui::Checkbox("Необязательный договор",
                                &is_contract_optional_checkbox)) {
                selectedCounterparty.is_contract_optional =
                    is_contract_optional_checkbox;
                isDirty = true;
            }
            ImGui::EndChild();
            ImGui::SameLine();

            CustomWidgets::VerticalSplitter("v_splitter", &editor_width);

            ImGui::SameLine();

            ImGui::BeginChild("PaymentDetails", ImVec2(0, 0), true);
            ImGui::Text("Расшифровки платежей:");
            if (ImGui::BeginTable(
                    "payment_details_table", 5,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX |
                        ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Дата");
                ImGui::TableSetupColumn("Номер док.");
                ImGui::TableSetupColumn("Сумма");
                ImGui::TableSetupColumn("КОСГУ");
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
                    ImGui::Text("%s", info.kosgu_code.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", info.description.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();

        } else {
            ImGui::BeginChild("BottomPane", ImVec2(0, 0), true);
            ImGui::Text(
                "Выберите контрагента для редактирования или добавьте нового.");
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

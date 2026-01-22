#include "ContractsView.h"
#include "../CustomWidgets.h"
#include "../IconsFontAwesome6.h"
#include "../SuspiciousWord.h"
#include "../UIManager.h" // Added include for UIManager
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream> // For std::ostringstream

ContractsView::ContractsView()
    : selectedContractIndex(-1),
      showEditModal(false),
      isAdding(false),
      isDirty(false) {
    Title = "Справочник 'Договоры'";
    memset(filterText, 0, sizeof(filterText));
    memset(counterpartyFilter, 0, sizeof(counterpartyFilter));
}

void ContractsView::SetDatabaseManager(DatabaseManager *manager) {
    dbManager = manager;
}

void ContractsView::SetPdfReporter(PdfReporter *reporter) {
    pdfReporter = reporter;
}

void ContractsView::SetUIManager(UIManager *manager) { uiManager = manager; }

void ContractsView::RefreshData() {
    if (dbManager) {
        contracts = dbManager->getContracts();
        selectedContractIndex = -1;
        UpdateFilteredContracts();
    }
}

void ContractsView::RefreshDropdownData() {
    if (dbManager) {
        counterpartiesForDropdown = dbManager->getCounterparties();
        suspiciousWordsForFilter = dbManager->getSuspiciousWords();
    }
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
ContractsView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Номер", "Дата", "Контрагент",
                                        "Сумма"};
    std::vector<std::vector<std::string>> rows;
    for (const auto &entry : contracts) {
        std::string counterpartyName = "N/A";
        for (const auto &cp : counterpartiesForDropdown) {
            if (cp.id == entry.counterparty_id) {
                counterpartyName = cp.name;
                break;
            }
        }
        rows.push_back({std::to_string(entry.id), entry.number, entry.date,
                        counterpartyName, std::to_string(entry.total_amount)});
    }
    return {headers, rows};
}

void ContractsView::OnDeactivate() { SaveChanges(); }

void ContractsView::SaveChanges() {
    if (!isDirty) {
        return;
    }

    if (dbManager) {
        if (isAdding) {
            dbManager->addContract(selectedContract);
            isAdding = false;
        } else if (selectedContract.id != -1) {
            dbManager->updateContract(selectedContract);
        }

        std::string current_number = selectedContract.number;
        std::string current_date = selectedContract.date;
        RefreshData();

        auto it = std::find_if(
            contracts.begin(), contracts.end(), [&](const Contract &c) {
                return c.number == current_number && c.date == current_date;
            });

        if (it != contracts.end()) {
            selectedContractIndex = std::distance(contracts.begin(), it);
            selectedContract = *it;
        } else {
            selectedContractIndex = -1;
        }
    }

    isDirty = false;
}

// Вспомогательная функция для сортировки
static void SortContracts(std::vector<Contract> &contracts,
                          const ImGuiTableSortSpecs *sort_specs) {
    std::sort(contracts.begin(), contracts.end(),
              [&](const Contract &a, const Contract &b) {
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
                          delta = (a.counterparty_id < b.counterparty_id)   ? -1
                                  : (a.counterparty_id > b.counterparty_id) ? 1
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

void ContractsView::Render() {
    if (!IsVisible) {
        if (isDirty) {
            SaveChanges();
        }
        return;
    }

    if (ImGui::Begin(GetTitle(), &IsVisible)) {

        if (dbManager && contracts.empty()) {
            RefreshData();
            RefreshDropdownData();
        }

        // Панель управления
        if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
            SaveChanges();
            isAdding = true;
            selectedContractIndex = -1;
            selectedContract = Contract{-1, "", "", -1};
            originalContract = selectedContract;
            isDirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
            if (!isAdding && selectedContractIndex != -1) {
                contract_id_to_delete = contracts[selectedContractIndex].id;
                show_delete_popup = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
            SaveChanges();
            RefreshData();
            RefreshDropdownData();
        }
        ImGui::SameLine();
        if (ImGui::Button("Список по фильтру")) { // New button for report
            if (uiManager && !m_filtered_contracts.empty()) {
                std::ostringstream oss;
                oss << "SELECT c.id AS ID, c.number AS 'Номер договора'"
                       ", c.date AS 'Дата договора', cp.name AS 'Контрагент'"
                       ", COALESCE(ds.total_details_amount, 0.0) AS 'Всего "
                       "сумма"
                       "по расшифровкам'"
                       ", COALESCE(ds.details_count, 0) AS 'Количество "
                       "расшифровок'"
                       ", CASE WHEN c.is_for_checking THEN 'Да'"
                       " ELSE 'Нет'"
                       " END AS 'Для проверки'"
                       ", CASE WHEN c.is_for_special_control THEN 'Да'"
                       " ELSE 'Нет'"
                       " END AS 'Для контроля', c.note AS 'Примечание'"
                       " FROM Contracts c LEFT JOIN Counterparties cp ON "
                       "c.counterparty_id = cp.id LEFT JOIN (SELECT "
                       "contract_id, SUM(amount) AS total_details_amount, "
                       "COUNT(*) AS details_count FROM PaymentDetails "
                       "GROUP "
                       "BY contract_id) AS ds ON c.id = ds.contract_id "
                       "WHERE contract_id IN (";

                for (size_t i = 0; i < m_filtered_contracts.size(); ++i) {
                    oss << m_filtered_contracts[i].id;
                    if (i < m_filtered_contracts.size() - 1) {
                        oss << ", ";
                    }
                }
                oss << ") "
                       "ORDER "
                       "BY c.date DESC ";
                uiManager->CreateSpecialQueryView("Отчет по договорам",
                                                  oss.str());
            } else if (uiManager && m_filtered_contracts.empty()) {
                // If no contracts are filtered, show an empty report or a
                // message
                uiManager->CreateSpecialQueryView(
                    "Отчет по договорам",
                    "SELECT c.number AS 'Номер договора'"
                    ", c.date AS 'Дата договора', cp.name AS 'Контрагент'"
                    ", COALESCE(ds.total_details_amount, 0.0) AS 'Всего сумма"
                    "по расшифровкам'"
                    ", COALESCE(ds.details_count, 0) AS 'Количество "
                    "расшифровок'"
                    ", CASE WHEN c.is_for_checking THEN 'Да'"
                    " ELSE 'Нет'"
                    " END AS 'Для проверки'"
                    ", CASE WHEN c.is_for_special_control THEN 'Да'"
                    " ELSE 'Нет'"
                    " END AS 'Для контроля', c.note AS 'Примечание'"
                    " FROM Contracts c LEFT JOIN Counterparties cp ON "
                    "c.counterparty_id = cp.id LEFT JOIN (SELECT "
                    "contract_id, SUM(amount) AS total_details_amount, "
                    "COUNT(*) AS details_count FROM PaymentDetails GROUP "
                    "BY contract_id) AS ds ON c.id = ds.contract_id ORDER "
                    "BY c.date DESC;"); // Empty result
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Подробный список")) { // New button for report
            if (uiManager && !m_filtered_contracts.empty()) {
                std::ostringstream oss;
                // oss << "SELECT * FROM Contracts WHERE id IN (";
                oss << "SELECT c.id AS ID, c.number AS 'Номер договора'"
                       ", c.date AS 'Дата договора', cp.name AS 'Контрагент'"
                       ", COALESCE(ds.total_details_amount, 0.0) AS 'Всего "
                       "сумма"
                       "по расшифровкам'"
                       ", COALESCE(ds.details_count, 0) AS 'Количество "
                       "расшифровок'"
                       ", CASE WHEN c.is_for_checking THEN 'Да'"
                       " ELSE 'Нет'"
                       " END AS 'Для проверки'"
                       ", CASE WHEN c.is_for_special_control THEN 'Да'"
                       " ELSE 'Нет'"
                       " END AS 'Для контроля', c.note AS 'Примечание'"
                       " FROM Contracts c LEFT JOIN Counterparties cp ON "
                       "c.counterparty_id = cp.id LEFT JOIN (SELECT "
                       "contract_id, SUM(amount) AS total_details_amount, "
                       "COUNT(*) AS details_count FROM PaymentDetails "
                       "GROUP "
                       "BY contract_id) AS ds ON c.id = ds.contract_id "
                       "WHERE contract_id IN (";

                for (size_t i = 0; i < m_filtered_contracts.size(); ++i) {
                    oss << m_filtered_contracts[i].id;
                    if (i < m_filtered_contracts.size() - 1) {
                        oss << ", ";
                    }
                }
                oss << ") "
                       "ORDER "
                       "BY c.date DESC ";
                uiManager->CreateSpecialQueryView("Отчет по договорам",
                                                  oss.str());
            } else if (uiManager && m_filtered_contracts.empty()) {
                // If no contracts are filtered, show an empty report or a
                // message
                uiManager->CreateSpecialQueryView(
                    "Отчет по договорам",
                    "SELECT c.number AS 'Номер договора'"
                    ", c.date AS 'Дата договора', cp.name AS 'Контрагент'"
                    ", COALESCE(ds.total_details_amount, 0.0) AS 'Всего сумма"
                    "по расшифровкам'"
                    ", COALESCE(ds.details_count, 0) AS 'Количество "
                    "расшифровок'"
                    ", CASE WHEN c.is_for_checking THEN 'Да'"
                    " ELSE 'Нет'"
                    " END AS 'Для проверки'"
                    ", CASE WHEN c.is_for_special_control THEN 'Да'"
                    " ELSE 'Нет'"
                    " END AS 'Для контроля', c.note AS 'Примечание'"
                    " FROM Contracts c LEFT JOIN Counterparties cp ON "
                    "c.counterparty_id = cp.id LEFT JOIN (SELECT "
                    "contract_id, SUM(amount) AS total_details_amount, "
                    "COUNT(*) AS details_count FROM PaymentDetails GROUP "
                    "BY contract_id) AS ds ON c.id = ds.contract_id ORDER "
                    "BY c.date DESC;"); // Empty result
            }
        }
        if (show_delete_popup) {
            ImGui::OpenPopup("Подтверждение удаления##Contract");
        }
        if (ImGui::BeginPopupModal("Подтверждение удаления##Contract",
                                   &show_delete_popup,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Вы уверены, что хотите удалить этот договор?\nЭто "
                        "действие нельзя отменить.");
            ImGui::Separator();
            if (ImGui::Button("Да", ImVec2(120, 0))) {
                if (dbManager && contract_id_to_delete != -1) {
                    dbManager->deleteContract(contract_id_to_delete);
                    RefreshData();
                    selectedContract = Contract{};
                    originalContract = Contract{};
                }
                contract_id_to_delete = -1;
                show_delete_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Нет", ImVec2(120, 0))) {
                contract_id_to_delete = -1;
                show_delete_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();

        bool filter_changed = false;

        float combo_width = 220.0f;
        // Estimate the width of the clear button. A simple square button is
        // usually FrameHeight.
        float clear_button_width =
            ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float input_width = ImGui::GetContentRegionAvail().x - combo_width -
                            clear_button_width - spacing;

        ImGui::PushItemWidth(input_width);
        if (ImGui::InputText("Фильтр", filterText, sizeof(filterText))) {
            filter_changed = true;
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK "##clear_filter_contract")) {
            if (filterText[0] != '\0') {
                filterText[0] = '\0';
                filter_changed = true;
            }
        }

        ImGui::SameLine();
        ImGui::PushItemWidth(combo_width);
        const char *filter_items[] = {"Все",
                                      "Для проверки",
                                      "Усиленный контроль",
                                      "С примечанием",
                                      "Подозрительное в платежах",
                                      "Ненайденные"};
        if (ImGui::Combo("Фильтр по статусу", &contract_filter_index,
                         filter_items, IM_ARRAYSIZE(filter_items))) {
            filter_changed = true;
        }
        ImGui::PopItemWidth();

        if (filter_changed) {
            UpdateFilteredContracts();
        }

        // Таблица со списком
        ImGui::BeginChild("ContractsList", ImVec2(0, list_view_height), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("contracts_table", 12,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
            ImGui::TableSetupColumn("Номер", 0, 0.0f, 1);
            ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_DefaultSort,
                                    0.0f, 2);
            ImGui::TableSetupColumn("Контрагент", 0, 0.0f, 3);
            ImGui::TableSetupColumn("Сумма договора", 0, 0.0f, 4);
            ImGui::TableSetupColumn("Сумма платежей", 0, 0.0f, 5);
            ImGui::TableSetupColumn("Дата окончания", 0, 0.0f, 6);
            ImGui::TableSetupColumn("ИКЗ", 0, 0.0f, 7);
            ImGui::TableSetupColumn("Примечание", 0, 0.0f, 8);
            ImGui::TableSetupColumn("Проверка", 0, 0.0f, 9);
            ImGui::TableSetupColumn("Контроль", 0, 0.0f, 10);
            ImGui::TableSetupColumn("Найден", 0, 0.0f, 11);
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty) {
                    SortContracts(m_filtered_contracts, sort_specs);
                    sort_specs->SpecsDirty = false;
                }
            }

            ImGuiListClipper clipper;
            clipper.Begin(m_filtered_contracts.size());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd;
                     ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    auto original_it = std::find_if(
                        contracts.begin(), contracts.end(),
                        [&](const Contract &c) {
                            return c.id == m_filtered_contracts[i].id;
                        });
                    int original_index =
                        (original_it == contracts.end())
                            ? -1
                            : std::distance(contracts.begin(), original_it);

                    bool is_selected =
                        (selectedContractIndex == original_index);
                    char label[256];
                    sprintf(label, "%d##%d", m_filtered_contracts[i].id, i);
                    if (ImGui::Selectable(
                            label, is_selected,
                            ImGuiSelectableFlags_SpanAllColumns)) {
                        if (selectedContractIndex != original_index &&
                            original_index != -1) {
                            SaveChanges();
                            selectedContractIndex = original_index;
                            selectedContract = contracts[original_index];
                            originalContract = contracts[original_index];
                            isAdding = false;
                            isDirty = false;
                            if (dbManager) {
                                payment_info =
                                    dbManager->getPaymentInfoForContract(
                                        selectedContract.id);
                            }
                        }
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", m_filtered_contracts[i].number.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", m_filtered_contracts[i].date.c_str());
                    ImGui::TableNextColumn();
                    const char *counterpartyName = "N/A";
                    for (const auto &cp : counterpartiesForDropdown) {
                        if (cp.id == m_filtered_contracts[i].counterparty_id) {
                            counterpartyName = cp.name.c_str();
                            break;
                        }
                    }
                    ImGui::Text("%s", counterpartyName);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f",
                                m_filtered_contracts[i].contract_amount);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", m_filtered_contracts[i].total_amount);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", m_filtered_contracts[i].end_date.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        "%s", m_filtered_contracts[i].procurement_code.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", m_filtered_contracts[i].note.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        m_filtered_contracts[i].is_for_checking ? "Да" : "Нет");
                    ImGui::TableNextColumn();
                    ImGui::Text(m_filtered_contracts[i].is_for_special_control
                                    ? "Да"
                                    : "Нет");
                    ImGui::TableNextColumn();
                    ImGui::Text(m_filtered_contracts[i].is_found ? "Да"
                                                                 : "Нет");
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        CustomWidgets::HorizontalSplitter("h_splitter", &list_view_height);

        // Редактор
        if (selectedContractIndex != -1 || isAdding) {
            ImGui::BeginChild("ContractEditor", ImVec2(editor_width, 0), true);

            if (isAdding) {
                ImGui::Text("Добавление нового договора");
            } else {
                ImGui::Text("Редактирование договора ID: %d",
                            selectedContract.id);
            }

            char numberBuf[256];
            char dateBuf[12];

            snprintf(numberBuf, sizeof(numberBuf), "%s",
                     selectedContract.number.c_str());
            snprintf(dateBuf, sizeof(dateBuf), "%s",
                     selectedContract.date.c_str());

            if (ImGui::InputText("Номер", numberBuf, sizeof(numberBuf))) {
                selectedContract.number = numberBuf;
                isDirty = true;
            }
            if (ImGui::InputText("Дата", dateBuf, sizeof(dateBuf))) {
                selectedContract.date = dateBuf;
                isDirty = true;
            }

            if (!counterpartiesForDropdown.empty()) {
                std::vector<CustomWidgets::ComboItem> counterpartyItems;
                for (const auto &cp : counterpartiesForDropdown) {
                    counterpartyItems.push_back({cp.id, cp.name});
                }
                if (CustomWidgets::ComboWithFilter(
                        "Контрагент", selectedContract.counterparty_id,
                        counterpartyItems, counterpartyFilter,
                        sizeof(counterpartyFilter), 0)) {
                    isDirty = true;
                }
            }

            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::InputDouble("Сумма договора",
                                   &selectedContract.contract_amount)) {
                isDirty = true;
            }

            char endDateBuf[12];
            snprintf(endDateBuf, sizeof(endDateBuf), "%s",
                     selectedContract.end_date.c_str());
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::InputText("Дата окончания", endDateBuf,
                                 sizeof(endDateBuf))) {
                selectedContract.end_date = endDateBuf;
                isDirty = true;
            }

            char procurementCodeBuf[256];
            snprintf(procurementCodeBuf, sizeof(procurementCodeBuf), "%s",
                     selectedContract.procurement_code.c_str());
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::InputText("ИКЗ", procurementCodeBuf,
                                 sizeof(procurementCodeBuf))) {
                selectedContract.procurement_code = procurementCodeBuf;
                isDirty = true;
            }

            char noteBuf[512];
            snprintf(noteBuf, sizeof(noteBuf), "%s",
                     selectedContract.note.c_str());
            if (ImGui::InputTextMultiline("Примечание", noteBuf,
                                          sizeof(noteBuf))) {
                selectedContract.note = noteBuf;
                isDirty = true;
            }

            if (ImGui::Checkbox("Для проверки",
                                &selectedContract.is_for_checking)) {
                isDirty = true;
            }
            if (ImGui::Checkbox("Усиленный контроль",
                                &selectedContract.is_for_special_control)) {
                isDirty = true;
            }
            if (ImGui::Checkbox("Найден", &selectedContract.is_found)) {
                isDirty = true;
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
                "Выберите договор для редактирования или добавьте новый.");
            ImGui::EndChild();
        }
    }

    ImGui::End();
}

void ContractsView::UpdateFilteredContracts() {
    if (!dbManager)
        return;

    // 1. Fetch all payment info once
    auto all_payment_info = dbManager->getAllContractPaymentInfo();
    m_contract_details_map.clear();
    for (const auto &info : all_payment_info) {
        m_contract_details_map[info.contract_id].push_back(info);
    }

    // 2. Text filter pass
    std::vector<Contract> text_filtered_contracts;
    if (filterText[0] != '\0') {
        for (const auto &entry : contracts) {
            bool contract_match = false;
            if (strcasestr(entry.number.c_str(), filterText) != nullptr ||
                strcasestr(entry.date.c_str(), filterText) != nullptr ||
                strcasestr(entry.end_date.c_str(), filterText) != nullptr ||
                strcasestr(entry.procurement_code.c_str(), filterText) !=
                    nullptr ||
                strcasestr(entry.note.c_str(), filterText) != nullptr) {
                contract_match = true;
            }

            char amount_str[32];
            snprintf(amount_str, sizeof(amount_str), "%.2f",
                     entry.contract_amount);
            if (strcasestr(amount_str, filterText) != nullptr) {
                contract_match = true;
            }

            // Check counterparty name
            for (const auto &cp : counterpartiesForDropdown) {
                if (cp.id == entry.counterparty_id) {
                    if (strcasestr(cp.name.c_str(), filterText) != nullptr) {
                        contract_match = true;
                        break;
                    }
                }
            }

            bool payment_detail_match = false;
            auto it = m_contract_details_map.find(entry.id);
            if (it != m_contract_details_map.end()) {
                for (const auto &detail : it->second) {
                    if (strcasestr(detail.date.c_str(), filterText) !=
                            nullptr ||
                        strcasestr(detail.doc_number.c_str(), filterText) !=
                            nullptr ||
                        strcasestr(detail.description.c_str(), filterText) !=
                            nullptr) {
                        payment_detail_match = true;
                        break;
                    }
                    char detail_amount_str[32];
                    snprintf(detail_amount_str, sizeof(detail_amount_str),
                             "%.2f", detail.amount);
                    if (strcasestr(detail_amount_str, filterText) != nullptr) {
                        payment_detail_match = true;
                        break;
                    }
                }
            }

            if (contract_match || payment_detail_match) {
                text_filtered_contracts.push_back(entry);
            }
        }
    } else {
        text_filtered_contracts = contracts;
    }

    // 3. Category filter pass
    m_filtered_contracts.clear();
    switch (contract_filter_index) {
    case 0: // Все
        m_filtered_contracts = text_filtered_contracts;
        break;
    case 1: // Для проверки
        for (const auto &contract : text_filtered_contracts) {
            if (contract.is_for_checking) {
                m_filtered_contracts.push_back(contract);
            }
        }
        break;
    case 2: // Усиленный контроль
        for (const auto &contract : text_filtered_contracts) {
            if (contract.is_for_special_control) {
                m_filtered_contracts.push_back(contract);
            }
        }
        break;
    case 3: // С примечанием
        for (const auto &contract : text_filtered_contracts) {
            if (!contract.note.empty()) {
                m_filtered_contracts.push_back(contract);
            }
        }
        break;
    case 4: // Подозрительное в платежах
        for (const auto &contract : text_filtered_contracts) {
            bool payment_has_suspicious = false;
            auto it = m_contract_details_map.find(contract.id);
            if (it != m_contract_details_map.end()) {
                for (const auto &detail : it->second) {
                    for (const auto &sw : suspiciousWordsForFilter) {
                        if (strcasestr(detail.description.c_str(),
                                       sw.word.c_str()) != nullptr) {
                            payment_has_suspicious = true;
                            break;
                        }
                    }
                    if (payment_has_suspicious)
                        break;
                }
            }
            if (payment_has_suspicious) {
                m_filtered_contracts.push_back(contract);
            }
        }
        break;
    case 5: // Ненайденные
        for (const auto &contract : text_filtered_contracts) {
            if (contract.is_for_checking && !contract.is_found) {
                m_filtered_contracts.push_back(contract);
            }
        }
        break;
    }
}

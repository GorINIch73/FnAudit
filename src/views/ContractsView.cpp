#include "ContractsView.h"
#include "../CustomWidgets.h"
#include "../IconsFontAwesome6.h"
#include <algorithm>
#include <cstring>
#include <iostream>

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

void ContractsView::RefreshData() {
    if (dbManager) {
        contracts = dbManager->getContracts();
        selectedContractIndex = -1;
    }
}

void ContractsView::RefreshDropdownData() {
    if (dbManager) {
        counterpartiesForDropdown = dbManager->getCounterparties();
    }
}


std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
ContractsView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Номер", "Дата", "Контрагент"};
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
                        counterpartyName});
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
            SaveChanges();
            if (!isAdding && selectedContractIndex != -1 && dbManager) {
                dbManager->deleteContract(contracts[selectedContractIndex].id);
                RefreshData();
                selectedContract = Contract{};
                originalContract = Contract{};
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

        // Таблица со списком
        ImGui::BeginChild("ContractsList", ImVec2(0, list_view_height), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("contracts_table", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
            ImGui::TableSetupColumn("Номер", 0, 0.0f, 1);
            ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_DefaultSort,
                                    0.0f, 2);
            ImGui::TableSetupColumn("Контрагент", 0, 0.0f, 3);
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty) {
                    SortContracts(contracts, sort_specs);
                    sort_specs->SpecsDirty = false;
                }
            }

            for (int i = 0; i < contracts.size(); ++i) {
                if (filterText[0] != '\0' &&
                    strcasestr(contracts[i].number.c_str(), filterText) ==
                        nullptr) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                bool is_selected = (selectedContractIndex == i);
                char label[256];
                sprintf(label, "%d##%d", contracts[i].id, i);
                if (ImGui::Selectable(label, is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    if (selectedContractIndex != i) {
                        SaveChanges();
                        selectedContractIndex = i;
                        selectedContract = contracts[i];
                        originalContract = contracts[i];
                        isAdding = false;
                        isDirty = false;
                        if (dbManager) {
                            payment_info = dbManager->getPaymentInfoForContract(
                                selectedContract.id);
                        }
                    }
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }

                ImGui::TableNextColumn();
                ImGui::Text("%s", contracts[i].number.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", contracts[i].date.c_str());
                ImGui::TableNextColumn();
                const char *counterpartyName = "N/A";
                for (const auto &cp : counterpartiesForDropdown) {
                    if (cp.id == contracts[i].counterparty_id) {
                        counterpartyName = cp.name.c_str();
                        break;
                    }
                }
                ImGui::Text("%s", counterpartyName);
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

#include "CounterpartiesView.h"
#include "../CustomWidgets.h"
#include "../IconsFontAwesome6.h"
#include <algorithm>
#include <cstring>
#include <iostream>

CounterpartiesView::CounterpartiesView()
    : selectedCounterpartyIndex(-1),
      showEditModal(false),
      isAdding(false),
      isDirty(false) {
    Title = "Справочник 'Контрагенты'";
    memset(filterText, 0, sizeof(filterText));
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
        selectedCounterpartyIndex = -1;
    }
}


std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
CounterpartiesView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Наименование", "ИНН", "Сумма"};
    std::vector<std::vector<std::string>> rows;
    for (const auto &entry : counterparties) {
        rows.push_back({std::to_string(entry.id), entry.name, entry.inn, std::to_string(entry.total_amount)});
    }
    return {headers, rows};
}

void CounterpartiesView::OnDeactivate() { SaveChanges(); }

void CounterpartiesView::SaveChanges() {
    if (!isDirty) {
        return;
    }

    if (dbManager) {
        if (isAdding) {
            dbManager->addCounterparty(selectedCounterparty);
            isAdding = false;
        } else if (selectedCounterparty.id != -1) {
            dbManager->updateCounterparty(selectedCounterparty);
        }

        // Find the name and inn of the currently selected counterparty before
        // refreshing
        std::string current_name = selectedCounterparty.name;
        std::string current_inn = selectedCounterparty.inn;
        RefreshData();

        // Find the index of the counterparty that was just saved
        auto it = std::find_if(counterparties.begin(), counterparties.end(),
                               [&](const Counterparty &c) {
                                   return c.name == current_name &&
                                          c.inn == current_inn;
                               });

        if (it != counterparties.end()) {
            selectedCounterpartyIndex =
                std::distance(counterparties.begin(), it);
            selectedCounterparty = *it;
        } else {
            selectedCounterpartyIndex = -1;
        }
    }

    isDirty = false;
}

// Вспомогательная функция для сортировки
static void SortCounterparties(std::vector<Counterparty> &counterparties,
                               const ImGuiTableSortSpecs *sort_specs) {
    std::sort(counterparties.begin(), counterparties.end(),
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
                      case 3:
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
            selectedCounterpartyIndex = -1; // Сброс выделения
            selectedCounterparty = Counterparty{-1, "", ""};
            originalCounterparty = selectedCounterparty;
            isDirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
            SaveChanges();
            if (!isAdding && selectedCounterpartyIndex != -1 && dbManager) {
                dbManager->deleteCounterparty(
                    counterparties[selectedCounterpartyIndex].id);
                RefreshData();
                selectedCounterparty = Counterparty{};
                originalCounterparty = Counterparty{};
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
            SaveChanges();
            RefreshData();
        }

        ImGui::Separator();

        ImGui::InputText("Фильтр по имени", filterText, sizeof(filterText));
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK "##clear_filter_counterparty")) {
            filterText[0] = '\0';
        }

        // Таблица со списком
        ImGui::BeginChild("CounterpartiesList", ImVec2(0, list_view_height),
                          true, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("counterparties_table", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
            ImGui::TableSetupColumn("Наименование",
                                    ImGuiTableColumnFlags_DefaultSort, 0.0f, 1);
            ImGui::TableSetupColumn("ИНН", 0, 0.0f, 2);
            ImGui::TableSetupColumn("Сумма", 0, 0.0f, 3);
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty) {
                    SortCounterparties(counterparties, sort_specs);
                    sort_specs->SpecsDirty = false;
                }
            }

            for (int i = 0; i < counterparties.size(); ++i) {
                if (filterText[0] != '\0' &&
                    strcasestr(counterparties[i].name.c_str(), filterText) ==
                        nullptr) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                bool is_selected = (selectedCounterpartyIndex == i);
                char label[256];
                sprintf(label, "%d##%d", counterparties[i].id, i);
                if (ImGui::Selectable(label, is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    if (selectedCounterpartyIndex != i) {
                        SaveChanges();
                        selectedCounterpartyIndex = i;
                        selectedCounterparty = counterparties[i];
                        originalCounterparty = counterparties[i];
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
                ImGui::Text("%s", counterparties[i].name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", counterparties[i].inn.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", counterparties[i].total_amount);
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        CustomWidgets::HorizontalSplitter("h_splitter", &list_view_height);

        // Редактор
        if (selectedCounterpartyIndex != -1 || isAdding) {
            ImGui::BeginChild("CounterpartyEditor", ImVec2(editor_width, 0),
                              true);

            if (isAdding) {
                ImGui::Text("Добавление нового контрагента");
            } else {
                ImGui::Text("Редактирование контрагента ID: %d",
                            selectedCounterparty.id);
            }

            char nameBuf[256];
            char innBuf[256];

            snprintf(nameBuf, sizeof(nameBuf), "%s",
                     selectedCounterparty.name.c_str());
            snprintf(innBuf, sizeof(innBuf), "%s",
                     selectedCounterparty.inn.c_str());

            if (ImGui::InputText("Наименование", nameBuf, sizeof(nameBuf))) {
                selectedCounterparty.name = nameBuf;
                isDirty = true;
            }
            if (ImGui::InputText("ИНН", innBuf, sizeof(innBuf))) {
                selectedCounterparty.inn = innBuf;
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
                "Выберите контрагента для редактирования или добавьте нового.");
            ImGui::EndChild();
        }

    }
    ImGui::End();
}

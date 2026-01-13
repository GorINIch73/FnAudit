#include "KosguView.h"
#include "../CustomWidgets.h"
#include "../IconsFontAwesome6.h"
#include <algorithm>
#include <cstring>
#include <iostream>

KosguView::KosguView()
    : selectedKosguIndex(-1),
      showEditModal(false),
      isAdding(false),
      isDirty(false) {
    Title = "Справочник КОСГУ";
    memset(filterText, 0, sizeof(filterText));
}

void KosguView::SetDatabaseManager(DatabaseManager *manager) {
    dbManager = manager;
}

void KosguView::SetPdfReporter(PdfReporter *reporter) {
    pdfReporter = reporter;
}

void KosguView::RefreshData() {
    if (dbManager) {
        kosguEntries = dbManager->getKosguEntries();
        selectedKosguIndex = -1;
    }
}


std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
KosguView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Код", "Наименование", "Сумма"};
    std::vector<std::vector<std::string>> rows;
    for (const auto &entry : kosguEntries) {
        rows.push_back({std::to_string(entry.id), entry.code, entry.name, std::to_string(entry.total_amount)});
    }
    return {headers, rows};
}

void KosguView::OnDeactivate() { SaveChanges(); }

void KosguView::SaveChanges() {
    if (!isDirty) {
        return;
    }

    if (dbManager) {
        if (isAdding) {
            dbManager->addKosguEntry(selectedKosgu);
            isAdding = false;
        } else if (selectedKosgu.id != -1) {
            dbManager->updateKosguEntry(selectedKosgu);
        }

        std::string current_code = selectedKosgu.code;
        RefreshData();

        auto it = std::find_if(
            kosguEntries.begin(), kosguEntries.end(),
            [&](const Kosgu &k) { return k.code == current_code; });

        if (it != kosguEntries.end()) {
            selectedKosguIndex = std::distance(kosguEntries.begin(), it);
            selectedKosgu = *it;
        } else {
            selectedKosguIndex = -1;
        }
    }

    isDirty = false;
}

// Вспомогательная функция для сортировки
static void SortKosgu(std::vector<Kosgu> &kosguEntries,
                      const ImGuiTableSortSpecs *sort_specs) {
    std::sort(kosguEntries.begin(), kosguEntries.end(),
              [&](const Kosgu &a, const Kosgu &b) {
                  for (int i = 0; i < sort_specs->SpecsCount; i++) {
                      const ImGuiTableColumnSortSpecs *column_spec =
                          &sort_specs->Specs[i];
                      int delta = 0;
                      switch (column_spec->ColumnIndex) {
                      case 0:
                          delta = (a.id < b.id) ? -1 : (a.id > b.id) ? 1 : 0;
                          break;
                      case 1:
                          delta = a.code.compare(b.code);
                          break;
                      case 2:
                          delta = a.name.compare(b.name);
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

void KosguView::Render() {
    if (!IsVisible) {
        if (isDirty) {
            SaveChanges();
        }
        return;
    }

    if (ImGui::Begin(GetTitle(), &IsVisible)) {

        if (dbManager && kosguEntries.empty()) {
            RefreshData();
        }

        // Панель управления
        if (ImGui::Button(ICON_FA_PLUS " Добавить")) {
            SaveChanges();
            isAdding = true;
            selectedKosguIndex = -1;
            selectedKosgu = Kosgu{-1, "", ""};
            originalKosgu = selectedKosgu;
            isDirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH " Удалить")) {
            if (!isAdding && selectedKosguIndex != -1) {
                kosgu_id_to_delete = kosguEntries[selectedKosguIndex].id;
                show_delete_popup = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
            SaveChanges();
            RefreshData();
        }

        if (show_delete_popup) {
            ImGui::OpenPopup("Подтверждение удаления##Kosgu");
        }
        if (ImGui::BeginPopupModal("Подтверждение удаления##Kosgu", &show_delete_popup, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Вы уверены, что хотите удалить эту запись КОСГУ?\nЭто действие нельзя отменить.");
            ImGui::Separator();
            if (ImGui::Button("Да", ImVec2(120, 0))) {
                if (dbManager && kosgu_id_to_delete != -1) {
                    dbManager->deleteKosguEntry(kosgu_id_to_delete);
                    RefreshData();
                    selectedKosgu = Kosgu{};
                    originalKosgu = Kosgu{};
                }
                kosgu_id_to_delete = -1;
                show_delete_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Нет", ImVec2(120, 0))) {
                kosgu_id_to_delete = -1;
                show_delete_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();

        ImGui::InputText("Фильтр", filterText,
                         sizeof(filterText));
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK)) {
            filterText[0] = '\0';
        }

        ImGui::BeginChild("KosguList", ImVec2(0, list_view_height), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("kosgu_table", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
            ImGui::TableSetupColumn("Код", ImGuiTableColumnFlags_DefaultSort,
                                    0.0f, 1);
            ImGui::TableSetupColumn("Наименование", 0, 0.0f, 2);
            ImGui::TableSetupColumn("Сумма", 0, 0.0f, 3);
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty) {
                    SortKosgu(kosguEntries, sort_specs);
                    sort_specs->SpecsDirty = false;
                }
            }

            std::vector<Kosgu> filtered_kosgu_entries;
            if (filterText[0] != '\0') {
                for (const auto &entry : kosguEntries) {
                    bool kosgu_match = false;
                    if (strcasestr(entry.code.c_str(), filterText) != nullptr || strcasestr(entry.name.c_str(), filterText) != nullptr) {
                        kosgu_match = true;
                    }

                    float filtered_amount = 0.0f;
                    bool payment_match = false;
                    if (dbManager) {
                        std::vector<ContractPaymentInfo> payment_details = dbManager->getPaymentInfoForKosgu(entry.id);
                        for (const auto& detail : payment_details) {
                            if (strcasestr(detail.date.c_str(), filterText) != nullptr ||
                                strcasestr(detail.doc_number.c_str(), filterText) != nullptr ||
                                strcasestr(detail.description.c_str(), filterText) != nullptr) {
                                filtered_amount += detail.amount;
                                payment_match = true;
                                continue;
                            }
                            char amount_str[32];
                            snprintf(amount_str, sizeof(amount_str), "%.2f", detail.amount);
                            if (strcasestr(amount_str, filterText) != nullptr) {
                                filtered_amount += detail.amount;
                                payment_match = true;
                            }
                        }
                    }

                    if (kosgu_match || payment_match) {
                        Kosgu filtered_entry = entry;
                        filtered_entry.total_amount = filtered_amount;
                        filtered_kosgu_entries.push_back(filtered_entry);
                    }
                }
            } else {
                filtered_kosgu_entries = kosguEntries;
            }

            for (int i = 0; i < filtered_kosgu_entries.size(); ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Find original index to maintain selection state
                auto original_it = std::find_if(kosguEntries.begin(), kosguEntries.end(), 
                                                [&](const Kosgu& k) { return k.id == filtered_kosgu_entries[i].id; });
                int original_index = std::distance(kosguEntries.begin(), original_it);

                bool is_selected = (selectedKosguIndex == original_index);
                char label[256];
                sprintf(label, "%d##%d", filtered_kosgu_entries[i].id, i);
                if (ImGui::Selectable(label, is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    if (selectedKosguIndex != original_index) {
                        SaveChanges();
                        selectedKosguIndex = original_index;
                        selectedKosgu = kosguEntries[original_index];
                        originalKosgu = kosguEntries[original_index];
                        isAdding = false;
                        isDirty = false;
                        if (dbManager) {
                            payment_info = dbManager->getPaymentInfoForKosgu(
                                selectedKosgu.id);
                        }
                    }
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }

                ImGui::TableNextColumn();
                ImGui::Text("%s", filtered_kosgu_entries[i].code.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", filtered_kosgu_entries[i].name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", filtered_kosgu_entries[i].total_amount);
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        CustomWidgets::HorizontalSplitter("h_splitter", &list_view_height);

        if (selectedKosguIndex != -1 || isAdding) {
            ImGui::BeginChild("KosguEditor", ImVec2(editor_width, 0), true);

            if (isAdding) {
                ImGui::Text("Добавление новой записи КОСГУ");
            } else {
                ImGui::Text("Редактирование КОСГУ ID: %d", selectedKosgu.id);
            }
            char codeBuf[256];
            char nameBuf[256];

            snprintf(codeBuf, sizeof(codeBuf), "%s",
                     selectedKosgu.code.c_str());
            snprintf(nameBuf, sizeof(nameBuf), "%s",
                     selectedKosgu.name.c_str());

            if (ImGui::InputText("Код", codeBuf, sizeof(codeBuf))) {
                selectedKosgu.code = codeBuf;
                isDirty = true;
            }
            if (ImGui::InputText("Наименование", nameBuf, sizeof(nameBuf))) {
                selectedKosgu.name = nameBuf;
                isDirty = true;
            }
            ImGui::EndChild();
            ImGui::SameLine();

            CustomWidgets::VerticalSplitter("v_splitter", &editor_width);

            ImGui::SameLine();

            ImGui::BeginChild("PaymentDetails", ImVec2(0, 0), true);
            ImGui::Text("Расшифровки платежей:");

            std::vector<ContractPaymentInfo> filtered_payment_info;
            if (filterText[0] != '\0') {
                for (const auto& info : payment_info) {
                    bool match = false;
                    if (strcasestr(info.date.c_str(), filterText) != nullptr ||
                        strcasestr(info.doc_number.c_str(), filterText) != nullptr ||
                        strcasestr(info.description.c_str(), filterText) != nullptr) {
                        match = true;
                    }
                    char amount_str[32];
                    snprintf(amount_str, sizeof(amount_str), "%.2f", info.amount);
                    if (strcasestr(amount_str, filterText) != nullptr) {
                        match = true;
                    }
                    if (match) {
                        filtered_payment_info.push_back(info);
                    }
                }
            } else {
                filtered_payment_info = payment_info;
            }

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

                for (const auto &info : filtered_payment_info) {
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
                "Выберите запись для редактирования или добавьте новую.");
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

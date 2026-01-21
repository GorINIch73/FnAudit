#include "KosguView.h"
#include "../CustomWidgets.h"
#include "../IconsFontAwesome6.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>

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
        suspiciousWordsForFilter = dbManager->getSuspiciousWords();
        selectedKosguIndex = -1;
        UpdateFilteredKosgu(); 
    }
}


std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
KosguView::GetDataAsStrings() {
    std::vector<std::string> headers = {"ID", "Код", "Наименование", "Сумма"};
    std::vector<std::vector<std::string>> rows;
    for (const auto &entry : m_filtered_kosgu_entries) {
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

void KosguView::UpdateFilteredKosgu() {
    if (!dbManager) return;

    // 1. Fetch all payment info once
    auto all_payment_info = dbManager->getAllKosguPaymentInfo();
    std::map<int, std::vector<KosguPaymentDetailInfo>> details_by_kosgu;
    for(const auto& info : all_payment_info) {
        details_by_kosgu[info.kosgu_id].push_back(info);
    }

    // 2. Text filter pass
    std::vector<Kosgu> text_filtered_entries;
    if (filterText[0] != '\0') {
        for (const auto &entry : kosguEntries) {
            bool kosgu_match = false;
            if (strcasestr(entry.code.c_str(), filterText) != nullptr || strcasestr(entry.name.c_str(), filterText) != nullptr) {
                kosgu_match = true;
            }

            float filtered_amount = 0.0f;
            bool payment_match = false;
            auto it = details_by_kosgu.find(entry.id);
            if (it != details_by_kosgu.end()) {
                for (const auto& detail : it->second) {
                    bool detail_match = false;
                    if (strcasestr(detail.date.c_str(), filterText) != nullptr ||
                        strcasestr(detail.doc_number.c_str(), filterText) != nullptr ||
                        strcasestr(detail.description.c_str(), filterText) != nullptr) {
                        detail_match = true;
                    }
                    char amount_str[32];
                    snprintf(amount_str, sizeof(amount_str), "%.2f", detail.amount);
                    if (strcasestr(amount_str, filterText) != nullptr) {
                        detail_match = true;
                    }

                    if (detail_match) {
                        filtered_amount += detail.amount;
                        payment_match = true;
                    }
                }
            }

            if (kosgu_match || payment_match) {
                Kosgu filtered_entry = entry;
                filtered_entry.total_amount = kosgu_match ? entry.total_amount : filtered_amount;
                text_filtered_entries.push_back(filtered_entry);
            }
        }
    } else {
        text_filtered_entries = kosguEntries;
    }

    // 3. Dropdown filter pass
    m_filtered_kosgu_entries.clear();
    if (m_filter_index == 0) { // "Все"
        m_filtered_kosgu_entries = text_filtered_entries;
    } else if (m_filter_index == 3) { // "Подозрительные слова"
        if (!suspiciousWordsForFilter.empty()) {
            for (const auto &entry : text_filtered_entries) {
                bool suspicious_found = false;
                auto it = details_by_kosgu.find(entry.id);
                if (it != details_by_kosgu.end()) {
                    for (const auto& detail : it->second) {
                        for (const auto &sw : suspiciousWordsForFilter) {
                            if (strcasestr(detail.description.c_str(), sw.word.c_str()) != nullptr) {
                                suspicious_found = true;
                                break;
                            }
                        }
                        if (suspicious_found) break;
                    }
                }
                if (suspicious_found) {
                    m_filtered_kosgu_entries.push_back(entry);
                }
            }
        }
    } else { // "С платежами" или "Без платежей"
        for (const auto& entry : text_filtered_entries) {
            bool has_payments = (entry.total_amount > 0.001);
            if (m_filter_index == 1 && has_payments) {
                 m_filtered_kosgu_entries.push_back(entry);
            } else if (m_filter_index == 2 && !has_payments) {
                 m_filtered_kosgu_entries.push_back(entry);
            }
        }
    }
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
        
        bool filter_changed = false;
        if (ImGui::InputText("Фильтр", filterText, sizeof(filterText))) {
            filter_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK)) {
            if (filterText[0] != '\0') {
                filterText[0] = '\0';
                filter_changed = true;
            }
        }

        ImGui::SameLine();
        float avail_width = ImGui::GetContentRegionAvail().x;
        ImGui::PushItemWidth(avail_width - ImGui::GetStyle().ItemSpacing.x);
        const char *filter_items[] = { "Все", "С платежами", "Без платежей", "Подозрительные слова" };
        if (ImGui::Combo("##FilterCombo", &m_filter_index, filter_items, IM_ARRAYSIZE(filter_items))) {
            filter_changed = true;
        }
        ImGui::PopItemWidth();

        if (filter_changed) {
            UpdateFilteredKosgu();
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
                    SortKosgu(m_filtered_kosgu_entries, sort_specs);
                    sort_specs->SpecsDirty = false;
                }
            }
            
            ImGuiListClipper clipper;
            clipper.Begin(m_filtered_kosgu_entries.size());
            while(clipper.Step()){
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    auto original_it = std::find_if(kosguEntries.begin(), kosguEntries.end(), 
                                                    [&](const Kosgu& k) { return k.id == m_filtered_kosgu_entries[i].id; });
                    int original_index = (original_it == kosguEntries.end()) ? -1 : std::distance(kosguEntries.begin(), original_it);

                    bool is_selected = (selectedKosguIndex == original_index);
                    char label[256];
                    sprintf(label, "%d##%d", m_filtered_kosgu_entries[i].id, i);
                    if (ImGui::Selectable(label, is_selected,
                                          ImGuiSelectableFlags_SpanAllColumns)) {
                        if (selectedKosguIndex != original_index && original_index != -1) {
                            SaveChanges();
                            selectedKosguIndex = original_index;
                            selectedKosgu = kosguEntries[original_index];
                            originalKosgu = kosguEntries[original_index];
                            isAdding = false;
                            isDirty = false;
                            if (dbManager) {
                                payment_info = dbManager->getPaymentInfoForKosgu(selectedKosgu.id);
                            }
                        }
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", m_filtered_kosgu_entries[i].code.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", m_filtered_kosgu_entries[i].name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", m_filtered_kosgu_entries[i].total_amount);
                }
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

            snprintf(codeBuf, sizeof(codeBuf), "%s", selectedKosgu.code.c_str());
            snprintf(nameBuf, sizeof(nameBuf), "%s", selectedKosgu.name.c_str());

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

            std::vector<ContractPaymentInfo> details_to_show = payment_info;
            if (m_filter_index == 3 && !suspiciousWordsForFilter.empty()) {
                std::vector<ContractPaymentInfo> suspicious_details;
                for (const auto& info : payment_info) {
                    bool found = false;
                    for (const auto& word : suspiciousWordsForFilter) {
                        if (strcasestr(info.description.c_str(), word.word.c_str()) != nullptr) {
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        suspicious_details.push_back(info);
                    }
                }
                details_to_show = suspicious_details;
            }

            std::vector<ContractPaymentInfo> filtered_payment_info;
            if (filterText[0] != '\0') {
                for (const auto& info : details_to_show) {
                    bool match = false;
                    if (strcasestr(info.date.c_str(), filterText) != nullptr ||
                        strcasestr(info.doc_number.c_str(), filterText) != nullptr ||
                        strcasestr(info.description.c_str(), filterText) != nullptr) {
                        match = true;
                    }
                    char amount_str[32];
                    snprintf(amount_str, sizeof(amount_str), "%.2f", info.amount);
                    if (!match && strcasestr(amount_str, filterText) != nullptr) {
                        match = true;
                    }
                    if (match) {
                        filtered_payment_info.push_back(info);
                    }
                }
            } else {
                filtered_payment_info = details_to_show;
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

                ImGuiListClipper clipper;
                clipper.Begin(filtered_payment_info.size());
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const auto &info = filtered_payment_info[i];
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
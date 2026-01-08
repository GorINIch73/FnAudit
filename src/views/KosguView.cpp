#include "KosguView.h"
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
    std::vector<std::string> headers = {"ID", "Код", "Наименование"};
    std::vector<std::vector<std::string>> rows;
    for (const auto &entry : kosguEntries) {
        rows.push_back({std::to_string(entry.id), entry.code, entry.name});
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
            SaveChanges();
            if (!isAdding && selectedKosguIndex != -1 && dbManager) {
                dbManager->deleteKosguEntry(
                    kosguEntries[selectedKosguIndex].id);
                RefreshData();
                selectedKosgu = Kosgu{};
                originalKosgu = Kosgu{};
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Обновить")) {
            SaveChanges();
            RefreshData();
        }

        ImGui::Separator();

        ImGui::InputText("Фильтр по наименованию", filterText,
                         sizeof(filterText));

        ImGui::BeginChild("KosguList", ImVec2(0, list_view_height), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("kosgu_table", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("ID", 0, 0.0f, 0);
            ImGui::TableSetupColumn("Код", ImGuiTableColumnFlags_DefaultSort,
                                    0.0f, 1);
            ImGui::TableSetupColumn("Наименование", 0, 0.0f, 2);
            ImGui::TableHeadersRow();

            if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty) {
                    SortKosgu(kosguEntries, sort_specs);
                    sort_specs->SpecsDirty = false;
                }
            }

            for (int i = 0; i < kosguEntries.size(); ++i) {
                if (filterText[0] != '\0' &&
                    strcasestr(kosguEntries[i].name.c_str(), filterText) ==
                        nullptr) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                bool is_selected = (selectedKosguIndex == i);
                char label[256];
                sprintf(label, "%d##%d", kosguEntries[i].id, i);
                if (ImGui::Selectable(label, is_selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    if (selectedKosguIndex != i) {
                        SaveChanges();
                        selectedKosguIndex = i;
                        selectedKosgu = kosguEntries[i];
                        originalKosgu = kosguEntries[i];
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
                ImGui::Text("%s", kosguEntries[i].code.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", kosguEntries[i].name.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        ImGui::InvisibleButton("h_splitter", ImVec2(-1, 8.0f));
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        if (ImGui::IsItemActive()) {
            list_view_height += ImGui::GetIO().MouseDelta.y;
            if (list_view_height < 50.0f)
                list_view_height = 50.0f;
        }

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

            ImGui::InvisibleButton("v_splitter", ImVec2(8.0f, -1));
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            if (ImGui::IsItemActive()) {
                editor_width += ImGui::GetIO().MouseDelta.x;
                if (editor_width < 100.0f)
                    editor_width = 100.0f;
            }
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
                "Выберите запись для редактирования или добавьте новую.");
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

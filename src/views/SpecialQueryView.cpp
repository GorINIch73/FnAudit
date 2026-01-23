#include "SpecialQueryView.h"
#include "../IconsFontAwesome6.h"
#include <cstring>
#include <iostream>
#include <sstream>

SpecialQueryView::SpecialQueryView(const std::string& title, const std::string& query)
    : query(query) {
    Title = title;
}

void SpecialQueryView::SetDatabaseManager(DatabaseManager *manager) {
    dbManager = manager;
    if (dbManager && dbManager->is_open()) {
        ExecuteQuery();
    }
}

void SpecialQueryView::SetPdfReporter(PdfReporter* reporter) {
    // Not used in this view
}

void SpecialQueryView::SetUIManager(UIManager* manager) {
    uiManager = manager;
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> SpecialQueryView::GetDataAsStrings() {
    return {queryResult.columns, queryResult.rows};
}

void SpecialQueryView::ExecuteQuery() {
    if (dbManager && dbManager->is_open()) {
        dbManager->executeSelect(query.c_str(), queryResult.columns, queryResult.rows);
        selected_cells.assign(queryResult.rows.size(), std::vector<bool>(queryResult.columns.size(), false));
        last_clicked_cell = ImVec2(-1, -1);
    } else {
        queryResult.columns.clear();
        queryResult.rows.clear();
        selected_cells.clear();
        std::cerr << "No database open to execute SQL query." << std::endl;
    }
}

void SpecialQueryView::Render() {
    if (!IsVisible) {
        return;
    }

    if (ImGui::Begin(GetTitle(), &IsVisible)) {
        if (ImGui::Button(ICON_FA_ROTATE " Обновить")) {
            ExecuteQuery();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_COPY " Копировать")) {
            std::stringstream ss;

            // Check for any selection
            bool any_selection = false;
            if (!selected_cells.empty()) {
                for (const auto& row : selected_cells) {
                    for (bool cell : row) {
                        if (cell) {
                            any_selection = true;
                            break;
                        }
                    }
                    if (any_selection) {
                        break;
                    }
                }
            }

            bool first_row = true;
            for (size_t i = 0; i < queryResult.rows.size(); ++i) {
                bool should_copy = !any_selection;
                if (any_selection) {
                    for (size_t j = 0; j < queryResult.columns.size(); ++j) {
                        if (selected_cells[i][j]) {
                            should_copy = true;
                            break;
                        }
                    }
                }

                if (should_copy) {
                    if (!first_row) {
                        ss << "\n";
                    }
                    for (size_t j = 0; j < queryResult.columns.size(); ++j) {
                        if (j > 0) {
                            ss << "\t";
                        }
                        ss << queryResult.rows[i][j];
                    }
                    first_row = false;
                }
            }
            ImGui::SetClipboardText(ss.str().c_str());
        }

        ImGui::Separator();
        ImGui::Text("Результат:");
        if (!queryResult.columns.empty()) {
            if (ImGui::BeginChild("##TableScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
                if (ImGui::BeginTable("special_query_result", queryResult.columns.size(),
                                     ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX)) {
                for (const auto &col : queryResult.columns) {
                    ImGui::TableSetupColumn(col.c_str());
                }
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(queryResult.rows.size());
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        ImGui::TableNextRow();
                        for (size_t j = 0; j < queryResult.columns.size(); ++j) {
                            ImGui::TableNextColumn();
                            char label[256];
                            snprintf(label, sizeof(label), "%s##%d_%zu", queryResult.rows[i][j].c_str(), i, j);
                            bool is_selected = selected_cells[i][j];

                            if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                                if (ImGui::GetIO().KeyCtrl) {
                                    selected_cells[i][j] = !selected_cells[i][j];
                                } else if (ImGui::GetIO().KeyShift && last_clicked_cell.x != -1) {
                                    for(auto& row : selected_cells) std::fill(row.begin(), row.end(), false);
                                    float r_min = std::min(last_clicked_cell.y, (float)i);
                                    float r_max = std::max(last_clicked_cell.y, (float)i);
                                    float c_min = std::min(last_clicked_cell.x, (float)j);
                                    float c_max = std::max(last_clicked_cell.x, (float)j);
                                    for(int r = r_min; r <= r_max; ++r) {
                                        for (int c = c_min; c <= c_max; ++c) {
                                            selected_cells[r][c] = true;
                                        }
                                    }
                                } else {
                                    for(auto& row : selected_cells) std::fill(row.begin(), row.end(), false);
                                    selected_cells[i][j] = true;
                                }
                                last_clicked_cell = ImVec2(j, i);
                            }
                        }
                    }
                }
                ImGui::EndTable();
                }
                ImGui::EndChild();
            }
        } else {
            ImGui::Text("Нет результатов или запрос не был выполнен.");
        }
    }
    ImGui::End();
}

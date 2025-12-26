#include "SqlQueryView.h"
#include <iostream>
#include <cstring>
#include "../IconsFontAwesome6.h"

SqlQueryView::SqlQueryView()
    : dbManager(nullptr), pdfReporter(nullptr) {
    memset(queryInputBuffer, 0, sizeof(queryInputBuffer));
}

void SqlQueryView::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
}

void SqlQueryView::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
}

void SqlQueryView::Render() {
    if (!IsVisible) {
        return;
    }

    if (!ImGui::Begin("SQL Запрос", &IsVisible)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Введите SQL запрос:");
    ImGui::InputTextMultiline("##SQLQueryInput", queryInputBuffer, sizeof(queryInputBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8));

    if (ImGui::Button(ICON_FA_PLAY " Выполнить")) {
        if (dbManager && dbManager->is_open()) {
            dbManager->executeSelect(queryInputBuffer, queryResult.columns, queryResult.rows);
        } else {
            queryResult.columns.clear();
            queryResult.rows.clear();
            std::cerr << "No database open to execute SQL query." << std::endl;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_PDF " Экспорт в PDF")) {
        if (pdfReporter && dbManager && dbManager->is_open() && !queryResult.columns.empty()) {
            pdfReporter->generatePdfFromTable("sql_query_report.pdf", "Результаты SQL Запроса", queryResult.columns, queryResult.rows);
        } else {
            std::cerr << "Cannot export to PDF: No database open, PdfReporter not set, or no query results." << std::endl;
        }
    }

    ImGui::Separator();
    ImGui::Text("Результат:");

    if (!queryResult.columns.empty()) {
        if (ImGui::BeginTable("sql_query_result", queryResult.columns.size(), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            for (const auto& col : queryResult.columns) {
                ImGui::TableSetupColumn(col.c_str());
            }
            ImGui::TableHeadersRow();

            for (const auto& row : queryResult.rows) {
                ImGui::TableNextRow();
                for (const auto& cell : row) {
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", cell.c_str());
                }
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::Text("Нет результатов или запрос не был выполнен.");
    }

    ImGui::End();
}

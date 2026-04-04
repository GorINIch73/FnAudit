#include "ReconciliationView.h"
#include "../IconsFontAwesome6.h"
#include "../UIManager.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>

ReconciliationView::ReconciliationView() {
    Title = "Сверка: Банк ↔ Документы Основания";
    memset(filter_buffer, 0, sizeof(filter_buffer));
}

void ReconciliationView::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
}

void ReconciliationView::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
}

void ReconciliationView::SetUIManager(UIManager* manager) {
    uiManager = manager;
}

void ReconciliationView::RefreshData() {
    if (!dbManager) return;

    std::string filter = filter_buffer;
    records = dbManager->getReconciliationData(filter);

    // Группировка по платежам
    payment_groups.clear();
    std::map<int, int> payment_to_group;

    for (int i = 0; i < records.size(); i++) {
        int pid = records[i].payment_id;
        if (payment_to_group.find(pid) == payment_to_group.end()) {
            PaymentGroup group;
            group.payment_id = pid;
            group.payment_date = records[i].payment_date;
            group.payment_doc_number = records[i].payment_doc_number;
            group.payment_amount = records[i].payment_amount;
            group.counterparty_name = records[i].counterparty_name;
            payment_to_group[pid] = static_cast<int>(payment_groups.size());
            payment_groups.push_back(group);
        }
        payment_groups[payment_to_group[pid]].record_indices.push_back(i);
    }

    filtered_records = records;
}

void ReconciliationView::OnDeactivate() {
    IsVisible = false;
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>
ReconciliationView::GetDataAsStrings() {
    std::vector<std::string> headers = {
        "Дата платежа", "№ ПП", "Сумма ПП", "Контрагент",
        "Дата ДО", "№ ДО", "Наименование ДО", "Содержание", "Сумма ДО"
    };
    std::vector<std::vector<std::string>> data;
    for (const auto& rec : records) {
        data.push_back({
            rec.payment_date,
            rec.payment_doc_number,
            std::to_string(rec.payment_amount),
            rec.counterparty_name,
            rec.base_doc_date,
            rec.base_doc_number,
            rec.base_doc_name,
            rec.base_detail_content,
            std::to_string(rec.base_detail_amount)
        });
    }
    return {headers, data};
}

void ReconciliationView::Render() {
    if (!IsVisible) return;

    RefreshData();

    ImGui::Begin(Title.c_str(), &IsVisible);

    // --- Панель фильтров ---
    ImGui::Text("Фильтр:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##ReconFilter", filter_buffer, sizeof(filter_buffer),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        RefreshData();
    }
    ImGui::SameLine();
    if (ImGui::Button("Применить")) {
        RefreshData();
    }

    ImGui::Separator();
    ImGui::Text("Записей: %zu | Платежей: %zu", records.size(), payment_groups.size());

    // --- Основной вид: список платежей слева, детали справа ---
    ImGui::BeginChild("ReconMainRegion", ImVec2(0, 0), true);

    // Левая панель - список платежей
    ImGui::BeginChild("PaymentList", ImVec2(350, 0), true);
    ImGui::TextUnformatted("Платёжные поручения");
    ImGui::Separator();

    if (ImGui::BeginTable("paymentGroupsTable", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("№");
        ImGui::TableSetupColumn("Сумма", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Для сверки", ImGuiTableColumnFlags_WidthFixed, 20.0f);
        ImGui::TableHeadersRow();

        for (int g = 0; g < payment_groups.size(); g++) {
            const auto& group = payment_groups[g];
            bool is_selected = (g == selected_index);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", group.payment_date.c_str());

            ImGui::TableNextColumn();
            if (ImGui::Selectable(group.payment_doc_number.c_str(), is_selected,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                selected_index = g;
                selected_payment_id = group.payment_id;
            }

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", group.payment_amount);

            ImGui::TableNextColumn();
            // Покажем если есть документы для сверки
            bool has_for_checking = false;
            for (int idx : group.record_indices) {
                if (records[idx].base_doc_for_checking) {
                    has_for_checking = true;
                    break;
                }
            }
            if (has_for_checking) ImGui::Text(ICON_FA_TRIANGLE_EXCLAMATION);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    // Правая панель - детали выбранного платежа
    ImGui::SameLine();
    ImGui::BeginChild("PaymentDetails", ImVec2(0, 0), true);

    if (selected_index >= 0 && selected_index < payment_groups.size()) {
        const auto& group = payment_groups[selected_index];

        ImGui::Text("Платёжное поручение: %s от %s", group.payment_doc_number.c_str(),
                    group.payment_date.c_str());
        ImGui::Text("Контрагент: %s", group.counterparty_name.c_str());
        ImGui::Text("Сумма ПП: %.2f", group.payment_amount);
        ImGui::Separator();

        // Таблица расшифровок
        if (ImGui::BeginTable("reconDetailsTable", 8,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Дата ДО", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("№ ДО", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Тип ДО");
            ImGui::TableSetupColumn("Содержание операции");
            ImGui::TableSetupColumn("Дебет", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Кредит", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("КОСГУ", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Сумма ДО", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableHeadersRow();

            double total_base_detail = 0.0;
            for (int idx : group.record_indices) {
                const auto& rec = records[idx];
                ImGui::TableNextRow();

                // Подсветка если не совпадает сумма
                double diff = rec.payment_amount - rec.base_doc_total;
                if (std::abs(diff) > 0.01 && rec.base_doc_id != -1) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(60, 40, 40, 255));
                }

                ImGui::TableNextColumn();
                ImGui::Text("%s", rec.base_doc_date.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", rec.base_doc_number.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", rec.base_doc_name.c_str());

                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", rec.base_detail_content.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", rec.base_detail_debit.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", rec.base_detail_credit.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", rec.base_detail_kosgu.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%.2f", rec.base_detail_amount);
                total_base_detail += rec.base_detail_amount;
            }

            // Итоговая строка
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("");
            ImGui::TableNextColumn();
            ImGui::Text("Итого ДО: %.2f", total_base_detail);

            // Разница
            ImGui::TableNextRow();
            for (int i = 0; i < 7; i++) {
                ImGui::TableNextColumn();
            }
            double diff = group.payment_amount - total_base_detail;
            ImU32 diff_color = std::abs(diff) < 0.01 ? IM_COL32(40, 80, 40, 255) : IM_COL32(120, 40, 40, 255);
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, diff_color);
            ImGui::TextUnformatted("");
            ImGui::TableNextColumn();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, diff_color);
            ImGui::Text("Разница: %.2f", diff);

            ImGui::EndTable();
        }
    } else {
        ImGui::TextUnformatted("Выберите платёжное поручение для просмотра");
    }

    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::End();
}

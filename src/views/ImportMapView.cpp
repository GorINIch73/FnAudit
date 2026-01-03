#include "ImportMapView.h"
#include "../ImportManager.h"
#include "../UIManager.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>

// Helper to split a string by a delimiter
static std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

static std::string get_regex_match(const std::string &text,
                                   const std::string &pattern) {
    std::cout << "Testing regex:" << std::endl;
    std::cout << "  Text:    '" << text << "'" << std::endl;
    std::cout << "  Pattern: '" << pattern << "'" << std::endl;
    try {
        std::regex re(pattern);
        std::smatch match;
        if (std::regex_search(text, match, re) && match.size() > 1) {
            std::cout << "  Match:   '" << match[1].str() << "'" << std::endl;
            return match[1].str();
        }
    } catch (const std::regex_error &e) {
        std::cerr << "  Regex error: " << e.what() << std::endl;
        return "Regex error: " + std::string(e.what());
    }
    std::cout << "  No match." << std::endl;
    return "No match";
}

ImportMapView::ImportMapView() {
    Title = "Сопоставление полей для импорта";
    IsVisible = false;
    targetFields = {"Дата",  "Номер док.", "Тип",
                    "Сумма", "Контрагент", "Назначение"};
}

void ImportMapView::Open(const std::string &filePath) {
    Reset();
    importFilePath = filePath;
    ReadPreviewData();
    RefreshRegexes();
    IsVisible = true;
}

void ImportMapView::Reset() {
    importFilePath.clear();
    fileHeaders.clear();
    sampleData.clear();
    currentMapping.clear();
    for (const auto &field : targetFields) {
        currentMapping[field] = -1; // -1 means "Not Mapped"
    }
    sample_description.clear();
}

void ImportMapView::ReadPreviewData() {
    if (importFilePath.empty())
        return;

    std::ifstream file(importFilePath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open file for reading header: "
                  << importFilePath << std::endl;
        return;
    }

    // Read header
    std::string headerLine;
    if (std::getline(file, headerLine)) {
        fileHeaders = split(headerLine, '\t');
    }

    // Read first 30 data rows
    std::string dataLine;
    int line_count = 0;
    while (std::getline(file, dataLine) && line_count < 30) {
        sampleData.push_back(split(dataLine, '\t'));
        line_count++;
    }
}

void ImportMapView::SetUIManager(UIManager *manager) { uiManager = manager; }

void ImportMapView::RefreshRegexes() {
    if (dbManager) {
        regexes = dbManager->getRegexes();
    }
}

void ImportMapView::Render() {
    if (!IsVisible) {
        return;
    }

    float footer_height =
        ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

    ImGui::SetNextWindowSize(ImVec2(700, 750), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Title.c_str(), &IsVisible)) {
        ImGui::Text("Файл: %s", importFilePath.c_str());
        ImGui::Separator();

        // --- Mapping Controls ---
        ImGui::Text("Укажите, какой столбец в файле соответствует какому полю "
                    "в программе.");
        if (ImGui::BeginTable("mapping_table", 2, ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Поле в программе",
                                    ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Столбец из файла");
            ImGui::TableHeadersRow();

            for (const auto &targetField : targetFields) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", targetField.c_str());

                ImGui::TableNextColumn();
                ImGui::PushID(targetField.c_str());

                const char *current_item =
                    (currentMapping[targetField] >= 0 &&
                     currentMapping[targetField] < fileHeaders.size())
                        ? fileHeaders[currentMapping[targetField]].c_str()
                        : "Не выбрано";

                if (ImGui::BeginCombo("", current_item)) {
                    bool is_selected = (currentMapping[targetField] == -1);
                    if (ImGui::Selectable("Не выбрано", is_selected)) {
                        currentMapping[targetField] = -1;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();

                    for (int i = 0; i < fileHeaders.size(); ++i) {
                        is_selected = (currentMapping[targetField] == i);
                        if (ImGui::Selectable(fileHeaders[i].c_str(),
                                              is_selected)) {
                            currentMapping[targetField] = i;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Spacing();

        bool description_changed = false;

        // --- Data Preview Table ---
        ImGui::Text("Предпросмотр данных (первые 30 строк):");
        ImGui::BeginChild("PreviewScrollRegion",
                          ImVec2(0, ImGui::GetTextLineHeight() * 7), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("preview_table", fileHeaders.size(),
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollX)) {
            for (const auto &header : fileHeaders) {
                ImGui::TableSetupColumn(header.c_str());
            }
            ImGui::TableHeadersRow();

            for (int i = 0; i < sampleData.size(); ++i) {
                ImGui::TableNextRow();
                for (int j = 0; j < sampleData[i].size(); ++j) {
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(
                            sampleData[i][j].c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns)) {
                        int desc_col = currentMapping["Назначение"];
                        if (desc_col != -1 && desc_col < sampleData[i].size()) {
                            sample_description = sampleData[i][desc_col];
                            description_changed = true;
                        }
                    }
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild(); // End PreviewScrollRegion

        ImGui::Separator();
        ImGui::Spacing();

        // --- Regex Testing UI ---
        ImGui::Text("Тестирование регулярных выражений");
        if (ImGui::InputTextMultiline(
                "Пример назначения платежа", &sample_description,
                ImVec2(-1, ImGui::GetTextLineHeight() * 4))) {
            description_changed = true;
        }

        if (description_changed) {
            if (contract_regex_index != -1)
                contract_match = get_regex_match(
                    sample_description, regexes[contract_regex_index].pattern);
            if (kosgu_regex_index != -1)
                kosgu_match = get_regex_match(
                    sample_description, regexes[kosgu_regex_index].pattern);
            if (invoice_regex_index != -1)
                invoice_match = get_regex_match(
                    sample_description, regexes[invoice_regex_index].pattern);
        }

        auto render_regex_selector = [&](const char *label, int &selected_index,
                                         std::string &match_result) {
            ImGui::PushID(label);
            const char *current_regex_name = "Не выбрано";
            if (selected_index >= 0 && selected_index < regexes.size()) {
                current_regex_name = regexes[selected_index].name.c_str();
            }

            if (ImGui::BeginCombo(label, current_regex_name)) {
                for (int i = 0; i < regexes.size(); ++i) {
                    bool is_selected = (selected_index == i);
                    if (ImGui::Selectable(regexes[i].name.c_str(),
                                          is_selected)) {
                        selected_index = i;
                        description_changed =
                            true; // Recalculate on selection change
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (selected_index >= 0 && selected_index < regexes.size()) {
                if (ImGui::InputText("##pattern",
                                     &regexes[selected_index].pattern)) {
                    description_changed = true; // Recalculate on pattern change
                }
                ImGui::Text("Результат: %s", match_result.c_str());
            }
            ImGui::PopID();
        };

        render_regex_selector("Договор", contract_regex_index, contract_match);
        render_regex_selector("КОСГУ", kosgu_regex_index, kosgu_match);
        render_regex_selector("Накладная", invoice_regex_index, invoice_match);

        // Pin buttons to the bottom
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - footer_height);

        ImGui::Separator();
        if (ImGui::Button("Импортировать")) {
            if (dbManager && uiManager && uiManager->importManager) {
                uiManager->isImporting = true;
                std::thread([this]() {
                    uiManager->importManager->ImportPaymentsFromTsv(
                        importFilePath, dbManager, currentMapping,
                        uiManager->importProgress, uiManager->importMessage,
                        uiManager->importMutex);
                    uiManager->isImporting = false;
                }).detach();
            }
            IsVisible = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) {
            IsVisible = false;
        }
    }
    ImGui::End();
}

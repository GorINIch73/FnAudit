#include "ImportMapView.h"
#include "../ImportManager.h"
#include "../UIManager.h"
#include "imgui.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>

// Helper to split a string by a delimiter
static std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

ImportMapView::ImportMapView() {
    Title = "Сопоставление полей для импорта";
    IsVisible = false;
    targetFields = {"Дата", "Номер док.", "Тип", "Сумма", "Получатель", "Назначение"};
}

void ImportMapView::Open(const std::string& filePath) {
    Reset();
    importFilePath = filePath;
    ReadPreviewData();
    IsVisible = true;
}

void ImportMapView::Reset() {
    importFilePath.clear();
    fileHeaders.clear();
    sampleData.clear();
    currentMapping.clear();
    for(const auto& field : targetFields) {
        currentMapping[field] = -1; // -1 means "Not Mapped"
    }
}

void ImportMapView::ReadPreviewData() {
    if (importFilePath.empty()) return;

    std::ifstream file(importFilePath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open file for reading header: " << importFilePath << std::endl;
        return;
    }

    // Read header
    std::string headerLine;
    if (std::getline(file, headerLine)) {
        fileHeaders = split(headerLine, '\t');
    }

    // Read first 5 data rows
    std::string dataLine;
    int line_count = 0;
    while (std::getline(file, dataLine) && line_count < 5) {
        sampleData.push_back(split(dataLine, '\t'));
        line_count++;
    }
}

void ImportMapView::SetUIManager(UIManager* manager) {
    uiManager = manager;
}

void ImportMapView::Render() {
    if (!IsVisible) {
        return;
    }

    float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing(); // Height of buttons + spacing

    ImGui::SetNextWindowSize(ImVec2(700, 550), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Title.c_str(), &IsVisible)) {
        ImGui::Text("Файл: %s", importFilePath.c_str());
        ImGui::Separator();

        // --- Mapping Controls ---
        ImGui::Text("Укажите, какой столбец в файле соответствует какому полю в программе.");
        if (ImGui::BeginTable("mapping_table", 2, ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Поле в программе", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Столбец из файла");
            ImGui::TableHeadersRow();

            for (const auto& targetField : targetFields) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", targetField.c_str());

                ImGui::TableNextColumn();
                ImGui::PushID(targetField.c_str());
                
                const char* current_item = (currentMapping[targetField] >= 0 && currentMapping[targetField] < fileHeaders.size()) 
                                           ? fileHeaders[currentMapping[targetField]].c_str() 
                                           : "Не выбрано";

                if (ImGui::BeginCombo("", current_item)) {
                    bool is_selected = (currentMapping[targetField] == -1);
                    if (ImGui::Selectable("Не выбрано", is_selected)) {
                        currentMapping[targetField] = -1;
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();

                    for (int i = 0; i < fileHeaders.size(); ++i) {
                        is_selected = (currentMapping[targetField] == i);
                        if (ImGui::Selectable(fileHeaders[i].c_str(), is_selected)) {
                            currentMapping[targetField] = i;
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Spacing();

        // --- Data Preview Table ---
        ImGui::Text("Предпросмотр данных (первые 5 строк):");
        float preview_start_y = ImGui::GetCursorPosY();
        
        // Calculate available height for preview
        float available_height_for_preview = ImGui::GetWindowHeight() - footer_height - preview_start_y - ImGui::GetStyle().ItemSpacing.y;

        ImGui::BeginChild("PreviewScrollRegion", ImVec2(0, available_height_for_preview), true, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("preview_table", fileHeaders.size(), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {
            for(const auto& header : fileHeaders) {
                ImGui::TableSetupColumn(header.c_str());
            }
            ImGui::TableHeadersRow();

            for(const auto& row : sampleData) {
                ImGui::TableNextRow();
                for(const auto& cell : row) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(cell.c_str());
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild(); // End PreviewScrollRegion

        // Pin buttons to the bottom
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - footer_height);

        ImGui::Separator();
        if (ImGui::Button("Импортировать")) {
            if (dbManager && uiManager && uiManager->importManager) {
                uiManager->isImporting = true;
                std::thread([this]() {
                    uiManager->importManager->ImportPaymentsFromTsv(
                        importFilePath,
                        dbManager,
                        currentMapping,
                        uiManager->importProgress,
                        uiManager->importMessage,
                        uiManager->importMutex
                    );
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
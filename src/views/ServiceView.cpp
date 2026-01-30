#include "ServiceView.h"
#include "../UIManager.h"
#include "../ExportManager.h"
#include "../IconsFontAwesome6.h"
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include <thread>

ServiceView::ServiceView() {
    Title = "Сервис";
    Reset();
}

void ServiceView::SetUIManager(UIManager* manager) {
    uiManager = manager;
}

void ServiceView::SetExportManager(ExportManager* manager) {
    exportManager = manager;
}

void ServiceView::Reset() {
    m_unfoundContracts.clear();
    m_successfulImports = 0;
    m_ikzImportStarted = false;
    m_showUnfoundContracts = false;
    m_lastExportCount = -1;
}

void ServiceView::Render() {
    if (!IsVisible) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Title.c_str(), &IsVisible)) {
        // --- IKZ Import Section ---
        ImGui::TextUnformatted("Импорт ИКЗ из файла (Колонки: Номер, Дата, ИКЗ)");
        ImGui::Spacing();
        
        ImGui::BeginDisabled(uiManager->isImporting);

        if (ImGui::Button(ICON_FA_FILE_IMPORT " Импортировать ИКЗ")) {
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.countSelectionMax = 1;
            config.userDatas = IGFD::UserDatas(nullptr);
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey_IKZ_Service", "Выберите файл для импорта ИКЗ", ".csv,.tsv", config);
        }

        ImGui::EndDisabled();

        if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey_IKZ_Service")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                
                m_ikzImportStarted = true;
                uiManager->isImporting = true;
                m_showUnfoundContracts = true;
                m_unfoundContracts.clear();
                m_successfulImports = 0;

                std::thread([this, filePathName]() {
                    uiManager->importManager->importIKZFromFile(
                        filePathName,
                        dbManager,
                        m_unfoundContracts,
                        m_successfulImports,
                        uiManager->importProgress,
                        uiManager->importMessage,
                        uiManager->importMutex
                    );
                    m_ikzImportStarted = false;
                    uiManager->isImporting = false;
                }).detach();
            }
            ImGuiFileDialog::Instance()->Close();
        }
        

        if (m_showUnfoundContracts) {
            ImGui::Spacing();
            ImGui::Separator();
            
            if (ImGui::Button("Очистить результаты импорта ИКЗ")) {
                m_showUnfoundContracts = false;
                m_unfoundContracts.clear();
                m_successfulImports = 0;
            }

            if (!m_ikzImportStarted && !uiManager->isImporting) {
                ImGui::Text("Импортировано записей: %d", m_successfulImports);
                ImGui::Text("Не найдено контрактов: %zu", m_unfoundContracts.size());
            }

            if (!m_unfoundContracts.empty()) {
                ImGui::Text("Ненайденные контракты:");
                if (ImGui::BeginTable("unfound_contracts_table_service", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 150))) {
                    ImGui::TableSetupColumn("Номер контракта");
                    ImGui::TableSetupColumn("Дата контракта");
                    ImGui::TableSetupColumn("ИКЗ из файла");
                    ImGui::TableHeadersRow();

                    for (const auto& contract : m_unfoundContracts) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(contract.number.c_str());
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(contract.date.c_str());
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(contract.ikz.c_str());
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::Spacing();
        }
        
        ImGui::Separator();
        ImGui::Spacing();

        // --- Contract Export Section ---
        ImGui::TextUnformatted("Экспорт договоров для проверки");
        ImGui::Spacing();

        ImGui::BeginDisabled(uiManager->isImporting);

        if (ImGui::Button(ICON_FA_FILE_EXPORT " Экспортировать договоры")) {
            m_lastExportCount = -1; // Reset on new export
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.countSelectionMax = 1;
            config.userDatas = IGFD::UserDatas(nullptr);
            ImGuiFileDialog::Instance()->OpenDialog("ExportContractsDlgKey", "Экспорт договоров для проверки", ".csv", config);
        }
        
        if (m_lastExportCount != -1) {
            ImGui::SameLine();
            ImGui::Text("Экспортировано %d договоров.", m_lastExportCount);
        }

        ImGui::EndDisabled();

        if (ImGuiFileDialog::Instance()->Display("ExportContractsDlgKey")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                
                uiManager->isImporting = true; // Reuse the "isImporting" flag as a generic "isProcessing"
                
                std::thread([this, filePathName]() {
                    {
                        std::lock_guard<std::mutex> lock(uiManager->importMutex);
                        uiManager->importProgress = 0.5f;
                        uiManager->importMessage = "Экспорт договоров...";
                    }
                    
                    int exportedCount = 0;
                    if (exportManager) {
                       exportedCount = exportManager->ExportContractsForChecking(filePathName);
                    }
                    m_lastExportCount = exportedCount;

                    {
                        std::lock_guard<std::mutex> lock(uiManager->importMutex);
                        uiManager->importProgress = 1.0f;
                        uiManager->importMessage = "Экспорт завершен.";
                    }
                    // The popup will close on its own after a short delay
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    uiManager->isImporting = false; 
                }).detach();
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }
    ImGui::End();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> ServiceView::GetDataAsStrings() {
    // This view doesn't present tabular data for export, so return empty.
    return {};
}

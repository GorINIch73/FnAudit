#include "ServiceView.h"
#include "../UIManager.h"
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

void ServiceView::Reset() {
    m_unfoundContracts.clear();
    m_successfulImports = 0;
    m_ikzImportStarted = false;
    m_showUnfoundContracts = false;
}

void ServiceView::Render() {
    if (!IsVisible) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Title.c_str(), &IsVisible)) {
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
        
        ImGui::EndDisabled();

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
                if (ImGui::BeginTable("unfound_contracts_table_service", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 250))) {
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
            } else if (!m_ikzImportStarted && !uiManager->isImporting) {
                // This message is now implicitly handled by the stats above
                // ImGui::Text("Все контракты из файла были найдены и обновлены.");
            }
            ImGui::Spacing();
        }
    }
    ImGui::End();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> ServiceView::GetDataAsStrings() {
    // This view doesn't present tabular data for export, so return empty.
    return {};
}

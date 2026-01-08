#include "UIManager.h"
#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <vector>

#include <GLFW/glfw3.h>
#include "ImGuiFileDialog.h"
#include "ImportManager.h"
#include "PdfReporter.h"
#include "views/BaseView.h"

const size_t MAX_RECENT_PATHS = 10;
const std::string RECENT_PATHS_FILE = ".recent_dbs.txt";

UIManager::UIManager()
    : dbManager(nullptr), pdfReporter(nullptr), importManager(nullptr), window(nullptr) {
    LoadRecentDbPaths();
}



UIManager::~UIManager() {
    SaveRecentDbPaths();
}

void UIManager::AddRecentDbPath(std::string path) {
    recentDbPaths.erase(std::remove(recentDbPaths.begin(), recentDbPaths.end(), path), recentDbPaths.end());
    recentDbPaths.insert(recentDbPaths.begin(), path);
    if (recentDbPaths.size() > MAX_RECENT_PATHS) {
        recentDbPaths.resize(MAX_RECENT_PATHS);
    }
    SaveRecentDbPaths();
}

void UIManager::LoadRecentDbPaths() {
    std::ifstream file(RECENT_PATHS_FILE);
    if (!file.is_open()) return;

    recentDbPaths.clear();
    std::string path;
    while (std::getline(file, path)) {
        if (!path.empty()) {
            recentDbPaths.push_back(path);
        }
    }

    if (recentDbPaths.size() > MAX_RECENT_PATHS) {
        recentDbPaths.resize(MAX_RECENT_PATHS);
    }
}

void UIManager::SaveRecentDbPaths() {
    std::ofstream file(RECENT_PATHS_FILE);
    if (!file.is_open()) return;
    for (const auto& path : recentDbPaths) {
        file << path << std::endl;
    }
}

void UIManager::SetDatabaseManager(DatabaseManager* manager) {
    dbManager = manager;
    for (auto& view : allViews) {
        view->SetDatabaseManager(manager);
    }
}

void UIManager::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
    for (auto& view : allViews) {
        view->SetPdfReporter(reporter);
    }
}

void UIManager::SetImportManager(ImportManager* manager) {
    importManager = manager;
}

void UIManager::SetWindow(GLFWwindow* w) {
    window = w;
}



void UIManager::SetWindowTitle(const std::string& db_path) {
    std::string title = "Financial Audit Application";
    if (!db_path.empty()) {
        title += " - [" + db_path + "]";
    }
    glfwSetWindowTitle(window, title.c_str());
}

void UIManager::HandleFileDialogs() {
    if (ImGuiFileDialog::Instance()->Display("ChooseDbFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            if (dbManager->createDatabase(filePathName)) {
                currentDbPath = filePathName;
                SetWindowTitle(currentDbPath);
                AddRecentDbPath(filePathName);
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("OpenDbFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            if (dbManager->open(filePathName)) {
                currentDbPath = filePathName;
                SetWindowTitle(currentDbPath);
                AddRecentDbPath(filePathName);
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("ImportTsvFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            if (dbManager->is_open()) {
                CreateView<ImportMapView>()->Open(filePathName);
            } else {
                // TODO: Show error message that a database must be open first.
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("SavePdfFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            
            BaseView* focusedView = nullptr;
            for(const auto& view : allViews) {
                if(view->IsVisible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                   // This is tricky. We need to check if the window with the view's title is focused.
                   // A simpler way for now is to find the last-focused window.
                   // Let's find any visible window for now.
                   focusedView = view.get();
                   break;
                }
            }

            if (focusedView) {
                auto data = focusedView->GetDataAsStrings();
                pdfReporter->generatePdfFromTable(filePathName, focusedView->GetTitle(), data.first, data.second);
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
}


void UIManager::Render() {
    // Render all views
    for (auto& view : allViews) {
        view->Render();
    }

    // Remove closed views
    allViews.erase(
        std::remove_if(allViews.begin(), allViews.end(), [](const std::unique_ptr<BaseView>& view) {
            if (!view->IsVisible) {
                view->OnDeactivate();
                return true;
            }
            return false;
        }),
        allViews.end()
    );

    if (isImporting) {
        ImGui::OpenPopup("Importing...");
    }

    if (ImGui::BeginPopupModal("Importing...", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        {
            std::lock_guard<std::mutex> lock(importMutex);
            ImGui::Text("%s", importMessage.c_str());
        }
        ImGui::ProgressBar(importProgress, ImVec2(200, 0));

        if (!isImporting) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

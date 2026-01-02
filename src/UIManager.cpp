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
    : dbManager(nullptr), pdfReporter(nullptr), importManager(nullptr), window(nullptr), activeView(nullptr) {
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
    paymentsView.SetDatabaseManager(manager);
    kosguView.SetDatabaseManager(manager);
    counterpartiesView.SetDatabaseManager(manager);
    contractsView.SetDatabaseManager(manager);
    invoicesView.SetDatabaseManager(manager);
    sqlQueryView.SetDatabaseManager(manager);
    settingsView.SetDatabaseManager(manager);
    importMapView.SetDatabaseManager(manager);
}

void UIManager::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter = reporter;
    paymentsView.SetPdfReporter(reporter);
    kosguView.SetPdfReporter(reporter);
    counterpartiesView.SetPdfReporter(reporter);
    contractsView.SetPdfReporter(reporter);
    invoicesView.SetPdfReporter(reporter);
    sqlQueryView.SetPdfReporter(reporter);
}

void UIManager::SetImportManager(ImportManager* manager) {
    importManager = manager;
}

void UIManager::SetWindow(GLFWwindow* w) {
    window = w;
}

void UIManager::SetActiveView(BaseView* view) {
    activeView = view;
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
                importMapView.Open(filePathName);
            } else {
                // TODO: Show error message that a database must be open first.
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("SavePdfFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            if (activeView) {
                auto data = activeView->GetDataAsStrings();
                pdfReporter->generatePdfFromTable(filePathName, activeView->GetTitle(), data.first, data.second);
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
}


void UIManager::Render() {
    if(paymentsView.IsVisible) activeView = &paymentsView;
    if(kosguView.IsVisible) activeView = &kosguView;
    if(counterpartiesView.IsVisible) activeView = &counterpartiesView;
    if(contractsView.IsVisible) activeView = &contractsView;
    if(invoicesView.IsVisible) activeView = &invoicesView;
    if(sqlQueryView.IsVisible) activeView = &sqlQueryView;
    if(settingsView.IsVisible) activeView = &settingsView;

    paymentsView.Render();
    kosguView.Render();
    counterpartiesView.Render();
    contractsView.Render();
    invoicesView.Render();
    sqlQueryView.Render();
    settingsView.Render();
    importMapView.Render();
}

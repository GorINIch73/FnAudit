#pragma once

#include "imgui.h"
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <memory> // Required for std::unique_ptr
#include <type_traits> // Required for std::is_same_v

#include "Kosgu.h"
#include "DatabaseManager.h"
#include "PdfReporter.h"
#include "views/BaseView.h"
#include "views/PaymentsView.h"
#include "views/KosguView.h"
#include "views/CounterpartiesView.h"
#include "views/ContractsView.h"
#include "views/InvoicesView.h"
#include "views/SqlQueryView.h"
#include "views/SettingsView.h"
#include "views/ImportMapView.h"
#include "views/RegexesView.h"

struct GLFWwindow;
class ImportManager;

class UIManager {
public:
    UIManager();
    ~UIManager();
    void Render();
    void SetDatabaseManager(DatabaseManager* dbManager);
    void SetPdfReporter(PdfReporter* pdfReporter);
    void SetImportManager(ImportManager* importManager);
    void SetWindow(GLFWwindow* window);
    void AddRecentDbPath(std::string path);
    void HandleFileDialogs();
    void SetWindowTitle(const std::string& db_path);

    template<typename T>
    T* CreateView() {
        auto view = std::make_unique<T>();
        T* viewPtr = view.get();
        view->SetDatabaseManager(dbManager);
        view->SetPdfReporter(pdfReporter);

        if constexpr (std::is_same_v<T, ImportMapView>) {
            viewPtr->SetUIManager(this);
        }

        std::string title = std::string(view->GetTitle());
        // Only add a unique ID if it's not a singleton view like Settings or ImportMap
        if constexpr (!std::is_same_v<T, SettingsView> && !std::is_same_v<T, ImportMapView>) {
             title += "###" + std::to_string(viewIdCounter++);
        }
        view->SetTitle(title);
        view->IsVisible = true;
        allViews.push_back(std::move(view));
        return viewPtr;
    }

    std::vector<std::string> recentDbPaths;
    std::string currentDbPath;

    std::atomic<bool> isImporting{false};
    std::atomic<float> importProgress{0.0f};
    std::string importMessage;
    std::mutex importMutex;

    ImportManager* importManager;
    std::vector<std::unique_ptr<BaseView>> allViews;

private:
    void LoadRecentDbPaths();
    void SaveRecentDbPaths();

    DatabaseManager* dbManager;
    PdfReporter* pdfReporter;
    GLFWwindow* window;
    int viewIdCounter = 0;
};

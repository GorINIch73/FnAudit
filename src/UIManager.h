#pragma once

#include "imgui.h"
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
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
    void SetActiveView(BaseView* view);

    std::vector<std::string> recentDbPaths;
    std::string currentDbPath;

    std::atomic<bool> isImporting{false};
    std::atomic<float> importProgress{0.0f};
    std::string importMessage;
    std::mutex importMutex;

    PaymentsView paymentsView;
    KosguView kosguView;
    CounterpartiesView counterpartiesView;
    ContractsView contractsView;
    InvoicesView invoicesView;
    SqlQueryView sqlQueryView;
    SettingsView settingsView;
    ImportMapView importMapView;
    ImportManager* importManager;
    BaseView* activeView = nullptr;

private:
    void LoadRecentDbPaths();
    void SaveRecentDbPaths();

    DatabaseManager* dbManager;
    PdfReporter* pdfReporter;
    GLFWwindow* window;
};

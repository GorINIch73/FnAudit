#pragma once

#include "imgui.h"
#include <vector>
#include <string>
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

    PaymentsView paymentsView;
    KosguView kosguView;
    CounterpartiesView counterpartiesView;
    ContractsView contractsView;
    InvoicesView invoicesView;
    SqlQueryView sqlQueryView;
    SettingsView settingsView;
    ImportMapView importMapView;
    BaseView* activeView = nullptr;

private:
    void LoadRecentDbPaths();
    void SaveRecentDbPaths();

    DatabaseManager* dbManager;
    PdfReporter* pdfReporter;
    ImportManager* importManager;
    GLFWwindow* window;
};

#pragma once

#include "BaseView.h"
#include "../ImportManager.h"
#include <vector>
#include <string>

class UIManager;
class ExportManager;

class ServiceView : public BaseView {
public:
    ServiceView();
    void Render() override;

    void SetDatabaseManager(DatabaseManager* manager) override { dbManager = manager; }
    void SetPdfReporter(PdfReporter* reporter) override { /* Not used */ }
    void SetUIManager(UIManager* manager) override;
    void SetExportManager(ExportManager* manager);
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;

private:
    void Reset();

    UIManager* uiManager = nullptr;
    ExportManager* exportManager = nullptr;
    
    // For IKZ Import
    std::vector<UnfoundContract> m_unfoundContracts;
    int m_successfulImports = 0;
    bool m_ikzImportStarted = false;
    bool m_showUnfoundContracts = false;

    // For Contract Export
    int m_lastExportCount = -1;
};

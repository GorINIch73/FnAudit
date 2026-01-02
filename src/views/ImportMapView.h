#pragma once

#include "BaseView.h"
#include <string>
#include <vector>
#include <map>

// Forward declaration
class UIManager;

// Represents the mapping from a target field name to the index of the column in the source file.
using ColumnMapping = std::map<std::string, int>;

class ImportMapView : public BaseView {
public:
    ImportMapView();
    void Render() override;

    // Open the mapping window for a specific file
    void Open(const std::string& filePath);

    // Implementing pure virtual functions from BaseView
    void SetDatabaseManager(DatabaseManager* manager) override { dbManager = manager; }
    void SetPdfReporter(PdfReporter* reporter) override { /* Not used */ }
    void SetUIManager(UIManager* manager);
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override { return {}; }
    const char* GetTitle() override { return Title.c_str(); }

private:
    void Reset();
    void ReadPreviewData();

    UIManager* uiManager = nullptr;
    std::string importFilePath;
    std::vector<std::string> fileHeaders;
    std::vector<std::vector<std::string>> sampleData; // To store first few rows for preview
    std::vector<std::string> targetFields;
    std::map<std::string, int> currentMapping; // Maps target field to file header index
};

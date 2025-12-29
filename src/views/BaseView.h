#pragma once

#include "imgui.h"
#include "../DatabaseManager.h"
#include "../PdfReporter.h"
#include <vector>
#include <string>
#include <utility>

class BaseView {
public:
    virtual ~BaseView() = default;
    virtual void Render() = 0;
    virtual void SetDatabaseManager(DatabaseManager* dbManager) = 0;
    virtual void SetPdfReporter(PdfReporter* pdfReporter) = 0;
    virtual std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() = 0;
    virtual const char* GetTitle() = 0;

    bool IsVisible = false;
};

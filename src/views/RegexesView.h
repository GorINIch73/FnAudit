#pragma once

#include "BaseView.h"
#include <vector>
#include "../Regex.h"

class RegexesView : public BaseView {
public:
    RegexesView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
    void OnDeactivate() override;

private:
    void RefreshData();
    void SaveChanges();

    std::vector<Regex> regexes;
    Regex selectedRegex;
    Regex originalRegex;
    int selectedRegexIndex;
    bool isAdding;
    bool isDirty = false;
    char filterText[256];
};

#pragma once

#include "BaseView.h"
#include <vector>
#include "../Kosgu.h"

class KosguView : public BaseView {
public:
    KosguView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;

private:
    void RefreshData();

    DatabaseManager* dbManager;
    PdfReporter* pdfReporter;

    std::vector<Kosgu> kosguEntries;
    Kosgu selectedKosgu;
    int selectedKosguIndex;
    bool showEditModal;
    bool isAdding;
    char filterText[256];
};

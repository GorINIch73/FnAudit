#pragma once

#include "BaseView.h"
#include <vector>
#include "../Counterparty.h"

class CounterpartiesView : public BaseView {
public:
    CounterpartiesView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
    const char* GetTitle() override;

private:
    void RefreshData();

    DatabaseManager* dbManager;
    PdfReporter* pdfReporter;

    std::vector<Counterparty> counterparties;
    Counterparty selectedCounterparty;
    int selectedCounterpartyIndex;
    bool showEditModal;
    bool isAdding;
    char filterText[256];
};

#pragma once

#include "BaseView.h"
#include <vector>
#include "../Invoice.h"
#include "../Contract.h"

class InvoicesView : public BaseView {
public:
    InvoicesView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;

private:
    void RefreshData();
    void RefreshDropdownData();

    DatabaseManager* dbManager;
    PdfReporter* pdfReporter;

    std::vector<Invoice> invoices;
    Invoice selectedInvoice;
    int selectedInvoiceIndex;
    bool showEditModal;
    bool isAdding;

    std::vector<Contract> contractsForDropdown;
    char filterText[256];
};

#pragma once

#include "BaseView.h"
#include <vector>
#include "../Invoice.h"
#include "../Contract.h"
#include "../Payment.h"

class InvoicesView : public BaseView {
public:
    InvoicesView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
    const char* GetTitle() override;

private:
    void RefreshData();
    void RefreshDropdownData();

    std::vector<Invoice> invoices;
    Invoice selectedInvoice;
    int selectedInvoiceIndex;
    bool showEditModal;
    bool isAdding;

    std::vector<ContractPaymentInfo> payment_info;
    std::vector<Contract> contractsForDropdown;
    char filterText[256];
    char contractFilter[256];
    float list_view_height = 200.0f;
    float editor_width = 400.0f;
};

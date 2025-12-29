#pragma once

#include "BaseView.h"
#include <vector>
#include "../Payment.h"
#include "../Counterparty.h"
#include "../Kosgu.h"
#include "../PaymentDetail.h"
#include "../Contract.h"
#include "../Invoice.h"

class PaymentsView : public BaseView {
public:
    PaymentsView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
    const char* GetTitle() override;

private:
    void RefreshData();
    void RefreshDropdownData();

    DatabaseManager* dbManager;
    PdfReporter* pdfReporter;

    std::vector<Payment> payments;
    Payment selectedPayment;
    int selectedPaymentIndex;
    bool isAdding;

    std::vector<PaymentDetail> paymentDetails;
    PaymentDetail selectedDetail;
    int selectedDetailIndex;
    bool isAddingDetail;

    std::vector<Counterparty> counterpartiesForDropdown;
    std::vector<Kosgu> kosguForDropdown;
    std::vector<Contract> contractsForDropdown;
    std::vector<Invoice> invoicesForDropdown;
    char filterText[256];
};

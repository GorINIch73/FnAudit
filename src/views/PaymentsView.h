#pragma once

#include "BaseView.h"
#include <vector>
#include <string>
#include "../Payment.h"
#include "../Counterparty.h"
#include "../Kosgu.h"
#include "../PaymentDetail.h"
#include "../Contract.h"
#include "../Invoice.h"
#include "imgui.h"
#include "CustomWidgets.h"

class PaymentsView : public BaseView {
public:
    PaymentsView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
    void OnDeactivate() override;

private:
    void RefreshData();
    void RefreshDropdownData();
    void SaveChanges();
    void SaveDetailChanges();

    std::vector<Payment> payments;
    Payment selectedPayment;
    Payment originalPayment;
    int selectedPaymentIndex;
    bool isAdding;
    bool isDirty = false;

    std::string descriptionBuffer;

    std::vector<PaymentDetail> paymentDetails;
    PaymentDetail selectedDetail;
    PaymentDetail originalDetail;
    int selectedDetailIndex;
    bool isAddingDetail;
    bool isDetailDirty = false;

    std::vector<Counterparty> counterpartiesForDropdown;
    std::vector<Kosgu> kosguForDropdown;
    std::vector<Contract> contractsForDropdown;
    std::vector<Invoice> invoicesForDropdown;
    char filterText[256];
    char counterpartyFilter[256];
    char kosguFilter[256];
    char contractFilter[256];
    char invoiceFilter[256];

    // Group operations
    int groupKosguId = -1;
    int groupContractId = -1;
    int groupInvoiceId = -1;
    char groupKosguFilter[256];
    char groupContractFilter[256];
    char groupInvoiceFilter[256];

    float list_view_height = 200.0f;
    float editor_width = 400.0f;
};

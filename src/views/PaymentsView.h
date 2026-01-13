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

    // Bulk replace operations
    bool show_replace_popup = false;
    bool show_add_kosgu_popup = false;
    int replacement_target = 0; // 0 for KOSGU, 1 for Contract, 2 for Invoice
    int replacement_kosgu_id = -1;
    int replacement_contract_id = -1;
    int replacement_invoice_id = -1;
    char replacement_kosgu_filter[256]{};
    char replacement_contract_filter[256]{};
    char replacement_invoice_filter[256]{};

    // For delete confirmation popups
    bool show_delete_payment_popup = false;
    bool show_delete_detail_popup = false;
    bool show_group_delete_popup = false;
    int payment_id_to_delete = -1;
    int detail_id_to_delete = -1;

    // For chunked group operations with progress bar
    enum GroupOperationType {
        NONE,
        ADD_KOSGU,
        REPLACE,
        DELETE_DETAILS
    };
    GroupOperationType current_operation = NONE;
    int processed_items = 0;
    std::vector<Payment> items_to_process;
    void ProcessGroupOperation();

    float list_view_height = 200.0f;
    float editor_width = 400.0f;
};

#pragma once

#include "imgui.h"
#include <vector>
#include "Kosgu.h"
#include "DatabaseManager.h"
#include "PdfReporter.h"

class UIManager {
public:
    UIManager();
    void Render();
    void SetDatabaseManager(DatabaseManager* dbManager);
    void SetPdfReporter(PdfReporter* pdfReporter);

    bool ShowKosguWindow;
    bool ShowSqlQueryWindow;
    bool ShowPaymentsWindow;
    bool ShowCounterpartiesWindow;
    bool ShowContractsWindow;
    bool ShowInvoicesWindow;

private:
    void RenderKosguWindow();
    void RefreshKosguData();
    void RenderSqlQueryWindow();
    void RenderPaymentsWindow();
    void RefreshPaymentsData();
    void RenderCounterpartiesWindow();
    void RefreshCounterpartiesData();
    void RenderContractsWindow();
    void RefreshContractsData();
    void RenderInvoicesWindow();
    void RefreshInvoicesData();

    DatabaseManager* dbManager;
    std::vector<Kosgu> kosguEntries;
    Kosgu selectedKosgu;
    int selectedKosguIndex;

    // Payments window
    std::vector<Payment> payments;
    Payment selectedPayment;
    int selectedPaymentIndex;
    bool isAddingPayment;

    // Counterparties window
    std::vector<Counterparty> counterparties;
    Counterparty selectedCounterparty;
    int selectedCounterpartyIndex;
    bool showEditCounterpartyModal;
    bool isAddingCounterparty;

    // Contracts window
    std::vector<Contract> contracts;
    Contract selectedContract;
    int selectedContractIndex;
    bool showEditContractModal;
    bool isAddingContract;

    // Invoices window
    std::vector<Invoice> invoices;
    Invoice selectedInvoice;
    int selectedInvoiceIndex;
    bool showEditInvoiceModal;
    bool isAddingInvoice;

    bool showEditKosguModal;
    bool isAddingKosgu;

    PdfReporter* pdfReporter; // Add PdfReporter pointer

    // For SQL Query Window
    char queryInputBuffer[4096];
    struct QueryResult {
        std::vector<std::string> columns;
        std::vector<std::vector<std::string>> rows;
    } queryResult;
};

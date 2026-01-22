#pragma once

#include "BaseView.h"
#include <vector>
#include <map> // Added for std::map
#include "../Contract.h"
#include "../Counterparty.h"
#include "../Payment.h"

class ContractsView : public BaseView {
public:
    ContractsView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
    void OnDeactivate() override;

private:
    void RefreshData();
    void RefreshDropdownData();
    void SaveChanges();

    std::vector<Contract> contracts;
    Contract selectedContract;
    Contract originalContract;
    int selectedContractIndex;
    bool showEditModal;
    bool isAdding;
    bool isDirty = false;
    bool show_delete_popup = false;
    int contract_id_to_delete = -1;

    std::vector<ContractPaymentInfo> payment_info;
    std::vector<Counterparty> counterpartiesForDropdown;
    char filterText[256];
    char counterpartyFilter[256];
    float list_view_height = 200.0f;
    float editor_width = 400.0f;

    int contract_filter_index = 0;
    std::vector<SuspiciousWord> suspiciousWordsForFilter;

    std::vector<Contract> m_filtered_contracts;
    void UpdateFilteredContracts();
    std::map<int, std::vector<ContractPaymentInfo>> m_contract_details_map;
};

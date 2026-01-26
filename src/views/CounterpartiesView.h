#pragma once

#include "BaseView.h"
#include <vector>
#include "../Counterparty.h"
#include "../Payment.h"

class CounterpartiesView : public BaseView {
public:
    CounterpartiesView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;
    void SetUIManager(UIManager* manager) override;
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
    void OnDeactivate() override;

private:
    void RefreshData();
    void SaveChanges();

    std::vector<Counterparty> counterparties;
    Counterparty selectedCounterparty;
    Counterparty originalCounterparty;
    int selectedCounterpartyIndex;
    bool showEditModal;
    bool isAdding;
    bool isDirty = false;
    bool show_delete_popup = false;
    int counterparty_id_to_delete = -1;
    std::vector<ContractPaymentInfo> payment_info;
    char filterText[256];
    float list_view_height = 200.0f;
    float editor_width = 400.0f;
};

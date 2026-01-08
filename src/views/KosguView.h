#pragma once

#include "BaseView.h"
#include <vector>
#include "../Kosgu.h"
#include "../Payment.h"

class KosguView : public BaseView {
public:
    KosguView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
    void OnDeactivate() override;

private:
    void RefreshData();
    void SaveChanges();

    std::vector<Kosgu> kosguEntries;
    Kosgu selectedKosgu;
    Kosgu originalKosgu;
    int selectedKosguIndex;
    bool showEditModal;
    bool isAdding;
    bool isDirty = false;
    std::vector<ContractPaymentInfo> payment_info;
    char filterText[256];
    float list_view_height = 200.0f;
    float editor_width = 400.0f;
};

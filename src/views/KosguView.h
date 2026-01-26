#pragma once

#include "BaseView.h"
#include <vector>
#include "../Kosgu.h"
#include "../Payment.h"
#include "../SuspiciousWord.h"

class KosguView : public BaseView {
public:
    KosguView();
    void Render() override;
    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* pdfReporter) override;
    void SetUIManager(UIManager* manager) override;
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
    bool show_delete_popup = false;
    int kosgu_id_to_delete = -1;
    std::vector<ContractPaymentInfo> payment_info;
    char filterText[256];
    
    std::vector<Kosgu> m_filtered_kosgu_entries;
    void UpdateFilteredKosgu();
    int m_filter_index = 0;
    std::vector<SuspiciousWord> suspiciousWordsForFilter;

    float list_view_height = 200.0f;
    float editor_width = 400.0f;
};

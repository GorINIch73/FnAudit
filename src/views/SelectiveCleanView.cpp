#include "views/SelectiveCleanView.h"
#include "imgui.h"
#include <string>

void SelectiveCleanView::Render() {
    if (!IsVisible) {
        return;
    }

    ImGui::Begin(GetTitle(), &IsVisible);

    ImGui::TextWrapped("Эта форма позволяет выборочно удалять данные из базы. Эти операции необратимы. Рекомендуется создать резервную копию файла базы данных перед продолжением.");
    ImGui::Separator();

    ImGui::Text("Очистка основных таблиц:");
    if (ImGui::Button(ICON_FA_MONEY_BILL " Удалить все платежи и расшифровки")) {
        currentCleanTarget = CleanTarget::Payments;
        confirmationMessage = "Вы уверены, что хотите удалить ВСЕ платежи и связанные с ними расшифровки?";
        ImGui::OpenPopup("ConfirmationModal");
    }

    if (ImGui::Button(ICON_FA_ADDRESS_BOOK " Удалить всех контрагентов")) {
        currentCleanTarget = CleanTarget::Counterparties;
        confirmationMessage = "Вы уверены, что хотите удалить ВСЕХ контрагентов? Это может нарушить целостность данных, если существуют связанные договоры или платежи.";
        ImGui::OpenPopup("ConfirmationModal");
    }

    if (ImGui::Button(ICON_FA_FILE_CONTRACT " Удалить все договоры")) {
        currentCleanTarget = CleanTarget::Contracts;
        confirmationMessage = "Вы уверены, что хотите удалить ВСЕ договоры? Это может нарушить целостность данных, если существуют связанные платежи.";
        ImGui::OpenPopup("ConfirmationModal");
    }

    if (ImGui::Button(ICON_FA_FILE_INVOICE " Удалить все накладные")) {
        currentCleanTarget = CleanTarget::Invoices;
        confirmationMessage = "Вы уверены, что хотите удалить ВСЕ накладные? Это может нарушить целостность данных, если существуют связанные платежи.";
        ImGui::OpenPopup("ConfirmationModal");
    }
    
    ImGui::Separator();
    ImGui::Text("Обслуживание связей:");
    if (ImGui::Button(ICON_FA_BROOM " Очистить 'повисшие' расшифровки")) {
        currentCleanTarget = CleanTarget::OrphanDetails;
        confirmationMessage = "Будет выполнен поиск и удаление записей в таблице расшифровок, которые не связаны ни с одним платежом. Продолжить?";
        ImGui::OpenPopup("ConfirmationModal");
    }

    if (!resultMessage.empty()) {
        ImGui::Separator();
        ImGui::Text("%s", resultMessage.c_str());
    }

    ShowConfirmationModal();

    ImGui::End();
}

void SelectiveCleanView::ShowConfirmationModal() {
    if (ImGui::BeginPopupModal("ConfirmationModal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", confirmationMessage.c_str());
        ImGui::Separator();
        if (ImGui::Button("Да, я уверен", ImVec2(120, 0))) {
            bool success = false;
            if (dbManager) {
                switch (currentCleanTarget) {
                    case CleanTarget::Payments:       success = dbManager->ClearPayments(); break;
                    case CleanTarget::Counterparties: success = dbManager->ClearCounterparties(); break;
                    case CleanTarget::Contracts:      success = dbManager->ClearContracts(); break;
                    case CleanTarget::Invoices:       success = dbManager->ClearInvoices(); break;
                    case CleanTarget::OrphanDetails:  success = dbManager->CleanOrphanPaymentDetails(); break;
                    case CleanTarget::None: break;
                }
            }
            if (success) {
                resultMessage = "Операция выполнена успешно.";
            } else {
                resultMessage = "Ошибка при выполнении операции.";
            }
            currentCleanTarget = CleanTarget::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(120, 0))) {
            resultMessage = "Операция отменена.";
            currentCleanTarget = CleanTarget::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SelectiveCleanView::SetDatabaseManager(DatabaseManager* manager) {
    this->dbManager = manager;
}

void SelectiveCleanView::SetUIManager(UIManager* manager) {
    this->uiManager = manager;
}

void SelectiveCleanView::SetPdfReporter(PdfReporter* reporter) {
    this->pdfReporter = reporter;
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> SelectiveCleanView::GetDataAsStrings() {
    // This view doesn't present tabular data for export, so return empty.
    return {};
}

#include "SettingsView.h"
#include "../CustomWidgets.h"
#include "imgui.h"
#include <iostream>

SettingsView::SettingsView() {
    Title = "Настройки";
    IsVisible = false;
}

void SettingsView::LoadSettings() {
    if (dbManager) {
        currentSettings = dbManager->getSettings();
    }
}

void SettingsView::SaveSettings() {
    if (dbManager) {
        if (dbManager->updateSettings(currentSettings)) {
            std::cout << "DEBUG: Settings saved successfully." << std::endl;
        } else {
            std::cout << "ERROR: Failed to save settings." << std::endl;
        }
    }
}

void SettingsView::Render() {
    if (!IsVisible) {
        return;
    }

    if (ImGui::Begin(Title.c_str(), &IsVisible)) {
        // Load settings only once when the view becomes visible
        if (ImGui::IsWindowAppearing()) {
            LoadSettings();
        }

        CustomWidgets::InputText("Название организации", &currentSettings.organization_name);
        CustomWidgets::InputText("Дата начала периода", &currentSettings.period_start_date);
        CustomWidgets::InputText("Дата окончания периода", &currentSettings.period_end_date);
        CustomWidgets::InputTextMultilineWithWrap("Примечание", &currentSettings.note, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4));
        ImGui::InputInt("Строк предпросмотра", &currentSettings.import_preview_lines);

        if (ImGui::Button("Сохранить")) {
            SaveSettings();
            IsVisible = false; // Close window on save
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) {
            // Revert changes by reloading from DB
            LoadSettings();
            IsVisible = false; // Close window on cancel
        }
    }
    ImGui::End();
}

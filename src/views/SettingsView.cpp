#include "SettingsView.h"
#include "imgui.h"
#include <cstring> // For strncpy
#include <iostream>

SettingsView::SettingsView() {
    Title = "Настройки";
    IsVisible = false;
    memset(organization_name_buf, 0, sizeof(organization_name_buf));
    memset(start_date_buf, 0, sizeof(start_date_buf));
    memset(end_date_buf, 0, sizeof(end_date_buf));
    memset(note_buf, 0, sizeof(note_buf));
}

void SettingsView::LoadSettings() {
    if (dbManager) {
        currentSettings = dbManager->getSettings();
        strncpy(organization_name_buf, currentSettings.organization_name.c_str(), sizeof(organization_name_buf) - 1);
        strncpy(start_date_buf, currentSettings.period_start_date.c_str(), sizeof(start_date_buf) - 1);
        strncpy(end_date_buf, currentSettings.period_end_date.c_str(), sizeof(end_date_buf) - 1);
        strncpy(note_buf, currentSettings.note.c_str(), sizeof(note_buf) - 1);
        import_preview_lines_buf = currentSettings.import_preview_lines;
    }
}

void SettingsView::SaveSettings() {
    if (dbManager) {
        currentSettings.organization_name = organization_name_buf;
        currentSettings.period_start_date = start_date_buf;
        currentSettings.period_end_date = end_date_buf;
        currentSettings.note = note_buf;
        currentSettings.import_preview_lines = import_preview_lines_buf;
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

        ImGui::InputText("Название организации", organization_name_buf, sizeof(organization_name_buf));
        ImGui::InputText("Дата начала периода", start_date_buf, sizeof(start_date_buf));
        ImGui::InputText("Дата окончания периода", end_date_buf, sizeof(end_date_buf));
        ImGui::InputTextMultiline("Примечание", note_buf, sizeof(note_buf));
        ImGui::InputInt("Строк предпросмотра", &import_preview_lines_buf);

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

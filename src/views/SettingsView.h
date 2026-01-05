#pragma once

#include "BaseView.h"
#include "../Settings.h"

class SettingsView : public BaseView {
public:
    SettingsView();
    void Render() override;

    // Implementing pure virtual functions from BaseView
    void SetDatabaseManager(DatabaseManager* manager) override { dbManager = manager; }
    void SetPdfReporter(PdfReporter* reporter) override { /* Not used in this view */ }
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override { return {}; }
    const char* GetTitle() override { return Title.c_str(); }

private:
    void LoadSettings();
    void SaveSettings();

    Settings currentSettings;
    char organization_name_buf[256];
    char start_date_buf[12]; // YYYY-MM-DD
    char end_date_buf[12];   // YYYY-MM-DD
    char note_buf[512];
    int import_preview_lines_buf;
};

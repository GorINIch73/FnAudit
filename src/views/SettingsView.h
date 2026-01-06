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
};

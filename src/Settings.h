#pragma once

#include <string>

struct Settings {
    int id;
    std::string organization_name;
    std::string period_start_date;
    std::string period_end_date;
    std::string note;
    int import_preview_lines;
    int theme = 0; // 0: Dark, 1: Light, 2: Classic
    int font_size = 24;
};

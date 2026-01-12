#pragma once

#include <string>

struct Contract {
    int id = -1;
    std::string number;
    std::string date; // Format YYYY-MM-DD
    int counterparty_id = -1;
    double total_amount = 0.0;
};

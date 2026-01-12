#pragma once

#include <string>

struct Counterparty {
    int id = -1;
    std::string name;
    std::string inn; // Individual Taxpayer Identification Number
    double total_amount = 0.0;
};

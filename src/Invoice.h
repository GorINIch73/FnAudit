#pragma once

#include <string>

struct Invoice {
    int id = -1;
    std::string number;
    std::string date;
    int contract_id = -1;
    double total_amount = 0.0;
};

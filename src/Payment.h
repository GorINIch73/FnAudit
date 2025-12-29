#pragma once

#include <string>

struct Payment {
    int id;
    std::string date;
    std::string doc_number;
    std::string type;
    double amount;
    std::string payer;
    std::string recipient;
    std::string description;
    int counterparty_id;
};

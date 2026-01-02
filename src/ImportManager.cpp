#include "ImportManager.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <regex>
#include <iostream>

ImportManager::ImportManager() {}

// Helper to split a string by a delimiter
static std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Helper to trim whitespace and quotes
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r\"");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\n\r\"");
    return str.substr(first, (last - first + 1));
}

// Helper to safely get a value from a row based on the mapping
static std::string get_value_from_row(const std::vector<std::string>& row, const ColumnMapping& mapping, const std::string& field_name) {
    auto it = mapping.find(field_name);
    if (it == mapping.end() || it->second == -1) {
        return ""; // Not mapped
    }
    int col_index = it->second;
    if (col_index >= row.size()) {
        return ""; // Index out of bounds
    }
    return trim(row[col_index]);
}


// Helper function to convert DD.MM.YY or DD.MM.YYYY to YYYY-MM-DD
static std::string convertDateToDBFormat(const std::string& date_str) {
    if (date_str.length() == 10 && date_str[2] == '.' && date_str[5] == '.') { // DD.MM.YYYY
        return date_str.substr(6, 4) + "-" + date_str.substr(3, 2) + "-" + date_str.substr(0, 2);
    } else if (date_str.length() == 8 && date_str[2] == '.' && date_str[5] == '.') { // DD.MM.YY
        std::string year_short = date_str.substr(6, 2);
        int year_int = std::stoi(year_short);
        std::string full_year = (year_int > 50) ? "19" + year_short : "20" + year_short; // Heuristic
        return full_year + "-" + date_str.substr(3, 2) + "-" + date_str.substr(0, 2);
    }
    return date_str; // Return as is if format is unexpected
}

bool ImportManager::ImportPaymentsFromTsv(const std::string& filepath, DatabaseManager* dbManager, const ColumnMapping& mapping) {
    if (!dbManager) {
        std::cerr << "DatabaseManager is null." << std::endl;
        return false;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open TSV file: " << filepath << std::endl;
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header line

    std::regex contract_regex("(?:по контракту|по контр|Контракт|дог\\.|К-т)(?: №)?\\s*([^\\s,]+)\\s*(?:от\\s*)?(\\d{2}\\.\\d{2}\\.(\\d{2}|\\d{4}))");
    std::regex invoice_regex("(?:акт|сч\\.?|сч-ф|счет на оплату|№)\\s*([^\\s,]+)\\s*от\\s*(\\d{2}\\.\\d{2}\\.(\\d{2}|\\d{4}))");
    std::regex kosgu_regex("К(\\d{3})");
    std::regex amount_regex("\\((\\d{3}-\\d{4}-\\d{10}-\\d{3}):\\s*([\\d=,]+)\\s*ЛС\\)");


    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::vector<std::string> row = split(line, '\t');
        Payment payment;

        payment.date = convertDateToDBFormat(get_value_from_row(row, mapping, "Дата"));
        payment.doc_number = get_value_from_row(row, mapping, "Номер док.");
        payment.type = get_value_from_row(row, mapping, "Тип");
        payment.payer = get_value_from_row(row, mapping, "Плательщик");
        payment.recipient = get_value_from_row(row, mapping, "Получатель");
        payment.description = get_value_from_row(row, mapping, "Назначение");

        try {
            std::string amount_str = get_value_from_row(row, mapping, "Сумма");
            std::replace(amount_str.begin(), amount_str.end(), ',', '.');
            payment.amount = std::stod(amount_str);
        } catch (const std::exception&) {
            payment.amount = 0.0;
        }

        // --- Heuristic to determine payment type if not provided ---
        if (payment.type.empty()) {
            if (!payment.payer.empty()) {
                payment.type = "income";
            } else if (!payment.recipient.empty()) {
                payment.type = "expense";
            } else {
                payment.type = "unknown";
            }
        }

        if (payment.date.empty() && payment.amount == 0.0) {
            continue; // Skip empty/invalid rows
        }

        // --- Existing logic for parsing description and handling counterparties ---
        Counterparty counterparty;
        if(payment.type == "income") {
            counterparty.name = payment.payer;
        } else {
            counterparty.name = payment.recipient;
        }

        int counterparty_id = -1;
        if (!counterparty.name.empty()) {
            counterparty_id = dbManager->getCounterpartyIdByName(counterparty.name);
            if (counterparty_id == -1) {
                if (dbManager->addCounterparty(counterparty)) {
                    counterparty_id = counterparty.id;
                }
            }
        }
        payment.counterparty_id = counterparty_id;

        int current_contract_id = -1;
        std::smatch contract_matches;
        if (std::regex_search(payment.description, contract_matches, contract_regex)) {
            if (contract_matches.size() >= 3) {
                std::string contract_number = contract_matches[1].str();
                std::string contract_date_db_format = convertDateToDBFormat(contract_matches[2].str());
                current_contract_id = dbManager->getContractIdByNumberDate(contract_number, contract_date_db_format);
                if (current_contract_id == -1) {
                    Contract contract_obj{-1, contract_number, contract_date_db_format, counterparty_id};
                    if (dbManager->addContract(contract_obj)) {
                        current_contract_id = contract_obj.id;
                    }
                }
            }
        }

        int current_invoice_id = -1;
        std::smatch invoice_matches;
        if (std::regex_search(payment.description, invoice_matches, invoice_regex)) {
            if (invoice_matches.size() >= 3) {
                std::string invoice_number = invoice_matches[1].str();
                std::string invoice_date_db_format = convertDateToDBFormat(invoice_matches[2].str());
                current_invoice_id = dbManager->getInvoiceIdByNumberDate(invoice_number, invoice_date_db_format);
                if (current_invoice_id == -1) {
                    Invoice invoice_obj{-1, invoice_number, invoice_date_db_format, current_contract_id};
                    if (dbManager->addInvoice(invoice_obj)) {
                        current_invoice_id = invoice_obj.id;
                    }
                }
            }
        }

        if (!dbManager->addPayment(payment)) {
            continue;
        }
        int new_payment_id = payment.id;
        
        std::sregex_iterator kosgu_begin(payment.description.begin(), payment.description.end(), kosgu_regex);
        std::sregex_iterator kosgu_end;

        if (std::distance(kosgu_begin, kosgu_end) == 0) {
            PaymentDetail detail;
            detail.payment_id = new_payment_id;
            detail.kosgu_id = -1;
            detail.contract_id = current_contract_id;
            detail.invoice_id = current_invoice_id;
            detail.amount = payment.amount;
            dbManager->addPaymentDetail(detail);
        } else {
            for (std::sregex_iterator i = kosgu_begin; i != kosgu_end; ++i) {
                std::smatch match = *i;
                if (match.size() > 1) {
                    std::string kosgu_code = match[1].str();
                    int kosgu_id = dbManager->getKosguIdByCode(kosgu_code);
                     if (kosgu_id == -1) {
                        Kosgu new_kosgu { -1, kosgu_code, "КОСГУ " + kosgu_code };
                        if (dbManager->addKosguEntry(new_kosgu)) {
                            kosgu_id = dbManager->getKosguIdByCode(kosgu_code);
                        }
                    }

                    double detail_amount = payment.amount;
                    std::smatch amount_match;
                    if(std::regex_search(payment.description, amount_match, amount_regex)){
                         if(amount_match.size() > 2){
                             std::string amount_str = amount_match[2].str();
                             size_t eq_pos = amount_str.find('=');
                             if(eq_pos != std::string::npos){
                                 amount_str = amount_str.substr(0, eq_pos);
                             }
                             std::replace(amount_str.begin(), amount_str.end(), ',', '.');
                             try{
                                 detail_amount = std::stod(amount_str);
                             } catch(const std::exception& e){}
                         }
                    }

                    PaymentDetail detail;
                    detail.payment_id = new_payment_id;
                    detail.kosgu_id = kosgu_id;
                    detail.contract_id = current_contract_id;
                    detail.invoice_id = current_invoice_id;
                    detail.amount = detail_amount;
                    
                    dbManager->addPaymentDetail(detail);
                }
            }
        }
    }

    file.close();
    return true;
}
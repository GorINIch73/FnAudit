#include "ImportManager.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm> // For std::remove
#include <regex>     // For regex parsing
#include <iostream>  // For std::cerr

ImportManager::ImportManager() {}

// Helper function to trim whitespace and quotes from a string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r\"");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r\"");
    return str.substr(first, (last - first + 1));
}

// Dummy implementation for parsing a single line of TSV data into a Payment object
// This is a placeholder and needs to be adapted to the actual TSV format
Payment ImportManager::parsePaymentLine(const std::string& line) {
    Payment payment;
    std::stringstream ss(line);
    std::string token;
    std::vector<std::string> tokens;

    // Split by tab delimiter
    while (std::getline(ss, token, '\t')) {
        tokens.push_back(trim(token));
    }

    // Actual TSV column mapping from платежки 2024г.tsv:
    // 0: Состояние (Status)
    // 1: Номер документа (Document Number)
    // 2: Дата документа (Document Date)
    // 3: Сумма (Amount)
    // 4: Л/с в ФО (Account in FO) - Ignored for payment object
    // 5: Наименование (Name) - Payer/Recipient Name
    // 6: Назначение платежа (Payment Purpose) - Description, where Contract/Invoice info is
    // 7: Тип БК и направление (Type BK and Direction) - Used to confirm payment type
    // 8: Дата принятия (Acceptance Date) - Ignored for payment object

    if (tokens.size() >= 9) {
        // --- Map basic payment details ---
        std::string status = tokens[0]; // "Принят" or "Расход"
        if (status == "Принят") {
            payment.type = "income";
        } else if (status == "Расход"){ // Corrected from "Расходная (платеж)" as token[0] contains "Расход"
             payment.type = "expense";
        } else {
             payment.type = "unknown";
        }

        payment.doc_number = tokens[1];
        payment.date = tokens[2];
        try {
            std::string amount_str = tokens[3];
            std::replace(amount_str.begin(), amount_str.end(), ',', '.');
            payment.amount = std::stod(amount_str);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing amount: " << e.what() << " for value " << tokens[3] << std::endl;
            payment.amount = 0.0;
        }
        
        // --- Determine Payer/Recipient Name based on payment type ---
        // For income, 'Наименование' (tokens[5]) is the Payer.
        // For expense, 'Наименование' (tokens[5]) is the Recipient.
        if (payment.type == "income") {
            payment.payer = tokens[5];
            payment.recipient = ""; // No explicit recipient from TSV for income
        } else if (payment.type == "expense") {
            payment.payer = "";     // No explicit payer from TSV for expense
            payment.recipient = tokens[5];
        }

        payment.description = tokens[6]; // This is 'Назначение платежа'

        // --- Extract INN from description using regex ---
        // Example Russian INN: 10 or 12 digits
        // std::regex inn_regex("\\b(\\d{10}|\\d{12})\\b"); // 10 or 12 digits
        // std::smatch inn_matches;
        // if (std::regex_search(payment.description, inn_matches, inn_regex)) {
        //     if (inn_matches.size() > 1) {
        //         // Assign INN based on payment type for consistency, if found in description
        //         if (payment.type == "income") {
        //             payment.payer_inn = inn_matches[1].str();
        //         } else if (payment.type == "expense") {
        //             payment.recipient_inn = inn_matches[1].str();
        //         }
        //     }
        // }
    } else {
        std::cerr << "Warning: TSV line has fewer than 9 columns: " << line << std::endl;
    }

    return payment;
}

// Helper to extract counterparty details from payer/recipient
// This function is now simplified, as we get the INN directly.
Counterparty ImportManager::extractCounterparty(const std::string& name, const std::string& inn) {
    Counterparty counterparty;
    counterparty.name = trim(name);
    counterparty.inn = trim(inn);
    return counterparty;
}


// Helper function to convert DD.MM.YYYY to YYYY-MM-DD
std::string convertDateToDBFormat(const std::string& date_ddmmyyyy) {
    if (date_ddmmyyyy.length() == 10) {
        return date_ddmmyyyy.substr(6, 4) + "-" + date_ddmmyyyy.substr(3, 2) + "-" + date_ddmmyyyy.substr(0, 2);
    }
    return date_ddmmyyyy; // Return as is if format is unexpected
}

bool ImportManager::ImportPaymentsFromTsv(const std::string& filepath, DatabaseManager* dbManager) {
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

    // Regex patterns
    std::regex contract_regex("по контракту\\s*([^\\s]+)\\s*(\\d{2}\\.\\d{2}\\.\\d{4})");
    std::regex invoice_regex("(?:док\\.о пр-ке пост\\.товаров|акт об оказ\\.услуг|тов\\.накладная|счет на оплату|№)\\s*([^\\s]+)\\s*от\\s*(\\d{2}\\.\\d{2}\\.\\d{4})");
    std::regex kosgu_regex("\\(000-0000-0000000000-(\\d+):\\s*([\\d=]+)\\s*ЛС\\s*\\d+\\)\\s*К(\\d+)");

    while (std::getline(file, line)) {
        Payment payment = parsePaymentLine(line);
        if (payment.date.empty() || payment.amount == 0.0) {
            std::cerr << "Skipping invalid payment line: " << line << std::endl;
            continue;
        }

        // --- Counterparty Handling (as before) ---
        Counterparty payer_counterparty = extractCounterparty(payment.payer, "");
        Counterparty recipient_counterparty = extractCounterparty(payment.recipient, "");
        int payer_id = -1;
        if (!payer_counterparty.name.empty()) {
            payer_id = dbManager->getCounterpartyIdByName(payer_counterparty.name);
            if (payer_id == -1) {
                if (dbManager->addCounterparty(payer_counterparty)) {
                    payer_id = payer_counterparty.id;
                }
            }
        }
        int recipient_id = -1;
        if (!recipient_counterparty.name.empty()) {
            recipient_id = dbManager->getCounterpartyIdByName(recipient_counterparty.name);
            if (recipient_id == -1) {
                if (dbManager->addCounterparty(recipient_counterparty)) {
                    recipient_id = recipient_counterparty.id;
                }
            }
        }
        payment.counterparty_id = (payment.type == "income") ? payer_id : recipient_id;
        
        int contract_counterparty_id = (payment.type == "income") ? payer_id : recipient_id;

        // --- Contract and Invoice Parsing (as before) ---
        int current_contract_id = -1;
        std::smatch contract_matches;
        if (std::regex_search(payment.description, contract_matches, contract_regex)) {
            if (contract_matches.size() == 3) {
                std::string contract_number = contract_matches[1].str();
                std::string contract_date_db_format = convertDateToDBFormat(contract_matches[2].str());
                current_contract_id = dbManager->getContractIdByNumberDate(contract_number, contract_date_db_format);
                if (current_contract_id == -1) {
                    Contract contract_obj{-1, contract_number, contract_date_db_format, contract_counterparty_id};
                    if (dbManager->addContract(contract_obj)) {
                        current_contract_id = contract_obj.id;
                    }
                }
            }
        }

        int current_invoice_id = -1;
        std::smatch invoice_matches;
        if (std::regex_search(payment.description, invoice_matches, invoice_regex)) {
            if (invoice_matches.size() == 3) {
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
        
        // --- Add Payment and get its ID ---
        if (!dbManager->addPayment(payment)) {
            std::cerr << "Failed to add payment from line: " << line << std::endl;
            continue; // Skip to next line if payment fails
        }
        int new_payment_id = payment.id;

        // --- KOSGU and Payment Detail Parsing ---
        auto kosgu_begin = std::sregex_iterator(payment.description.begin(), payment.description.end(), kosgu_regex);
        auto kosgu_end = std::sregex_iterator();

        for (std::sregex_iterator i = kosgu_begin; i != kosgu_end; ++i) {
            std::smatch match = *i;
            if (match.size() == 4) {
                std::string kosgu_code_from_desc = match[3].str(); // "225" from К225
                std::string amount_str_from_desc = match[2].str(); // "5537=40"
                
                size_t equals_pos = amount_str_from_desc.find('=');
                if (equals_pos != std::string::npos) {
                    amount_str_from_desc = amount_str_from_desc.substr(0, equals_pos);
                }

                double detail_amount = 0.0;
                try {
                    detail_amount = std::stod(amount_str_from_desc);
                } catch (const std::exception& e) {
                    std::cerr << "Could not parse detail amount '" << amount_str_from_desc << "' from description." << std::endl;
                    continue;
                }

                int kosgu_id = dbManager->getKosguIdByCode(kosgu_code_from_desc);
                if (kosgu_id == -1) {
                    Kosgu new_kosgu { -1, kosgu_code_from_desc, "КОСГУ " + kosgu_code_from_desc };
                    dbManager->addKosguEntry(new_kosgu);
                    kosgu_id = dbManager->getKosguIdByCode(kosgu_code_from_desc);
                }

                if (kosgu_id != -1) {
                    PaymentDetail detail;
                    detail.payment_id = new_payment_id;
                    detail.kosgu_id = kosgu_id;
                    detail.contract_id = current_contract_id;
                    detail.invoice_id = current_invoice_id;
                    detail.amount = detail_amount;
                    
                    if (!dbManager->addPaymentDetail(detail)) {
                        std::cerr << "Failed to add payment detail for payment ID " << new_payment_id << std::endl;
                    }
                }
            }
        }
    }

    file.close();
    return true;
}

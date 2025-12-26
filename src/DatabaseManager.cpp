#include "DatabaseManager.h"
#include <iostream>
#include <vector>

DatabaseManager::DatabaseManager() : db(nullptr) {}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::open(const std::string& filepath) {
    if (db) {
        close();
    }
    int rc = sqlite3_open(filepath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

void DatabaseManager::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool DatabaseManager::execute(const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errmsg << std::endl;
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool DatabaseManager::is_open() const {
    return db != nullptr;
}

bool DatabaseManager::createDatabase(const std::string& filepath) {
    if (!open(filepath)) {
        return false;
    }

    std::vector<std::string> create_tables_sql = {
        // Справочник КОСГУ
        "CREATE TABLE IF NOT EXISTS KOSGU ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "code TEXT NOT NULL UNIQUE,"
        "name TEXT NOT NULL);",

        // Справочник контрагентов
        "CREATE TABLE IF NOT EXISTS Counterparties ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "inn TEXT UNIQUE);",

        // Справочник договоров
        "CREATE TABLE IF NOT EXISTS Contracts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "number TEXT NOT NULL,"
        "date TEXT NOT NULL,"
        "counterparty_id INTEGER,"
        "FOREIGN KEY(counterparty_id) REFERENCES Counterparties(id));",

        // Справочник платежей (банк)
        "CREATE TABLE IF NOT EXISTS Payments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date TEXT NOT NULL,"
        "doc_number TEXT,"
        "type TEXT NOT NULL CHECK(type IN ('income', 'expense')),"
        "amount REAL NOT NULL,"
        "payer TEXT,"
        "recipient TEXT,"
        "description TEXT,"
        "counterparty_id INTEGER,"
        "kosgu_id INTEGER,"
        "contract_id INTEGER,"
        "invoice_id INTEGER,"
        "FOREIGN KEY(counterparty_id) REFERENCES Counterparties(id),"
        "FOREIGN KEY(kosgu_id) REFERENCES KOSGU(id),"
        "FOREIGN KEY(contract_id) REFERENCES Contracts(id),"
        "FOREIGN KEY(invoice_id) REFERENCES Invoices(id));",
        
        // Справочник накладных
        "CREATE TABLE IF NOT EXISTS Invoices ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "number TEXT NOT NULL,"
        "date TEXT NOT NULL,"
        "contract_id INTEGER,"
        "FOREIGN KEY(contract_id) REFERENCES Contracts(id));"
    };

    for (const auto& sql : create_tables_sql) {
        if (!execute(sql)) {
            // Если одна из таблиц не создалась, закрываем соединение и возвращаем ошибку
            close();
            return false;
        }
    }

    return true;
}

// Callback функция для getKosguEntries
static int kosgu_select_callback(void* data, int argc, char** argv, char** azColName) {
    std::vector<Kosgu>* kosgu_list = static_cast<std::vector<Kosgu>*>(data);
    Kosgu entry;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id") {
            entry.id = argv[i] ? std::stoi(argv[i]) : -1;
        } else if (colName == "code") {
            entry.code = argv[i] ? argv[i] : "";
        } else if (colName == "name") {
            entry.name = argv[i] ? argv[i] : "";
        }
    }
    kosgu_list->push_back(entry);
    return 0;
}

std::vector<Kosgu> DatabaseManager::getKosguEntries() {
    std::vector<Kosgu> entries;
    if (!db) return entries;

    std::string sql = "SELECT id, code, name FROM KOSGU;";
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), kosgu_select_callback, &entries, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select KOSGU entries: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
    return entries;
}

bool DatabaseManager::addKosguEntry(const Kosgu& entry) {
    if (!db) return false;
    std::string sql = "INSERT INTO KOSGU (code, name) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, entry.code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, entry.name.c_str(), -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to add KOSGU entry: " << sqlite3_errmsg(db) 
                  << " (code: " << rc << ", extended code: " << extended_code << ")" << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::updateKosguEntry(const Kosgu& entry) {
    if (!db) return false;
    std::string sql = "UPDATE KOSGU SET code = ?, name = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, entry.code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, entry.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, entry.id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to update KOSGU entry: " << sqlite3_errmsg(db) 
                  << " (code: " << rc << ", extended code: " << extended_code << ")" << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteKosguEntry(int id) {
    if (!db) return false;
    std::string sql = "DELETE FROM KOSGU WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to delete KOSGU entry: " << sqlite3_errmsg(db) 
                  << " (code: " << rc << ", extended code: " << extended_code << ")" << std::endl;
        return false;
    }
    return true;
}


// Counterparty CRUD
bool DatabaseManager::addCounterparty(Counterparty& counterparty) {
    if (!db) return false;
    std::string sql = "INSERT INTO Counterparties (name, inn) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, counterparty.name.c_str(), -1, SQLITE_STATIC);
    if (counterparty.inn.empty()) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_text(stmt, 2, counterparty.inn.c_str(), -1, SQLITE_STATIC);
    }
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to add counterparty: " << sqlite3_errmsg(db) 
                  << " (code: " << rc << ", extended code: " << extended_code << ")" << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    counterparty.id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return true;
}

int DatabaseManager::getCounterpartyIdByNameInn(const std::string& name, const std::string& inn) {
    if (!db) return -1;
    std::string sql = "SELECT id FROM Counterparties WHERE name = ? AND inn = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, inn.c_str(), -1, SQLITE_STATIC);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

int DatabaseManager::getCounterpartyIdByName(const std::string& name) {
    if (!db) return -1;
    std::string sql = "SELECT id FROM Counterparties WHERE name = ? AND inn IS NULL;"; // Look for NULL INN
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for counterparty lookup by name: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

// Contract CRUD
bool DatabaseManager::addContract(Contract& contract) {
    if (!db) return false;
    std::string sql = "INSERT INTO Contracts (number, date, counterparty_id) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, contract.number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, contract.date.c_str(), -1, SQLITE_STATIC);
    if (contract.counterparty_id != -1) {
        sqlite3_bind_int(stmt, 3, contract.counterparty_id);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to add contract: " << sqlite3_errmsg(db) 
                  << " (code: " << rc << ", extended code: " << extended_code << ")" << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    contract.id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return true;
}

int DatabaseManager::getContractIdByNumberDate(const std::string& number, const std::string& date) {
    if (!db) return -1;
    std::string sql = "SELECT id FROM Contracts WHERE number = ? AND date = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }
    sqlite3_bind_text(stmt, 1, number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_STATIC);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

// Callback функция для getContracts
static int contract_select_callback(void* data, int argc, char** argv, char** azColName) {
    std::vector<Contract>* contract_list = static_cast<std::vector<Contract>*>(data);
    Contract entry;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id") {
            entry.id = argv[i] ? std::stoi(argv[i]) : -1;
        } else if (colName == "number") {
            entry.number = argv[i] ? argv[i] : "";
        } else if (colName == "date") {
            entry.date = argv[i] ? argv[i] : "";
        } else if (colName == "counterparty_id") {
            entry.counterparty_id = argv[i] ? std::stoi(argv[i]) : -1;
        }
    }
    contract_list->push_back(entry);
    return 0;
}

std::vector<Contract> DatabaseManager::getContracts() {
    std::vector<Contract> entries;
    if (!db) return entries;

    std::string sql = "SELECT id, number, date, counterparty_id FROM Contracts;";
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), contract_select_callback, &entries, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select Contract entries: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
    return entries;
}

bool DatabaseManager::updateContract(const Contract& contract) {
    if (!db) return false;
    std::string sql = "UPDATE Contracts SET number = ?, date = ?, counterparty_id = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for contract update: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, contract.number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, contract.date.c_str(), -1, SQLITE_STATIC);
    if (contract.counterparty_id != -1) {
        sqlite3_bind_int(stmt, 3, contract.counterparty_id);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_int(stmt, 4, contract.id);

    int rc_step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc_step != SQLITE_DONE) {
        std::cerr << "Failed to update Contract entry: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteContract(int id) {
    if (!db) return false;
    std::string sql = "DELETE FROM Contracts WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    int rc_step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc_step != SQLITE_DONE) {
        std::cerr << "Failed to delete Contract entry: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

// Invoice CRUD
bool DatabaseManager::addInvoice(Invoice& invoice) {
    if (!db) return false;
    std::string sql = "INSERT INTO Invoices (number, date, contract_id) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, invoice.number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, invoice.date.c_str(), -1, SQLITE_STATIC);
    if (invoice.contract_id != -1) {
        sqlite3_bind_int(stmt, 3, invoice.contract_id);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to add invoice: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    invoice.id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return true;
}

int DatabaseManager::getInvoiceIdByNumberDate(const std::string& number, const std::string& date) {
    if (!db) return -1;
    std::string sql = "SELECT id FROM Invoices WHERE number = ? AND date = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for invoice lookup: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }
    sqlite3_bind_text(stmt, 1, number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_STATIC);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

// Callback функция для getInvoices
static int invoice_select_callback(void* data, int argc, char** argv, char** azColName) {
    std::vector<Invoice>* invoice_list = static_cast<std::vector<Invoice>*>(data);
    Invoice entry;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id") {
            entry.id = argv[i] ? std::stoi(argv[i]) : -1;
        } else if (colName == "number") {
            entry.number = argv[i] ? argv[i] : "";
        } else if (colName == "date") {
            entry.date = argv[i] ? argv[i] : "";
        } else if (colName == "contract_id") {
            entry.contract_id = argv[i] ? std::stoi(argv[i]) : -1;
        }
    }
    invoice_list->push_back(entry);
    return 0;
}

std::vector<Invoice> DatabaseManager::getInvoices() {
    std::vector<Invoice> entries;
    if (!db) return entries;

    std::string sql = "SELECT id, number, date, contract_id FROM Invoices;";
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), invoice_select_callback, &entries, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select Invoice entries: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
    return entries;
}

bool DatabaseManager::updateInvoice(const Invoice& invoice) {
    if (!db) return false;
    std::string sql = "UPDATE Invoices SET number = ?, date = ?, contract_id = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for invoice update: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, invoice.number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, invoice.date.c_str(), -1, SQLITE_STATIC);
    if (invoice.contract_id != -1) {
        sqlite3_bind_int(stmt, 3, invoice.contract_id);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_int(stmt, 4, invoice.id);

    int rc_step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc_step != SQLITE_DONE) {
        std::cerr << "Failed to update Invoice entry: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteInvoice(int id) {
    if (!db) return false;
    std::string sql = "DELETE FROM Invoices WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for invoice delete: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    int rc_step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc_step != SQLITE_DONE) {
        std::cerr << "Failed to delete Invoice entry: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}


// Payment CRUD
bool DatabaseManager::addPayment(const Payment& payment) {
    if (!db) return false;

    std::cout << "DB: ADDING PAYMENT ->" 
              << " Date: " << payment.date
              << ", Type: " << payment.type
              << ", Amount: " << payment.amount
              << ", Desc: " << payment.description << std::endl;

    std::string sql = "INSERT INTO Payments (date, doc_number, type, amount, payer, recipient, description, counterparty_id, kosgu_id, contract_id, invoice_id) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, payment.date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, payment.doc_number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, payment.type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, payment.amount);
    sqlite3_bind_text(stmt, 5, payment.payer.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, payment.recipient.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, payment.description.c_str(), -1, SQLITE_STATIC);
    if (payment.counterparty_id != -1) {
        sqlite3_bind_int(stmt, 8, payment.counterparty_id);
    } else {
        sqlite3_bind_null(stmt, 8);
    }
    if (payment.kosgu_id != -1) {
        sqlite3_bind_int(stmt, 9, payment.kosgu_id);
    } else {
        sqlite3_bind_null(stmt, 9);
    }
    if (payment.contract_id != -1) {
        sqlite3_bind_int(stmt, 10, payment.contract_id);
    } else {
        sqlite3_bind_null(stmt, 10);
    }
    if (payment.invoice_id != -1) {
        sqlite3_bind_int(stmt, 11, payment.invoice_id);
    } else {
        sqlite3_bind_null(stmt, 11);
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to add payment: " << sqlite3_errmsg(db) 
                  << " (code: " << rc << ", extended code: " << extended_code << ")" << std::endl;
        return false;
    }
    return true;
}

// Callback for getPayments
static int payment_select_callback(void* data, int argc, char** argv, char** azColName) {
    auto* payments = static_cast<std::vector<Payment>*>(data);
    Payment p;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id") p.id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "date") p.date = argv[i] ? argv[i] : "";
        else if (colName == "doc_number") p.doc_number = argv[i] ? argv[i] : "";
        else if (colName == "type") p.type = argv[i] ? argv[i] : "";
        else if (colName == "amount") p.amount = argv[i] ? std::stod(argv[i]) : 0.0;
        else if (colName == "payer") p.payer = argv[i] ? argv[i] : "";
        else if (colName == "recipient") p.recipient = argv[i] ? argv[i] : "";
        else if (colName == "description") p.description = argv[i] ? argv[i] : "";
        else if (colName == "counterparty_id") p.counterparty_id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "kosgu_id") p.kosgu_id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "contract_id") p.contract_id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "invoice_id") p.invoice_id = argv[i] ? std::stoi(argv[i]) : -1;
    }
    payments->push_back(p);
    return 0;
}

std::vector<Payment> DatabaseManager::getPayments() {
    std::vector<Payment> payments;
    if (!db) return payments;

    std::string sql = "SELECT * FROM Payments;";
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), payment_select_callback, &payments, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select Payments: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
    return payments;
}

bool DatabaseManager::updatePayment(const Payment& payment) {
    if (!db) return false;
    std::string sql = "UPDATE Payments SET date = ?, doc_number = ?, type = ?, amount = ?, "
                      "payer = ?, recipient = ?, description = ?, counterparty_id = ?, kosgu_id = ?, "
                      "contract_id = ?, invoice_id = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for payment update: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, payment.date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, payment.doc_number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, payment.type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, payment.amount);
    sqlite3_bind_text(stmt, 5, payment.payer.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, payment.recipient.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, payment.description.c_str(), -1, SQLITE_STATIC);
    if (payment.counterparty_id != -1) {
        sqlite3_bind_int(stmt, 8, payment.counterparty_id);
    } else {
        sqlite3_bind_null(stmt, 8);
    }
    if (payment.kosgu_id != -1) {
        sqlite3_bind_int(stmt, 9, payment.kosgu_id);
    } else {
        sqlite3_bind_null(stmt, 9);
    }
    if (payment.contract_id != -1) {
        sqlite3_bind_int(stmt, 10, payment.contract_id);
    } else {
        sqlite3_bind_null(stmt, 10);
    }
    if (payment.invoice_id != -1) {
        sqlite3_bind_int(stmt, 11, payment.invoice_id);
    } else {
        sqlite3_bind_null(stmt, 11);
    }
    sqlite3_bind_int(stmt, 12, payment.id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to update payment: " << sqlite3_errmsg(db) 
                  << " (code: " << rc << ", extended code: " << extended_code << ")" << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deletePayment(int id) {
    if (!db) return false;
    std::string sql = "DELETE FROM Payments WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for payment delete: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to delete payment: " << sqlite3_errmsg(db)
                  << " (code: " << rc << ", extended code: " << extended_code << ")" << std::endl;
        return false;
    }
    return true;
}

// Callback for generic SELECT queries
static int callback_collect_data(void* data, int argc, char** argv, char** azColName) {
    auto* result_pair = static_cast<std::pair<std::vector<std::string>*, std::vector<std::vector<std::string>>*>*>(data);
    std::vector<std::string>* columns = result_pair->first;
    std::vector<std::vector<std::string>>* rows = result_pair->second;

    // Populate columns if not already done (first row)
    if (columns->empty()) {
        for (int i = 0; i < argc; i++) {
            columns->push_back(azColName[i]);
        }
    }

    // Populate row data
    std::vector<std::string> row_data;
    for (int i = 0; i < argc; i++) {
        row_data.push_back(argv[i] ? argv[i] : "NULL");
    }
    rows->push_back(row_data);

    return 0;
}

bool DatabaseManager::executeSelect(const std::string& sql, std::vector<std::string>& columns, std::vector<std::vector<std::string>>& rows) {
    if (!db) return false;

    // Clear previous results
    columns.clear();
    rows.clear();

    // Use a pair to pass both vectors to the callback
    std::pair<std::vector<std::string>*, std::vector<std::vector<std::string>>*> result_pair(&columns, &rows);
    
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), callback_collect_data, &result_pair, &errmsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL SELECT error: " << errmsg << std::endl;
        sqlite3_free(errmsg);
        return false;
    }
    
    return true;
}

// Callback функция для getCounterparties
static int counterparty_select_callback(void* data, int argc, char** argv, char** azColName) {
    std::vector<Counterparty>* counterparty_list = static_cast<std::vector<Counterparty>*>(data);
    Counterparty entry;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id") {
            entry.id = argv[i] ? std::stoi(argv[i]) : -1;
        } else if (colName == "name") {
            entry.name = argv[i] ? argv[i] : "";
        } else if (colName == "inn") {
            entry.inn = argv[i] ? argv[i] : "";
        }
    }
    counterparty_list->push_back(entry);
    return 0;
}

std::vector<Counterparty> DatabaseManager::getCounterparties() {
    std::vector<Counterparty> entries;
    if (!db) return entries;

    std::string sql = "SELECT id, name, inn FROM Counterparties;";
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), counterparty_select_callback, &entries, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select Counterparty entries: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
    return entries;
}

bool DatabaseManager::updateCounterparty(const Counterparty& counterparty) {
    if (!db) return false;
    std::string sql = "UPDATE Counterparties SET name = ?, inn = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for counterparty update: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, counterparty.name.c_str(), -1, SQLITE_STATIC);
    if (counterparty.inn.empty()) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_text(stmt, 2, counterparty.inn.c_str(), -1, SQLITE_STATIC);
    }
    sqlite3_bind_int(stmt, 3, counterparty.id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to update Counterparty entry: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteCounterparty(int id) {
    if (!db) return false;
    std::string sql = "DELETE FROM Counterparties WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for counterparty delete: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to delete Counterparty entry: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}
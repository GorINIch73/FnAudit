#include "DatabaseManager.h"
#include "ExportManager.h"
#include <iostream>
#include <vector>

DatabaseManager::DatabaseManager()
    : db(nullptr) {}

DatabaseManager::~DatabaseManager() { close(); }

bool DatabaseManager::open(const std::string &filepath) {
    if (db) {
        close();
    }
    int rc = sqlite3_open(filepath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }

    checkAndUpdateDatabaseSchema();

    return true;
}

void DatabaseManager::checkAndUpdateDatabaseSchema() {
    if (!db)
        return;
    // пока ничего не делает
    // Потом возможно напишем проверку структуры и миграцию
}

void DatabaseManager::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool DatabaseManager::execute(const std::string &sql) {
    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errmsg << std::endl;
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool DatabaseManager::is_open() const { return db != nullptr; }

bool DatabaseManager::createDatabase(const std::string &filepath) {
    if (!open(filepath)) {
        return false;
    }

    std::vector<std::string> create_tables_sql = {
        // Справочник КОСГУ
        "CREATE TABLE IF NOT EXISTS KOSGU ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "code TEXT NOT NULL UNIQUE,"
        "name TEXT NOT NULL,"
        "note TEXT);",

        // Справочник контрагентов
        "CREATE TABLE IF NOT EXISTS Counterparties ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "inn TEXT UNIQUE,"
        "is_contract_optional INTEGER DEFAULT 0);",
        // Справочник договоров
        "CREATE TABLE IF NOT EXISTS Contracts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "number TEXT NOT NULL,"
        "date TEXT NOT NULL,"
        "counterparty_id INTEGER,"
        "contract_amount REAL DEFAULT 0.0,"
        "end_date TEXT,"
        "procurement_code TEXT,"
        "note TEXT,"
        "is_for_checking INTEGER DEFAULT 0,"
        "is_for_special_control INTEGER DEFAULT 0,"
        "is_found INTEGER DEFAULT 0,"
        "FOREIGN KEY(counterparty_id) REFERENCES Counterparties(id));",

        // Справочник платежей (банк)
        "CREATE TABLE IF NOT EXISTS Payments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date TEXT NOT NULL,"
        "doc_number TEXT,"
        "type INTEGER NOT NULL DEFAULT 0,"
        "amount REAL NOT NULL,"
        "recipient TEXT,"
        "description TEXT,"
        "counterparty_id INTEGER,"
        "note TEXT,"
        "FOREIGN KEY(counterparty_id) REFERENCES Counterparties(id));",

        // Справочник накладных
        "CREATE TABLE IF NOT EXISTS Invoices ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "number TEXT NOT NULL,"
        "date TEXT NOT NULL,"
        "contract_id INTEGER,"
        "FOREIGN KEY(contract_id) REFERENCES Contracts(id));",

        // Расшифровка платежа
        "CREATE TABLE IF NOT EXISTS PaymentDetails ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "payment_id INTEGER NOT NULL,"
        "kosgu_id INTEGER,"
        "contract_id INTEGER,"
        "invoice_id INTEGER,"
        "amount REAL NOT NULL,"
        "FOREIGN KEY(payment_id) REFERENCES Payments(id) ON DELETE CASCADE,"
        "FOREIGN KEY(kosgu_id) REFERENCES KOSGU(id),"
        "FOREIGN KEY(contract_id) REFERENCES Contracts(id),"
        "FOREIGN KEY(invoice_id) REFERENCES Invoices(id));",

        // Справочник REGEX строк
        "CREATE TABLE IF NOT EXISTS Regexes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL UNIQUE,"
        "pattern TEXT NOT NULL);",

        // Справочник подозрительных слов
        "CREATE TABLE IF NOT EXISTS SuspiciousWords ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "word TEXT NOT NULL UNIQUE);"};

    for (const auto &sql : create_tables_sql) {
        if (!execute(sql)) {
            // Если одна из таблиц не создалась, закрываем соединение и
            // возвращаем ошибку
            close();
            return false;
        }
    }

    // Create Settings table if it doesn't exist and insert default row
    std::string create_settings_table_sql =
        "CREATE TABLE IF NOT EXISTS Settings ("
        "id INTEGER PRIMARY KEY,"
        "organization_name TEXT,"
        "period_start_date TEXT,"
        "period_end_date TEXT,"
        "note TEXT,"
        "import_preview_lines INTEGER DEFAULT 20,"
        "theme INTEGER DEFAULT 0,"
        "font_size INTEGER DEFAULT 24"
        ");";
    if (!execute(create_settings_table_sql)) {
        close();
        return false;
    }

    std::string insert_default_settings =
        "INSERT OR IGNORE INTO Settings (id, organization_name, "
        "period_start_date, period_end_date, note, import_preview_lines, "
        "theme, font_size) "
        "VALUES (1, '', '', '', '', 20, 0, 24);";
    if (!execute(insert_default_settings)) {
        close();
        return false;
    }

    std::vector<std::string> default_regexes = {
        "INSERT OR IGNORE INTO Regexes (name, pattern) VALUES ('Контракты', "
        "'(?:по контракту|по контр|Контракт|дог\\.|К-т)(?: "
        "№)?\\s*([^\\s,]+)\\s*(?:от\\s*)?(\\d{2}\\.\\d{2}\\.(?:\\d{4}|\\d{2}))'"
        ");",
        "INSERT OR IGNORE INTO Regexes (name, pattern) VALUES ('КОСГУ', "
        "'К(\\d{3})');",
        "INSERT OR IGNORE INTO Regexes (name, pattern) VALUES ('Исполнение', "
        "'(?:акт|сч\\.?|сч-ф|счет на "
        "оплату|№)\\s*([^\\s,]+)\\s*(?:от\\s*)?(\\d{2}\\.\\d{2}\\.(?:\\d{4}|"
        "\\d{2}))');"};

    for (const auto &sql : default_regexes) {
        if (!execute(sql)) {
            close();
            return false;
        }
    }

    return true;
}

// Callback функция для getKosguEntries
static int kosgu_select_callback(void *data, int argc, char **argv,
                                 char **azColName) {
    std::vector<Kosgu> *kosgu_list = static_cast<std::vector<Kosgu> *>(data);
    Kosgu entry;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id") {
            entry.id = argv[i] ? std::stoi(argv[i]) : -1;
        } else if (colName == "code") {
            entry.code = argv[i] ? argv[i] : "";
        } else if (colName == "name") {
            entry.name = argv[i] ? argv[i] : "";
        } else if (colName == "note") {
            entry.note = argv[i] ? argv[i] : "";
        } else if (colName == "total_amount") {
            entry.total_amount = argv[i] ? std::stod(argv[i]) : 0.0;
        }
    }
    kosgu_list->push_back(entry);
    return 0;
}

std::vector<Kosgu> DatabaseManager::getKosguEntries() {
    std::vector<Kosgu> entries;
    if (!db)
        return entries;

    std::string sql =
        "SELECT k.id, k.code, k.name, k.note, IFNULL(SUM(pd.amount), "
        "0.0) as total_amount "
        "FROM KOSGU k "
        "LEFT JOIN PaymentDetails pd ON k.id = pd.kosgu_id "
        "GROUP BY k.id, k.code, k.name, k.note;";
    char *errmsg = nullptr;
    int rc =
        sqlite3_exec(db, sql.c_str(), kosgu_select_callback, &entries, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select KOSGU entries: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
    return entries;
}

bool DatabaseManager::addKosguEntry(const Kosgu &entry) {
    if (!db)
        return false;
    std::string sql = "INSERT INTO KOSGU (code, name, note) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, entry.code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, entry.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, entry.note.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to add KOSGU entry: " << sqlite3_errmsg(db)
                  << " (code: " << rc << ", extended code: " << extended_code
                  << ")" << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::updateKosguEntry(const Kosgu &entry) {
    if (!db)
        return false;
    std::string sql =
        "UPDATE KOSGU SET code = ?, name = ?, note = ? WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, entry.code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, entry.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, entry.note.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, entry.id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to update KOSGU entry: " << sqlite3_errmsg(db)
                  << " (code: " << rc << ", extended code: " << extended_code
                  << ")" << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteKosguEntry(int id) {
    if (!db)
        return false;
    std::string sql = "DELETE FROM KOSGU WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to delete KOSGU entry: " << sqlite3_errmsg(db)
                  << " (code: " << rc << ", extended code: " << extended_code
                  << ")" << std::endl;
        return false;
    }
    return true;
}

int DatabaseManager::getKosguIdByCode(const std::string &code) {
    if (!db)
        return -1;
    std::string sql = "SELECT id FROM KOSGU WHERE code = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for KOSGU lookup by code: "
                  << sqlite3_errmsg(db) << std::endl;
        return -1;
    }
    sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_STATIC);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

// Counterparty CRUD
bool DatabaseManager::addCounterparty(Counterparty &counterparty) {
    if (!db)
        return false;
    std::string sql = "INSERT INTO Counterparties (name, inn, "
                      "is_contract_optional) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, counterparty.name.c_str(), -1, SQLITE_STATIC);
    if (counterparty.inn.empty()) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_text(stmt, 2, counterparty.inn.c_str(), -1, SQLITE_STATIC);
    }
    sqlite3_bind_int(stmt, 3, counterparty.is_contract_optional ? 1 : 0);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to add counterparty: " << sqlite3_errmsg(db)
                  << " (code: " << rc << ", extended code: " << extended_code
                  << ")" << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    counterparty.id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return true;
}

int DatabaseManager::getCounterpartyIdByNameInn(const std::string &name,
                                                const std::string &inn) {
    if (!db)
        return -1;
    std::string sql =
        "SELECT id FROM Counterparties WHERE name = ? AND inn = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
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

int DatabaseManager::getCounterpartyIdByName(const std::string &name) {
    if (!db)
        return -1;
    std::string sql =
        "SELECT id FROM Counterparties WHERE name = ? AND inn IS NULL;"; // Look
                                                                         // for
                                                                         // NULL
                                                                         // INN
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr
            << "Failed to prepare statement for counterparty lookup by name: "
            << sqlite3_errmsg(db) << std::endl;
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

// Callback функция для getCounterparties
static int counterparty_select_callback(void *data, int argc, char **argv,
                                        char **azColName) {
    std::vector<Counterparty> *counterparty_list =
        static_cast<std::vector<Counterparty> *>(data);
    Counterparty entry;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id") {
            entry.id = argv[i] ? std::stoi(argv[i]) : -1;
        } else if (colName == "name") {
            entry.name = argv[i] ? argv[i] : "";
        } else if (colName == "inn") {
            entry.inn = argv[i] ? argv[i] : "";
        } else if (colName == "is_contract_optional") {
            entry.is_contract_optional =
                argv[i] ? (std::stoi(argv[i]) == 1) : false;
        } else if (colName == "total_amount") {
            entry.total_amount = argv[i] ? std::stod(argv[i]) : 0.0;
        }
    }
    counterparty_list->push_back(entry);
    return 0;
}

std::vector<Counterparty> DatabaseManager::getCounterparties() {
    std::vector<Counterparty> entries;
    if (!db)
        return entries;

    std::string sql = "SELECT c.id, c.name, c.inn, c.is_contract_optional, "
                      "IFNULL(p_sum.total, 0.0) as total_amount "
                      "FROM Counterparties c "
                      "LEFT JOIN ( "
                      "    SELECT counterparty_id, SUM(amount) as total "
                      "    FROM Payments "
                      "    WHERE counterparty_id IS NOT NULL "
                      "    GROUP BY counterparty_id "
                      ") p_sum ON c.id = p_sum.counterparty_id;";
    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), counterparty_select_callback,
                          &entries, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select Counterparty entries: " << errmsg
                  << std::endl;
        sqlite3_free(errmsg);
    }
    return entries;
}

bool DatabaseManager::updateCounterparty(const Counterparty &counterparty) {
    if (!db)
        return false;
    std::string sql = "UPDATE Counterparties SET name = ?, inn = ?, "
                      "is_contract_optional = ? WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for counterparty update: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, counterparty.name.c_str(), -1, SQLITE_STATIC);
    if (counterparty.inn.empty()) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_text(stmt, 2, counterparty.inn.c_str(), -1, SQLITE_STATIC);
    }
    sqlite3_bind_int(stmt, 3, counterparty.is_contract_optional ? 1 : 0);
    sqlite3_bind_int(stmt, 4, counterparty.id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to update Counterparty entry: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteCounterparty(int id) {
    if (!db)
        return false;
    std::string sql = "DELETE FROM Counterparties WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to delete Counterparty entry: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

std::vector<ContractPaymentInfo>
DatabaseManager::getPaymentInfoForCounterparty(int counterparty_id) {
    std::vector<ContractPaymentInfo> results;
    if (!db)
        return results;

    std::string sql = "SELECT p.date, p.doc_number, p.amount, p.description "
                      "FROM Payments p "
                      "WHERE p.counterparty_id = ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr
            << "Failed to prepare statement for getPaymentInfoForCounterparty: "
            << sqlite3_errmsg(db) << std::endl;
        return results;
    }

    sqlite3_bind_int(stmt, 1, counterparty_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ContractPaymentInfo info;
        info.date = (const char *)sqlite3_column_text(stmt, 0);
        info.doc_number = (const char *)sqlite3_column_text(stmt, 1);
        info.amount = sqlite3_column_double(stmt, 2);
        info.description = (const char *)sqlite3_column_text(stmt, 3);
        results.push_back(info);
    }

    sqlite3_finalize(stmt);
    return results;
}

// Contract CRUD
int DatabaseManager::addContract(Contract &contract) {
    if (!db)
        return -1;
    std::string sql = "INSERT INTO Contracts (number, date, counterparty_id, "
                      "contract_amount, end_date, procurement_code, note, "
                      "is_for_checking, is_for_special_control, is_found) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return -1;
    }
    sqlite3_bind_text(stmt, 1, contract.number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, contract.date.c_str(), -1, SQLITE_STATIC);
    if (contract.counterparty_id != -1) {
        sqlite3_bind_int(stmt, 3, contract.counterparty_id);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_double(stmt, 4, contract.contract_amount);
    sqlite3_bind_text(stmt, 5, contract.end_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, contract.procurement_code.c_str(), -1,
                      SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, contract.note.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 8, contract.is_for_checking ? 1 : 0);
    sqlite3_bind_int(stmt, 9, contract.is_for_special_control ? 1 : 0);
    sqlite3_bind_int(stmt, 10, contract.is_found ? 1 : 0);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to add contract: " << sqlite3_errmsg(db)
                  << " (code: " << rc << ", extended code: " << extended_code
                  << ")" << std::endl;
        sqlite3_finalize(stmt);
        return -1;
    }
    int id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return id;
}

int DatabaseManager::getContractIdByNumberDate(const std::string &number,
                                               const std::string &date) {
    if (!db)
        return -1;
    std::string sql = "SELECT id FROM Contracts WHERE number = ? AND date = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
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

int DatabaseManager::updateContractProcurementCode(
    const std::string &number, const std::string &date,
    const std::string &procurement_code) {
    if (!db)
        return 0;
    std::string sql =
        "UPDATE Contracts SET procurement_code = ? WHERE number = ? AND date = "
        "? AND (procurement_code IS NULL OR procurement_code = '');";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr
            << "Failed to prepare statement for contract procurement code "
               "update: "
            << sqlite3_errmsg(db) << std::endl;
        return 0;
    }

    sqlite3_bind_text(stmt, 1, procurement_code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, date.c_str(), -1, SQLITE_STATIC);

    int rc_step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc_step != SQLITE_DONE) {
        std::cerr << "Failed to update Contract procurement code: "
                  << sqlite3_errmsg(db) << std::endl;
        return 0;
    }

    return sqlite3_changes(db);
}

// Callback функция для getContracts
static int contract_select_callback(void *data, int argc, char **argv,
                                    char **azColName) {
    std::vector<Contract> *contract_list =
        static_cast<std::vector<Contract> *>(data);
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
        } else if (colName == "total_amount") {
            entry.total_amount = argv[i] ? std::stod(argv[i]) : 0.0;
        } else if (colName == "contract_amount") {
            entry.contract_amount = argv[i] ? std::stod(argv[i]) : 0.0;
        } else if (colName == "end_date") {
            entry.end_date = argv[i] ? argv[i] : "";
        } else if (colName == "procurement_code") {
            entry.procurement_code = argv[i] ? argv[i] : "";
        } else if (colName == "note") {
            entry.note = argv[i] ? argv[i] : "";
        } else if (colName == "is_for_checking") {
            entry.is_for_checking = argv[i] ? (std::stoi(argv[i]) == 1) : false;
        } else if (colName == "is_for_special_control") {
            entry.is_for_special_control =
                argv[i] ? (std::stoi(argv[i]) == 1) : false;
        } else if (colName == "is_found") {
            entry.is_found = argv[i] ? (std::stoi(argv[i]) == 1) : false;
        }
    }
    contract_list->push_back(entry);
    return 0;
}

std::vector<Contract> DatabaseManager::getContracts() {
    std::vector<Contract> entries;
    if (!db)
        return entries;

    std::string sql = "SELECT c.id, c.number, c.date, c.counterparty_id, "
                      "c.contract_amount, c.end_date, c.procurement_code, "
                      "c.note, c.is_for_checking, c.is_for_special_control, "
                      "c.is_found, IFNULL(SUM(pd.amount), 0.0) as total_amount "
                      "FROM Contracts c "
                      "LEFT JOIN PaymentDetails pd ON c.id = pd.contract_id "
                      "GROUP BY c.id, c.number, c.date, c.counterparty_id;";
    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), contract_select_callback, &entries,
                          &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select Contract entries: " << errmsg
                  << std::endl;
        sqlite3_free(errmsg);
    }
    return entries;
}

bool DatabaseManager::updateContract(const Contract &contract) {
    if (!db)
        return false;
    std::string sql = "UPDATE Contracts SET number = ?, date = ?, "
                      "counterparty_id = ?, contract_amount = ?, end_date = ?, "
                      "procurement_code = ?, note = ?, is_for_checking = ?, "
                      "is_for_special_control = ?, is_found = ? WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for contract update: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, contract.number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, contract.date.c_str(), -1, SQLITE_STATIC);
    if (contract.counterparty_id != -1) {
        sqlite3_bind_int(stmt, 3, contract.counterparty_id);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_double(stmt, 4, contract.contract_amount);
    sqlite3_bind_text(stmt, 5, contract.end_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, contract.procurement_code.c_str(), -1,
                      SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, contract.note.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 8, contract.is_for_checking ? 1 : 0);
    sqlite3_bind_int(stmt, 9, contract.is_for_special_control ? 1 : 0);
    sqlite3_bind_int(stmt, 10, contract.is_found ? 1 : 0);
    sqlite3_bind_int(stmt, 11, contract.id);

    int rc_step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc_step != SQLITE_DONE) {
        std::cerr << "Failed to update Contract entry: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteContract(int id) {
    if (!db)
        return false;
    std::string sql = "DELETE FROM Contracts WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    int rc_step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc_step != SQLITE_DONE) {
        std::cerr << "Failed to delete Contract entry: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    return true;
}

void DatabaseManager::transferPaymentDetails(int from_contract_id,
                                             int to_contract_id) {
    if (!db)
        return;

    std::string sql =
        "UPDATE PaymentDetails SET contract_id = ? WHERE contract_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr
            << "Failed to prepare statement for transferring payment details: "
            << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_int(stmt, 1, to_contract_id);
    sqlite3_bind_int(stmt, 2, from_contract_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to transfer payment details: "
                  << sqlite3_errmsg(db) << std::endl;
    }
}

std::vector<ContractPaymentInfo>
DatabaseManager::getPaymentInfoForContract(int contract_id) {
    std::vector<ContractPaymentInfo> results;
    if (!db)
        return results;

    std::string sql = "SELECT p.date, p.doc_number, p.amount, p.description "
                      "FROM Payments p "
                      "JOIN PaymentDetails pd ON p.id = pd.payment_id "
                      "WHERE pd.contract_id = ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr
            << "Failed to prepare statement for getPaymentInfoForContract: "
            << sqlite3_errmsg(db) << std::endl;
        return results;
    }

    sqlite3_bind_int(stmt, 1, contract_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ContractPaymentInfo info;
        info.date = (const char *)sqlite3_column_text(stmt, 0);
        info.doc_number = (const char *)sqlite3_column_text(stmt, 1);
        info.amount = sqlite3_column_double(stmt, 2);
        info.description = (const char *)sqlite3_column_text(stmt, 3);
        results.push_back(info);
    }

    sqlite3_finalize(stmt);
    return results;
}

// Invoice CRUD
int DatabaseManager::addInvoice(Invoice &invoice) {
    if (!db)
        return -1;
    std::string sql =
        "INSERT INTO Invoices (number, date, contract_id) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return -1;
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
        std::cerr << "Failed to add invoice: " << sqlite3_errmsg(db)
                  << std::endl;
        sqlite3_finalize(stmt);
        return -1;
    }
    int id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return id;
}

int DatabaseManager::getInvoiceIdByNumberDate(const std::string &number,
                                              const std::string &date) {
    if (!db)
        return -1;
    std::string sql = "SELECT id FROM Invoices WHERE number = ? AND date = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
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
static int invoice_select_callback(void *data, int argc, char **argv,
                                   char **azColName) {
    std::vector<Invoice> *invoice_list =
        static_cast<std::vector<Invoice> *>(data);
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
        } else if (colName == "total_amount") {
            entry.total_amount = argv[i] ? std::stod(argv[i]) : 0.0;
        }
    }
    invoice_list->push_back(entry);
    return 0;
}

std::vector<Invoice> DatabaseManager::getInvoices() {
    std::vector<Invoice> entries;
    if (!db)
        return entries;

    std::string sql = "SELECT i.id, i.number, i.date, i.contract_id, "
                      "IFNULL(SUM(pd.amount), 0.0) as total_amount "
                      "FROM Invoices i "
                      "LEFT JOIN PaymentDetails pd ON i.id = pd.invoice_id "
                      "GROUP BY i.id, i.number, i.date, i.contract_id;";
    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), invoice_select_callback, &entries,
                          &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select Invoice entries: " << errmsg
                  << std::endl;
        sqlite3_free(errmsg);
    }
    return entries;
}

bool DatabaseManager::updateInvoice(const Invoice &invoice) {
    if (!db)
        return false;
    std::string sql = "UPDATE Invoices SET number = ?, date = ?, contract_id = "
                      "? WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for invoice update: "
                  << sqlite3_errmsg(db) << std::endl;
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
        std::cerr << "Failed to update Invoice entry: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteInvoice(int id) {
    if (!db)
        return false;
    std::string sql = "DELETE FROM Invoices WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    int rc_step = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc_step != SQLITE_DONE) {
        std::cerr << "Failed to delete Invoice entry: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    return true;
}

std::vector<ContractPaymentInfo>
DatabaseManager::getPaymentInfoForInvoice(int invoice_id) {
    std::vector<ContractPaymentInfo> results;
    if (!db)
        return results;

    std::string sql = "SELECT p.date, p.doc_number, p.amount, p.description "
                      "FROM Payments p "
                      "JOIN PaymentDetails pd ON p.id = pd.payment_id "
                      "WHERE pd.invoice_id = ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr
            << "Failed to prepare statement for getPaymentInfoForInvoice: "
            << sqlite3_errmsg(db) << std::endl;
        return results;
    }

    sqlite3_bind_int(stmt, 1, invoice_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ContractPaymentInfo info;
        info.date = (const char *)sqlite3_column_text(stmt, 0);
        info.doc_number = (const char *)sqlite3_column_text(stmt, 1);
        info.amount = sqlite3_column_double(stmt, 2);
        info.description = (const char *)sqlite3_column_text(stmt, 3);
        results.push_back(info);
    }

    sqlite3_finalize(stmt);
    return results;
}

// Payment CRUD
bool DatabaseManager::addPayment(Payment &payment) {
    if (!db)
        return false;

    // std::cout << "DB: ADDING PAYMENT ->"
    //           << " Date: " << payment.date << ", Type: " << payment.type
    //           << ", Amount: " << payment.amount
    //           << ", Desc: " << payment.description << std::endl;

    std::string sql = "INSERT INTO Payments (date, doc_number, type, amount, "
                      "recipient, description, counterparty_id, note) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, payment.date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, payment.doc_number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, payment.type ? 1 : 0);
    sqlite3_bind_double(stmt, 4, payment.amount);
    sqlite3_bind_text(stmt, 5, payment.recipient.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, payment.description.c_str(), -1, SQLITE_STATIC);
    if (payment.counterparty_id != -1) {
        sqlite3_bind_int(stmt, 7, payment.counterparty_id);
    } else {
        sqlite3_bind_null(stmt, 7);
    }
    sqlite3_bind_text(stmt, 8, payment.note.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to add payment: " << sqlite3_errmsg(db)
                  << " (code: " << rc << ", extended code: " << extended_code
                  << ")" << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    payment.id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return true;
}

// Callback for getPayments
static int payment_select_callback(void *data, int argc, char **argv,
                                   char **azColName) {
    auto *payments = static_cast<std::vector<Payment> *>(data);
    Payment p;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id")
            p.id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "date")
            p.date = argv[i] ? argv[i] : "";
        else if (colName == "doc_number")
            p.doc_number = argv[i] ? argv[i] : "";
        else if (colName == "type")
            p.type = (argv[i] && std::stoi(argv[i]) == 1);
        else if (colName == "amount")
            p.amount = argv[i] ? std::stod(argv[i]) : 0.0;
        else if (colName == "recipient")
            p.recipient = argv[i] ? argv[i] : "";
        else if (colName == "description")
            p.description = argv[i] ? argv[i] : "";
        else if (colName == "counterparty_id")
            p.counterparty_id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "note")
            p.note = argv[i] ? argv[i] : "";
    }
    payments->push_back(p);
    return 0;
}

std::vector<Payment> DatabaseManager::getPayments() {
    std::vector<Payment> payments;
    if (!db)
        return payments;

    std::string sql = "SELECT id, date, doc_number, type, amount, recipient, "
                      "description, counterparty_id, note FROM Payments;";
    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), payment_select_callback, &payments,
                          &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select Payments: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
    return payments;
}

bool DatabaseManager::updatePayment(const Payment &payment) {
    if (!db)
        return false;
    std::string sql =
        "UPDATE Payments SET date = ?, doc_number = ?, type = ?, amount = ?, "
        "recipient = ?, description = ?, counterparty_id = ?, note = ? "
        "WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for payment update: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, payment.date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, payment.doc_number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, payment.type ? 1 : 0);
    sqlite3_bind_double(stmt, 4, payment.amount);
    sqlite3_bind_text(stmt, 5, payment.recipient.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, payment.description.c_str(), -1, SQLITE_STATIC);
    if (payment.counterparty_id != -1) {
        sqlite3_bind_int(stmt, 7, payment.counterparty_id);
    } else {
        sqlite3_bind_null(stmt, 7);
    }
    sqlite3_bind_text(stmt, 8, payment.note.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 9, payment.id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to update payment: " << sqlite3_errmsg(db)
                  << " (code: " << rc << ", extended code: " << extended_code
                  << ")" << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deletePayment(int id) {
    if (!db)
        return false;
    std::string sql = "DELETE FROM Payments WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for payment delete: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        int extended_code = sqlite3_extended_errcode(db);
        std::cerr << "Failed to delete payment: " << sqlite3_errmsg(db)
                  << " (code: " << rc << ", extended code: " << extended_code
                  << ")" << std::endl;
        return false;
    }
    return true;
}

std::vector<ContractPaymentInfo>
DatabaseManager::getPaymentInfoForKosgu(int kosgu_id) {
    std::vector<ContractPaymentInfo> results;
    if (!db)
        return results;

    std::string sql = "SELECT p.date, p.doc_number, pd.amount, p.description "
                      "FROM Payments p "
                      "JOIN PaymentDetails pd ON p.id = pd.payment_id "
                      "WHERE pd.kosgu_id = ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for getPaymentInfoForKosgu: "
                  << sqlite3_errmsg(db) << std::endl;
        return results;
    }

    sqlite3_bind_int(stmt, 1, kosgu_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ContractPaymentInfo info;
        info.date = (const char *)sqlite3_column_text(stmt, 0);
        info.doc_number = (const char *)sqlite3_column_text(stmt, 1);
        info.amount = sqlite3_column_double(stmt, 2);
        info.description = (const char *)sqlite3_column_text(stmt, 3);
        results.push_back(info);
    }

    sqlite3_finalize(stmt);
    return results;
}

std::vector<KosguPaymentDetailInfo> DatabaseManager::getAllKosguPaymentInfo() {
    std::vector<KosguPaymentDetailInfo> results;
    if (!db)
        return results;

    std::string sql =
        "SELECT pd.kosgu_id, p.date, p.doc_number, pd.amount, p.description "
        "FROM PaymentDetails pd "
        "JOIN Payments p ON pd.payment_id = p.id "
        "WHERE pd.kosgu_id IS NOT NULL;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for getAllKosguPaymentInfo: "
                  << sqlite3_errmsg(db) << std::endl;
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        KosguPaymentDetailInfo info;
        info.kosgu_id = sqlite3_column_int(stmt, 0);
        const unsigned char *date_text = sqlite3_column_text(stmt, 1);
        info.date = date_text ? (const char *)date_text : "";
        const unsigned char *doc_num_text = sqlite3_column_text(stmt, 2);
        info.doc_number = doc_num_text ? (const char *)doc_num_text : "";
        info.amount = sqlite3_column_double(stmt, 3);
        const unsigned char *desc_text = sqlite3_column_text(stmt, 4);
        info.description = desc_text ? (const char *)desc_text : "";
        results.push_back(info);
    }

    sqlite3_finalize(stmt);
    return results;
}

static int contract_payment_detail_info_select_callback(void *data, int argc,
                                                        char **argv,
                                                        char **azColName) {
    auto *results = static_cast<std::vector<ContractPaymentInfo> *>(data);
    ContractPaymentInfo info;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "contract_id") {
            info.contract_id = argv[i] ? std::stoi(argv[i]) : -1;
        } else if (colName == "date") {
            info.date = argv[i] ? argv[i] : "";
        } else if (colName == "doc_number") {
            info.doc_number = argv[i] ? argv[i] : "";
        } else if (colName == "amount") {
            info.amount = argv[i] ? std::stod(argv[i]) : 0.0;
        } else if (colName == "description") {
            info.description = argv[i] ? argv[i] : "";
        }
    }
    results->push_back(info);
    return 0;
}

std::vector<ContractPaymentInfo> DatabaseManager::getAllContractPaymentInfo() {
    std::vector<ContractPaymentInfo> results;
    if (!db)
        return results;

    std::string sql =
        "SELECT pd.contract_id, p.date, p.doc_number, pd.amount, p.description "
        "FROM PaymentDetails pd "
        "JOIN Payments p ON pd.payment_id = p.id "
        "WHERE pd.contract_id IS NOT NULL;";

    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(),
                          contract_payment_detail_info_select_callback,
                          &results, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select getAllContractPaymentInfo: " << errmsg
                  << std::endl;
        sqlite3_free(errmsg);
    }
    return results;
}

static int counterparty_payment_detail_info_select_callback(void *data,
                                                            int argc,
                                                            char **argv,
                                                            char **azColName) {
    auto *results = static_cast<std::vector<CounterpartyPaymentInfo> *>(data);
    CounterpartyPaymentInfo info;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "counterparty_id") {
            info.counterparty_id = argv[i] ? std::stoi(argv[i]) : -1;
        } else if (colName == "date") {
            info.date = argv[i] ? argv[i] : "";
        } else if (colName == "doc_number") {
            info.doc_number = argv[i] ? argv[i] : "";
        } else if (colName == "amount") {
            info.amount = argv[i] ? std::stod(argv[i]) : 0.0;
        } else if (colName == "description") {
            info.description = argv[i] ? argv[i] : "";
        }
    }
    results->push_back(info);
    return 0;
}

std::vector<CounterpartyPaymentInfo>
DatabaseManager::getAllCounterpartyPaymentInfo() {
    std::vector<CounterpartyPaymentInfo> results;
    if (!db)
        return results;

    std::string sql = "SELECT p.counterparty_id, p.date, p.doc_number, "
                      "p.amount, p.description "
                      "FROM Payments p "
                      "WHERE p.counterparty_id IS NOT NULL;";

    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(),
                          counterparty_payment_detail_info_select_callback,
                          &results, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select getAllCounterpartyPaymentInfo: "
                  << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
    return results;
}

std::vector<ContractExportData> DatabaseManager::getContractsForExport() {
    std::vector<ContractExportData> results;
    if (!db)
        return results;

    // 1. Get all contracts marked for checking
    std::string sql_contracts =
        "SELECT id, number, date, counterparty_id, is_for_special_control, "
        "note, procurement_code FROM Contracts WHERE is_for_checking = 1;";
    sqlite3_stmt *stmt_contracts = nullptr;
    if (sqlite3_prepare_v2(db, sql_contracts.c_str(), -1, &stmt_contracts,
                           nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for getContractsForExport: "
                  << sqlite3_errmsg(db) << std::endl;
        return results;
    }

    while (sqlite3_step(stmt_contracts) == SQLITE_ROW) {
        ContractExportData data;
        int contract_id = sqlite3_column_int(stmt_contracts, 0);
        data.contract_number =
            (const char *)sqlite3_column_text(stmt_contracts, 1);
        data.contract_date =
            (const char *)sqlite3_column_text(stmt_contracts, 2);
        int counterparty_id = sqlite3_column_int(stmt_contracts, 3);
        data.is_for_special_control =
            sqlite3_column_int(stmt_contracts, 4) == 1;
        const unsigned char *note_text = sqlite3_column_text(stmt_contracts, 5);
        data.note = note_text ? (const char *)note_text : "";
        const unsigned char *pk_text = sqlite3_column_text(stmt_contracts, 6);
        data.procurement_code = pk_text ? (const char *)pk_text : "";

        // 2. Get counterparty name
        if (counterparty_id != 0) {
            std::string sql_cp =
                "SELECT name FROM Counterparties WHERE id = ?;";
            sqlite3_stmt *stmt_cp = nullptr;
            if (sqlite3_prepare_v2(db, sql_cp.c_str(), -1, &stmt_cp, nullptr) ==
                SQLITE_OK) {
                sqlite3_bind_int(stmt_cp, 1, counterparty_id);
                if (sqlite3_step(stmt_cp) == SQLITE_ROW) {
                    const unsigned char *cp_name =
                        sqlite3_column_text(stmt_cp, 0);
                    data.counterparty_name =
                        cp_name ? (const char *)cp_name : "";
                }
                sqlite3_finalize(stmt_cp);
            }
        }

        // 3. Get KOSGU codes
        std::string sql_kosgu =
            "SELECT DISTINCT k.code FROM KOSGU k JOIN PaymentDetails pd ON "
            "k.id = pd.kosgu_id WHERE pd.contract_id = ?;";
        sqlite3_stmt *stmt_kosgu = nullptr;
        if (sqlite3_prepare_v2(db, sql_kosgu.c_str(), -1, &stmt_kosgu,
                               nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt_kosgu, 1, contract_id);
            std::string kosgu_list;
            while (sqlite3_step(stmt_kosgu) == SQLITE_ROW) {
                const unsigned char *code = sqlite3_column_text(stmt_kosgu, 0);
                if (code) {
                    if (!kosgu_list.empty()) {
                        kosgu_list += ", ";
                    }
                    kosgu_list += (const char *)code;
                }
            }
            data.kosgu_codes = kosgu_list;
            sqlite3_finalize(stmt_kosgu);
        }

        results.push_back(data);
    }

    sqlite3_finalize(stmt_contracts);
    return results;
}

// PaymentDetail CRUD
bool DatabaseManager::addPaymentDetail(PaymentDetail &detail) {
    if (!db)
        return false;
    std::string sql =
        "INSERT INTO PaymentDetails (payment_id, kosgu_id, contract_id, "
        "invoice_id, amount) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for payment detail: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, detail.payment_id);
    sqlite3_bind_int(stmt, 2, detail.kosgu_id);
    sqlite3_bind_int(stmt, 3, detail.contract_id);
    sqlite3_bind_int(stmt, 4, detail.invoice_id);
    sqlite3_bind_double(stmt, 5, detail.amount);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to add payment detail: " << sqlite3_errmsg(db)
                  << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
    detail.id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return true;
}

static int payment_detail_select_callback(void *data, int argc, char **argv,
                                          char **azColName) {
    auto *details = static_cast<std::vector<PaymentDetail> *>(data);
    PaymentDetail pd;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id")
            pd.id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "payment_id")
            pd.payment_id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "kosgu_id")
            pd.kosgu_id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "contract_id")
            pd.contract_id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "invoice_id")
            pd.invoice_id = argv[i] ? std::stoi(argv[i]) : -1;
        else if (colName == "amount")
            pd.amount = argv[i] ? std::stod(argv[i]) : 0.0;
    }
    details->push_back(pd);
    return 0;
}

std::vector<PaymentDetail> DatabaseManager::getPaymentDetails(int payment_id) {
    std::vector<PaymentDetail> details;
    if (!db)
        return details;

    std::string sql = "SELECT * FROM PaymentDetails WHERE payment_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for getting payment details: "
                  << sqlite3_errmsg(db) << std::endl;
        return details;
    }
    sqlite3_bind_int(stmt, 1, payment_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PaymentDetail pd;
        pd.id = sqlite3_column_int(stmt, 0);
        pd.payment_id = sqlite3_column_int(stmt, 1);
        pd.kosgu_id = sqlite3_column_int(stmt, 2);
        pd.contract_id = sqlite3_column_int(stmt, 3);
        pd.invoice_id = sqlite3_column_int(stmt, 4);
        pd.amount = sqlite3_column_double(stmt, 5);
        details.push_back(pd);
    }

    sqlite3_finalize(stmt);
    return details;
}

std::vector<PaymentDetail> DatabaseManager::getAllPaymentDetails() {
    std::vector<PaymentDetail> details;
    if (!db)
        return details;

    std::string sql = "SELECT * FROM PaymentDetails;";
    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), payment_detail_select_callback,
                          &details, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select all PaymentDetails: " << errmsg
                  << std::endl;
        sqlite3_free(errmsg);
    }
    return details;
}

bool DatabaseManager::updatePaymentDetail(const PaymentDetail &detail) {
    if (!db)
        return false;
    std::string sql = "UPDATE PaymentDetails SET kosgu_id = ?, contract_id = "
                      "?, invoice_id = ?, amount = ? WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for updating payment detail: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_int(stmt, 1, detail.kosgu_id);
    sqlite3_bind_int(stmt, 2, detail.contract_id);
    sqlite3_bind_int(stmt, 3, detail.invoice_id);
    sqlite3_bind_double(stmt, 4, detail.amount);
    sqlite3_bind_int(stmt, 5, detail.id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to update payment detail: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deletePaymentDetail(int id) {
    if (!db)
        return false;
    std::string sql = "DELETE FROM PaymentDetails WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for deleting payment detail: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to delete payment detail: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteAllPaymentDetails(int payment_id) {
    if (!db)
        return false;
    std::string sql = "DELETE FROM PaymentDetails WHERE payment_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr
            << "Failed to prepare statement for deleting all payment details: "
            << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, payment_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to delete all payment details: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::bulkUpdatePaymentDetails(
    const std::vector<int> &payment_ids, const std::string &field_to_update,
    int new_id) {
    if (!db || payment_ids.empty()) {
        return false;
    }

    // Basic validation to prevent SQL injection
    if (field_to_update != "kosgu_id" && field_to_update != "contract_id" &&
        field_to_update != "invoice_id") {
        std::cerr << "Invalid field to update: " << field_to_update
                  << std::endl;
        return false;
    }

    std::string sql = "UPDATE PaymentDetails SET " + field_to_update +
                      " = ? WHERE payment_id IN (";
    for (size_t i = 0; i < payment_ids.size(); ++i) {
        sql += (i == 0 ? "?" : ",?");
    }
    sql += ");";

    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for bulk update: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // Bind the new_id for the SET part
    sqlite3_bind_int(stmt, 1, new_id);

    // Bind all payment_ids for the IN clause
    for (size_t i = 0; i < payment_ids.size(); ++i) {
        sqlite3_bind_int(stmt, i + 2, payment_ids[i]);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to execute bulk update: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }

    return true;
}

// Regex CRUD
static int regex_select_callback(void *data, int argc, char **argv,
                                 char **azColName) {
    std::vector<Regex> *regex_list = static_cast<std::vector<Regex> *>(data);
    Regex entry;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id") {
            entry.id = argv[i] ? std::stoi(argv[i]) : -1;
        } else if (colName == "name") {
            entry.name = argv[i] ? argv[i] : "";
        } else if (colName == "pattern") {
            entry.pattern = argv[i] ? argv[i] : "";
        }
    }
    regex_list->push_back(entry);
    return 0;
}

std::vector<Regex> DatabaseManager::getRegexes() {
    std::vector<Regex> entries;
    if (!db)
        return entries;

    std::string sql = "SELECT id, name, pattern FROM Regexes;";
    char *errmsg = nullptr;
    int rc =
        sqlite3_exec(db, sql.c_str(), regex_select_callback, &entries, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select Regex entries: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
    return entries;
}

bool DatabaseManager::addRegex(Regex &regex) {
    if (!db)
        return false;

    std::cout << "regex: " << regex.name << " " << regex.pattern << std::endl;
    std::string sql = "INSERT INTO Regexes (name, pattern) VALUES (?, ?);";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, regex.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, regex.pattern.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return false;
    }
    regex.id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return true;
}

bool DatabaseManager::updateRegex(const Regex &regex) {
    if (!db)
        return false;
    std::string sql = "UPDATE Regexes SET name = ?, pattern = ? WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, regex.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, regex.pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, regex.id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool DatabaseManager::deleteRegex(int id) {
    if (!db)
        return false;
    std::string sql = "DELETE FROM Regexes WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

int DatabaseManager::getRegexIdByName(const std::string &name) {
    if (!db)
        return -1;
    std::string sql = "SELECT id FROM Regexes WHERE name = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for regex lookup by name: "
                  << sqlite3_errmsg(db) << std::endl;
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

// Suspicious Words CRUD
static int suspicious_word_select_callback(void *data, int argc, char **argv,
                                           char **azColName) {
    auto *words = static_cast<std::vector<SuspiciousWord> *>(data);
    SuspiciousWord sw;
    for (int i = 0; i < argc; i++) {
        std::string colName = azColName[i];
        if (colName == "id") {
            sw.id = argv[i] ? std::stoi(argv[i]) : -1;
        } else if (colName == "word") {
            sw.word = argv[i] ? argv[i] : "";
        }
    }
    words->push_back(sw);
    return 0;
}

std::vector<SuspiciousWord> DatabaseManager::getSuspiciousWords() {
    std::vector<SuspiciousWord> words;
    if (!db)
        return words;

    std::string sql = "SELECT id, word FROM SuspiciousWords;";
    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), suspicious_word_select_callback,
                          &words, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to select SuspiciousWords: " << errmsg
                  << std::endl;
        sqlite3_free(errmsg);
    }
    return words;
}

bool DatabaseManager::addSuspiciousWord(SuspiciousWord &word) {
    if (!db)
        return false;
    std::string sql = "INSERT INTO SuspiciousWords (word) VALUES (?);";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, word.word.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return false;
    }
    word.id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return true;
}

bool DatabaseManager::updateSuspiciousWord(const SuspiciousWord &word) {
    if (!db)
        return false;
    std::string sql = "UPDATE SuspiciousWords SET word = ? WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, word.word.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, word.id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool DatabaseManager::deleteSuspiciousWord(int id) {
    if (!db)
        return false;
    std::string sql = "DELETE FROM SuspiciousWords WHERE id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

int DatabaseManager::getSuspiciousWordIdByWord(const std::string &word) {
    if (!db)
        return -1;
    std::string sql = "SELECT id FROM SuspiciousWords WHERE word = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for suspicious word lookup "
                     "by word: "
                  << sqlite3_errmsg(db) << std::endl;
        return -1;
    }
    sqlite3_bind_text(stmt, 1, word.c_str(), -1, SQLITE_STATIC);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

// Maintenance methods
bool DatabaseManager::ClearPayments() {
    if (!db)
        return false;
    // Delete details first, although ON DELETE CASCADE should handle it.
    bool success = execute("DELETE FROM PaymentDetails;");
    if (success) {
        success = execute("DELETE FROM Payments;");
    }
    if (success) {
        execute("VACUUM;");
    }
    return success;
}

bool DatabaseManager::ClearCounterparties() {
    if (!db)
        return false;
    // Note: This can fail if foreign key constraints are violated.
    // The UI should warn the user about this.
    bool success = execute("DELETE FROM Counterparties;");
    if (success)
        execute("VACUUM;");
    return success;
}

bool DatabaseManager::ClearContracts() {
    if (!db)
        return false;
    bool success = execute("DELETE FROM Contracts;");
    if (success)
        execute("VACUUM;");
    return success;
}

bool DatabaseManager::ClearInvoices() {
    if (!db)
        return false;
    bool success = execute("DELETE FROM Invoices;");
    if (success)
        execute("VACUUM;");
    return success;
}

bool DatabaseManager::CleanOrphanPaymentDetails() {
    if (!db)
        return false;
    // Deletes details for which the corresponding payment does not exist.
    bool success = execute("DELETE FROM PaymentDetails WHERE payment_id NOT IN "
                           "(SELECT id FROM Payments);");
    if (success)
        execute("VACUUM;");
    return success;
}

// Callback for generic SELECT queries
static int callback_collect_data(void *data, int argc, char **argv,
                                 char **azColName) {
    auto *result_pair =
        static_cast<std::pair<std::vector<std::string> *,
                              std::vector<std::vector<std::string>> *> *>(data);
    std::vector<std::string> *columns = result_pair->first;
    std::vector<std::vector<std::string>> *rows = result_pair->second;

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

bool DatabaseManager::executeSelect(
    const std::string &sql, std::vector<std::string> &columns,
    std::vector<std::vector<std::string>> &rows) {
    if (!db)
        return false;

    // Clear previous results
    columns.clear();
    rows.clear();

    // Use a pair to pass both vectors to the callback
    std::pair<std::vector<std::string> *,
              std::vector<std::vector<std::string>> *>
        result_pair(&columns, &rows);

    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), callback_collect_data, &result_pair,
                          &errmsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL SELECT error: " << errmsg << std::endl;
        sqlite3_free(errmsg);
        return false;
    }

    return true;
}

// Settings
Settings DatabaseManager::getSettings() {
    Settings settings = {1, "", "", "", "", 20, 0, 24}; // Default settings
    if (!db)
        return settings;

    std::string sql =
        "SELECT organization_name, period_start_date, "
        "period_end_date, note, import_preview_lines, theme, font_size FROM "
        "Settings WHERE id = 1;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for getSettings: "
                  << sqlite3_errmsg(db) << std::endl;
        return settings;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *org_name = sqlite3_column_text(stmt, 0);
        const unsigned char *start_date = sqlite3_column_text(stmt, 1);
        const unsigned char *end_date = sqlite3_column_text(stmt, 2);
        const unsigned char *note = sqlite3_column_text(stmt, 3);
        settings.import_preview_lines = sqlite3_column_int(stmt, 4);

        settings.theme = sqlite3_column_int(stmt, 5);

        settings.font_size = sqlite3_column_int(stmt, 6);

        settings.organization_name = org_name ? (const char *)org_name : "";
        settings.period_start_date = start_date ? (const char *)start_date : "";
        settings.period_end_date = end_date ? (const char *)end_date : "";
        settings.note = note ? (const char *)note : "";
    }

    sqlite3_finalize(stmt);
    return settings;
}

bool DatabaseManager::updateSettings(const Settings &settings) {
    if (!db)
        return false;

    std::string sql =
        "UPDATE Settings SET organization_name = ?, period_start_date = ?, "
        "period_end_date = ?, note = ?, import_preview_lines = ?, theme = ?, "
        "font_size = ? "
        "WHERE id = 1;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement for updateSettings: "
                  << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, settings.organization_name.c_str(), -1,
                      SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, settings.period_start_date.c_str(), -1,
                      SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, settings.period_end_date.c_str(), -1,
                      SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, settings.note.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, settings.import_preview_lines);
    sqlite3_bind_int(stmt, 6, settings.theme);

    sqlite3_bind_int(stmt, 7, settings.font_size);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to update Settings: " << sqlite3_errmsg(db)
                  << std::endl;
        return false;
    }
    return true;
}

bool DatabaseManager::backupTo(const std::string &backupFilepath) {
    if (!db) {
        return false;
    }

    sqlite3 *pBackupDb = nullptr;
    int rc = sqlite3_open(backupFilepath.c_str(), &pBackupDb);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open backup database: "
                  << sqlite3_errmsg(pBackupDb) << std::endl;
        sqlite3_close(pBackupDb);
        return false;
    }

    sqlite3_backup *pBackup =
        sqlite3_backup_init(pBackupDb, "main", db, "main");
    if (!pBackup) {
        std::cerr << "Failed to init backup: " << sqlite3_errmsg(pBackupDb)
                  << std::endl;
        sqlite3_close(pBackupDb);
        return false;
    }

    rc = sqlite3_backup_step(pBackup, -1); // -1 to copy all pages

    if (rc != SQLITE_DONE) {
        std::cerr << "Backup step failed: " << sqlite3_errmsg(pBackupDb)
                  << std::endl;
    }

    sqlite3_backup_finish(pBackup);

    // Check for errors during backup finish
    rc = sqlite3_errcode(pBackupDb);
    if (rc != SQLITE_OK) {
        std::cerr << "Backup finish failed: " << sqlite3_errmsg(pBackupDb)
                  << std::endl;
    }

    sqlite3_close(pBackupDb);

    return rc == SQLITE_OK;
}

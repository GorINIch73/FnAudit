#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>

#include "Kosgu.h"
#include "Counterparty.h"
#include "Contract.h"
#include "Payment.h"
#include "Invoice.h"
#include "PaymentDetail.h"
#include "Settings.h"
#include "Regex.h"
#include "SuspiciousWord.h"

class DatabaseManager {
public:
    DatabaseManager();
~DatabaseManager();

    bool open(const std::string& filepath);
    void close();
    bool createDatabase(const std::string& filepath);
    bool backupTo(const std::string& backupFilepath);
    bool is_open() const;

    // Settings
    Settings getSettings();
    bool updateSettings(const Settings& settings);

    std::vector<Kosgu> getKosguEntries();
    bool addKosguEntry(const Kosgu& entry);
    bool updateKosguEntry(const Kosgu& entry);
    bool deleteKosguEntry(int id);
    int getKosguIdByCode(const std::string& code);

    bool addCounterparty(Counterparty& counterparty); // Pass by reference to get the id back
    int getCounterpartyIdByNameInn(const std::string& name, const std::string& inn);
    int getCounterpartyIdByName(const std::string& name);
    std::vector<Counterparty> getCounterparties();
    bool updateCounterparty(const Counterparty& counterparty);
    bool deleteCounterparty(int id);
    std::vector<ContractPaymentInfo> getPaymentInfoForCounterparty(int counterparty_id);

    int addContract(Contract& contract); // Pass by reference to get the id back
    int getContractIdByNumberDate(const std::string& number, const std::string& date);
    std::vector<Contract> getContracts();
    bool updateContract(const Contract& contract);
    bool deleteContract(int id);
    std::vector<ContractPaymentInfo> getPaymentInfoForContract(int contract_id);

    int addInvoice(Invoice& invoice); // Pass by reference to get the id back
    int getInvoiceIdByNumberDate(const std::string& number, const std::string& date);
    std::vector<Invoice> getInvoices();
    bool updateInvoice(const Invoice& invoice);
    bool deleteInvoice(int id);
    std::vector<ContractPaymentInfo> getPaymentInfoForInvoice(int invoice_id);


    std::vector<Payment> getPayments();
    bool addPayment(Payment& payment);
    bool updatePayment(const Payment& payment);
    bool deletePayment(int id);
    std::vector<ContractPaymentInfo> getPaymentInfoForKosgu(int kosgu_id);
    std::vector<KosguPaymentDetailInfo> getAllKosguPaymentInfo();
    std::vector<ContractPaymentInfo> getAllContractPaymentInfo();

    bool addPaymentDetail(PaymentDetail& detail);
    std::vector<PaymentDetail> getPaymentDetails(int payment_id);
    std::vector<PaymentDetail> getAllPaymentDetails();
    bool updatePaymentDetail(const PaymentDetail& detail);
    bool deletePaymentDetail(int id);
    bool deleteAllPaymentDetails(int payment_id);
    bool bulkUpdatePaymentDetails(const std::vector<int>& payment_ids, const std::string& field_to_update, int new_id);

    // Regex CRUD
    std::vector<Regex> getRegexes();
    bool addRegex(Regex& regex);
    bool updateRegex(const Regex& regex);
    bool deleteRegex(int id);
    int getRegexIdByName(const std::string& name);

    // Suspicious Words CRUD
    std::vector<SuspiciousWord> getSuspiciousWords();
    bool addSuspiciousWord(SuspiciousWord& word);
    bool updateSuspiciousWord(const SuspiciousWord& word);
    bool deleteSuspiciousWord(int id);
    int getSuspiciousWordIdByWord(const std::string& word);

    // Maintenance methods
    bool ClearPayments();
    bool ClearCounterparties();
    bool ClearContracts();
    bool ClearInvoices();
    bool CleanOrphanPaymentDetails();
    
    bool executeSelect(const std::string& sql, std::vector<std::string>& columns, std::vector<std::vector<std::string>>& rows);

private:
    bool execute(const std::string& sql);
    void checkAndUpdateDatabaseSchema();
    
    sqlite3* db;
};

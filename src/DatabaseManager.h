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

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    bool open(const std::string& filepath);
    void close();
    bool createDatabase(const std::string& filepath);
    bool is_open() const;

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

    bool addContract(Contract& contract); // Pass by reference to get the id back
    int getContractIdByNumberDate(const std::string& number, const std::string& date);
    std::vector<Contract> getContracts();
    bool updateContract(const Contract& contract);
    bool deleteContract(int id);

    bool addInvoice(Invoice& invoice); // Pass by reference to get the id back
    int getInvoiceIdByNumberDate(const std::string& number, const std::string& date);
    std::vector<Invoice> getInvoices();
    bool updateInvoice(const Invoice& invoice);
    bool deleteInvoice(int id);


    std::vector<Payment> getPayments();
    bool addPayment(Payment& payment);
    bool updatePayment(const Payment& payment);
    bool deletePayment(int id);

    bool addPaymentDetail(PaymentDetail& detail);
    std::vector<PaymentDetail> getPaymentDetails(int payment_id);
    bool updatePaymentDetail(const PaymentDetail& detail);
    bool deletePaymentDetail(int id);

    // Generic SQL query execution for SELECT statements
    bool executeSelect(const std::string& sql, std::vector<std::string>& columns, std::vector<std::vector<std::string>>& rows);

private:
    bool execute(const std::string& sql);
    
    sqlite3* db;
};

# Financial Audit Application

This is a C++ application for financial auditing of a public sector organization. It uses ImGui for the graphical user interface and SQLite3 for database management.

## Features Implemented:
*   **KOSGU Management:** CRUD operations for KOSGU entries.
*   **Payments Management:** Add, Update, Delete functionality for payment records. Automatic date pre-filling for new entries.
*   **Counterparties Management:** CRUD operations for Counterparty entries. Robust import logic handles name-only lookups and NULL INN values.
*   **TSV Import:** Enhanced import functionality from TSV files. It parses payment details, automatically creates/updates Counterparties, and extracts/links Contracts and Invoices from payment descriptions.
*   **PDF Reporting:** Basic PDF generation for KOSGU, SQL query results, and Payments.
*   **SQL Query Runner:** A tool to execute arbitrary SQL SELECT queries and display results.

## Build Instructions:
This project uses CMake.
1.   `mkdir build`  `cd build`
2.   `cmake .. `
3.   `cd ..`
4.   `cmake --build build/ `

## Running the Application:
 `build/FinancialAudit`

## Install Application

 `sudo cmake --install build `

## Current Development Status:
*   Payments Form (including filters, group operations, and regex processing) - **Completed**
*   KOSGU Form (including filters and performance optimizations) - **Completed**
*   PDF Export for Payments, KOSGU, SQL Query - **Completed**
*   Counterparties CRUD UI (including robust import logic) - **Completed**
*   TSV Import (parsing Contracts and Invoices from description) - **Completed**
*   Contracts CRUD UI - **Completed**
*   Invoices CRUD UI - **Completed**

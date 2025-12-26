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
1.  Navigate to the `build` directory: `cd build`
2.  Run CMake: `cmake ..`
3.  Build the project: `make`

## Running the Application:
From the `build` directory: `./FinancialAudit`

## Current Development Status:
*   Payments Add/Update/Delete (including auto date) - **Completed**
*   PDF Export for Payments, KOSGU, SQL Query - **Completed**
*   Counterparties CRUD UI (including robust import logic) - **Completed**
*   TSV Import (parsing Contracts and Invoices from description) - **Completed**
*   Contracts CRUD UI - **In Progress**
*   Invoices CRUD UI - **Pending**
*   Refactor Payments Form with dropdowns - **Pending**

# GEMINI Project Analysis: Financial Audit Application

## Project Overview

This is a C++ desktop application designed for financial auditing within a public sector context. It provides a graphical user interface for managing and analyzing financial data, including payments, counterparties, contracts, and invoices.

The application is built using C++17 and leverages several open-source libraries:
*   **User Interface:** [ImGui](https://github.com/ocornut/imgui) (docking branch) is used for the entire graphical user interface, rendered with OpenGL.
*   **Windowing & Input:** [GLFW](https://www.glfw.org/) is used for creating and managing the application window and handling user input.
*   **Database:** [SQLite3](https://www.sqlite.org/) is used for all data storage. The application interacts with the database directly through the C API.
*   **PDF Generation:** The application includes functionality to generate PDF reports.

The architecture is modular, centered around a `UIManager` that manages various "views" (e.g., `PaymentsView`, `CounterpartiesView`). A `DatabaseManager` encapsulates all SQL interactions, providing a clear data access layer for the rest of the application. Other managers handle specific tasks like data import (`ImportManager`) and PDF creation (`PdfReporter`).

## Building and Running

The project is built using CMake.

### Prerequisites
*   A C++17 compliant compiler (e.g., GCC, Clang)
*   CMake (version 3.20 or higher)
*   The following libraries must be installed on the system:
    *   glfw3
    *   OpenGL
    *   SQLite3
    *   X11

### Build Steps

1.  **Create a build directory:**
    ```bash
    mkdir build
    cd build
    ```

2.  **Configure the project with CMake:**
    ```bash
    cmake ..
    ```

3.  **Compile the project:**
    ```bash
    make
    ```

### Running the Application

After a successful build, the executable will be located in the `build` directory.

```bash
./FinancialAudit
```

## Development Conventions

*   **Language:** C++17.
*   **Dependencies:** External dependencies like ImGui are managed using CMake's `FetchContent`. System-level dependencies (GLFW, OpenGL, SQLite3) are expected to be installed and are found using `find_package`.
*   **Architecture:** The UI is organized into `View` classes (e.g., `PaymentsView`, `ContractsView`) that inherit from a `BaseView`. These views are instantiated and managed by a central `UIManager`. All database operations are handled by a dedicated `DatabaseManager` class, which is passed to the views that require data access.
*   **File Structure:**
    *   Header files (`.h`) are located in `src/` and `src/views/`.
    *   Implementation files (`.cpp`) are located in `src/` and `src/views/`.
    *   The main application entry point is `src/main.cpp`.
    *   Data models (structs representing database tables) are defined in their own headers (e.g., `src/Payment.h`, `src/Contract.h`).
*   **Data Handling:** The application works with a single SQLite database file (`.db`). Users can create a new database or open an existing one. The application keeps a list of recently used database files for quick access.
*   **UI:** The UI is built with ImGui and is organized into a main menu bar and multiple dockable windows, each managed by a `View` class. Font Awesome icons are integrated into the UI for better visual cues.

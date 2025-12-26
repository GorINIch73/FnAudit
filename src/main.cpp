#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>

#include "UIManager.h"
#include "DatabaseManager.h"
#include "ImGuiFileDialog.h"
#include "ImportManager.h"
#include "PdfReporter.h"

// Функция обратного вызова для ошибок GLFW
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void SetWindowTitle(GLFWwindow* window, const std::string& db_path) {
    std::string title = "Financial Audit Application";
    if (!db_path.empty()) {
        title += " - [" + db_path + "]";
    }
    glfwSetWindowTitle(window, title.c_str());
}

int main(int, char**) {
    // Установка обработчика ошибок GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    // Задаём версию OpenGL (3.3 Core)
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Для macOS

    // Создание окна
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Financial Audit Application", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Включить V-Sync

    // --- Настройка ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Включить навигацию с клавиатуры
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Включить докинг

    // Установка стиля ImGui
    ImGui::StyleColorsDark();

    // Загрузка шрифта с поддержкой кириллицы (Roboto)
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    ImFont* font = io.Fonts->AddFontFromFileTTF("../data/Roboto-Regular.ttf", 16.0f, &font_cfg, io.Fonts->GetGlyphRangesCyrillic());
    IM_ASSERT(font != NULL);

    // Инициализация бэкендов для GLFW и OpenGL
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // --- Создание менеджеров ---
    UIManager uiManager;
    DatabaseManager dbManager;
    ImportManager importManager;
    PdfReporter pdfReporter;
    uiManager.SetDatabaseManager(&dbManager);
    uiManager.SetPdfReporter(&pdfReporter);
    std::string currentDbPath;


    // Главный цикл приложения
    while (!glfwWindowShouldClose(window)) {
        // Обработка событий
        glfwPollEvents();

        // Начало нового кадра ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Включаем возможность докинга
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // --- Рендеринг главного меню ---
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Файл")) {
                if (ImGui::MenuItem("Создать новую базу")) {
                    ImGuiFileDialog::Instance()->OpenDialog("ChooseDbFileDlgKey", "Выберите файл для новой базы", ".db");
                }
                if (ImGui::MenuItem("Открыть фай базы")) {
                    ImGuiFileDialog::Instance()->OpenDialog("OpenDbFileDlgKey", "Выберите файл базы данных", ".db");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Выход")) {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Справочники")) {
                if (ImGui::MenuItem("КОСГУ")) {
                    uiManager.ShowKosguWindow = true;
                }
                if (ImGui::MenuItem("Банк")) {
                    uiManager.ShowPaymentsWindow = true;
                }
                if (ImGui::MenuItem("Контрагенты")) {
                    uiManager.ShowCounterpartiesWindow = true;
                }
                if (ImGui::MenuItem("Договоры")) {
                    uiManager.ShowContractsWindow = true;
                }
                if (ImGui::MenuItem("Накладные")) {
                    uiManager.ShowInvoicesWindow = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Импорт")) {
                if (ImGui::MenuItem("Импорт из TSV")) {
                    ImGuiFileDialog::Instance()->OpenDialog("ImportTsvFileDlgKey", "Выберите TSV файл для импорта", ".tsv");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Отчеты")) {
                if (ImGui::MenuItem("SQL Запрос")) {
                    uiManager.ShowSqlQueryWindow = true;
                }
                if (ImGui::MenuItem("Экспорт в PDF")) {
                    ImGuiFileDialog::Instance()->OpenDialog("SavePdfFileDlgKey", "Сохранить отчет в PDF", ".pdf");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // --- Обработка диалогов выбора файлов ---
        if (ImGuiFileDialog::Instance()->Display("ChooseDbFileDlgKey")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                if (dbManager.createDatabase(filePathName)) {
                    currentDbPath = filePathName;
                    SetWindowTitle(window, currentDbPath);
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }
        if (ImGuiFileDialog::Instance()->Display("OpenDbFileDlgKey")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                if (dbManager.open(filePathName)) {
                    currentDbPath = filePathName;
                    SetWindowTitle(window, currentDbPath);
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }
        if (ImGuiFileDialog::Instance()->Display("ImportTsvFileDlgKey")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                // Check if a database is open before importing
                if (dbManager.is_open()) { // Assuming dbManager has an is_open() method
                    importManager.ImportPaymentsFromTsv(filePathName, &dbManager);
                    // After import, refresh any relevant UI data
                } else {
                    // Show error message: No database open
                    // ImGui::OpenPopup("NoDbOpenError"); // Example error handling
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }
        if (ImGuiFileDialog::Instance()->Display("SavePdfFileDlgKey")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                // Placeholder for actual data, can be from current view or a specific report
                std::vector<std::string> dummy_columns = {"Column1", "Column2"};
                std::vector<std::vector<std::string>> dummy_rows = {{"Data1", "Data2"}, {"Data3", "Data4"}};
                pdfReporter.generatePdfFromTable(filePathName, "Sample Report", dummy_columns, dummy_rows);
            }
            ImGuiFileDialog::Instance()->Close();
        }


        // --- Рендеринг окон через UIManager ---
        uiManager.Render();

        // Рендеринг
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Обновление и отрисовка окна
        glfwSwapBuffers(window);
    }

    // --- Очистка ресурсов ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
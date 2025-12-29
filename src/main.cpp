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
#include "IconsFontAwesome6.h"

// Функция обратного вызова для ошибок GLFW
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
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
    io.Fonts->AddFontFromFileTTF("../data/Roboto-Regular.ttf", 16.0f, &font_cfg, io.Fonts->GetGlyphRangesCyrillic());
    
    // Объединение с иконочным шрифтом
    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    io.Fonts->AddFontFromFileTTF("../data/fa-solid-900.otf", 16.0f, &config, icon_ranges);

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
    uiManager.SetImportManager(&importManager);
    uiManager.SetWindow(window);


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
            if (ImGui::BeginMenu(ICON_FA_FILE " Файл")) {
                if (ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS " Создать новую базу")) {
                    ImGuiFileDialog::Instance()->OpenDialog("ChooseDbFileDlgKey", "Выберите файл для новой базы", ".db");
                }
                if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Открыть фай базы")) {
                    ImGuiFileDialog::Instance()->OpenDialog("OpenDbFileDlgKey", "Выберите файл базы данных", ".db");
                }
                if (ImGui::BeginMenu(ICON_FA_CLOCK_ROTATE_LEFT " Недавние файлы")) {
                    for (const auto& path : uiManager.recentDbPaths) {
                        if (ImGui::MenuItem(path.c_str())) {
                            if (dbManager.open(path)) {
                                uiManager.currentDbPath = path;
                                uiManager.SetWindowTitle(uiManager.currentDbPath);
                                uiManager.AddRecentDbPath(path);
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_ARROW_RIGHT_FROM_BRACKET " Выход")) {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(ICON_FA_BOOK " Справочники")) {
                if (ImGui::MenuItem(ICON_FA_HASHTAG " КОСГУ")) {
                    uiManager.kosguView.IsVisible = true;
                }
                if (ImGui::MenuItem(ICON_FA_BUILDING_COLUMNS " Банк")) {
                    uiManager.paymentsView.IsVisible = true;
                }
                if (ImGui::MenuItem(ICON_FA_ADDRESS_BOOK " Контрагенты")) {
                    uiManager.counterpartiesView.IsVisible = true;
                }
                if (ImGui::MenuItem(ICON_FA_FILE_CONTRACT " Договоры")) {
                    uiManager.contractsView.IsVisible = true;
                }
                if (ImGui::MenuItem(ICON_FA_FILE_INVOICE " Накладные")) {
                    uiManager.invoicesView.IsVisible = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(ICON_FA_FILE_IMPORT " Импорт")) {
                if (ImGui::MenuItem(ICON_FA_TABLE_CELLS " Импорт из TSV")) {
                    ImGuiFileDialog::Instance()->OpenDialog("ImportTsvFileDlgKey", "Выберите TSV файл для импорта", ".tsv");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(ICON_FA_FILE_PDF " Отчеты")) {
                if (ImGui::MenuItem(ICON_FA_DATABASE " SQL Запрос")) {
                    uiManager.sqlQueryView.IsVisible = true;
                }
                if (ImGui::MenuItem(ICON_FA_FILE_EXPORT " Экспорт в PDF")) {
                    ImGuiFileDialog::Instance()->OpenDialog("SavePdfFileDlgKey", "Сохранить отчет в PDF", ".pdf");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // --- Обработка диалогов выбора файлов ---
        uiManager.HandleFileDialogs();


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
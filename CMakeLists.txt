# Задаём минимальную требуемую версию CMake
cmake_minimum_required(VERSION 3.20)

# Определяем проект
project(FinancialAudit LANGUAGES C CXX)

# Устанавливаем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Включаем FetchContent для управления зависимостями
include(FetchContent)

# --- Зависимости ---

# 1. ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

# 2. Поиск системных библиотек
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(X11 REQUIRED)

# --- Исходные файлы ---

# Create a static library for ImGui
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)

target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/cpp
)

set(APP_SOURCES
    src/main.cpp
    src/UIManager.cpp
    src/DatabaseManager.cpp
    src/ImGuiFileDialog.cpp
    src/ImportManager.cpp
    src/PdfReporter.cpp
    src/pdfgen.c
    src/CustomWidgets.cpp
    src/views/PaymentsView.cpp
    src/views/KosguView.cpp
    src/views/CounterpartiesView.cpp
    src/views/ContractsView.cpp
    src/views/InvoicesView.cpp
    src/views/SqlQueryView.cpp
    src/views/SettingsView.cpp
    src/views/ImportMapView.cpp
    src/views/RegexesView.cpp
    src/views/SelectiveCleanView.cpp
    src/views/SuspiciousWordsView.cpp
    src/views/SpecialQueryView.cpp
)

set_source_files_properties(src/pdfgen.c PROPERTIES LANGUAGE C)

# Добавляем исполняемый файл
add_executable(${PROJECT_NAME} ${APP_SOURCES})

# --- Подключение библиотек ---

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/views
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Подключаем все необходимые библиотеки к исполняемому файлу
target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    OpenGL::GL
    SQLite::SQLite3
    X11::X11
    imgui_lib
)

#!/bin/bash
# Скрипт установки Financial Audit Application в Linux
# Требуются права root для системной установки

set -e

APP_NAME="FinancialAudit"
DESKTOP_FILE="FnAudit.desktop"
ICON_FILE="FnAudit.svg"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr}"

echo "=== Установка Financial Audit Application ==="

# Определяем директорию установки
if [ "$EUID" -eq 0 ]; then
    INSTALL_DIR="$INSTALL_PREFIX/bin"
    ICON_DIR="$INSTALL_PREFIX/share/icons/hicolor/scalable/apps"
    DESKTOP_DIR="$INSTALL_PREFIX/share/applications"
    echo "Системная установка (требуются права root)"
else
    INSTALL_DIR="$HOME/.local/bin"
    ICON_DIR="$HOME/.local/share/icons/hicolor/scalable/apps"
    DESKTOP_DIR="$HOME/.local/share/applications"
    echo "Локальная установка (без прав root)"
fi

# Создаём директории
echo "Создание директорий..."
mkdir -p "$INSTALL_DIR"
mkdir -p "$ICON_DIR"
mkdir -p "$DESKTOP_DIR"

# Копируем исполняемый файл
if [ -f "build/$APP_NAME" ]; then
    echo "Копирование исполняемого файла в $INSTALL_DIR..."
    cp "build/$APP_NAME" "$INSTALL_DIR/"
    chmod +x "$INSTALL_DIR/$APP_NAME"
else
    echo "Ошибка: исполняемый файл build/$APP_NAME не найден!"
    echo "Сначала выполните сборку проекта."
    exit 1
fi

# Копируем иконку
if [ -f "data/$ICON_FILE" ]; then
    echo "Копирование иконки в $ICON_DIR..."
    cp "data/$ICON_FILE" "$ICON_DIR/$APP_NAME.svg"
else
    echo "Предупреждение: иконка не найдена, пропускаем..."
fi

# Устанавливаем .desktop файл
if [ -f "$DESKTOP_FILE" ]; then
    echo "Установка .desktop файла в $DESKTOP_DIR..."
    cp "$DESKTOP_FILE" "$DESKTOP_DIR/"
    
    # Обновляем путь к иконке в .desktop файле
    sed -i "s|Icon=$APP_NAME|Icon=$APP_NAME|" "$DESKTOP_DIR/$DESKTOP_FILE"
else
    echo "Ошибка: .desktop файл не найден!"
    exit 1
fi

# Обновляем кэш иконок и меню приложений
if [ "$EUID" -eq 0 ]; then
    echo "Обновление кэша иконок..."
    if command -v gtk-update-icon-cache &> /dev/null; then
        gtk-update-icon-cache -f "$INSTALL_PREFIX/share/icons/hicolor" 2>/dev/null || true
    fi
    
    echo "Обновление кэша приложений..."
    if command -v update-desktop-database &> /dev/null; then
        update-desktop-database "$INSTALL_PREFIX/share/applications" 2>/dev/null || true
    fi
else
    echo "Для локальной установки может потребоваться перезапуск сессии"
    echo "для отображения приложения в меню."
fi

echo ""
echo "=== Установка завершена ==="
echo "Исполняемый файл: $INSTALL_DIR/$APP_NAME"
echo "Иконка: $ICON_DIR/$APP_NAME.svg"
echo "Desktop файл: $DESKTOP_DIR/$DESKTOP_FILE"
echo ""
echo "Теперь приложение доступно в меню приложений и dmenu/rofi"

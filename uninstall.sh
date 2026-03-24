#!/bin/bash
# Скрипт удаления Financial Audit Application из Linux

set -e

APP_NAME="FinancialAudit"
DESKTOP_FILE="FnAudit.desktop"
ICON_FILE="FnAudit.svg"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr}"

echo "=== Удаление Financial Audit Application ==="

# Определяем директорию установки
if [ "$EUID" -eq 0 ]; then
    INSTALL_DIR="$INSTALL_PREFIX/bin"
    ICON_DIR="$INSTALL_PREFIX/share/icons/hicolor/scalable/apps"
    DESKTOP_DIR="$INSTALL_PREFIX/share/applications"
    echo "Системное удаление (требуются права root)"
else
    INSTALL_DIR="$HOME/.local/bin"
    ICON_DIR="$HOME/.local/share/icons/hicolor/scalable/apps"
    DESKTOP_DIR="$HOME/.local/share/applications"
    echo "Локальное удаление (без прав root)"
fi

# Удаляем файлы
echo "Удаление исполняемого файла..."
rm -f "$INSTALL_DIR/$APP_NAME"

echo "Удаление иконки..."
rm -f "$ICON_DIR/$APP_NAME.svg"

echo "Удаление .desktop файла..."
rm -f "$DESKTOP_DIR/$DESKTOP_FILE"

# Обновляем кэш
if [ "$EUID" -eq 0 ]; then
    echo "Обновление кэша иконок..."
    if command -v gtk-update-icon-cache &> /dev/null; then
        gtk-update-icon-cache -f "$INSTALL_PREFIX/share/icons/hicolor" 2>/dev/null || true
    fi
    
    echo "Обновление кэша приложений..."
    if command -v update-desktop-database &> /dev/null; then
        update-desktop-database "$INSTALL_PREFIX/share/applications" 2>/dev/null || true
    fi
fi

echo ""
echo "=== Удаление завершено ==="
echo "Приложение удалено из системы"

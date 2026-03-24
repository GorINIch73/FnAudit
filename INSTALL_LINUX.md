# Установка в Linux

## Автоматическая установка

### Системная установка (требуются права root)

```bash
# Сборка проекта
cd /path/to/FnAudit
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Установка в систему
sudo ./install.sh
```

Или используя CMake:

```bash
sudo cmake --install build
```

После установки приложение будет доступно:
- В меню приложений графической оболочки
- В поиске dmenu/rofi (нажмите `Mod+d` и введите "Financial" или "Финансовый")
- По команде `FinancialAudit` в терминале

### Локальная установка (без прав root)

```bash
./install.sh
```

Приложение будет установлено в `~/.local/` и доступно после перезапуска сессии.

---

## Ручная установка

### 1. Скопируйте исполняемый файл

```bash
sudo cp build/FinancialAudit /usr/local/bin/
```

### 2. Установите иконку

```bash
sudo cp data/FnAudit.svg /usr/local/share/icons/hicolor/scalable/apps/FinancialAudit.svg
```

### 3. Установите .desktop файл

```bash
sudo cp FnAudit.desktop /usr/local/share/applications/
```

### 4. Обновите кэши

```bash
sudo gtk-update-icon-cache -f /usr/local/share/icons/hicolor
sudo update-desktop-database /usr/local/share/applications
```

---

## Удаление

### Автоматическое удаление

```bash
# Системное
sudo ./uninstall.sh

# Локальное
./uninstall.sh
```

### Ручное удаление

```bash
sudo rm /usr/local/bin/FinancialAudit
sudo rm /usr/local/share/icons/hicolor/scalable/apps/FinancialAudit.svg
sudo rm /usr/local/share/applications/FnAudit.desktop
```

---

## Проверка установки

### Проверка .desktop файла

```bash
cat /usr/local/share/applications/FnAudit.desktop
```

### Проверка иконки

```bash
ls -la /usr/local/share/icons/hicolor/scalable/apps/FinancialAudit.svg
```

### Запуск приложения

```bash
FinancialAudit
```

Или найдите в меню приложений:
- GNOME: Нажмите `Super` (Win) и введите "Financial" или "Финансовый"
- KDE: Меню приложений → Категория "Офис" или "Финансы"
- XFCE: Меню → Офис → Financial Audit
- dmenu: `Mod+d` → введите "financial"
- rofi: `Mod+d` → введите "financial"

Вы абсолютно правы, это важный момент!

Для того чтобы получить имя выбранного REGEX из комбобокса, я использую уже существующие в коде переменные:
1.  `selectedRegexIdForCreatePopup`: Это переменная, в которой хранится `id` текущего выбранного элемента в выпадающем списке "Выбор Regex". Она обновляется каждый раз, когда вы выбираете новый элемент.
2.  `regexesForCreatePopup`: Это `std::vector<Regex>`, который содержит полный список всех REGEX выражений (с их `id`, `name` и `pattern`), загруженных из базы данных при открытии диалога "Создать из назначения".

Логика, которую я предложил для получения имени, выглядит так:
```cpp
// Находим имя выбранного REGEX, чтобы предварительно заполнить поле ввода
std::string original_name = "";
// Проверяем, что что-то выбрано в комбобоксе
if (selectedRegexIdForCreatePopup != -1) {
    // Ищем объект Regex в векторе regexesForCreatePopup,
    // ID которого совпадает с выбранным ID (selectedRegexIdForCreatePopup)
    auto it = std::find_if(regexesForCreatePopup.begin(), regexesForCreatePopup.end(),
                           [&](const Regex& r){ return r.id == selectedRegexIdForCreatePopup; });
    
    // Если объект найден
    if (it != regexesForCreatePopup.end()) {
        original_name = it->name; // Извлекаем его имя
    }
}
// Затем original_name копируется в newRegexNameBuffer
// strncpy(newRegexNameBuffer, original_name.c_str(), sizeof(newRegexNameBuffer) - 1);
// newRegexNameBuffer[sizeof(newRegexNameBuffer) - 1] = '\0';
```
Эта часть кода:
*   Ищет в списке `regexesForCreatePopup` тот REGEX, `id` которого соответствует `selectedRegexIdForCreatePopup` (ID выбранного вами REGEX из комбобокса).
*   Когда нужный REGEX найден, из него извлекается поле `name`.
*   Это имя затем используется для предварительного заполнения буфера `newRegexNameBuffer`, который отображается в поле ввода имени в новом диалоге.

Таким образом, мы используем `id` из комбобокса для поиска полного объекта `Regex` и извлекаем его `name`.

Хотите, чтобы я применил это изменение? Оно должно правильно предварительно заполнять имя в диалоге "Сохранить новое Regex выражение".
Для преобразования `int` в `std::string` в C++ есть несколько стандартных способов:

**1. Использование `std::to_string` (рекомендуется для C++11 и выше):**
Это самый простой и часто используемый способ.

```cpp
#include <string>
#include <iostream>

int main() {
    int number = 12345;
    std::string str_number = std::to_string(number);
    std::cout << "Число как строка: " << str_number << std::endl; // Вывод: Число как строка: 12345
    return 0;
}
```

**2. Использование `std::ostringstream` (для более сложного форматирования):**
Этот метод позволяет форматировать число перед преобразованием в строку, аналогично работе с `std::cout`, но в строку.

```cpp
#include <string>
#include <sstream> // Для std::ostringstream
#include <iostream>
#include <iomanip> // Для форматирования, например std::fixed, std::setprecision

int main() {
    int number = 123;
    double floating_point = 123.456;

    // Преобразование int
    std::ostringstream oss_int;
    oss_int << number;
    std::string str_number = oss_int.str();
    std::cout << "Число как строка: " << str_number << std::endl; // Вывод: Число как строка: 123

    // Преобразование с форматированием (например, для double)
    std::ostringstream oss_double;
    oss_double << std::fixed << std::setprecision(2) << floating_point;
    std::string str_double = oss_double.str();
    std::cout << "Дробное число как строка: " << str_double << std::endl; // Вывод: Дробное число как строка: 123.46
    
    return 0;
}
```

Для вашего проекта, где вы уже используете C++17, `std::to_string` будет самым подходящим и читаемым вариантом для простого преобразования.
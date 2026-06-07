# Шаблон интеграции C++ в Python через Pybind11

## Требования

### Python

* Python 3.8+
* Поддержка `venv`

### Компилятор

| Платформа | Требование                       |
| --------- | -------------------------------- |
| Linux     | GCC/G++ или Clang                |
| macOS     | Xcode Command Line Tools         |
| Windows   | MSVC (Visual Studio Build Tools) |

### Системные библиотеки

#### Ubuntu / Debian

```bash
sudo apt install python3-dev build-essential
```

#### Fedora / RHEL

```bash
sudo dnf install python3-devel gcc-c++
```

#### macOS (Homebrew)

```bash
xcode-select --install
brew install python3
```

#### Windows

* Visual Studio Build Tools
* Python Development Headers

### Python-пакеты

Устанавливаются внутри виртуального окружения:

```bash
pip install pybind11 numpy
```

---

# Подготовка и быстрый старт (Linux/macOS)

## 1. Установка системных зависимостей

### Ubuntu / Debian

```bash
sudo apt install python3-dev python3-venv build-essential
```

### Fedora

```bash
sudo dnf install python3-devel gcc-c++
```

---

## 2. Создание виртуального окружения

```bash
python3 -m venv venv
```

---

## 3. Активация виртуального окружения

### Linux / macOS

```bash
source venv/bin/activate
```

### Windows

```cmd
venv\Scripts\activate
```

---

## 4. Установка pybind11 и NumPy

```bash
pip install pybind11 numpy
```

---

## 5. Компиляция C++ модуля

> Важно: компиляция должна выполняться при активированном виртуальном окружении.

```bash
c++ -O3 -Wall -shared -std=c++11 -fPIC \
    $(python3 -m pybind11 --includes) \
    example.cpp \
    -o example$(python3-config --extension-suffix)
```

---

## 6. Запуск тестов

```bash
python test.py
```

---

# Что делает команда компиляции

```bash
c++ -O3 -Wall -shared -std=c++11 -fPIC \
    $(python3 -m pybind11 --includes) \
    example.cpp \
    -o example$(python3-config --extension-suffix)
```

| Параметр                            | Назначение                                             |
| ----------------------------------- | ------------------------------------------------------ |
| `-O3`                               | Максимальная оптимизация                               |
| `-Wall`                             | Показывать предупреждения компилятора                  |
| `-shared`                           | Создать динамическую библиотеку для Python             |
| `-std=c++11`                        | Использовать стандарт C++11                            |
| `-fPIC`                             | Генерация позиционно-независимого кода                 |
| `$(python3 -m pybind11 --includes)` | Пути к заголовкам Python и Pybind11                    |
| `example.cpp`                       | Исходный файл                                          |
| `python3-config --extension-suffix` | Автоматически подставляет правильное расширение модуля |

---

# Примечания

### Python.h

Пакет `python3-dev` (или `python3-devel`) содержит заголовочные файлы Python, включая:

```cpp
#include <Python.h>
```

Без них невозможно собрать любое C/C++ расширение для Python.

---

### Использование venv

Команда

```bash
python3 -m pybind11 --includes
```

берёт пути к заголовочным файлам из текущего активного виртуального окружения.

---

### Автоматическое расширение модуля

Команда

```bash
python3-config --extension-suffix
```

может вернуть, например:

```text
.cpython-313-x86_64-linux-gnu.so
```

или на macOS:

```text
.dylib
```

Поэтому итоговый файл будет иметь корректное имя для конкретной версии Python и платформы.

---

### Windows

Для Windows обычно используется компилятор:

```text
cl.exe
```

из Visual Studio Build Tools.

Инструкция по сборке отличается и обычно приводится отдельно.

---

# Частые ошибки

## Python.h не найден

Ошибка:

```text
fatal error: Python.h: No such file or directory
```

Причина:

Не установлен пакет разработки Python.

Решение:

### Ubuntu / Debian

```bash
sudo apt install python3-dev
```

### Fedora

```bash
sudo dnf install python3-devel
```

---

## Не найден pybind11

Ошибка:

```text
ModuleNotFoundError: No module named 'pybind11'
```

Причина:

Pybind11 не установлен в текущем виртуальном окружении.

Решение:

```bash
pip install pybind11
```

---

## Undefined symbol: PyExc_ValueError

Ошибка:

```text
undefined symbol: PyExc_ValueError
```

Причина:

Модуль собран без флага:

```bash
-shared
```

Решение:

Пересобрать модуль с корректными параметрами компиляции.

---

## Python не видит модуль

Ошибка:

```text
ModuleNotFoundError: No module named 'example'
```

Проверьте:

* Модуль успешно скомпилировался.
* Файл `.so` (Linux/macOS) или `.pyd` (Windows) находится в текущей директории.
* Имя модуля в `PYBIND11_MODULE(...)` совпадает с именем файла.
* Вы запускаете Python из правильной директории.

---

# Минимальная структура проекта

```text
project/
├── venv/
├── example.cpp
├── test.py
└── README.md
```

После сборки:

```text
project/
├── venv/
├── example.cpp
├── example.cpython-313-x86_64-linux-gnu.so
├── test.py
└── README.md
```

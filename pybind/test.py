"""
test.py - Тесты для скомпилированного pybind11 модуля
ЗАПУСКАТЬ ПОСЛЕ КОМПИЛЯЦИИ example.cpp
"""

import sys
import os

# Добавляем текущую директорию в путь (на случай, если .so/.pyd здесь)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    import example
    print("✓ Модуль 'example' успешно импортирован\n")
except ImportError as e:
    print("✗ Ошибка импорта модуля 'example':", e)
    print("\nСначала скомпилируйте C++ код!")
    print("Инструкции в example.py или readme.txt")
    sys.exit(1)

# Попробуем импортировать numpy (опционально)
try:
    import numpy as np
    HAS_NUMPY = True
    print("✓ Numpy доступен\n")
except ImportError:
    HAS_NUMPY = False
    print("⚠ Numpy не установлен (pip install numpy). Тесты numpy будут пропущены.\n")


def test_section(title):
    """Декоратор-разделитель для красивого вывода"""
    print("=" * 60)
    print(f"  {title}")
    print("=" * 60)


# ================================================================
# ТЕСТ 1: Простые функции
# ================================================================
test_section("ТЕСТ 1: ПРОСТЫЕ ФУНКЦИИ")

assert example.add(5, 3) == 8
print(f"  add(5, 3) = {example.add(5, 3)}")

assert abs(example.multiply(2.5, 4.0) - 10.0) < 1e-10
print(f"  multiply(2.5, 4.0) = {example.multiply(2.5, 4.0)}")

greeting = example.greet("Python")
assert "Python" in greeting
print(f"  greet('Python') = {greeting}")

assert example.is_positive(10) == True
assert example.is_positive(-5) == False
print(f"  is_positive(10) = {example.is_positive(10)}")
print(f"  is_positive(-5) = {example.is_positive(-5)}")

print("  ✓ Все тесты пройдены\n")


# ================================================================
# ТЕСТ 2: Передача через строки (тупой, но надёжный способ)
# ================================================================
test_section("ТЕСТ 2: ПЕРЕДАЧА ДАННЫХ ЧЕРЕЗ СТРОКИ")

# Пример 1: массив как строка с разделителями
input_str = "1.0,2.0,3.0,4.5"
result_str = example.process_array_via_string(input_str)
print(f"  process_array_via_string('{input_str}') = '{result_str}'")
# Проверяем
values = [float(x) for x in result_str.split(",")]
expected = [2.0, 4.0, 6.0, 9.0]
assert all(abs(a - b) < 1e-10 for a, b in zip(values, expected))

# Пример 2: матрица через строку (CSV)
matrix_str = "1,2;3,4;5,6"
result_mat = example.matrix_multiply_string(matrix_str, 10.0)
print(f"  matrix_multiply_string('{matrix_str}', 10) = '{result_mat}'")
assert result_mat == "10,20;30,40;50,60"

print("  ✓ Все тесты пройдены\n")


# ================================================================
# ТЕСТ 3: STL контейнеры
# ================================================================
test_section("ТЕСТ 3: STL КОНТЕЙНЕРЫ (list ↔ vector)")

# square_vector
input_list = [1, 2, 3, 4, 5]
result_list = example.square_vector(input_list)
print(f"  square_vector({input_list}) = {result_list}")
assert result_list == [1, 4, 9, 16, 25]

# count_words
words = ["apple", "banana", "apple", "cherry", "banana", "apple"]
word_counts = example.count_words(words)
print(f"  count_words({words}) = {word_counts}")
assert word_counts == {"apple": 3, "banana": 2, "cherry": 1}

# sum_of_list
values = [1.5, 2.5, 3.0]
total = example.sum_of_list(values)
print(f"  sum_of_list({values}) = {total}")
assert abs(total - 7.0) < 1e-10

print("  ✓ Все тесты пройдены\n")


# ================================================================
# ТЕСТ 4: Numpy массивы (если numpy доступен)
# ================================================================
if HAS_NUMPY:
    test_section("ТЕСТ 4: NUMPY МАССИВЫ (эффективная передача)")
    
    # Умножение на скаляр
    arr = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float64)
    result = example.numpy_multiply_scalar(arr, 5.0)
    print(f"  numpy_multiply_scalar({arr}, 5.0) = {result}")
    assert np.allclose(result, np.array([5.0, 10.0, 15.0, 20.0]))
    
    # Транспонирование матрицы
    matrix = np.array([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]], dtype=np.float64)
    transposed = example.numpy_matrix_transpose(matrix)
    print(f"  numpy_matrix_transpose:\n{matrix}\n  ->\n{transposed}")
    assert np.allclose(transposed, matrix.T)
    
    # In-place модификация
    arr_inplace = np.array([10.0, 20.0, 30.0], dtype=np.float64)
    print(f"  Исходный массив: {arr_inplace}")
    example.numpy_add_inplace(arr_inplace, 5.0)
    print(f"  После numpy_add_inplace(5.0): {arr_inplace}")
    assert np.allclose(arr_inplace, [15.0, 25.0, 35.0])
    
    # Генерация массива
    arange_arr = example.numpy_arange(0.0, 1.0, 0.25)
    print(f"  numpy_arange(0, 1, 0.25) = {arange_arr}")
    assert np.allclose(arange_arr, np.array([0.0, 0.25, 0.5, 0.75]))
    
    print("  ✓ Все тесты numpy пройдены\n")
else:
    test_section("ТЕСТ 4: NUMPY МАССИВЫ — ПРОПУЩЕН (numpy не установлен)\n")


# ================================================================
# ТЕСТ 5: Классы
# ================================================================
test_section("ТЕСТ 5: КЛАССЫ")

calc = example.Calculator()
print(f"  Создан Calculator: {calc}")

# Методы
sum_result = calc.add(10.0, 20.0)
print(f"  calc.add(10, 20) = {sum_result}")
assert abs(sum_result - 30.0) < 1e-10

mul_result = calc.multiply(7.0, 3.0)
print(f"  calc.multiply(7, 3) = {mul_result}")
assert abs(mul_result - 21.0) < 1e-10

# Память
calc.memory_store(42.0)
mem = calc.memory_recall()
print(f"  Сохранили 42.0 в память, memory_recall() = {mem}")
assert abs(mem - 42.0) < 1e-10

# История
history = calc.get_history()
print(f"  История вычислений: {history}")
assert len(history) == 2  # два вызова add и multiply

calc.clear_history()
assert len(calc.get_history()) == 0
print(f"  После clear_history(): {calc.get_history()}")

# Статический метод
sin_result = example.Calculator.sin_deg(30.0)
print(f"  Calculator.sin_deg(30) = {sin_result}")
assert abs(sin_result - 0.5) < 1e-10

# Оператор вызова
call_result = calc(3.0, 4.0)  # 3² + 4² = 25
print(f"  calc(3, 4) = {call_result} (должно быть 25)")
assert abs(call_result - 25.0) < 1e-10

print("  ✓ Все тесты классов пройдены\n")


# ================================================================
# ТЕСТ 6: Структуры
# ================================================================
test_section("ТЕСТ 6: СТРУКТУРЫ (Point)")

p1 = example.Point(3.0, 4.0, 0.0)
print(f"  p1 = {p1}")
magnitude = p1.magnitude()
print(f"  p1.magnitude() = {magnitude}")
assert abs(magnitude - 5.0) < 1e-10

p2 = example.Point(1.0, 2.0, 3.0)
p3 = p1 + p2
print(f"  p1 + p2 = {p3}")
assert abs(p3.x - 4.0) < 1e-10
assert abs(p3.y - 6.0) < 1e-10
assert abs(p3.z - 3.0) < 1e-10

print("  ✓ Все тесты Point пройдены\n")


# ================================================================
# ТЕСТ 7: Исключения
# ================================================================
test_section("ТЕСТ 7: ИСКЛЮЧЕНИЯ")

result = example.safe_divide(10.0, 2.0)
print(f"  safe_divide(10, 2) = {result}")
assert abs(result - 5.0) < 1e-10

try:
    example.safe_divide(10.0, 0.0)
    print("  ✗ Должно было выбросить исключение!")
except ValueError as e:
    print(f"  ✓ Исключение поймано: {e}")

print()


# ================================================================
# ТЕСТ 8: Callback функции
# ================================================================
test_section("ТЕСТ 8: CALLBACK (передача Python-функций в C++)")

# Простая функция
result = example.apply_function(lambda x: x * x, 5.0)
print(f"  apply_function(lambda x: x², 5.0) = {result}")
assert abs(result - 25.0) < 1e-10

# map функция
import math
result_list = example.map_function(lambda x: math.sin(x), [0.0, 1.57079632679, 3.14159265359])
print(f"  map_function(sin, [0, π/2, π]) = {result_list}")
assert abs(result_list[0]) < 1e-10
assert abs(result_list[1] - 1.0) < 1e-10
assert abs(result_list[2]) < 1e-10

print("  ✓ Все тесты callback пройдены\n")


# ================================================================
# КОНСТАНТЫ МОДУЛЯ
# ================================================================
test_section("КОНСТАНТЫ МОДУЛЯ")
print(f"  example.__version__ = {example.__version__}")
print(f"  example.PI = {example.PI}")
assert example.__version__ == "1.0.0"
print("  ✓ OK\n")


# ================================================================
# Справка
# ================================================================
test_section("СПРАВКА ПО МОДУЛЮ (help)")
print("  Для получения справки выполните: help(example)")
print("  Или help(example.add) для конкретной функции\n")


print("=" * 60)
print("  ВСЕ ТЕСТЫ УСПЕШНО ПРОЙДЕНЫ! 🎉")
print("=" * 60)
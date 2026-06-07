#include <pybind11/pybind11.h>
#include <pybind11/stl.h>        // Для автоматической конвертации std::vector, std::map и т.д.
#include <pybind11/numpy.h>      // Для работы с numpy массивами
#include <pybind11/complex.h>    // Для std::complex
#include <pybind11/functional.h> // Для std::function

#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cmath>

namespace py = pybind11;

// ============================================================
// ПРИМЕР 1: ПРОСТЫЕ ФУНКЦИИ С БАЗОВЫМИ ТИПАМИ
// Самый простой способ - возвращаем стандартные типы
// pybind11 автоматически конвертирует int, float, string, bool
// ============================================================

int add(int a, int b) {
    return a + b;
}

double multiply(double a, double b) {
    return a * b;
}

std::string greet(const std::string& name) {
    return "Hello, " + name + " from C++!";
}

bool is_positive(int x) {
    return x > 0;
}

// ============================================================
// ПРИМЕР 2: ПЕРЕДАЧА ЧЕРЕЗ СТРОКИ (ТУПОЙ И ПРОСТОЙ СПОСОБ)
// Самый примитивный способ обмена данными - тупо строка с разделителями
// Плюсы: не требует numpy, работает везде, просто отлаживать
// Минусы: медленно на больших данных, парсинг строки вручную
// ============================================================

std::string process_array_via_string(const std::string& data) {
    // Принимаем строку вида "1.0,2.0,3.0,4.5" 
    // Возвращаем удвоенные значения такой же строкой
    std::vector<double> values;
    std::stringstream ss(data);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        try {
            values.push_back(std::stod(token));
        } catch (...) {
            // Игнорируем битые токены
        }
    }
    
    // Обработка: умножаем на 2
    for (auto& v : values) {
        v *= 2.0;
    }
    
    // Собираем обратно в строку
    std::stringstream result;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) result << ",";
        result << values[i];
    }
    
    return result.str();
}

// Ещё пример: матрица через строку (CSV формат)
std::string matrix_multiply_string(const std::string& matrix_str, double scalar) {
    // matrix_str: "1,2;3,4"  - строки разделены ';', элементы ','
    std::stringstream result;
    std::stringstream ss(matrix_str);
    std::string row;
    bool first_row = true;
    
    while (std::getline(ss, row, ';')) {
        if (!first_row) result << ";";
        first_row = false;
        
        std::stringstream row_ss(row);
        std::string val;
        bool first_val = true;
        
        while (std::getline(row_ss, val, ',')) {
            if (!first_val) result << ",";
            first_val = false;
            try {
                result << (std::stod(val) * scalar);
            } catch (...) {
                result << "0";
            }
        }
    }
    
    return result.str();
}

// ============================================================
// ПРИМЕР 3: STL КОНТЕЙНЕРЫ (АВТОМАТИЧЕСКАЯ КОНВЕРТАЦИЯ)
// pybind11 сам конвертирует list<->vector, dict<->map
// Удобно, но копирует данные при передаче
// ============================================================

std::vector<int> square_vector(const std::vector<int>& input) {
    std::vector<int> result;
    result.reserve(input.size());
    for (int x : input) {
        result.push_back(x * x);
    }
    return result;
}

std::map<std::string, int> count_words(const std::vector<std::string>& words) {
    std::map<std::string, int> counts;
    for (const auto& w : words) {
        counts[w]++;
    }
    return counts;
}

double sum_of_list(const std::vector<double>& values) {
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    return sum;
}

// ============================================================
// ПРИМЕР 4: NUMPY МАССИВЫ (ЭФФЕКТИВНАЯ ПЕРЕДАЧА БЕЗ КОПИРОВАНИЯ)
// Самый быстрый способ для больших данных
// py::array_t<T> позволяет работать с numpy массивами напрямую
// Без копирования, через общий буфер памяти
// ============================================================

// Пример с одномерным массивом
py::array_t<double> numpy_multiply_scalar(py::array_t<double> input, double scalar) {
    // Запрашиваем буфер - это даёт доступ к сырым данным без копирования
    auto buf = input.request();
    
    // buf.ptr - указатель на данные
    // buf.size - общее количество элементов
    // buf.shape - вектор размерностей
    double* ptr = static_cast<double*>(buf.ptr);
    
    // Создаём новый массив для результата (можно модифицировать и входной!)
    auto result = py::array_t<double>(buf.size);
    auto result_buf = result.request();
    double* result_ptr = static_cast<double*>(result_buf.ptr);
    
    // Обработка
    for (ssize_t i = 0; i < buf.size; ++i) {
        result_ptr[i] = ptr[i] * scalar;
    }
    
    // Приводим форму результата к форме входа (важно для многомерных!)
    result = result.reshape(buf.shape);
    
    return result;
}

// Пример с двумерным массивом (матрицей)
py::array_t<double> numpy_matrix_transpose(py::array_t<double> input) {
    auto buf = input.request();
    
    // Проверяем размерность
    if (buf.ndim != 2) {
        throw std::runtime_error("Input must be a 2D array (matrix)");
    }
    
    ssize_t rows = buf.shape[0];
    ssize_t cols = buf.shape[1];
    double* ptr = static_cast<double*>(buf.ptr);
    
    // Создаём транспонированную матрицу
    auto result = py::array_t<double>({cols, rows});
    auto rbuf = result.request();
    double* rptr = static_cast<double*>(rbuf.ptr);
    
    for (ssize_t i = 0; i < rows; ++i) {
        for (ssize_t j = 0; j < cols; ++j) {
            rptr[j * rows + i] = ptr[i * cols + j];  // транспонирование
        }
    }
    
    return result;
}

// Пример с in-place модификацией (меняем входной массив)
void numpy_add_inplace(py::array_t<double> array, double value) {
    auto buf = array.request();
    double* ptr = static_cast<double*>(buf.ptr);
    
    for (ssize_t i = 0; i < buf.size; ++i) {
        ptr[i] += value;
    }
    // Не возвращаем ничего - модифицировали массив на месте
}

// Генерация нового numpy массива из C++
py::array_t<double> numpy_arange(double start, double end, double step) {
    int size = static_cast<int>((end - start) / step);
    auto result = py::array_t<double>(size);
    auto buf = result.request();
    double* ptr = static_cast<double*>(buf.ptr);
    
    for (int i = 0; i < size; ++i) {
        ptr[i] = start + i * step;
    }
    
    return result;
}

// ============================================================
// ПРИМЕР 5: РАБОТА С КЛАССАМИ
// Можно биндить целые классы с методами и свойствами
// ============================================================

class Calculator {
private:
    double memory_;
    std::vector<double> history_;
    
public:
    Calculator() : memory_(0.0) {}
    
    // Простые методы
    double add(double a, double b) {
        double result = a + b;
        history_.push_back(result);
        return result;
    }
    
    double multiply(double a, double b) {
        double result = a * b;
        history_.push_back(result);
        return result;
    }
    
    // Сохранение в память
    void memory_store(double value) {
        memory_ = value;
    }
    
    double memory_recall() const {
        return memory_;
    }
    
    // История вычислений
    std::vector<double> get_history() const {
        return history_;
    }
    
    void clear_history() {
        history_.clear();
    }
    
    // Статический метод - не требует создания объекта
    static double sin_deg(double degrees) {
        return std::sin(degrees * M_PI / 180.0);
    }
    
    // Перегрузка оператора (будет доступна в Python!)
    double operator()(double a, double b) const {
        return a * a + b * b;  // сумма квадратов
    }
};

// ============================================================
// ПРИМЕР 6: СЛОЖНЫЕ СТРУКТУРЫ
// ============================================================

struct Point {
    double x, y, z;
    
    Point() : x(0), y(0), z(0) {}
    Point(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    
    double magnitude() const {
        return std::sqrt(x*x + y*y + z*z);
    }
    
    Point operator+(const Point& other) const {
        return Point(x + other.x, y + other.y, z + other.z);
    }
    
    std::string to_string() const {
        return "Point(" + std::to_string(x) + ", " + 
                         std::to_string(y) + ", " + 
                         std::to_string(z) + ")";
    }
};

// ============================================================
// ПРИМЕР 7: ИСКЛЮЧЕНИЯ В C++ ПЕРЕДАЮТСЯ В PYTHON
// ============================================================

double safe_divide(double a, double b) {
    if (b == 0.0) {
        throw std::invalid_argument("Division by zero!");
    }
    return a / b;
}

// ============================================================
// ПРИМЕР 8: ПЕРЕДАЧА ФУНКЦИЙ (CALLBACK) ИЗ PYTHON В C++
// ============================================================

double apply_function(const std::function<double(double)>& func, double value) {
    // Вызываем переданную из Python функцию
    return func(value);
}

std::vector<double> map_function(const std::function<double(double)>& func, 
                                  const std::vector<double>& values) {
    std::vector<double> result;
    result.reserve(values.size());
    for (double v : values) {
        result.push_back(func(v));
    }
    return result;
}


// ============================================================
// МОДУЛЬ PYBIND11 - ТОЧКА ВХОДА
// ============================================================

PYBIND11_MODULE(example, m) {
    m.doc() = "Пример модуля на pybind11 с разными способами передачи данных"; 
    // docstring будет виден в Python через help()
    
    // --- ПРИМЕР 1: Простые функции ---
    m.def("add", &add, 
          py::arg("a"), py::arg("b"),  // именованные аргументы
          "Сложение двух целых чисел");
    m.def("multiply", &multiply, "Умножение двух чисел с плавающей точкой");
    m.def("greet", &greet, "Приветствие");
    m.def("is_positive", &is_positive, "Проверка на положительность");
    
    // --- ПРИМЕР 2: Передача через строки ---
    m.def("process_array_via_string", &process_array_via_string,
          py::arg("data"),
          "Обработка массива через строку с разделителями (тупой, но простой способ)\n"
          "Принимает: '1.0,2.0,3.0'\n"
          "Возвращает: удвоенные значения");
    m.def("matrix_multiply_string", &matrix_multiply_string,
          py::arg("matrix_str"), py::arg("scalar"),
          "Умножение матрицы на скаляр через строку\n"
          "Формат: '1,2;3,4' (строки через ';', элементы через ',')");
    
    // --- ПРИМЕР 3: STL контейнеры ---
    m.def("square_vector", &square_vector, 
          "Возвращает список квадратов чисел (list -> vector -> list)");
    m.def("count_words", &count_words, 
          "Подсчёт слов (list -> dict)");
    m.def("sum_of_list", &sum_of_list,
          "Сумма элементов списка");
    
    // --- ПРИМЕР 4: Numpy массивы ---
    m.def("numpy_multiply_scalar", &numpy_multiply_scalar,
          py::arg("array"), py::arg("scalar"),
          "Поэлементное умножение numpy массива на скаляр (без копирования буфера)");
    m.def("numpy_matrix_transpose", &numpy_matrix_transpose,
          py::arg("matrix"),
          "Транспонирование 2D numpy массива");
    m.def("numpy_add_inplace", &numpy_add_inplace,
          py::arg("array"), py::arg("value"),
          "Прибавление скаляра к массиву in-place (модифицирует входной массив!)");
    m.def("numpy_arange", &numpy_arange,
          py::arg("start"), py::arg("end"), py::arg("step"),
          "Генерация массива как np.arange в C++");
    
    // --- ПРИМЕР 5: Классы ---
    py::class_<Calculator>(m, "Calculator", "Калькулятор с памятью и историей")
        .def(py::init<>())  // конструктор
        .def("add", &Calculator::add, 
             py::arg("a"), py::arg("b"),
             "Сложение двух чисел")
        .def("multiply", &Calculator::multiply,
             py::arg("a"), py::arg("b"),
             "Умножение двух чисел")
        .def("memory_store", &Calculator::memory_store,
             py::arg("value"),
             "Сохранить значение в память")
        .def("memory_recall", &Calculator::memory_recall,
             "Извлечь значение из памяти")
        .def("get_history", &Calculator::get_history,
             "Получить историю вычислений")
        .def("clear_history", &Calculator::clear_history,
             "Очистить историю")
        .def_static("sin_deg", &Calculator::sin_deg,
                    py::arg("degrees"),
                    "Синус угла в градусах (статический метод)")
        .def("__call__", &Calculator::operator(),
             py::arg("a"), py::arg("b"),
             "Вызов объекта как функции: calc(a, b) -> a² + b²");
    
    // --- ПРИМЕР 6: Структуры ---
    py::class_<Point>(m, "Point", "Трёхмерная точка")
        .def(py::init<>())
        .def(py::init<double, double, double>(),
             py::arg("x"), py::arg("y"), py::arg("z"))
        .def_readwrite("x", &Point::x)
        .def_readwrite("y", &Point::y)
        .def_readwrite("z", &Point::z)
        .def("magnitude", &Point::magnitude, "Длина вектора")
        .def("__add__", &Point::operator+, py::arg("other"), "Сложение точек")
        .def("__repr__", &Point::to_string)
        .def("to_string", &Point::to_string);
    
    // --- ПРИМЕР 7: Исключения ---
    m.def("safe_divide", &safe_divide, 
          py::arg("a"), py::arg("b"),
          "Безопасное деление с исключением при делении на ноль");
    
    // --- ПРИМЕР 8: Callback функции ---
    m.def("apply_function", &apply_function,
          py::arg("func"), py::arg("value"),
          "Применить Python-функцию к значению: f(value)");
    m.def("map_function", &map_function,
          py::arg("func"), py::arg("values"),
          "Применить Python-функцию к списку значений: [f(v) for v in values]");
    
    // --- Константы модуля ---
    m.attr("__version__") = "1.0.0";
    m.attr("PI") = M_PI;
}
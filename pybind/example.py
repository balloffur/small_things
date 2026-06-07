"""
example.py - Модуль-обёртка с подсказками по компиляции и импорту
Запускать не нужно. Используйте test.py для проверки.
Сначала скомпилируйте C++ код в .so/.pyd файл.
"""

# После компиляции этот импорт будет работать:
# from example import add, Calculator, numpy_multiply_scalar

# ============================================================
# ИНСТРУКЦИЯ ПО КОМПИЛЯЦИИ (просто скопируйте нужную команду)
# ============================================================

"""
# Linux / macOS (gcc или clang):
c++ -O3 -Wall -shared -std=c++11 -fPIC \
    $(python3 -m pybind11 --includes) \
    example.cpp -o example$(python3-config --extension-suffix)

# macOS с Homebrew (если python3-config не найден):
c++ -O3 -Wall -shared -std=c++11 -fPIC \
    $(python3 -m pybind11 --includes) \
    example.cpp -o example$(python3 -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))")

# Windows (Visual Studio Developer Command Prompt):
cl /O2 /EHsc /std:c++11 /LD /Fe:example.pyd ^
   example.cpp ^
   /I %VENV_DIR%\Lib\site-packages\pybind11\include ^
   /I %PYTHON_DIR%\include ^
   /link /LIBPATH:%PYTHON_DIR%\libs

# Альтернативно, можно использовать setup.py или pip install -e .
# Для этого создайте setup.py рядом:

### ФАЙЛ setup.py (опционально, если не хотите компилить вручную):
'''
from setuptools import setup, Extension
import pybind11

ext = Extension(
    'example',
    sources=['example.cpp'],
    include_dirs=[pybind11.get_include()],
    language='c++',
    extra_compile_args=['-O3', '-std=c++11']
)

setup(name='example', ext_modules=[ext])
'''
# Потом: pip install -e .
"""

# ============================================================
# ПРИМЕРЫ ИСПОЛЬЗОВАНИЯ (для справки, реальные тесты в test.py)
# ============================================================

if __name__ == "__main__":
    print("Сначала скомпилируйте example.cpp, затем запускайте test.py")
    print("Смотрите инструкции выше ↑")
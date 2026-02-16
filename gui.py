#!/usr/bin/env python3
# cyph_gui_tk_v3.py — cleaner Tkinter (ttk) GUI for cyph.exe/cyph
#
# Changes vs earlier versions:
#   - Cleaner layout, fewer "frames in frames", no heavy LabelFrames
#   - Sections are just headers + separators
#   - Right-side action column in Encrypt/Decrypt tabs
#   - Clears typed secrets right after starting a run
#
# Build onefile (Windows):
#   pyinstaller --onefile --windowed cyph_gui_tk_v3.py --add-binary "cyph.exe;."
#
# Build onefile (Linux/macOS):
#   pyinstaller --onefile --windowed cyph_gui_tk_v3.py --add-binary "cyph:."

import os
import sys
import shlex
import queue
import threading
import subprocess
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox


APP_TITLE = "cyph GUI"


GUI_HELP = r'''cyph GUI — гайд, советы и справка
=================================

Эта программа — GUI-обёртка над cyph (CLI). Она:
• собирает аргументы,
• запускает cyph.exe / cyph,
• показывает реальную команду и вывод внизу.

──────────────────────────────────────────────────────────────────────────────
1) Быстрый гайд: самые частые действия
──────────────────────────────────────────────────────────────────────────────

A) Зашифровать 1 файл “быстро и правильно”
------------------------------------------
1) Вкладка Encrypt
2) Add files… → выбери файл(ы)
3) Key (-k):
   • Prompt → введи ключ в “Key text” (поле с точками •••)
4) Output:
   • Auto (рекомендуется)
5) Нажми Run Encrypt

Что получится:
• рядом с исходным файлом появится *.cyph
  example: photo.jpg → photo.jpg.cyph

Рекомендация:
• для настоящих паролей используй Prompt (-k?) или keyfile, а не Inline.


B) Расшифровать 1 файл “быстро и правильно”
-------------------------------------------
1) Вкладка Decrypt
2) Add .cyph… → выбери *.cyph
3) Key (-k):
   • Prompt → введи ключ в “Key text”
4) Опции:
   • -d restore name (обычно включено) — восстановит исходное имя
5) Output:
   • Auto (рекомендуется)
6) Run Decrypt

Что получится:
• файл восстановится рядом с *.cyph
• если включен -d — будет восстановлено исходное имя, сохранённое в контейнере


C) Создать “файл-ключ” (обычный keyfile) и шифровать им
-------------------------------------------------------
Самый простой вариант:
1) Создай текстовый файл mykey.txt
2) Напиши туда пароль/фразу (можно с пробелами и переносами)
   Важно: если файл заканчивается на .txt — cyph нормализует ключ:
   • все буквы в lowercase
   • удалит все пробелы/табуляции/переводы строк

Дальше:
• Encrypt: Key → File → выбери mykey.txt
• Decrypt: Key → File → выбери mykey.txt


D) Сгенерировать случайный ключ через cyph (-gen)
-------------------------------------------------
1) Вкладка Key tools → Generate key (-gen)
2) Оставь “Save to file (-o)” включённым
3) Browse… → выбери куда сохранить
4) Run

Это удобно, если ты хочешь не “пароль”, а реально случайный ключ.


E) Сделать защищённый “ключ-контейнер” .cyphkey (wrap)
------------------------------------------------------
Зачем:
• хранить реальный ключ (особенно бинарный) безопаснее в .cyphkey,
  который открывается только master-ключом.

Как:
1) Подготовь keyfile (это может быть .txt или бинарный файл)
2) Вкладка Key tools → Wrap key file into .cyphkey (-key)
3) Input key file → выбери keyfile
4) Output (-o) → имя/путь для результата (расширение .cyphkey добавится само)
5) Master (-K):
   • Prompt → введи master в “Master text”
6) Run wrap (-key)

Как использовать потом:
• Encrypt/Decrypt → Key file выбираешь *.cyphkey
• Появится блок Master (-K) — его тоже нужно указать


F) Обмен ключами (Exchange) — 2 шага
------------------------------------
Это для получения общего “shared key” с другой стороной.

Step 1: create
1) Вкладка Exchange → Step 1: create
2) Step1 -o → задай имя/путь (создастся .cyphkey)
3) Password key (-k): обычно Prompt
4) Run Exchange
5) В выводе появится:
   • Public key: cyphx1:...
   • Fingerprint (6 words)

Public key отправляешь второй стороне (в мессенджере можно).
Fingerprint лучше сверить по голосу/видео.

Step 2: finalize
1) Exchange → Step 2: finalize
2) Step2 file → выбери тот же .cyphkey, созданный на шаге 1
3) Peer pubkey → вставь ключ второй стороны (cyphx1:...)
4) Run Exchange
5) После подтверждения fingerprint файл .cyphkey будет перезаписан shared key’ом.


──────────────────────────────────────────────────────────────────────────────
2) Типичные проблемы и быстрые решения
──────────────────────────────────────────────────────────────────────────────

“cyph not found / cyph.exe не найден”
-------------------------------------
• Положи cyph.exe рядом с GUI (в ту же папку).
• Или укажи путь через кнопку Advanced…


“Wrong key / Wrong password / corrupt file”
-------------------------------------------
• Чаще всего — неверный ключ/мастер-ключ.
• Проверь:
  - точно тот keyfile?
  - если keyfile *.txt — помни про нормализацию (lowercase + no whitespace)
  - если используешь *.cyphkey — нужен Master (-K), без него не откроется


Пути с кириллицей на Windows
----------------------------
Если cyph ругается примерно так:
  “filesystem error: Cannot convert character sequence: Illegal byte sequence”
То это проблема конвертации символов (UTF-16/UTF-8/локаль).

Что делать:
• Самое надёжное: использовать пути латиницей (временно).
• Либо запускать из окружения/терминала, где UTF-8 настроен корректно.
GUI тут ни при чём — это уровень std::filesystem / окружения.


Output “не там где ожидал”
--------------------------
• В режиме Auto (-o не передаётся) cyph пишет рядом с входными.
• В режиме File/Dir можно указать относительный путь — он будет относителен
  к рабочей папке GUI (обычно там, где лежит exe).
Если хочешь без сюрпризов — указывай полный путь или используй Auto.


──────────────────────────────────────────────────────────────────────────────
3) Директивы и как они включаются в GUI
──────────────────────────────────────────────────────────────────────────────

Основные режимы cyph:
---------------------
Encrypt:
  -f <file...>      входные файлы для шифрования
Decrypt:
  -i <file...>      входные .cyph для расшифровки
Key generation:
  -gen              сгенерировать ключ
Key wrap:
  -key <keyfile>    упаковать keyfile в .cyphkey
Exchange:
  -e ...            обмен ключами (2 шага)

GUI включает их так:
• Encrypt вкладка → cyph -f ...
• Decrypt вкладка → cyph -i ...
• Key tools → отдельные кнопки для -gen и -key
• Exchange → Step 1/2 для -e

Output (-o):
------------
• Encrypt:
  - Auto → -o не используется
  - File → -o <file>
  - Dir  → -o <dir> (для нескольких входных)

• Decrypt:
  - Auto → -o не используется (cyph пишет рядом)
  - File/Dir → -o <...>

Важно:
• Для Encrypt, если входных файлов несколько и задан -o,
  cyph ожидает директорию (GUI предлагает Dir).


Key (-k):
---------
В GUI это блок “Key (-k)”, 3 режима:
• File   → -k <path>
• Inline → -k=<text>
• Prompt → -k?  (и GUI отправляет введённый текст в stdin)

Master (-K):
------------
Появляется только когда нужен:
• если Key file заканчивается на *.cyphkey
• или при wrap (-key) мастер обязателен всегда

Режимы Master такие же:
• -K <path>
• -K=<text>
• -K?  (stdin)


Extra args:
-----------
На Encrypt/Decrypt есть поле “Extra args”.
Туда можно дописать любые аргументы cyph вручную (через пробелы),
и GUI добавит их в команду.


──────────────────────────────────────────────────────────────────────────────
4) Безопасность, уровни, anon, .cyphkey, WIPE — подробно
──────────────────────────────────────────────────────────────────────────────

Inline (-k=...) — почему нежелательно
-------------------------------------
Когда ты используешь -k=пароль:
• пароль попадает в командную строку процесса
• может светиться в логах, истории shell, диспетчере задач и т.п.

Используй Inline только для тестов.


Prompt (-k?) — что происходит
-----------------------------
В режиме Prompt cyph ожидает ввод из stdin.
GUI делает так:
• ты вводишь ключ в поле “Key text”
• GUI отправляет его в stdin процессу cyph
• после старта команда очищает поле (best-effort)

Это лучше, чем Inline, но важно понимать:
• если на машине есть админский дебаггер/малварь, ключ можно вытащить из памяти.
GUI не может “магически” защитить от полностью скомпрометированной системы.


KDF level (-level 0/1/2)
------------------------
Level влияет на “дороговизну” вывода ключа (libsodium crypto_pwhash):
• 0 — быстрее (interactive)
• 1 — умеренно
• 2 — медленнее и крепче (sensitive)

Плюс:
• параметры KDF сохраняются в заголовке .cyph/.cyphkey,
  так что при расшифровке не нужно помнить level.

Рекомендация:
• level 0 для повседневного
• level 1/2 для “важного” (учти, что будет медленнее)


-anon (Encrypt only)
--------------------
cyph хранит оригинальное имя файла в первом зашифрованном “meta frame”,
чтобы при -d восстановить имя.

Если включить -anon:
• вместо настоящего имени внутри контейнера будет “anonymous”
• это полезно для приватности (чтобы даже тот, кто расшифрует, не увидел исходное имя)


.cyphkey (wrapped keys)
-----------------------
.cyphkey — это “контейнер ключа”, то есть cyph-файл, внутри которого лежит ключ.
Идея:
• реальный ключ можно хранить как случайные байты (лучше, чем пароль)
• .cyphkey защищается master-ключом (-K...)

Плюсы:
• удобно носить “ключ” отдельным файлом
• можно хранить высокоэнтропийные ключи
• master можно вводить через Prompt

Минусы:
• потеря master = потеря доступа
• безопасность всё равно зависит от твоей машины и хранения файлов


-WIPE (осторожно)
-----------------
-WIPE делает best-effort удаление:
• после успешного Encrypt — удалит оригинальные входные файлы (-f)
  и (если -k <file>) удалит keyfile
• после успешного Decrypt — (если -k <file>) удалит keyfile

Важно:
• это НЕ гарантированное “secure erase” (особенно на SSD и journaling FS)
• это просто удобная автоматизация “удалить после успеха”
GUI при -WIPE автоматически отвечает “Yes” на подтверждение cyph.

Если сомневаешься — не включай -WIPE.


──────────────────────────────────────────────────────────────────────────────
5) Ссылка на проект / исходники
──────────────────────────────────────────────────────────────────────────────

cyph на GitHub:
https://github.com/balloffur/small_things/tree/main/cyph

'''
LOG_BG = "#0b0f14"
LOG_FG = "#e6edf3"
LOG_STDERR = "#f7768e"
LOG_META = "#a6accd"
LOG_CMD = "#9cdcfe"


def is_windows() -> bool:
    return sys.platform.startswith("win")


def bundled_base_dir() -> Path:
    # PyInstaller onefile: sys._MEIPASS is extraction dir; otherwise script dir
    return Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))


def find_cyph() -> Path | None:
    base = bundled_base_dir()
    names = ["cyph.exe", "cyph"] if is_windows() else ["cyph"]
    candidates: list[Path] = []

    for n in names:
        candidates.append(base / n)

    for n in names:
        candidates.append(Path.cwd() / n)
        candidates.append(Path.cwd() / "build" / n)

    for n in names:
        for p in os.environ.get("PATH", "").split(os.pathsep):
            if p:
                candidates.append(Path(p) / n)

    for c in candidates:
        try:
            if c.exists() and c.is_file():
                return c.resolve()
        except Exception:
            pass
    return None


def mono_font():
    if is_windows():
        return ("Cascadia Mono", 10)
    return ("Menlo", 11)


def ui_font():
    if is_windows():
        return ("Segoe UI", 10)
    return ("Noto Sans", 10)


def hlabel(parent, text: str) -> ttk.Label:
    return ttk.Label(parent, text=text, style="Header.TLabel")


def section(parent, title: str) -> ttk.Frame:
    row = ttk.Frame(parent)
    row.columnconfigure(0, weight=1)
    hlabel(row, title).grid(row=0, column=0, sticky="w")
    ttk.Separator(row, orient="horizontal").grid(row=1, column=0, sticky="ew", pady=(6, 0))
    return row


# ----------------------------
# Non-blocking process runner
# ----------------------------
class ProcRunner:
    def __init__(self, on_line, on_done):
        self.on_line = on_line
        self.on_done = on_done
        self._q: queue.Queue[tuple[str, str]] = queue.Queue()
        self._proc: subprocess.Popen | None = None

    def running(self) -> bool:
        return self._proc is not None

    def stop(self):
        if self._proc is None:
            return
        try:
            self._proc.terminate()
            self._q.put(("meta", "[terminate sent]\n"))
        except Exception as e:
            self._q.put(("stderr", f"[stop error] {e}\n"))

    def run(self, cmd: list[str], stdin_text: str | None = None, cwd: str | None = None):
        if self._proc is not None:
            return

        def worker():
            try:
                self._proc = subprocess.Popen(
                    cmd,
                    cwd=cwd,
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    bufsize=1,
                    universal_newlines=True,
                )
                assert self._proc.stdout is not None
                assert self._proc.stderr is not None

                if stdin_text is not None:
                    try:
                        assert self._proc.stdin is not None
                        self._proc.stdin.write(stdin_text)
                        self._proc.stdin.flush()
                        self._proc.stdin.close()
                    except Exception as e:
                        self._q.put(("stderr", f"[stdin error] {e}\n"))

                def pump(stream, kind: str):
                    for line in stream:
                        self._q.put((kind, line))
                    stream.close()

                threading.Thread(target=pump, args=(self._proc.stdout, "stdout"), daemon=True).start()
                threading.Thread(target=pump, args=(self._proc.stderr, "stderr"), daemon=True).start()

                code = self._proc.wait()
                self._q.put(("meta", f"[exit code: {code}]\n\n"))
            except Exception as e:
                self._q.put(("stderr", f"[error] {e}\n"))
            finally:
                self._q.put(("done", ""))

        threading.Thread(target=worker, daemon=True).start()

    def poll(self):
        try:
            while True:
                kind, payload = self._q.get_nowait()
                if kind == "done":
                    self._proc = None
                    self.on_done()
                else:
                    self.on_line(kind, payload)
        except queue.Empty:
            pass


# ----------------------------
# Key panel
# ----------------------------
class KeyPanel(ttk.Frame):
    """
    Key sources:
      - file:   -k <file>
      - inline: -k=<text>
      - prompt: -k?   (GUI sends one line to stdin)

    If allow_master_for_cyphkey=True and key file ends with .cyphkey,
    master (-K...) is required; UI shows master controls.
    """

    def __init__(self, parent, allow_master_for_cyphkey: bool, title: str = "Key"):
        super().__init__(parent)
        self.allow_master_for_cyphkey = allow_master_for_cyphkey

        self.mode = tk.StringVar(value="file")  # file/inline/prompt
        self.key_file = tk.StringVar()
        self.key_inline = tk.StringVar()

        self.master_mode = tk.StringVar(value="prompt")  # file/inline/prompt
        self.master_file = tk.StringVar()
        self.master_inline = tk.StringVar()

        row0 = ttk.Frame(self)
        row0.grid(row=0, column=0, sticky="ew")
        row0.columnconfigure(1, weight=1)
        hlabel(row0, title).grid(row=0, column=0, sticky="w")
        ttk.Separator(row0, orient="horizontal").grid(row=1, column=0, columnspan=2, sticky="ew", pady=(6, 0))

        row1 = ttk.Frame(self)
        row1.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        ttk.Radiobutton(row1, text="File", variable=self.mode, value="file", command=self._update_ui).grid(row=0, column=0, padx=(0, 12))
        ttk.Radiobutton(row1, text="Inline", variable=self.mode, value="inline", command=self._update_ui).grid(row=0, column=1, padx=(0, 12))
        ttk.Radiobutton(row1, text="Prompt", variable=self.mode, value="prompt", command=self._update_ui).grid(row=0, column=2)

        row2 = ttk.Frame(self)
        row2.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        row2.columnconfigure(1, weight=1)
        ttk.Label(row2, text="Key file").grid(row=0, column=0, sticky="w")
        self.key_file_ent = ttk.Entry(row2, textvariable=self.key_file)
        self.key_file_ent.grid(row=0, column=1, sticky="ew", padx=(10, 0))
        ttk.Button(row2, text="Browse…", command=self._pick_key_file).grid(row=0, column=2, padx=(10, 0))

        row3 = ttk.Frame(self)
        row3.grid(row=3, column=0, sticky="ew", pady=(8, 0))
        row3.columnconfigure(1, weight=1)
        ttk.Label(row3, text="Key text").grid(row=0, column=0, sticky="w")
        self.key_inline_ent = ttk.Entry(row3, textvariable=self.key_inline, show="•")
        self.key_inline_ent.grid(row=0, column=1, sticky="ew", padx=(10, 0))
        ttk.Button(row3, text="Clear", command=lambda: self.key_inline.set("")).grid(row=0, column=2, padx=(10, 0))

        # master (hidden unless needed)
        self.master_area = ttk.Frame(self)
        self.master_area.grid(row=4, column=0, sticky="ew", pady=(14, 0))
        self.master_area.columnconfigure(0, weight=1)

        hlabel(self.master_area, "Master (-K) for .cyphkey").grid(row=0, column=0, sticky="w")
        ttk.Separator(self.master_area, orient="horizontal").grid(row=1, column=0, sticky="ew", pady=(6, 0))

        m1 = ttk.Frame(self.master_area)
        m1.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        ttk.Radiobutton(m1, text="File", variable=self.master_mode, value="file", command=self._update_ui).grid(row=0, column=0, padx=(0, 12))
        ttk.Radiobutton(m1, text="Inline", variable=self.master_mode, value="inline", command=self._update_ui).grid(row=0, column=1, padx=(0, 12))
        ttk.Radiobutton(m1, text="Prompt", variable=self.master_mode, value="prompt", command=self._update_ui).grid(row=0, column=2)

        m2 = ttk.Frame(self.master_area)
        m2.grid(row=3, column=0, sticky="ew", pady=(8, 0))
        m2.columnconfigure(1, weight=1)
        ttk.Label(m2, text="Master file").grid(row=0, column=0, sticky="w")
        self.master_file_ent = ttk.Entry(m2, textvariable=self.master_file)
        self.master_file_ent.grid(row=0, column=1, sticky="ew", padx=(10, 0))
        ttk.Button(m2, text="Browse…", command=self._pick_master_file).grid(row=0, column=2, padx=(10, 0))

        m3 = ttk.Frame(self.master_area)
        m3.grid(row=4, column=0, sticky="ew", pady=(8, 0))
        m3.columnconfigure(1, weight=1)
        ttk.Label(m3, text="Master text").grid(row=0, column=0, sticky="w")
        self.master_inline_ent = ttk.Entry(m3, textvariable=self.master_inline, show="•")
        self.master_inline_ent.grid(row=0, column=1, sticky="ew", padx=(10, 0))
        ttk.Button(m3, text="Clear", command=lambda: self.master_inline.set("")).grid(row=0, column=2, padx=(10, 0))

        self.master_area.grid_remove()

        self.key_file.trace_add("write", lambda *_: self._update_ui())
        self.mode.trace_add("write", lambda *_: self._update_ui())
        self.master_mode.trace_add("write", lambda *_: self._update_ui())

        self._update_ui()

    def _pick_key_file(self):
        p = filedialog.askopenfilename(title="Select key file")
        if p:
            self.key_file.set(p)

    def _pick_master_file(self):
        p = filedialog.askopenfilename(title="Select master key file")
        if p:
            self.master_file.set(p)

    def _keyfile_is_cyphkey(self) -> bool:
        p = (self.key_file.get() or "").strip().lower()
        return p.endswith(".cyphkey")

    def _need_master(self) -> bool:
        return self.allow_master_for_cyphkey and self.mode.get() == "file" and self._keyfile_is_cyphkey()

    def _update_ui(self):
        self.key_file_ent.configure(state="normal" if self.mode.get() == "file" else "disabled")
        self.key_inline_ent.configure(state="normal" if self.mode.get() in ("inline", "prompt") else "disabled")

        if self._need_master():
            self.master_area.grid()
        else:
            self.master_area.grid_remove()

        if self._need_master():
            self.master_file_ent.configure(state="normal" if self.master_mode.get() == "file" else "disabled")
            self.master_inline_ent.configure(state="normal" if self.master_mode.get() in ("inline", "prompt") else "disabled")

    def clear_secrets(self):
        # best-effort hygiene for typed secrets (not file paths)
        if self.mode.get() in ("inline", "prompt"):
            self.key_inline.set("")
        if self._need_master() and self.master_mode.get() in ("inline", "prompt"):
            self.master_inline.set("")

    def build_key_and_master(self, flag_k: str = "-k", flag_K: str = "-K") -> tuple[list[str], list[str]]:
        args: list[str] = []
        stdin_lines: list[str] = []

        mode = self.mode.get()
        if mode == "file":
            p = self.key_file.get().strip()
            if not p:
                raise ValueError("Key file not set.")
            args += [flag_k, p]
        elif mode == "inline":
            t = self.key_inline.get()
            if not t:
                raise ValueError("Inline key is empty.")
            args.append(f"{flag_k}={t}")
        else:
            args.append(f"{flag_k}?")
            t = self.key_inline.get()
            if not t:
                raise ValueError("Enter key text for Prompt mode (will be sent to stdin).")
            stdin_lines.append(t)

        if self._need_master():
            mm = self.master_mode.get()
            if mm == "file":
                p = self.master_file.get().strip()
                if not p:
                    raise ValueError("Master key file not set.")
                args += [flag_K, p]
            elif mm == "inline":
                t = self.master_inline.get()
                if not t:
                    raise ValueError("Master inline key is empty.")
                args.append(f"{flag_K}={t}")
            else:
                args.append(f"{flag_K}?")
                t = self.master_inline.get()
                if not t:
                    raise ValueError("Enter master key text for Prompt mode (will be sent to stdin).")
                stdin_lines.append(t)

        return args, stdin_lines

    def build_master_only(self, flag_K: str = "-K") -> tuple[list[str], list[str]]:
        args: list[str] = []
        stdin_lines: list[str] = []

        mm = self.master_mode.get()
        if mm == "file":
            p = self.master_file.get().strip()
            if not p:
                raise ValueError("Master key file not set.")
            args += [flag_K, p]
        elif mm == "inline":
            t = self.master_inline.get()
            if not t:
                raise ValueError("Master inline key is empty.")
            args.append(f"{flag_K}={t}")
        else:
            args.append(f"{flag_K}?")
            t = self.master_inline.get()
            if not t:
                raise ValueError("Enter master key text for Prompt mode (will be sent to stdin).")
            stdin_lines.append(t)

        return args, stdin_lines


# ----------------------------
# GUI
# ----------------------------
class CyphGUI(ttk.Frame):
    def __init__(self, master: tk.Tk):
        super().__init__(master, padding=12)
        self.master = master
        self.master.title(APP_TITLE)
        self.master.minsize(1060, 720)

        self.cyph_path = find_cyph()

        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass

        style.configure(".", font=ui_font())
        style.configure("Header.TLabel", font=(ui_font()[0], ui_font()[1], "bold"))
        style.configure("Run.TButton", font=(ui_font()[0], ui_font()[1], "bold"))
        style.configure("Status.TLabel", foreground="#37474f")
        style.configure("Small.TLabel", foreground="#546e7a")

        self.grid(sticky="nsew")
        master.columnconfigure(0, weight=1)
        master.rowconfigure(0, weight=1)
        self.columnconfigure(0, weight=1)
        self.rowconfigure(2, weight=1)

        # top bar
        top = ttk.Frame(self)
        top.grid(row=0, column=0, sticky="ew", pady=(0, 10))
        top.columnconfigure(1, weight=1)

        hlabel(top, "cyph").grid(row=0, column=0, sticky="w")
        self.cyph_label = ttk.Label(top, text=str(self.cyph_path) if self.cyph_path else "(not found)")
        self.cyph_label.grid(row=0, column=1, sticky="w", padx=(10, 0))
        ttk.Button(top, text="Advanced…", command=self.advanced_dialog).grid(row=0, column=2, sticky="e")

        if not self.cyph_path:
            messagebox.showwarning(
                "cyph not found",
                "Не нашёл бинарник cyph.\n\n"
                "Положи 'cyph.exe' рядом с GUI (или 'cyph' на Linux),\n"
                "или добавь в PATH.\n\n"
                "Можно указать путь в Advanced…",
            )

        # notebook
        self.nb = ttk.Notebook(self)
        self.nb.grid(row=1, column=0, sticky="nsew")
        self.rowconfigure(1, weight=1)

        self.encrypt_tab = ttk.Frame(self.nb, padding=12)
        self.decrypt_tab = ttk.Frame(self.nb, padding=12)
        self.tools_tab = ttk.Frame(self.nb, padding=12)
        self.exchange_tab = ttk.Frame(self.nb, padding=12)
        self.manual_tab = ttk.Frame(self.nb, padding=12)

        self.nb.add(self.encrypt_tab, text="Encrypt")
        self.nb.add(self.decrypt_tab, text="Decrypt")
        self.nb.add(self.tools_tab, text="Key tools")
        self.nb.add(self.exchange_tab, text="Exchange")
        self.nb.add(self.manual_tab, text="Manual")

        # bottom output
        bottom = ttk.Frame(self)
        bottom.grid(row=2, column=0, sticky="nsew", pady=(10, 0))
        bottom.columnconfigure(0, weight=1)
        bottom.rowconfigure(1, weight=1)

        controls = ttk.Frame(bottom)
        controls.grid(row=0, column=0, sticky="ew")
        controls.columnconfigure(2, weight=1)

        self.status_var = tk.StringVar(value="Ready")
        self.run_btn = ttk.Button(controls, text="Run", style="Run.TButton", command=self.run_current_tab)
        self.run_btn.grid(row=0, column=0, sticky="w")

        self.stop_btn = ttk.Button(controls, text="Stop", command=self.stop, state="disabled")
        self.stop_btn.grid(row=0, column=1, sticky="w", padx=(8, 0))

        ttk.Label(controls, textvariable=self.status_var, style="Status.TLabel").grid(row=0, column=2, sticky="e")

        self.output = tk.Text(
            bottom,
            wrap="word",
            height=14,
            font=mono_font(),
            bg=LOG_BG,
            fg=LOG_FG,
            insertbackground=LOG_FG,
            relief="flat",
            padx=10,
            pady=10,
        )
        self.output.grid(row=1, column=0, sticky="nsew")

        scroll = ttk.Scrollbar(bottom, orient="vertical", command=self.output.yview)
        scroll.grid(row=1, column=1, sticky="ns")
        self.output.configure(yscrollcommand=scroll.set)

        self.output.tag_configure("cmd", foreground=LOG_CMD)
        self.output.tag_configure("stdout", foreground=LOG_FG)
        self.output.tag_configure("stderr", foreground=LOG_STDERR)
        self.output.tag_configure("meta", foreground=LOG_META)

        actions = ttk.Frame(bottom)
        actions.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        ttk.Button(actions, text="Clear output", command=lambda: self.output.delete("1.0", "end")).grid(row=0, column=0, sticky="w")
        ttk.Button(actions, text="Copy all", command=self.copy_all_output).grid(row=0, column=1, padx=(8, 0), sticky="w")

        # runner
        self.runner = ProcRunner(self.on_line, self.on_done)
        self.master.after(50, self.poll_runner)

        # build tabs
        self._build_encrypt_tab()
        self._build_decrypt_tab()
        self._build_tools_tab()
        self._build_exchange_tab()
        self._build_manual_tab()

    # -------- output helpers --------
    def copy_all_output(self):
        txt = self.output.get("1.0", "end-1c")
        self.master.clipboard_clear()
        self.master.clipboard_append(txt)

    # -------- runner plumbing --------
    def poll_runner(self):
        self.runner.poll()
        self.master.after(50, self.poll_runner)

    def on_line(self, kind: str, text: str):
        self.output.insert("end", text, kind)
        self.output.see("end")

    def on_done(self):
        self.set_running(False)

    def set_running(self, running: bool):
        self.run_btn.configure(state="disabled" if running else "normal")
        self.stop_btn.configure(state="normal" if running else "disabled")
        self.status_var.set("Running…" if running else "Ready")

    def stop(self):
        self.runner.stop()

    def append_cmd(self, cmd: list[str]):
        pretty = " ".join(shlex.quote(x) for x in cmd)
        self.output.insert("end", f"$ {pretty}\n", "cmd")
        self.output.see("end")

    def need_cyph(self) -> bool:
        if not self.cyph_path:
            messagebox.showerror("cyph not found", "cyph binary не найден. Укажи путь в Advanced…")
            return False
        return True

    # -------- advanced --------
    def advanced_dialog(self):
        w = tk.Toplevel(self.master)
        w.title("Advanced")
        w.transient(self.master)
        w.grab_set()
        w.resizable(False, False)

        frm = ttk.Frame(w, padding=12)
        frm.grid(sticky="nsew")
        frm.columnconfigure(1, weight=1)

        path_var = tk.StringVar(value=str(self.cyph_path) if self.cyph_path else "")

        hlabel(frm, "cyph path").grid(row=0, column=0, sticky="w")
        ttk.Entry(frm, textvariable=path_var, width=72).grid(row=0, column=1, sticky="ew", padx=(10, 0))

        def browse():
            p = filedialog.askopenfilename(title="Select cyph binary")
            if p:
                path_var.set(p)

        ttk.Button(frm, text="Browse…", command=browse).grid(row=0, column=2, padx=(10, 0))

        def apply():
            p = Path(path_var.get().strip())
            if not p.exists():
                messagebox.showerror("Bad path", "Файл не найден.")
                return
            self.cyph_path = p.resolve()
            self.cyph_label.configure(text=str(self.cyph_path))
            w.destroy()

        btns = ttk.Frame(frm)
        btns.grid(row=1, column=0, columnspan=3, sticky="e", pady=(12, 0))
        ttk.Button(btns, text="Cancel", command=w.destroy).grid(row=0, column=0, padx=(0, 8))
        ttk.Button(btns, text="Apply", command=apply).grid(row=0, column=1)

    # ----------------------------
    # Encrypt tab
    # ----------------------------
    def _build_encrypt_tab(self):
        t = self.encrypt_tab
        t.columnconfigure(0, weight=1)
        t.columnconfigure(1, weight=0)

        left = ttk.Frame(t)
        left.grid(row=0, column=0, sticky="nsew")
        left.columnconfigure(0, weight=1)

        right = ttk.Frame(t)
        right.grid(row=0, column=1, sticky="nsew", padx=(16, 0))
        right.columnconfigure(0, weight=1)

        section(left, "Input files (-f)").grid(row=0, column=0, sticky="ew")
        files_row = ttk.Frame(left)
        files_row.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        files_row.columnconfigure(0, weight=1)

        self.enc_files: list[str] = []
        self.enc_listbox = tk.Listbox(files_row, height=7)
        self.enc_listbox.grid(row=0, column=0, sticky="ew")

        section(left, "Output (-o)").grid(row=2, column=0, sticky="ew", pady=(18, 0))
        out_row = ttk.Frame(left)
        out_row.grid(row=3, column=0, sticky="ew", pady=(10, 0))
        out_row.columnconfigure(1, weight=1)

        self.enc_out_mode = tk.StringVar(value="auto")  # auto/file/dir
        self.enc_out_path = tk.StringVar()

        modes = ttk.Frame(out_row)
        modes.grid(row=0, column=0, columnspan=3, sticky="w")
        ttk.Radiobutton(modes, text="Auto", variable=self.enc_out_mode, value="auto", command=self._enc_out_ui).grid(row=0, column=0, padx=(0, 12))
        ttk.Radiobutton(modes, text="File", variable=self.enc_out_mode, value="file", command=self._enc_out_ui).grid(row=0, column=1, padx=(0, 12))
        ttk.Radiobutton(modes, text="Dir", variable=self.enc_out_mode, value="dir", command=self._enc_out_ui).grid(row=0, column=2)

        ttk.Label(out_row, text="Path").grid(row=1, column=0, sticky="w", pady=(10, 0))
        self.enc_out_entry = ttk.Entry(out_row, textvariable=self.enc_out_path)
        self.enc_out_entry.grid(row=1, column=1, sticky="ew", pady=(10, 0), padx=(10, 0))
        self.enc_out_browse = ttk.Button(out_row, text="Browse…", command=self.enc_pick_out)
        self.enc_out_browse.grid(row=1, column=2, pady=(10, 0))

        section(left, "Options").grid(row=4, column=0, sticky="ew", pady=(18, 0))
        opt_row = ttk.Frame(left)
        opt_row.grid(row=5, column=0, sticky="ew", pady=(10, 0))
        opt_row.columnconfigure(4, weight=1)

        self.enc_level = tk.IntVar(value=0)
        ttk.Label(opt_row, text="KDF level").grid(row=0, column=0, sticky="w")
        ttk.Radiobutton(opt_row, text="0", variable=self.enc_level, value=0).grid(row=0, column=1, padx=(10, 0))
        ttk.Radiobutton(opt_row, text="1", variable=self.enc_level, value=1).grid(row=0, column=2, padx=(10, 0))
        ttk.Radiobutton(opt_row, text="2", variable=self.enc_level, value=2).grid(row=0, column=3, padx=(10, 0))

        self.enc_anon = tk.BooleanVar(value=False)
        self.enc_wipe = tk.BooleanVar(value=False)
        ttk.Checkbutton(opt_row, text="-anon", variable=self.enc_anon).grid(row=0, column=5, padx=(18, 0))
        ttk.Checkbutton(opt_row, text="-WIPE", variable=self.enc_wipe).grid(row=0, column=6, padx=(12, 0))

        self.enc_key = KeyPanel(left, allow_master_for_cyphkey=True, title="Key (-k)")
        self.enc_key.grid(row=6, column=0, sticky="ew", pady=(18, 0))

        section(left, "Extra args").grid(row=7, column=0, sticky="ew", pady=(18, 0))
        extra_row = ttk.Frame(left)
        extra_row.grid(row=8, column=0, sticky="ew", pady=(10, 0))
        extra_row.columnconfigure(0, weight=1)
        self.enc_extra = tk.StringVar()
        ttk.Entry(extra_row, textvariable=self.enc_extra).grid(row=0, column=0, sticky="ew")

        # right actions
        hlabel(right, "Actions").grid(row=0, column=0, sticky="w")
        ttk.Separator(right, orient="horizontal").grid(row=1, column=0, sticky="ew", pady=(6, 10))
        ttk.Button(right, text="Add files…", command=self.enc_add_files).grid(row=2, column=0, sticky="ew")
        ttk.Button(right, text="Remove selected", command=self.enc_remove_selected).grid(row=3, column=0, sticky="ew", pady=(8, 0))
        ttk.Button(right, text="Clear list", command=self.enc_clear).grid(row=4, column=0, sticky="ew", pady=(8, 0))
        ttk.Separator(right, orient="horizontal").grid(row=5, column=0, sticky="ew", pady=(14, 12))
        ttk.Button(right, text="Run Encrypt", style="Run.TButton", command=self.run_encrypt).grid(row=6, column=0, sticky="ew")
        ttk.Label(
            right,
            text="Tip: avoid -k=... for real passwords.\nUse Prompt (-k?) or keyfiles.",
            style="Small.TLabel",
            justify="left",
        ).grid(row=7, column=0, sticky="w", pady=(14, 0))

        self._enc_out_ui()

    def _enc_out_ui(self):
        mode = self.enc_out_mode.get()
        if mode == "auto":
            self.enc_out_entry.configure(state="disabled")
            self.enc_out_browse.configure(state="disabled")
        else:
            self.enc_out_entry.configure(state="normal")
            self.enc_out_browse.configure(state="normal")

    def enc_add_files(self):
        files = filedialog.askopenfilenames(title="Select file(s) to encrypt")
        if not files:
            return
        for f in files:
            if f not in self.enc_files:
                self.enc_files.append(f)
                self.enc_listbox.insert("end", f)

    def enc_remove_selected(self):
        sel = list(self.enc_listbox.curselection())
        if not sel:
            return
        for i in reversed(sel):
            path = self.enc_listbox.get(i)
            self.enc_listbox.delete(i)
            try:
                self.enc_files.remove(path)
            except ValueError:
                pass

    def enc_clear(self):
        self.enc_files.clear()
        self.enc_listbox.delete(0, "end")

    def enc_pick_out(self):
        if self.enc_out_mode.get() == "dir":
            d = filedialog.askdirectory(title="Select output directory")
            if d:
                self.enc_out_path.set(d)
        else:
            f = filedialog.asksaveasfilename(title="Select output file (will add .cyph if missing)")
            if f:
                self.enc_out_path.set(f)

    def run_encrypt(self):
        if not self.need_cyph() or self.runner.running():
            return
        if not self.enc_files:
            messagebox.showerror("No input", "Добавь хотя бы один файл для шифрования.")
            return

        cmd = [str(self.cyph_path), "-f", *self.enc_files]

        try:
            key_args, stdin_lines = self.enc_key.build_key_and_master("-k", "-K")
        except ValueError as e:
            messagebox.showerror("Key error", str(e))
            return
        cmd += key_args

        if self.enc_out_mode.get() != "auto":
            outp = self.enc_out_path.get().strip()
            if not outp:
                messagebox.showerror("Output", "Укажи output path (или выбери Auto).")
                return
            cmd += ["-o", outp]

        cmd += ["-level", str(self.enc_level.get())]
        if self.enc_anon.get():
            cmd.append("-anon")

        wipe_requested = bool(self.enc_wipe.get())
        if wipe_requested:
            cmd.append("-WIPE")

        extra = self.enc_extra.get().strip()
        if extra:
            try:
                cmd += shlex.split(extra)
            except ValueError as e:
                messagebox.showerror("Bad extra args", str(e))
                return

        stdin_payload: list[str] = []
        stdin_payload.extend(stdin_lines)
        if wipe_requested:
            stdin_payload.append("Yes")

        stdin_text = ("\n".join(stdin_payload) + "\n") if stdin_payload else None

        self.append_cmd(cmd)
        self.set_running(True)
        self.runner.run(cmd, stdin_text=stdin_text)

        # hygiene: clear typed secrets immediately
        self.enc_key.clear_secrets()

    # ----------------------------
    # Decrypt tab
    # ----------------------------
    def _build_decrypt_tab(self):
        t = self.decrypt_tab
        t.columnconfigure(0, weight=1)
        t.columnconfigure(1, weight=0)

        left = ttk.Frame(t)
        left.grid(row=0, column=0, sticky="nsew")
        left.columnconfigure(0, weight=1)

        right = ttk.Frame(t)
        right.grid(row=0, column=1, sticky="nsew", padx=(16, 0))
        right.columnconfigure(0, weight=1)

        section(left, "Input .cyph files (-i)").grid(row=0, column=0, sticky="ew")
        files_row = ttk.Frame(left)
        files_row.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        files_row.columnconfigure(0, weight=1)

        self.dec_files: list[str] = []
        self.dec_listbox = tk.Listbox(files_row, height=7)
        self.dec_listbox.grid(row=0, column=0, sticky="ew")

        section(left, "Output (-o)").grid(row=2, column=0, sticky="ew", pady=(18, 0))
        out_row = ttk.Frame(left)
        out_row.grid(row=3, column=0, sticky="ew", pady=(10, 0))
        out_row.columnconfigure(1, weight=1)

        self.dec_out_mode = tk.StringVar(value="auto")
        self.dec_out_path = tk.StringVar()

        modes = ttk.Frame(out_row)
        modes.grid(row=0, column=0, columnspan=3, sticky="w")
        ttk.Radiobutton(modes, text="Auto", variable=self.dec_out_mode, value="auto", command=self._dec_out_ui).grid(row=0, column=0, padx=(0, 12))
        ttk.Radiobutton(modes, text="File", variable=self.dec_out_mode, value="file", command=self._dec_out_ui).grid(row=0, column=1, padx=(0, 12))
        ttk.Radiobutton(modes, text="Dir", variable=self.dec_out_mode, value="dir", command=self._dec_out_ui).grid(row=0, column=2)

        ttk.Label(out_row, text="Path").grid(row=1, column=0, sticky="w", pady=(10, 0))
        self.dec_out_entry = ttk.Entry(out_row, textvariable=self.dec_out_path)
        self.dec_out_entry.grid(row=1, column=1, sticky="ew", pady=(10, 0), padx=(10, 0))
        self.dec_out_browse = ttk.Button(out_row, text="Browse…", command=self.dec_pick_out)
        self.dec_out_browse.grid(row=1, column=2, pady=(10, 0))

        section(left, "Options").grid(row=4, column=0, sticky="ew", pady=(18, 0))
        opt_row = ttk.Frame(left)
        opt_row.grid(row=5, column=0, sticky="ew", pady=(10, 0))

        self.dec_restore = tk.BooleanVar(value=True)
        self.dec_stdout = tk.BooleanVar(value=False)
        self.dec_wipe = tk.BooleanVar(value=False)
        ttk.Checkbutton(opt_row, text="-d restore name", variable=self.dec_restore).grid(row=0, column=0, padx=(0, 12))
        ttk.Checkbutton(opt_row, text="-s stdout", variable=self.dec_stdout).grid(row=0, column=1, padx=(0, 12))
        ttk.Checkbutton(opt_row, text="-WIPE", variable=self.dec_wipe).grid(row=0, column=2)

        self.dec_key = KeyPanel(left, allow_master_for_cyphkey=True, title="Key (-k)")
        self.dec_key.grid(row=6, column=0, sticky="ew", pady=(18, 0))

        section(left, "Extra args").grid(row=7, column=0, sticky="ew", pady=(18, 0))
        extra_row = ttk.Frame(left)
        extra_row.grid(row=8, column=0, sticky="ew", pady=(10, 0))
        extra_row.columnconfigure(0, weight=1)
        self.dec_extra = tk.StringVar()
        ttk.Entry(extra_row, textvariable=self.dec_extra).grid(row=0, column=0, sticky="ew")

        # right actions
        hlabel(right, "Actions").grid(row=0, column=0, sticky="w")
        ttk.Separator(right, orient="horizontal").grid(row=1, column=0, sticky="ew", pady=(6, 10))
        ttk.Button(right, text="Add .cyph…", command=self.dec_add_files).grid(row=2, column=0, sticky="ew")
        ttk.Button(right, text="Remove selected", command=self.dec_remove_selected).grid(row=3, column=0, sticky="ew", pady=(8, 0))
        ttk.Button(right, text="Clear list", command=self.dec_clear).grid(row=4, column=0, sticky="ew", pady=(8, 0))
        ttk.Separator(right, orient="horizontal").grid(row=5, column=0, sticky="ew", pady=(14, 12))
        ttk.Button(right, text="Run Decrypt", style="Run.TButton", command=self.run_decrypt).grid(row=6, column=0, sticky="ew")

        self._dec_out_ui()

    def _dec_out_ui(self):
        mode = self.dec_out_mode.get()
        if mode == "auto":
            self.dec_out_entry.configure(state="disabled")
            self.dec_out_browse.configure(state="disabled")
        else:
            self.dec_out_entry.configure(state="normal")
            self.dec_out_browse.configure(state="normal")

    def dec_add_files(self):
        files = filedialog.askopenfilenames(
            title="Select .cyph file(s) to decrypt",
            filetypes=[("cyph containers", "*.cyph"), ("all files", "*.*")],
        )
        if not files:
            return
        for f in files:
            if f not in self.dec_files:
                self.dec_files.append(f)
                self.dec_listbox.insert("end", f)

    def dec_remove_selected(self):
        sel = list(self.dec_listbox.curselection())
        if not sel:
            return
        for i in reversed(sel):
            path = self.dec_listbox.get(i)
            self.dec_listbox.delete(i)
            try:
                self.dec_files.remove(path)
            except ValueError:
                pass

    def dec_clear(self):
        self.dec_files.clear()
        self.dec_listbox.delete(0, "end")

    def dec_pick_out(self):
        if self.dec_out_mode.get() == "dir":
            d = filedialog.askdirectory(title="Select output directory")
            if d:
                self.dec_out_path.set(d)
        else:
            f = filedialog.asksaveasfilename(title="Select output file")
            if f:
                self.dec_out_path.set(f)

    def run_decrypt(self):
        if not self.need_cyph() or self.runner.running():
            return
        if not self.dec_files:
            messagebox.showerror("No input", "Добавь хотя бы один .cyph файл.")
            return

        cmd = [str(self.cyph_path), "-i", *self.dec_files]

        try:
            key_args, stdin_lines = self.dec_key.build_key_and_master("-k", "-K")
        except ValueError as e:
            messagebox.showerror("Key error", str(e))
            return
        cmd += key_args

        if self.dec_out_mode.get() != "auto":
            outp = self.dec_out_path.get().strip()
            if not outp:
                messagebox.showerror("Output", "Укажи output path (или выбери Auto).")
                return
            cmd += ["-o", outp]

        if self.dec_restore.get():
            cmd.append("-d")
        if self.dec_stdout.get():
            cmd.append("-s")

        wipe_requested = bool(self.dec_wipe.get())
        if wipe_requested:
            cmd.append("-WIPE")

        extra = self.dec_extra.get().strip()
        if extra:
            try:
                cmd += shlex.split(extra)
            except ValueError as e:
                messagebox.showerror("Bad extra args", str(e))
                return

        stdin_payload: list[str] = []
        stdin_payload.extend(stdin_lines)
        if wipe_requested:
            stdin_payload.append("Yes")

        stdin_text = ("\n".join(stdin_payload) + "\n") if stdin_payload else None

        self.append_cmd(cmd)
        self.set_running(True)
        self.runner.run(cmd, stdin_text=stdin_text)

        self.dec_key.clear_secrets()

    # ----------------------------
    # Key tools tab
    # ----------------------------
    def _build_tools_tab(self):
        t = self.tools_tab
        t.columnconfigure(0, weight=1)

        section(t, "Generate key (-gen)").grid(row=0, column=0, sticky="ew")
        gen_row = ttk.Frame(t)
        gen_row.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        gen_row.columnconfigure(1, weight=1)

        self.gen_save = tk.BooleanVar(value=True)
        self.gen_out = tk.StringVar()

        ttk.Checkbutton(gen_row, text="Save to file (-o)", variable=self.gen_save, command=self._gen_ui).grid(row=0, column=0, sticky="w")
        self.gen_out_ent = ttk.Entry(gen_row, textvariable=self.gen_out)
        self.gen_out_ent.grid(row=0, column=1, sticky="ew", padx=(10, 0))
        self.gen_out_btn = ttk.Button(gen_row, text="Browse…", command=self.gen_pick)
        self.gen_out_btn.grid(row=0, column=2, padx=(10, 0))
        ttk.Button(gen_row, text="Run", style="Run.TButton", command=self.run_gen).grid(row=0, column=3, padx=(12, 0))

        section(t, "Wrap key file into .cyphkey (-key)").grid(row=2, column=0, sticky="ew", pady=(22, 0))
        wrap_row = ttk.Frame(t)
        wrap_row.grid(row=3, column=0, sticky="ew", pady=(10, 0))
        wrap_row.columnconfigure(1, weight=1)

        self.wrap_in = tk.StringVar()
        self.wrap_out = tk.StringVar()
        self.wrap_level = tk.IntVar(value=0)

        ttk.Label(wrap_row, text="Input key file").grid(row=0, column=0, sticky="w")
        ttk.Entry(wrap_row, textvariable=self.wrap_in).grid(row=0, column=1, sticky="ew", padx=(10, 0))
        ttk.Button(wrap_row, text="Browse…", command=self.wrap_pick_in).grid(row=0, column=2, padx=(10, 0))

        ttk.Label(wrap_row, text="Output (-o)").grid(row=1, column=0, sticky="w", pady=(10, 0))
        ttk.Entry(wrap_row, textvariable=self.wrap_out).grid(row=1, column=1, sticky="ew", padx=(10, 0), pady=(10, 0))
        ttk.Button(wrap_row, text="Browse…", command=self.wrap_pick_out).grid(row=1, column=2, padx=(10, 0), pady=(10, 0))

        lvl = ttk.Frame(t)
        lvl.grid(row=4, column=0, sticky="w", pady=(12, 0))
        ttk.Label(lvl, text="KDF level").grid(row=0, column=0, sticky="w")
        ttk.Radiobutton(lvl, text="0", variable=self.wrap_level, value=0).grid(row=0, column=1, padx=(10, 0))
        ttk.Radiobutton(lvl, text="1", variable=self.wrap_level, value=1).grid(row=0, column=2, padx=(10, 0))
        ttk.Radiobutton(lvl, text="2", variable=self.wrap_level, value=2).grid(row=0, column=3, padx=(10, 0))

        self.wrap_master = KeyPanel(t, allow_master_for_cyphkey=False, title="Master (-K)")
        # Always show master controls for wrap:
        self.wrap_master.master_area.grid()
        self.wrap_master.grid(row=5, column=0, sticky="ew", pady=(18, 0))

        ttk.Button(t, text="Run wrap (-key)", style="Run.TButton", command=self.run_wrap).grid(row=6, column=0, sticky="e", pady=(14, 0))

        self._gen_ui()

    def _gen_ui(self):
        state = "normal" if self.gen_save.get() else "disabled"
        self.gen_out_ent.configure(state=state)
        self.gen_out_btn.configure(state=state)
        if not self.gen_save.get():
            self.gen_out.set("")

    def gen_pick(self):
        f = filedialog.asksaveasfilename(title="Save generated key to file")
        if f:
            self.gen_out.set(f)

    def run_gen(self):
        if not self.need_cyph() or self.runner.running():
            return
        cmd = [str(self.cyph_path), "-gen"]
        if self.gen_save.get():
            out = self.gen_out.get().strip()
            if not out:
                messagebox.showerror("Missing output", "Укажи файл для -o.")
                return
            cmd += ["-o", out]
        self.append_cmd(cmd)
        self.set_running(True)
        self.runner.run(cmd)

    def wrap_pick_in(self):
        p = filedialog.askopenfilename(title="Select key file to wrap")
        if p:
            self.wrap_in.set(p)

    def wrap_pick_out(self):
        p = filedialog.asksaveasfilename(title="Output .cyphkey name/path")
        if p:
            self.wrap_out.set(p)

    def run_wrap(self):
        if not self.need_cyph() or self.runner.running():
            return
        inp = self.wrap_in.get().strip()
        out = self.wrap_out.get().strip()
        if not inp or not Path(inp).exists():
            messagebox.showerror("Bad input", "Выбери существующий key file.")
            return
        if not out:
            messagebox.showerror("Bad output", "Укажи -o для .cyphkey.")
            return

        try:
            m_args, m_stdin = self.wrap_master.build_master_only("-K")
        except ValueError as e:
            messagebox.showerror("Missing master key", str(e))
            return

        cmd = [str(self.cyph_path), "-key", inp, "-o", out, "-level", str(self.wrap_level.get())] + m_args
        stdin_text = ("\n".join(m_stdin) + "\n") if m_stdin else None

        self.append_cmd(cmd)
        self.set_running(True)
        self.runner.run(cmd, stdin_text=stdin_text)

        self.wrap_master.clear_secrets()

    # ----------------------------
    # Exchange tab
    # ----------------------------
    def _build_exchange_tab(self):
        t = self.exchange_tab
        t.columnconfigure(0, weight=1)
        t.columnconfigure(1, weight=0)

        left = ttk.Frame(t)
        left.grid(row=0, column=0, sticky="nsew")
        left.columnconfigure(0, weight=1)

        right = ttk.Frame(t)
        right.grid(row=0, column=1, sticky="nsew", padx=(16, 0))
        right.columnconfigure(0, weight=1)

        section(left, "Exchange (-e)").grid(row=0, column=0, sticky="ew")
        ex_row = ttk.Frame(left)
        ex_row.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        ex_row.columnconfigure(1, weight=1)

        self.ex_step = tk.IntVar(value=1)
        self.ex_out = tk.StringVar()
        self.ex_file = tk.StringVar()
        self.ex_peer_pub = tk.StringVar()
        self.ex_peer_confirm = tk.BooleanVar(value=True)
        self.ex_level = tk.IntVar(value=0)

        step_row = ttk.Frame(ex_row)
        step_row.grid(row=0, column=0, columnspan=3, sticky="w")
        ttk.Radiobutton(step_row, text="Step 1: create", variable=self.ex_step, value=1, command=self._ex_ui).grid(row=0, column=0, padx=(0, 12))
        ttk.Radiobutton(step_row, text="Step 2: finalize", variable=self.ex_step, value=2, command=self._ex_ui).grid(row=0, column=1)

        ttk.Label(ex_row, text="Step1 -o").grid(row=1, column=0, sticky="w", pady=(10, 0))
        self.ex_out_ent = ttk.Entry(ex_row, textvariable=self.ex_out)
        self.ex_out_ent.grid(row=1, column=1, sticky="ew", pady=(10, 0), padx=(10, 0))
        self.ex_out_btn = ttk.Button(ex_row, text="Browse…", command=self.ex_pick_out)
        self.ex_out_btn.grid(row=1, column=2, pady=(10, 0))

        ttk.Label(ex_row, text="Step2 file").grid(row=2, column=0, sticky="w", pady=(10, 0))
        self.ex_file_ent = ttk.Entry(ex_row, textvariable=self.ex_file)
        self.ex_file_ent.grid(row=2, column=1, sticky="ew", pady=(10, 0), padx=(10, 0))
        self.ex_file_btn = ttk.Button(ex_row, text="Browse…", command=self.ex_pick_file)
        self.ex_file_btn.grid(row=2, column=2, pady=(10, 0))

        ttk.Label(ex_row, text="Peer pubkey").grid(row=3, column=0, sticky="w", pady=(10, 0))
        self.ex_peer_ent = ttk.Entry(ex_row, textvariable=self.ex_peer_pub)
        self.ex_peer_ent.grid(row=3, column=1, columnspan=2, sticky="ew", pady=(10, 0), padx=(10, 0))

        lvl = ttk.Frame(left)
        lvl.grid(row=2, column=0, sticky="w", pady=(14, 0))
        ttk.Label(lvl, text="KDF level").grid(row=0, column=0, sticky="w")
        ttk.Radiobutton(lvl, text="0", variable=self.ex_level, value=0).grid(row=0, column=1, padx=(10, 0))
        ttk.Radiobutton(lvl, text="1", variable=self.ex_level, value=1).grid(row=0, column=2, padx=(10, 0))
        ttk.Radiobutton(lvl, text="2", variable=self.ex_level, value=2).grid(row=0, column=3, padx=(10, 0))

        self.ex_key = KeyPanel(left, allow_master_for_cyphkey=False, title="Password key (-k)")
        self.ex_key.grid(row=3, column=0, sticky="ew", pady=(18, 0))

        conf = ttk.Frame(left)
        conf.grid(row=4, column=0, sticky="w", pady=(12, 0))
        ttk.Checkbutton(conf, text="Auto-confirm fingerprint (send Yes)", variable=self.ex_peer_confirm).grid(row=0, column=0, sticky="w")

        # right actions
        hlabel(right, "Actions").grid(row=0, column=0, sticky="w")
        ttk.Separator(right, orient="horizontal").grid(row=1, column=0, sticky="ew", pady=(6, 10))
        ttk.Button(right, text="Run Exchange", style="Run.TButton", command=self.run_exchange).grid(row=2, column=0, sticky="ew")
        ttk.Label(
            right,
            text="Step 1 prints your public key.\nSend it to peer.\n\nStep 2: paste peer key\nand run finalize.",
            style="Small.TLabel",
            justify="left",
        ).grid(row=3, column=0, sticky="w", pady=(14, 0))

        self._ex_ui()

    def _ex_ui(self):
        step = self.ex_step.get()
        if step == 1:
            self.ex_out_ent.configure(state="normal")
            self.ex_out_btn.configure(state="normal")
            self.ex_file_ent.configure(state="disabled")
            self.ex_file_btn.configure(state="disabled")
            self.ex_peer_ent.configure(state="disabled")
        else:
            self.ex_out_ent.configure(state="disabled")
            self.ex_out_btn.configure(state="disabled")
            self.ex_file_ent.configure(state="normal")
            self.ex_file_btn.configure(state="normal")
            self.ex_peer_ent.configure(state="normal")

    def ex_pick_out(self):
        p = filedialog.asksaveasfilename(title="Output name/path for exchange keyfile (.cyphkey will be added)")
        if p:
            self.ex_out.set(p)

    def ex_pick_file(self):
        p = filedialog.askopenfilename(
            title="Select exchange .cyphkey",
            filetypes=[("cyphkey", "*.cyphkey"), ("all", "*.*")],
        )
        if p:
            self.ex_file.set(p)

    def run_exchange(self):
        if not self.need_cyph() or self.runner.running():
            return

        try:
            key_args, stdin_lines = self.ex_key.build_key_and_master("-k", "-K")
        except ValueError as e:
            messagebox.showerror("Missing key", str(e))
            return

        step = self.ex_step.get()
        if step == 1:
            out = self.ex_out.get().strip()
            if not out:
                messagebox.showerror("Missing output", "Укажи -o для step1.")
                return
            cmd = [str(self.cyph_path), "-e", "-o", out, "-level", str(self.ex_level.get())] + key_args
            stdin_text = ("\n".join(stdin_lines) + "\n") if stdin_lines else None
        else:
            f = self.ex_file.get().strip()
            if not f:
                messagebox.showerror("Missing file", "Выбери .cyphkey для step2.")
                return
            peer = self.ex_peer_pub.get().strip()
            if not peer.startswith("cyphx1:"):
                messagebox.showerror("Bad peer key", "Peer public key должен начинаться с 'cyphx1:'.")
                return
            cmd = [str(self.cyph_path), "-e", f, "-level", str(self.ex_level.get())] + key_args
            stdin_payload: list[str] = []
            stdin_payload.extend(stdin_lines)
            stdin_payload.append(peer)
            stdin_payload.append("Yes" if self.ex_peer_confirm.get() else "No")
            stdin_text = "\n".join(stdin_payload) + "\n"

        self.append_cmd(cmd)
        self.set_running(True)
        self.runner.run(cmd, stdin_text=stdin_text)

        self.ex_key.clear_secrets()

    # ----------------------------
    # Manual tab
    # ----------------------------
    def _build_manual_tab(self):
        t = self.manual_tab
        t.columnconfigure(0, weight=1)
        t.rowconfigure(2, weight=1)

        section(t, "Manual / Help").grid(row=0, column=0, sticky="ew")

        bar = ttk.Frame(t)
        bar.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        bar.columnconfigure(6, weight=1)

        ttk.Button(bar, text="--help", command=lambda: self.manual_load(["--help"])).grid(row=0, column=0, sticky="w")
        ttk.Button(bar, text="-man", command=lambda: self.manual_load(["-man"])).grid(row=0, column=1, sticky="w", padx=(8, 0))
        ttk.Button(bar, text="--version", command=lambda: self.manual_load(["--version"])).grid(row=0, column=2, sticky="w", padx=(8, 0))
        ttk.Button(bar, text="Recommendations", command=self.manual_show_gui_help).grid(row=0, column=3, sticky="w", padx=(8, 0))
        ttk.Button(bar, text="Clear", command=lambda: self.manual_text.delete("1.0", "end")).grid(row=0, column=4, sticky="w", padx=(8, 0))

        self.manual_text = tk.Text(
            t,
            wrap="word",
            font=mono_font(),
            bg=LOG_BG,
            fg=LOG_FG,
            insertbackground=LOG_FG,
            relief="flat",
            padx=10,
            pady=10,
        )
        self.manual_text.grid(row=2, column=0, sticky="nsew", pady=(10, 0))
        s = ttk.Scrollbar(t, orient="vertical", command=self.manual_text.yview)
        s.grid(row=2, column=1, sticky="ns", pady=(10, 0))
        self.manual_text.configure(yscrollcommand=s.set)
        self.manual_text.insert("end", "Press a button above to load cyph help/man/version.\n")

    def manual_load(self, args: list[str]):
        if not self.need_cyph():
            return
        if self.runner.running():
            messagebox.showinfo("Busy", "Сейчас выполняется другая команда. Дождись окончания или нажми Stop.")
            return

        cmd = [str(self.cyph_path), *args]
        self.append_cmd(cmd)

        def worker():
            try:
                p = subprocess.run(cmd, capture_output=True, text=True)
                out = (p.stdout or "") + (p.stderr or "")
                if not out.strip():
                    out = f"(no output) [exit code {p.returncode}]\n"
            except Exception as e:
                out = f"[error] {e}\n"

            def ui():
                self.manual_text.delete("1.0", "end")
                self.manual_text.insert("end", out)
                self.manual_text.see("1.0")

            self.master.after(0, ui)

        threading.Thread(target=worker, daemon=True).start()

    def manual_print_to_file(self):
        if not self.need_cyph():
            return
        if self.runner.running():
            messagebox.showinfo("Busy", "Сейчас выполняется другая команда. Дождись окончания или нажми Stop.")
            return
        cmd = [str(self.cyph_path), "-manprint"]
        self.append_cmd(cmd)
        self.set_running(True)
        self.runner.run(cmd)
        self.manual_text.insert("end", "\n(manprint) writes: cyph_manual.txt in current working directory\n")
        self.manual_text.see("end")

    def manual_show_gui_help(self):
        self.manual_text.delete("1.0", "end")
        self.manual_text.insert("end", GUI_HELP)
        self.manual_text.see("1.0")

    # ----------------------------
    # Tab router
    # ----------------------------
    def run_current_tab(self):
        if self.runner.running():
            return
        idx = self.nb.index(self.nb.select())
        if idx == 0:
            self.run_encrypt()
        elif idx == 1:
            self.run_decrypt()
        elif idx == 2:
            messagebox.showinfo("Key tools", "На вкладке Key tools жми Run рядом с нужным действием.")
        elif idx == 3:
            self.run_exchange()
        else:
            messagebox.showinfo("Manual", "На вкладке Manual используй кнопки help/man/version.")


def main():
    root = tk.Tk()
    if is_windows():
        try:
            from ctypes import windll
            windll.shcore.SetProcessDpiAwareness(1)
        except Exception:
            pass

    CyphGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()

import ctypes
import json
import platform
import tkinter as tk
from datetime import datetime, timezone
from pathlib import Path
from tkinter import ttk


TESTS_RELATIVE_PATH = "tests"
LOGS_RELATIVE_PATH = "logs"


class SyntaxColors:
    KEYWORD = "#6fb7ff"
    STRING = "#e6b37a"
    COMMENT = "#7ecf8f"
    CODE_BG = "#101820"
    TEXT = "#f2f5f8"


CPP_KEYWORDS = {
    "auto", "bool", "break", "case", "char", "class", "const", "continue", "default", "delete",
    "double", "else", "enum", "false", "float", "for", "if", "include", "int", "long", "namespace",
    "new", "nullptr", "private", "public", "return", "short", "sizeof", "static", "std", "struct",
    "switch", "true", "using", "void", "while",
}

PYTHON_KEYWORDS = {
    "and", "as", "assert", "break", "class", "continue", "def", "elif", "else", "False", "for",
    "from", "if", "import", "in", "is", "lambda", "None", "not", "or", "pass", "return", "True",
    "try", "while", "with", "yield",
}


def enable_windows_dpi_awareness():
    if platform.system() != "Windows":
        return
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)
    except Exception:
        try:
            ctypes.windll.user32.SetProcessDPIAware()
        except Exception:
            pass


class TestSession:
    def __init__(self, test_data):
        self.test = test_data
        self.current_card_index = 0
        self.started_at = datetime.now(timezone.utc)
        self.finished = False
        self.answers = [None for _ in self.cards]
        self.answered = [False for _ in self.cards]
        self.dirty = [False for _ in self.cards]

    @property
    def cards(self):
        return self.test.get("cards", [])

    @property
    def current_card(self):
        if not self.cards:
            return {}
        return self.cards[self.current_card_index]

    def normalized_answer(self, index):
        answer = self.answers[index]
        card_type = self.cards[index].get("type", "single")
        if card_type == "multi":
            return sorted(answer or [])
        if card_type == "text":
            return (answer or "").strip()
        return answer

    def is_correct(self, index):
        if not self.answered[index]:
            return False
        card = self.cards[index]
        correct = card.get("correct")
        answer = self.normalized_answer(index)
        if card.get("type") == "text":
            return str(answer).strip().lower() == str(correct).strip().lower()
        if card.get("type") == "multi":
            return sorted(answer) == sorted(correct or [])
        return answer == correct

    def state_for_card(self, index):
        if not self.answered[index]:
            return "empty"
        if self.test.get("mode", "exercise") == "exercise" and self.is_correct(index):
            return "correct"
        if self.test.get("mode", "exercise") != "exercise":
            return "answered"
        return "selected"

    def stats(self):
        correct = sum(1 for i in range(len(self.cards)) if self.is_correct(i))
        answered = sum(1 for value in self.answered if value)
        elapsed = int((datetime.now(timezone.utc) - self.started_at).total_seconds())
        return {"total": len(self.cards), "answered": answered, "correct": correct, "elapsedSeconds": elapsed}


class ScrollableFrame(ttk.Frame):
    def __init__(self, parent, bg):
        super().__init__(parent)
        self.canvas = tk.Canvas(self, highlightthickness=0, bg=bg)
        self.scrollbar = ttk.Scrollbar(self, orient="vertical", command=self.canvas.yview)
        self.content = ttk.Frame(self.canvas)
        self.window_id = self.canvas.create_window((0, 0), window=self.content, anchor="nw")
        self.canvas.configure(yscrollcommand=self.scrollbar.set)
        self.canvas.pack(side="left", fill="both", expand=True)
        self.scrollbar.pack(side="right", fill="y")
        self.content.bind("<Configure>", self._on_content_configure)
        self.canvas.bind("<Configure>", self._on_canvas_configure)
        self.canvas.bind("<Enter>", self._bind_mousewheel)
        self.canvas.bind("<Leave>", self._unbind_mousewheel)

    def _on_content_configure(self, _event):
        self.canvas.configure(scrollregion=self.canvas.bbox("all"))

    def _on_canvas_configure(self, event):
        self.canvas.itemconfigure(self.window_id, width=event.width)

    def _bind_mousewheel(self, _event):
        self.canvas.bind_all("<MouseWheel>", self._on_mousewheel)
        self.canvas.bind_all("<Button-4>", self._on_mousewheel)
        self.canvas.bind_all("<Button-5>", self._on_mousewheel)

    def _unbind_mousewheel(self, _event):
        self.canvas.unbind_all("<MouseWheel>")
        self.canvas.unbind_all("<Button-4>")
        self.canvas.unbind_all("<Button-5>")

    def _on_mousewheel(self, event):
        if event.num == 4:
            delta = -1
        elif event.num == 5:
            delta = 1
        else:
            delta = -1 if event.delta > 0 else 1
        self.canvas.yview_scroll(delta, "units")


class QtTrenTkApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.base_dir = Path(__file__).resolve().parent
        self.tests_dir = self.base_dir / TESTS_RELATIVE_PATH
        self.logs_dir = self.base_dir / LOGS_RELATIVE_PATH
        self.tests = []
        self.session = None
        self.test_tiles = []
        self.progress_buttons = []
        self.answer_vars = {}
        self.paned = None

        self.colors = {
            "bg": "#15191f",
            "panel": "#20262e",
            "panel_soft": "#252c35",
            "panel_active": "#26394a",
            "border": "#36414e",
            "text": "#f2f5f8",
            "muted": "#aeb8c4",
            "accent": "#6fb7ff",
            "correct": "#65d184",
            "selected": "#f0c76c",
            "answered": "#c9d1d9",
            "empty": "#48525f",
            "button_bg": "#e5edf5",
            "button_text": "#111820",
        }

        self.title("QtTren - tkinter")
        self.geometry("1220x780")
        self.minsize(980, 620)
        self.configure(bg=self.colors["bg"])
        self._configure_scaling()
        self._configure_styles()
        self._build_ui()
        self._load_tests()
        if self.tests:
            self.select_test(0)

    def _configure_scaling(self):
        if platform.system() == "Windows":
            try:
                dpi = self.winfo_fpixels("1i")
                self.tk.call("tk", "scaling", dpi / 72.0)
            except tk.TclError:
                self.tk.call("tk", "scaling", 1.5)

    def _configure_styles(self):
        self.style = ttk.Style(self)
        self.style.theme_use("clam")
        self.style.configure("App.TFrame", background=self.colors["bg"])
        self.style.configure("Panel.TFrame", background=self.colors["panel"])
        self.style.configure("Card.TFrame", background=self.colors["panel_soft"])
        self.style.configure("TScrollbar", background=self.colors["panel"], troughcolor=self.colors["bg"])

    def _build_ui(self):
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)

        self.paned = tk.PanedWindow(self, orient=tk.HORIZONTAL, bg=self.colors["bg"], bd=0,
                                    sashwidth=8, sashrelief="flat", showhandle=False,
                                    opaqueresize=True)
        self.paned.grid(row=0, column=0, sticky="nsew")

        left = tk.Frame(self.paned, bg=self.colors["panel"], width=330, padx=18, pady=18)
        left.grid_propagate(False)
        left.rowconfigure(1, weight=1)
        left.columnconfigure(0, weight=1)

        tk.Label(left, text="Набор тестов", bg=self.colors["panel"], fg=self.colors["text"],
                 font=("Segoe UI", 24, "bold"), anchor="w").grid(row=0, column=0, sticky="ew", pady=(0, 14))

        self.tests_scroll = ScrollableFrame(left, self.colors["panel"])
        self.tests_scroll.grid(row=1, column=0, sticky="nsew")

        right = tk.Frame(self.paned, bg=self.colors["bg"], padx=22, pady=22)
        right.columnconfigure(0, weight=1)
        right.rowconfigure(2, weight=1)

        header = tk.Frame(right, bg=self.colors["bg"])
        header.grid(row=0, column=0, sticky="ew")
        header.columnconfigure(0, weight=1)
        self.title_label = tk.Label(header, bg=self.colors["bg"], fg=self.colors["text"],
                                    font=("Segoe UI", 26, "bold"), anchor="w")
        self.title_label.grid(row=0, column=0, sticky="ew")
        self.mode_label = tk.Label(header, bg=self.colors["bg"], fg=self.colors["muted"],
                                   font=("Segoe UI", 14), anchor="w")
        self.mode_label.grid(row=1, column=0, sticky="ew", pady=(4, 0))
        self.finish_button = self._button(header, "Завершить", self.finish_test)
        self.finish_button.grid(row=0, column=1, rowspan=2, padx=(12, 0))

        self.progress_frame = tk.Frame(right, bg=self.colors["bg"])
        self.progress_frame.grid(row=1, column=0, sticky="ew", pady=(18, 14))

        self.card_scroll = ScrollableFrame(right, self.colors["panel"])
        self.card_scroll.grid(row=2, column=0, sticky="nsew")

        actions = tk.Frame(right, bg=self.colors["bg"])
        actions.grid(row=3, column=0, sticky="ew", pady=(14, 0))
        actions.columnconfigure(2, weight=1)
        self.reset_button = self._button(actions, "Сбросить", self.reset_card)
        self.save_button = self._button(actions, "Сохранить", self.save_card)
        self.back_button = self._button(actions, "Назад", self.previous_card)
        self.next_button = self._button(actions, "Вперед", self.next_card)
        self.reset_button.grid(row=0, column=0, padx=(0, 10))
        self.save_button.grid(row=0, column=1, padx=(0, 10))
        self.back_button.grid(row=0, column=3, padx=(0, 10))
        self.next_button.grid(row=0, column=4)

        self.paned.add(left, minsize=260)
        self.paned.add(right, minsize=620)
        self.after_idle(lambda: self.paned.sash_place(0, 330, 0))

    def _button(self, parent, text, command):
        return tk.Button(parent, text=text, command=command, bg=self.colors["button_bg"], fg=self.colors["button_text"],
                         activebackground="#b9d6f2", activeforeground=self.colors["button_text"],
                         disabledforeground="#5f6872", relief="flat", padx=14, pady=7,
                         font=("Segoe UI", 10, "bold"), cursor="hand2")

    def _load_tests(self):
        self.tests.clear()
        if self.tests_dir.exists():
            for path in sorted(self.tests_dir.glob("*.json")):
                try:
                    with path.open("r", encoding="utf-8") as file:
                        data = json.load(file)
                    if data.get("id") and data.get("cards"):
                        self.tests.append(data)
                except (OSError, json.JSONDecodeError) as error:
                    print(f"Cannot load {path}: {error}")
        self._rebuild_tests_panel()

    def _rebuild_tests_panel(self):
        for child in self.tests_scroll.content.winfo_children():
            child.destroy()
        self.test_tiles.clear()

        if not self.tests:
            tk.Label(self.tests_scroll.content, text=f"Нет JSON-файлов в {TESTS_RELATIVE_PATH}",
                     bg=self.colors["panel"], fg=self.colors["muted"], anchor="w",
                     font=("Segoe UI", 12)).pack(fill="x")
            return

        for index, test in enumerate(self.tests):
            tile = tk.Frame(self.tests_scroll.content, bg=self.colors["panel_soft"],
                            highlightbackground=self.colors["border"], highlightthickness=1, padx=10, pady=8)
            tile.pack(fill="x", pady=(0, 10))
            tile.bind("<Button-1>", lambda _event, i=index: self.select_test(i))
            tile.configure(cursor="hand2")

            title = tk.Label(tile, text=test.get("title", "Без названия"), bg=self.colors["panel_soft"],
                             fg=self.colors["text"], font=("Segoe UI", 11, "bold"), anchor="w", justify="left")
            title.pack(fill="x")
            desc = tk.Label(tile, text=test.get("description", ""), bg=self.colors["panel_soft"],
                            fg=self.colors["text"], font=("Segoe UI", 10), anchor="w", justify="left", wraplength=275)
            desc.pack(fill="x", pady=(2, 0))
            stats = tk.Label(tile, bg=self.colors["panel_soft"], fg=self.colors["text"],
                             font=("Segoe UI", 10), anchor="w")
            stats.pack(fill="x", pady=(2, 0))
            self._bind_test_tile_click(tile, index)
            self.test_tiles.append((tile, title, desc, stats))

        self._update_test_tiles()

    def _bind_test_tile_click(self, widget, index):
        widget.bind("<Button-1>", lambda _event, i=index: self.select_test(i))
        widget.configure(cursor="hand2")
        for child in widget.winfo_children():
            self._bind_test_tile_click(child, index)

    def _update_test_tiles(self):
        for index, test in enumerate(self.tests):
            tile, title, desc, stats = self.test_tiles[index]
            active = self.session is not None and self.session.test is test
            bg = self.colors["panel_active"] if active else self.colors["panel_soft"]
            border = self.colors["accent"] if active else self.colors["border"]
            for widget in (tile, title, desc, stats):
                widget.configure(bg=bg)
            tile.configure(highlightbackground=border)

            answered = correct = 0
            if active:
                session_stats = self.session.stats()
                answered = session_stats["answered"]
                correct = session_stats["correct"]
            mode = "учебный" if test.get("mode", "exercise") == "exercise" else "проверка"
            stats.configure(text=f"{answered}/{len(test.get('cards', []))} отвечено  |  {correct} верно  |  {mode}")

    def select_test(self, index):
        if index < 0 or index >= len(self.tests):
            return
        if self.session is not None:
            self._log_current_card("leave")
        self.session = TestSession(self.tests[index])
        self._refresh_all()

    def select_card(self, index):
        if self.session is None or index < 0 or index >= len(self.session.cards):
            return
        self._log_current_card("leave")
        self.session.current_card_index = index
        self.session.finished = False
        self._refresh_all()

    def previous_card(self):
        if self.session and self.session.current_card_index > 0:
            self.select_card(self.session.current_card_index - 1)

    def next_card(self):
        if not self.session:
            return
        if self.session.current_card_index + 1 < len(self.session.cards):
            self.select_card(self.session.current_card_index + 1)
        else:
            self.finish_test()

    def reset_card(self):
        if not self.session:
            return
        index = self.session.current_card_index
        self.session.answers[index] = None
        self.session.answered[index] = False
        self.session.dirty[index] = False
        self._log_current_card("reset")
        self._refresh_all()

    def save_card(self):
        self._log_current_card("save")
        self._refresh_all()

    def finish_test(self):
        if not self.session:
            return
        self._log_current_card("leave")
        self.session.finished = True
        self._append_log({
            "event": "finish",
            "testId": self.session.test.get("id"),
            "timestamp": self._timestamp(),
            "stats": self.session.stats(),
        })
        self._refresh_all()

    def _set_single_answer(self, option_id):
        index = self.session.current_card_index
        self.session.answers[index] = option_id
        self.session.answered[index] = True
        self.session.dirty[index] = True
        self._refresh_progress_and_tiles()

    def _toggle_multi_answer(self, option_id, variable):
        index = self.session.current_card_index
        selected = set(self.session.answers[index] or [])
        if variable.get():
            selected.add(option_id)
        else:
            selected.discard(option_id)
        self.session.answers[index] = sorted(selected)
        self.session.answered[index] = bool(selected)
        self.session.dirty[index] = True
        self._refresh_progress_and_tiles()

    def _set_text_answer(self, variable):
        index = self.session.current_card_index
        self.session.answers[index] = variable.get()
        self.session.answered[index] = bool(variable.get().strip())
        self.session.dirty[index] = True
        self._refresh_progress_and_tiles()

    def _refresh_all(self):
        self._update_header()
        self._rebuild_progress()
        self._rebuild_card()
        self._update_test_tiles()

    def _refresh_progress_and_tiles(self):
        self._rebuild_progress()
        self._update_test_tiles()

    def _update_header(self):
        if not self.session:
            self.title_label.configure(text="Тесты не найдены")
            self.mode_label.configure(text="")
            return
        self.title_label.configure(text=self.session.test.get("title", "Без названия"))
        self.mode_label.configure(text="Учебный режим: правильные ответы подсвечиваются сразу"
                                  if self.session.test.get("mode", "exercise") == "exercise"
                                  else "Проверка знаний: правильность видна в итоговой статистике")
        finished = self.session.finished
        self.finish_button.configure(state="disabled" if finished else "normal")
        self.reset_button.configure(state="disabled" if finished else "normal")
        self.save_button.configure(state="disabled" if finished else "normal")
        self.back_button.configure(state="normal" if self.session.current_card_index > 0 and not finished else "disabled")
        self.next_button.configure(text="Итоги" if self.session.current_card_index + 1 >= len(self.session.cards) else "Вперед",
                                   state="disabled" if finished else "normal")

    def _rebuild_progress(self):
        for child in self.progress_frame.winfo_children():
            child.destroy()
        self.progress_buttons.clear()
        if not self.session:
            return
        for index, _card in enumerate(self.session.cards):
            state = self.session.state_for_card(index)
            border = "#ffffff" if index == self.session.current_card_index and not self.session.finished else "#222931"
            square = tk.Label(self.progress_frame, text=str(index + 1), width=3,
                              bg=self.colors[state], fg="#101820",
                              relief="flat", highlightthickness=2, highlightbackground=border,
                              font=("Segoe UI", 10, "bold"), cursor="hand2")
            square.bind("<Button-1>", lambda _event, i=index: self.select_card(i))
            square.pack(side="left", padx=(0, 7), ipadx=3, ipady=6)
            self.progress_buttons.append(square)

    def _rebuild_card(self):
        for child in self.card_scroll.content.winfo_children():
            child.destroy()
        self.answer_vars.clear()
        if not self.session:
            return
        if self.session.finished:
            self._build_result_card()
            return

        card = self.session.current_card
        tk.Label(self.card_scroll.content,
                 text=f"Карточка {self.session.current_card_index + 1} из {len(self.session.cards)}",
                 bg=self.colors["panel"], fg=self.colors["muted"], font=("Segoe UI", 14),
                 anchor="w").pack(fill="x", padx=18, pady=(18, 10))
        self._add_blocks(card.get("description", []))
        tk.Frame(self.card_scroll.content, bg=self.colors["border"], height=1).pack(fill="x", padx=18, pady=12)
        self._add_blocks(card.get("question", []))

        card_type = card.get("type", "single")
        if card_type == "text":
            value = tk.StringVar(value=self.session.answers[self.session.current_card_index] or "")
            value.trace_add("write", lambda *_args, var=value: self._set_text_answer(var))
            entry = tk.Entry(self.card_scroll.content, textvariable=value, bg="#242b34", fg=self.colors["text"],
                             insertbackground=self.colors["text"], relief="flat", font=("Segoe UI", 13))
            entry.pack(fill="x", padx=18, pady=(12, 18), ipady=9)
        else:
            for option in card.get("options", []):
                self._add_option(option, card_type)

    def _add_blocks(self, blocks):
        for block in blocks:
            if block.get("type") == "code":
                self._add_code_block(block.get("text", ""), block.get("language", "cpp"))
            else:
                tk.Label(self.card_scroll.content, text=block.get("text", ""), bg=self.colors["panel"],
                         fg=self.colors["text"], font=("Segoe UI", 13), anchor="w", justify="left",
                         wraplength=760).pack(fill="x", padx=18, pady=3)

    def _add_code_block(self, code, language):
        text = tk.Text(self.card_scroll.content, height=max(2, min(12, code.count("\n") + 1)),
                       bg=SyntaxColors.CODE_BG, fg=SyntaxColors.TEXT, relief="flat", padx=12, pady=8,
                       font=("Consolas", 11), wrap="none")
        text.pack(fill="x", padx=18, pady=6)
        text.insert("1.0", code)
        text.configure(state="disabled")
        self._highlight_text_widget(text, code, language)

    def _highlight_text_widget(self, text_widget, code, language):
        text_widget.tag_configure("keyword", foreground=SyntaxColors.KEYWORD, font=("Consolas", 11, "bold"))
        text_widget.tag_configure("comment", foreground=SyntaxColors.COMMENT)
        text_widget.tag_configure("string", foreground=SyntaxColors.STRING)
        keywords = PYTHON_KEYWORDS if language.lower() == "python" else CPP_KEYWORDS

        def add_tag(start, end, tag):
            text_widget.configure(state="normal")
            text_widget.tag_add(tag, f"1.0+{start}c", f"1.0+{end}c")
            text_widget.configure(state="disabled")

        index = 0
        while index < len(code):
            char = code[index]
            if char in ('"', "'"):
                quote = char
                end = index + 1
                while end < len(code):
                    if code[end] == quote and code[end - 1] != "\\":
                        end += 1
                        break
                    end += 1
                add_tag(index, end, "string")
                index = end
                continue
            if code.startswith("//", index) or code[index] == "#":
                end = code.find("\n", index)
                end = len(code) if end == -1 else end
                add_tag(index, end, "comment")
                index = end
                continue
            if char.isalpha() or char == "_":
                end = index + 1
                while end < len(code) and (code[end].isalnum() or code[end] == "_"):
                    end += 1
                if code[index:end] in keywords:
                    add_tag(index, end, "keyword")
                index = end
                continue
            index += 1

    def _add_option(self, option, card_type):
        frame = tk.Frame(self.card_scroll.content, bg=self.colors["panel_soft"],
                         highlightthickness=1, highlightbackground=self.colors["border"], padx=10, pady=8)
        frame.pack(fill="x", padx=18, pady=6)
        option_id = option.get("id")
        selected = self._option_selected(option_id)

        if card_type == "multi":
            var = tk.BooleanVar(value=selected)
            control = tk.Checkbutton(frame, variable=var, bg=self.colors["panel_soft"], fg=self.colors["text"],
                                     activebackground=self.colors["panel_soft"], selectcolor=self.colors["panel_soft"],
                                     command=lambda oid=option_id, v=var: self._toggle_multi_answer(oid, v))
        else:
            if "single" not in self.answer_vars:
                self.answer_vars["single"] = tk.StringVar(value=self.session.answers[self.session.current_card_index] or "")
            control = tk.Radiobutton(frame, variable=self.answer_vars["single"], value=option_id,
                                     bg=self.colors["panel_soft"], fg=self.colors["text"],
                                     activebackground=self.colors["panel_soft"], selectcolor=self.colors["panel_soft"],
                                     command=lambda oid=option_id: self._set_single_answer(oid))
        control.pack(side="left", padx=(0, 10))

        content = tk.Frame(frame, bg=self.colors["panel_soft"])
        content.pack(side="left", fill="x", expand=True)
        for block in option.get("blocks", []):
            if block.get("type") == "code":
                label = tk.Label(content, text=block.get("text", ""), bg=self.colors["panel_soft"],
                                 fg=self.colors["text"], font=("Consolas", 11), anchor="w", justify="left")
            else:
                label = tk.Label(content, text=block.get("text", ""), bg=self.colors["panel_soft"],
                                 fg=self.colors["text"], font=("Segoe UI", 13), anchor="w", justify="left",
                                 wraplength=700)
            label.pack(fill="x")

    def _option_selected(self, option_id):
        answer = self.session.answers[self.session.current_card_index]
        if isinstance(answer, list):
            return option_id in answer
        return answer == option_id

    def _build_result_card(self):
        stats = self.session.stats()
        outer = tk.Frame(self.card_scroll.content, bg=self.colors["panel"], padx=18, pady=18)
        outer.pack(fill="both", expand=True)
        tk.Label(outer, text="Статистика прохождения", bg=self.colors["panel"], fg=self.colors["text"],
                 font=("Segoe UI", 26, "bold"), anchor="w").pack(fill="x")
        rows = [
            ("Правильных ответов", f"{stats['correct']} из {stats['total']}", self.colors["correct"]),
            ("Дано ответов", f"{stats['answered']} из {stats['total']}", self.colors["text"]),
            ("Время прохождения", f"{stats['elapsedSeconds'] // 60} мин {stats['elapsedSeconds'] % 60} сек", self.colors["text"]),
        ]
        for label, value, color in rows:
            row = tk.Frame(outer, bg=self.colors["panel"])
            row.pack(fill="x", pady=(16, 0))
            tk.Label(row, text=label, bg=self.colors["panel"], fg=self.colors["muted"],
                     font=("Segoe UI", 13), width=22, anchor="w").pack(side="left")
            tk.Label(row, text=value, bg=self.colors["panel"], fg=color,
                     font=("Segoe UI", 18, "bold"), anchor="w").pack(side="left")
        self._button(outer, "Вернуться к карточкам",
                     lambda: self.select_card(max(0, self.session.current_card_index))).pack(anchor="e", pady=(28, 0))

    def _log_current_card(self, event):
        if not self.session or not self.session.cards:
            return
        index = self.session.current_card_index
        if event == "leave" and not self.session.dirty[index]:
            return
        self._append_log({
            "event": event,
            "testId": self.session.test.get("id"),
            "cardId": self.session.cards[index].get("id"),
            "answer": self.session.normalized_answer(index),
            "timestamp": self._timestamp(),
        })
        self.session.dirty[index] = False

    def _append_log(self, record):
        self.logs_dir.mkdir(parents=True, exist_ok=True)
        path = self.logs_dir / "attempts_tkinter.json"
        records = []
        if path.exists():
            try:
                records = json.loads(path.read_text(encoding="utf-8"))
            except json.JSONDecodeError:
                records = []
        records.append(record)
        path.write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding="utf-8")

    @staticmethod
    def _timestamp():
        return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


if __name__ == "__main__":
    enable_windows_dpi_awareness()
    app = QtTrenTkApp()
    app.mainloop()

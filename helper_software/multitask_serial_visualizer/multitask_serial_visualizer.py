"""
Serial Monitor — Single-Window Task Router
Reads one serial port; routes lines to three panels based on label prefix.

Expected serial format (configurable):
    TASK1: sensor reading here
    TASK2: another value
    TASK3: third task output

Requires: pip install pyserial
"""

import tkinter as tk
from tkinter import ttk
import threading
import queue
import time
import random
import re
import serial
import serial.tools.list_ports


# ─────────────────────── Palette ───────────────────────
BG      = "#0d1117"
SURFACE = "#161b22"
BORDER  = "#30363d"
ACCENT  = ["#58a6ff", "#3fb950", "#f78166"]
DIM     = "#8b949e"
TEXT    = "#e6edf3"
MONO    = ("Courier New", 10)
UI      = ("Segoe UI", 9)
BOLD    = ("Segoe UI", 10, "bold")
BAUD_RATES = [9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600]


# ─────────────────────── Serial Worker ─────────────────────────
class SerialWorker(threading.Thread):
    def __init__(self, port, baud, task_queues, prefixes):
        super().__init__(daemon=True)
        self.port        = port
        self.baud        = baud
        self.task_queues = task_queues
        self.prefixes    = prefixes
        self._stop_evt   = threading.Event()
        self._pause_evt  = threading.Event()
        self._ser        = None
        self._build_pattern()

    def _build_pattern(self):
        escaped = [re.escape(p) for p in self.prefixes]
        self._re = re.compile(
            r"^\s*(" + "|".join(escaped) + r")\s*[:\|>\-]?\s*(.*)", re.IGNORECASE
        )

    def _dispatch(self, raw):
        m = self._re.match(raw)
        if m:
            matched = m.group(1).upper()
            body    = m.group(2)
            for i, p in enumerate(self.prefixes):
                if p.upper() == matched:
                    self.task_queues[i].put(("data", body + "\n"))
                    return
        for q in self.task_queues:
            q.put(("unmatched", raw + "\n"))

    def run(self):
        try:
            self._ser = serial.Serial(self.port, self.baud, timeout=0.5)
            for q in self.task_queues:
                q.put(("info", f"[connected] {self.port} @ {self.baud} baud\n"))
        except Exception as e:
            for q in self.task_queues:
                q.put(("error", f"[error] Cannot open {self.port}: {e}\n"))
            return

        while not self._stop_evt.is_set():
            if self._pause_evt.is_set():
                time.sleep(0.05)
                continue
            try:
                raw = self._ser.readline()
                if raw:
                    self._dispatch(raw.decode("utf-8", errors="replace").rstrip("\r\n"))
            except serial.SerialException as e:
                for q in self.task_queues:
                    q.put(("error", f"[error] {e}\n"))
                break

        if self._ser and self._ser.is_open:
            self._ser.close()
        for q in self.task_queues:
            q.put(("info", "[disconnected]\n"))

    def send(self, text):
        if self._ser and self._ser.is_open:
            self._ser.write((text + "\n").encode())

    def stop(self):   self._stop_evt.set()
    def pause(self):  self._pause_evt.set()
    def resume(self): self._pause_evt.clear()


# ─────────────────────── Demo Worker ───────────────────────────
class DemoWorker(threading.Thread):
    TEMPLATES = [
        "sensor x={x:.3f} y={y:.3f}",
        "uptime={t}s  status=ok",
        "temp={c:.1f}C  fan=on",
        "seq={s} len=64 crc=OK",
        "gpio pin3=HIGH pin7=LOW",
        "loop dt={dt}ms",
        "voltage={v:.2f}V",
        "packet dropped — retrying",
    ]

    def __init__(self, task_queues, prefixes):
        super().__init__(daemon=True)
        self.task_queues = task_queues
        self.prefixes    = prefixes
        self._stop_evt   = threading.Event()
        self._pause_evt  = threading.Event()
        self._seq        = 0
        self._t0         = time.time()

    def run(self):
        for i, q in enumerate(self.task_queues):
            q.put(("info", f"[demo] Routing \"{self.prefixes[i]}:\" lines to this panel\n"))

        while not self._stop_evt.is_set():
            if self._pause_evt.is_set():
                time.sleep(0.05)
                continue
            idx  = random.randint(0, 2)
            t    = int(time.time() - self._t0)
            body = random.choice(self.TEMPLATES).format(
                x=random.gauss(0, 1), y=random.gauss(0, 1),
                t=t, c=55 + random.uniform(-3, 3),
                s=self._seq, dt=random.randint(8, 18),
                v=3.2 + random.uniform(-0.1, 0.1),
            )
            self.task_queues[idx].put(("data", body + "\n"))
            self._seq += 1
            time.sleep(random.uniform(0.08, 0.4))

    def send(self, text): pass
    def stop(self):   self._stop_evt.set()
    def pause(self):  self._pause_evt.set()
    def resume(self): self._pause_evt.clear()


# ─────────────────────── Task Panel ────────────────────────────
class TaskPanel(tk.Frame):
    """One of the three output panels embedded in the main window."""

    def __init__(self, master, task_id, prefix_var, **kw):
        super().__init__(master, bg=BG, **kw)
        self.task_id     = task_id
        self.color       = ACCENT[task_id - 1]
        self.prefix_var  = prefix_var
        self._q          = queue.Queue()
        self._line_count = 0
        self._autoscroll = tk.BooleanVar(value=True)
        self._timestamps = tk.BooleanVar(value=False)
        self._status_var = tk.StringVar(value="IDLE")
        self._count_var  = tk.StringVar(value="0 lines")
        self._build_ui()
        self._poll()

    def _build_ui(self):
        # ── panel header ──
        bar = tk.Frame(self, bg=SURFACE, height=38)
        bar.pack(fill="x")
        bar.pack_propagate(False)

        dot = tk.Canvas(bar, width=9, height=9, bg=SURFACE, highlightthickness=0)
        dot.pack(side="left", padx=(10, 6), pady=14)
        dot.create_oval(1, 1, 8, 8, fill=self.color, outline="")

        tk.Label(bar, text=f"Task {self.task_id}", bg=SURFACE, fg=TEXT,
                 font=("Segoe UI", 10, "bold")).pack(side="left")

        tk.Label(bar, text="▸", bg=SURFACE, fg=DIM, font=UI).pack(side="left", padx=(8, 2))
        tk.Label(bar, textvariable=self.prefix_var, bg=SURFACE,
                 fg=self.color, font=("Courier New", 9, "bold")).pack(side="left")

        tk.Label(bar, textvariable=self._status_var, bg=SURFACE,
                 fg=DIM, font=UI).pack(side="left", padx=10)

        tk.Label(bar, textvariable=self._count_var, bg=SURFACE,
                 fg=DIM, font=UI).pack(side="right", padx=10)

        # ── mini toolbar ──
        tb = tk.Frame(self, bg=BG, pady=3)
        tb.pack(fill="x", padx=8)

        def chk(text, var):
            return tk.Checkbutton(tb, text=text, variable=var, bg=BG, fg=DIM,
                                  selectcolor=SURFACE, activebackground=BG,
                                  font=("Segoe UI", 8), relief="flat")
        chk("Autoscroll", self._autoscroll).pack(side="left")
        chk("Timestamp",  self._timestamps).pack(side="left", padx=(8, 0))
        tk.Button(tb, text="Clear", command=self.clear, bg=SURFACE, fg=DIM,
                  font=("Segoe UI", 8), relief="flat", padx=6,
                  cursor="hand2").pack(side="right")

        # ── divider ──
        tk.Frame(self, bg=BORDER, height=1).pack(fill="x")

        # ── text area ──
        frm = tk.Frame(self, bg=BG)
        frm.pack(fill="both", expand=True)

        self._txt = tk.Text(
            frm, bg=BG, fg=TEXT, font=MONO, wrap="word",
            relief="flat", bd=0, highlightthickness=0,
            selectbackground=BORDER, selectforeground=TEXT,
            insertbackground=self.color, padx=8, pady=6,
        )
        sb = ttk.Scrollbar(frm, command=self._txt.yview)
        self._txt.configure(yscrollcommand=sb.set)
        self._txt.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")

        self._txt.tag_config("data",      foreground=TEXT)
        self._txt.tag_config("info",      foreground=self.color, font=(*MONO[:2], "bold"))
        self._txt.tag_config("error",     foreground=ACCENT[2])
        self._txt.tag_config("unmatched", foreground=BORDER)
        self._txt.tag_config("ts",        foreground=DIM)
        self._txt.config(state="disabled")

    def clear(self):
        self._txt.config(state="normal")
        self._txt.delete("1.0", "end")
        self._txt.config(state="disabled")
        self._line_count = 0
        self._count_var.set("0 lines")

    def set_status(self, s):
        self._status_var.set(s)

    def append(self, text, tag="data"):
        self._txt.config(state="normal")
        if self._timestamps.get() and tag == "data":
            self._txt.insert("end", f"[{time.strftime('%H:%M:%S')}] ", "ts")
        self._txt.insert("end", text, tag)
        self._line_count += text.count("\n")
        self._count_var.set(f"{self._line_count:,} lines")
        if self._autoscroll.get():
            self._txt.see("end")
        self._txt.config(state="disabled")

    def _poll(self):
        try:
            while True:
                kind, text = self._q.get_nowait()
                self.append(text, kind)
        except queue.Empty:
            pass
        self.after(40, self._poll)


# ─────────────────────── Main Window ───────────────────────────
class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Serial Monitor")
        self.configure(bg=BG)
        self.geometry("1200x780")
        self.minsize(800, 500)

        self._prefix_vars = [
            tk.StringVar(value="SD TASK:"),
            tk.StringVar(value="BLE TASK:"),
            tk.StringVar(value="SAMPLING TASK:"),
        ]
        self._worker  = None
        self._running = False
        self._paused  = False

        self._build_ui()

    def _build_ui(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TCombobox", fieldbackground=SURFACE, background=SURFACE,
                        foreground=TEXT, selectforeground=TEXT,
                        selectbackground=BORDER, arrowcolor=DIM, bordercolor=BORDER)
        style.configure("TScrollbar", background=SURFACE, troughcolor=BG,
                        bordercolor=BG, arrowcolor=DIM)
        style.configure("TSash", sashrelief="flat", sashthickness=6,
                        background=BORDER)

        # ══ top control bar ══════════════════════════════════════
        top = tk.Frame(self, bg=SURFACE, height=52)
        top.pack(fill="x", side="top")
        top.pack_propagate(False)

        # # logo
        # tk.Label(top, text="⬡", bg=SURFACE, fg=ACCENT[0],
        #          font=("Courier New", 16)).pack(side="left", padx=(14, 4), pady=12)
        # tk.Label(top, text="SERIAL MONITOR", bg=SURFACE, fg=TEXT,
        #          font=("Courier New", 11, "bold")).pack(side="left", padx=(0, 20))

        # tk.Frame(top, bg=BORDER, width=1).pack(side="left", fill="y", pady=10)

        # port
        tk.Label(top, text="PORT", bg=SURFACE, fg=DIM,
                 font=("Segoe UI", 8)).pack(side="left", padx=(16, 4))
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self._port_var = tk.StringVar(value=ports[0] if ports else "DEMO")
        self._port_cb = ttk.Combobox(top, textvariable=self._port_var,
                                     values=ports + ["DEMO"], width=13,
                                     font=UI, state="readonly")
        self._port_cb.pack(side="left", padx=(0, 4))
        tk.Button(top, text="↺", command=self._refresh_ports, bg=SURFACE,
                  fg=DIM, font=UI, relief="flat", cursor="hand2",
                  bd=0, padx=2).pack(side="left", padx=(0, 12))

        # baud
        tk.Label(top, text="BAUD", bg=SURFACE, fg=DIM,
                 font=("Segoe UI", 8)).pack(side="left", padx=(0, 4))
        self._baud_var = tk.IntVar(value=115200)
        ttk.Combobox(top, textvariable=self._baud_var, values=BAUD_RATES,
                     width=9, font=UI, state="readonly").pack(side="left", padx=(0, 16))

        tk.Frame(top, bg=BORDER, width=1).pack(side="left", fill="y", pady=10)

        # prefixes
        tk.Label(top, text="PREFIXES", bg=SURFACE, fg=DIM,
                 font=("Segoe UI", 8)).pack(side="left", padx=(16, 8))
        for i, (var, color) in enumerate(zip(self._prefix_vars, ACCENT)):
            dot = tk.Canvas(top, width=7, height=7, bg=SURFACE, highlightthickness=0)
            dot.pack(side="left", padx=(0, 3))
            dot.create_oval(1, 1, 6, 6, fill=color, outline="")
            tk.Entry(top, textvariable=var, bg=BG, fg=color,
                     insertbackground=color, font=("Courier New", 9, "bold"),
                     relief="flat", highlightthickness=1,
                     highlightbackground=BORDER, highlightcolor=color,
                     width=8, bd=3).pack(side="left", padx=(0, 10))

        tk.Frame(top, bg=BORDER, width=1).pack(side="left", fill="y", pady=10)

        # # send
        # self._send_var = tk.StringVar()
        # send_e = tk.Entry(top, textvariable=self._send_var, bg=BG, fg=TEXT,
        #                   insertbackground=ACCENT[0], font=MONO, relief="flat",
        #                   highlightthickness=1, highlightbackground=BORDER,
        #                   highlightcolor=ACCENT[0], width=20, bd=3)
        # send_e.pack(side="left", padx=(16, 4))
        # send_e.bind("<Return>", self._send)
        # tk.Button(top, text="Send ↵", command=self._send, bg=ACCENT[0], fg=BG,
        #           font=BOLD, relief="flat", padx=8, cursor="hand2",
        #           bd=0).pack(side="left", padx=(0, 16))

        # tk.Frame(top, bg=BORDER, width=1).pack(side="left", fill="y", pady=10)

        right_frame = tk.Frame(top, bg=SURFACE)
        right_frame.pack(side="right", padx=10)

        # action buttons — right side
        tk.Button(top, text="⊘ Clear All", command=self._clear_all,
                  bg=SURFACE, fg=DIM, font=BOLD, relief="flat",
                  padx=10, cursor="hand2", bd=0).pack(side="right", padx=(0, 14))

        tk.Button(top, text="✕ Disconnect", command=self._disconnect,
                  bg=SURFACE, fg=DIM, font=BOLD, relief="flat",
                  padx=10, cursor="hand2", bd=0).pack(side="right", padx=4)

        self._btn_pause = tk.Button(top, text="⏸ Pause", command=self._toggle_pause,
                                    bg=SURFACE, fg=DIM, font=BOLD, relief="flat",
                                    padx=10, cursor="hand2", bd=0, state="disabled")
        self._btn_pause.pack(side="right", padx=4)

        self._btn_connect = tk.Button(top, text="▶ Connect", command=self._connect,
                                      bg=ACCENT[1], fg=BG, font=BOLD, relief="flat",
                                      padx=10, cursor="hand2", bd=0)
        self._btn_connect.pack(side="right", padx=4)


        # status dot
        self._conn_status = tk.StringVar(value="● Disconnected")
        tk.Label(top, textvariable=self._conn_status, bg=SURFACE,
                 fg=DIM, font=("Courier New", 8)).pack(side="right", padx=12)

        # ══ thin separator ══
        tk.Frame(self, bg=BORDER, height=1).pack(fill="x")

        # ══ three panels in a horizontal PanedWindow ═════════════
        pane = tk.PanedWindow(self, orient="horizontal", bg=BORDER,
                              sashwidth=5, sashrelief="flat",
                              handlesize=0, showhandle=False)
        pane.pack(fill="both", expand=True)

        self._panels = []
        for i in range(3):
            panel = TaskPanel(pane, task_id=i + 1,
                              prefix_var=self._prefix_vars[i])
            pane.add(panel, stretch="always", minsize=200)
            self._panels.append(panel)

        # ══ status bar ══════════════════════════════════════════
        sb = tk.Frame(self, bg=SURFACE, height=22)
        sb.pack(fill="x", side="bottom")
        sb.pack_propagate(False)
        hint = 'Unmatched lines (no prefix) appear dimmed in all panels. ' \
               'Set port to DEMO for simulated data.'
        tk.Label(sb, text=hint, bg=SURFACE, fg=DIM,
                 font=("Segoe UI", 8)).pack(side="left", padx=10)

    # ── actions ──────────────────────────────────────────────────
    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()] + ["DEMO"]
        self._port_cb["values"] = ports
        if self._port_var.get() not in ports:
            self._port_var.set(ports[-1])

    def _connect(self):
        if self._running:
            return
        port     = self._port_var.get()
        baud     = self._baud_var.get()
        prefixes = [v.get().strip() for v in self._prefix_vars]
        queues   = [p._q for p in self._panels]

        self._worker = DemoWorker(queues, prefixes) if port == "DEMO" \
                       else SerialWorker(port, baud, queues, prefixes)
        self._running = True
        self._paused  = False
        self._worker.start()

        self._btn_connect.config(state="disabled")
        self._btn_pause.config(state="normal", text="⏸ Pause", fg=TEXT)
        self._port_cb.config(state="disabled")
        self._conn_status.set(f"● {port}  {baud} baud")
        for p in self._panels:
            p.set_status("RUNNING")

    def _disconnect(self):
        if self._worker:
            self._worker.stop()
            self._worker = None
        self._running = False
        self._paused  = False
        self._btn_connect.config(state="normal")
        self._btn_pause.config(state="disabled", text="⏸ Pause", fg=DIM)
        self._port_cb.config(state="readonly")
        self._conn_status.set("● Disconnected")
        for p in self._panels:
            p.set_status("IDLE")

    def _toggle_pause(self):
        if not self._worker:
            return
        if self._paused:
            self._worker.resume()
            self._paused = False
            self._btn_pause.config(text="⏸ Pause")
            self._conn_status.set("● Running")
            for p in self._panels:
                p.set_status("RUNNING")
        else:
            self._worker.pause()
            self._paused = True
            self._btn_pause.config(text="▶ Resume")
            self._conn_status.set("● Paused")
            for p in self._panels:
                p.set_status("PAUSED")

    def _clear_all(self):
        for p in self._panels:
            p.clear()

    def _send(self, _event=None):
        msg = self._send_var.get().strip()
        if not msg:
            return
        if self._worker:
            self._worker.send(msg)
        self._send_var.set("")


if __name__ == "__main__":
    app = App()
    app.mainloop()
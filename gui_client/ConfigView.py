import tkinter as tk
from tkinter import ttk, messagebox

MAX_QUEUE_SIZE = 100


class ConfigView:
    def __init__(self, parent, callback_iniciar):
        self.parent = parent
        self.callback_iniciar = callback_iniciar

        # ======================================================
        # CONTENEDOR PRINCIPAL SCROLLEABLE
        # ======================================================

        self.container = tk.Frame(parent)
        self.container.pack(fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(
            self.container,
            highlightthickness=0
        )

        self.scrollbar = tk.Scrollbar(
            self.container,
            orient=tk.VERTICAL,
            command=self.canvas.yview
        )

        self.content_frame = tk.Frame(self.canvas)

        self.content_window = self.canvas.create_window(
            (0, 0),
            window=self.content_frame,
            anchor="nw"
        )

        self.canvas.configure(
            yscrollcommand=self.scrollbar.set
        )

        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        # Actualiza el área scrolleable cuando cambia el contenido
        self.content_frame.bind(
            "<Configure>",
            self._on_frame_configure
        )

        # Hace que el frame interno use el ancho completo del canvas
        self.canvas.bind(
            "<Configure>",
            self._on_canvas_configure
        )

        # Scroll con mouse wheel en Windows/macOS y Linux
        self.canvas.bind_all("<MouseWheel>", self._on_mousewheel)
        self.canvas.bind_all("<Button-4>", self._on_mousewheel)
        self.canvas.bind_all("<Button-5>", self._on_mousewheel)

        # ======================================================
        # TÍTULO
        # ======================================================

        title = tk.Label(
            self.content_frame,
            text="Configuración del Canal",
            font=("Arial", 16, "bold"),
            fg="navy"
        )
        title.pack(pady=20)

        # ======================================================
        # FRAME DE CONFIGURACIÓN GENERAL
        # ======================================================

        frame_general = tk.Frame(
            self.content_frame,
            bg="lightblue",
            padx=20,
            pady=10
        )
        frame_general.pack(pady=10, fill=tk.X, padx=20)

        # Largo del canal
        tk.Label(
            frame_general,
            text="Largo del Canal (unidades):",
            bg="lightblue",
            font=("Arial", 10, "bold")
        ).grid(row=0, column=0, sticky=tk.W, pady=5)

        self.entry_largo = tk.Entry(frame_general, width=10)
        self.entry_largo.insert(0, "6")
        self.entry_largo.grid(row=0, column=1, pady=5)

        # Tamaño interno de cola
        tk.Label(
            frame_general,
            text=f"Tamaño máximo interno de cola (1-{MAX_QUEUE_SIZE}):",
            bg="lightblue",
            font=("Arial", 10, "bold")
        ).grid(row=1, column=0, sticky=tk.W, pady=5)

        self.entry_queue_size = tk.Entry(frame_general, width=10)
        self.entry_queue_size.insert(0, "4")
        self.entry_queue_size.grid(row=1, column=1, pady=5)

        # Barcos visibles por cola
        tk.Label(
            frame_general,
            text="Barcos visibles por cola (1-4):",
            bg="lightblue",
            font=("Arial", 10, "bold")
        ).grid(row=2, column=0, sticky=tk.W, pady=5)

        self.entry_visible_queue = tk.Entry(frame_general, width=10)
        self.entry_visible_queue.insert(0, "4")
        self.entry_visible_queue.grid(row=2, column=1, pady=5)

        # Scheduler
        tk.Label(
            frame_general,
            text="Algoritmo de Scheduling:",
            bg="lightblue",
            font=("Arial", 10, "bold")
        ).grid(row=3, column=0, sticky=tk.W, pady=5)

        self.combo_algo = ttk.Combobox(
            frame_general,
            values=["FCFS", "SJF", "Round Robin", "Priority", "STRN", "EDF"],
            width=15,
            state="readonly"
        )
        self.combo_algo.current(0)
        self.combo_algo.grid(row=3, column=1, pady=5)
        self.combo_algo.bind("<<ComboboxSelected>>", self._update_fields)

        # Flujo
        tk.Label(
            frame_general,
            text="Control de Flujo:",
            bg="lightblue",
            font=("Arial", 10, "bold")
        ).grid(row=4, column=0, sticky=tk.W, pady=5)

        self.combo_flujo = ttk.Combobox(
            frame_general,
            values=["Equidad", "Letrero", "Tico"],
            width=15,
            state="readonly"
        )
        self.combo_flujo.current(1)
        self.combo_flujo.grid(row=4, column=1, pady=5)
        self.combo_flujo.bind("<<ComboboxSelected>>", self._update_fields)

        # Modo de generación
        tk.Label(
            frame_general,
            text="Modo de Generación:",
            bg="lightblue",
            font=("Arial", 10, "bold")
        ).grid(row=5, column=0, sticky=tk.W, pady=5)

        self.combo_modo = ttk.Combobox(
            frame_general,
            values=["Fijo", "Dinamico"],
            width=15,
            state="readonly"
        )
        self.combo_modo.current(0)
        self.combo_modo.grid(row=5, column=1, pady=5)

        # Fairness W
        tk.Label(
            frame_general,
            text="Fairness W:",
            bg="lightblue",
            font=("Arial", 10, "bold")
        ).grid(row=6, column=0, sticky=tk.W, pady=5)

        self.entry_fairness_w = tk.Entry(frame_general, width=10)
        self.entry_fairness_w.insert(0, "1")
        self.entry_fairness_w.grid(row=6, column=1, pady=5)

        # Sign Interval
        tk.Label(
            frame_general,
            text="Sign Interval (ms):",
            bg="lightblue",
            font=("Arial", 10, "bold")
        ).grid(row=7, column=0, sticky=tk.W, pady=5)

        self.entry_sign_interval = tk.Entry(frame_general, width=10)
        self.entry_sign_interval.insert(0, "1000")
        self.entry_sign_interval.grid(row=7, column=1, pady=5)

        # RR Quantum
        tk.Label(
            frame_general,
            text="RR Quantum:",
            bg="lightblue",
            font=("Arial", 10, "bold")
        ).grid(row=8, column=0, sticky=tk.W, pady=5)

        self.entry_rr_quantum = tk.Entry(frame_general, width=10)
        self.entry_rr_quantum.insert(0, "10")
        self.entry_rr_quantum.grid(row=8, column=1, pady=5)

        # ======================================================
        # FRAME DE BARCOS INICIALES
        # ======================================================

        frame_barcos = tk.Frame(
            self.content_frame,
            bg="lightgreen",
            padx=20,
            pady=10
        )
        frame_barcos.pack(pady=10, fill=tk.X, padx=20)

        tk.Label(
            frame_barcos,
            text="Barcos Iniciales",
            font=("Arial", 12, "bold"),
            bg="lightgreen",
            fg="darkgreen"
        ).pack(pady=10)

        subframe = tk.Frame(frame_barcos, bg="lightgreen")
        subframe.pack()

        # ======================================================
        # COLA IZQUIERDA
        # ======================================================

        frame_izq = tk.Frame(
            subframe,
            bg="lightyellow",
            padx=10,
            pady=10
        )
        frame_izq.pack(side=tk.LEFT, padx=20)

        tk.Label(
            frame_izq,
            text="Cola Izquierda",
            font=("Arial", 11, "bold"),
            bg="lightyellow",
            fg="orange"
        ).pack(pady=5)

        tk.Label(frame_izq, text="Normal:", bg="lightyellow").pack(anchor=tk.W)
        self.entry_left_normal = tk.Entry(frame_izq, width=5)
        self.entry_left_normal.insert(0, "0")
        self.entry_left_normal.pack()

        tk.Label(frame_izq, text="Pesquera:", bg="lightyellow").pack(anchor=tk.W)
        self.entry_left_pesquera = tk.Entry(frame_izq, width=5)
        self.entry_left_pesquera.insert(0, "0")
        self.entry_left_pesquera.pack()

        tk.Label(frame_izq, text="Patrulla:", bg="lightyellow").pack(anchor=tk.W)
        self.entry_left_patrulla = tk.Entry(frame_izq, width=5)
        self.entry_left_patrulla.insert(0, "0")
        self.entry_left_patrulla.pack()

        # ======================================================
        # COLA DERECHA
        # ======================================================

        frame_der = tk.Frame(
            subframe,
            bg="lightcyan",
            padx=10,
            pady=10
        )
        frame_der.pack(side=tk.LEFT, padx=20)

        tk.Label(
            frame_der,
            text="Cola Derecha",
            font=("Arial", 11, "bold"),
            bg="lightcyan",
            fg="blue"
        ).pack(pady=5)

        tk.Label(frame_der, text="Normal:", bg="lightcyan").pack(anchor=tk.W)
        self.entry_right_normal = tk.Entry(frame_der, width=5)
        self.entry_right_normal.insert(0, "0")
        self.entry_right_normal.pack()

        tk.Label(frame_der, text="Pesquera:", bg="lightcyan").pack(anchor=tk.W)
        self.entry_right_pesquera = tk.Entry(frame_der, width=5)
        self.entry_right_pesquera.insert(0, "0")
        self.entry_right_pesquera.pack()

        tk.Label(frame_der, text="Patrulla:", bg="lightcyan").pack(anchor=tk.W)
        self.entry_right_patrulla = tk.Entry(frame_der, width=5)
        self.entry_right_patrulla.insert(0, "0")
        self.entry_right_patrulla.pack()

        # ======================================================
        # BOTÓN FINAL
        # ======================================================

        button = tk.Button(
            self.content_frame,
            text="Establecer Configuración",
            command=self.enviar_datos,
            bg="green",
            fg="white",
            font=("Arial", 12, "bold"),
            padx=20,
            pady=10
        )
        button.pack(pady=20)

        # Inicializar estados visuales de campos
        self._update_fields()

    # ======================================================
    # MÉTODOS DE SCROLL
    # ======================================================

    def _on_frame_configure(self, event=None):
        self.canvas.configure(
            scrollregion=self.canvas.bbox("all")
        )

    def _on_canvas_configure(self, event):
        self.canvas.itemconfig(
            self.content_window,
            width=event.width
        )

    def _on_mousewheel(self, event):
        # Linux: rueda hacia arriba
        if getattr(event, "num", None) == 4:
            self.canvas.yview_scroll(-1, "units")
            return

        # Linux: rueda hacia abajo
        if getattr(event, "num", None) == 5:
            self.canvas.yview_scroll(1, "units")
            return

        # Windows/macOS
        if getattr(event, "delta", 0) != 0:
            self.canvas.yview_scroll(
                int(-1 * (event.delta / 120)),
                "units"
            )

    # ======================================================
    # HABILITAR / DESHABILITAR CAMPOS SEGÚN CONFIGURACIÓN
    # ======================================================

    def _update_fields(self, event=None):
        scheduler = self.combo_algo.get()
        flow = self.combo_flujo.get()

        fields = [
            self.entry_fairness_w,
            self.entry_sign_interval,
            self.entry_rr_quantum,
        ]

        for field in fields:
            field.config(state="normal", bg="white")

        if scheduler != "Round Robin":
            self._disable_entry(self.entry_rr_quantum)

        if flow != "Letrero":
            self._disable_entry(self.entry_sign_interval)

        if flow != "Equidad":
            self._disable_entry(self.entry_fairness_w)

    def _disable_entry(self, entry):
        entry.config(
            state="disabled",
            disabledbackground="#e0e0e0"
        )

    # ======================================================
    # ENVÍO DE CONFIGURACIÓN
    # ======================================================

    def enviar_datos(self):
        scheduler = self.combo_algo.get()
        flow = self.combo_flujo.get()

        try:
            canal_length = int(self.entry_largo.get())
            queue_size = int(self.entry_queue_size.get())
            visible_queue = int(self.entry_visible_queue.get())

            left_normal = int(self.entry_left_normal.get())
            left_pesquera = int(self.entry_left_pesquera.get())
            left_patrulla = int(self.entry_left_patrulla.get())

            right_normal = int(self.entry_right_normal.get())
            right_pesquera = int(self.entry_right_pesquera.get())
            right_patrulla = int(self.entry_right_patrulla.get())

            if canal_length <= 0:
                messagebox.showerror(
                    "Error de Validación",
                    "El largo del canal debe ser mayor a 0."
                )
                return

            if queue_size <= 0 or queue_size > MAX_QUEUE_SIZE:
                messagebox.showerror(
                    "Error de Validación",
                    f"El tamaño de la cola debe ser entre 1 y {MAX_QUEUE_SIZE}."
                )
                return

            if visible_queue <= 0 or visible_queue > 4:
                messagebox.showerror(
                    "Error de Validación",
                    "La cantidad de barcos visibles por cola debe estar entre 1 y 4."
                )
                return

            if visible_queue > queue_size:
                messagebox.showerror(
                    "Error de Validación",
                    "La cantidad visible no puede ser mayor que el tamaño interno de cola."
                )
                return

            left_total = left_normal + left_pesquera + left_patrulla
            right_total = right_normal + right_pesquera + right_patrulla

            if left_total > queue_size:
                messagebox.showerror(
                    "Error de Validación",
                    "La cantidad inicial de barcos en la cola izquierda supera el tamaño máximo de cola."
                )
                return

            if right_total > queue_size:
                messagebox.showerror(
                    "Error de Validación",
                    "La cantidad inicial de barcos en la cola derecha supera el tamaño máximo de cola."
                )
                return

            if flow == "Equidad":
                fairness_w = int(self.entry_fairness_w.get())

                if fairness_w <= 0 or fairness_w > queue_size:
                    messagebox.showerror(
                        "Error de Validación",
                        "W debe ser mayor a 0 y menor o igual al tamaño de la cola."
                    )
                    return
            else:
                fairness_w = 0

            if flow == "Letrero":
                sign_interval = int(self.entry_sign_interval.get())

                if sign_interval <= 0:
                    messagebox.showerror(
                        "Error de Validación",
                        "El intervalo del letrero debe ser mayor a 0."
                    )
                    return
            else:
                sign_interval = 0

            if scheduler == "Round Robin":
                rr_quantum = int(self.entry_rr_quantum.get())

                if rr_quantum <= 0:
                    messagebox.showerror(
                        "Error de Validación",
                        "El quantum de Round Robin debe ser mayor a 0."
                    )
                    return
            else:
                rr_quantum = 0

        except ValueError:
            messagebox.showerror(
                "Error de Validación",
                "Los valores numéricos deben ser enteros válidos."
            )
            return

        config = {
            "canalLength": canal_length,
            "queueSize": queue_size,
            "schedulerType": scheduler,
            "flowType": flow,
            "generationMode": self.combo_modo.get(),
            "leftInitialShips": {
                "normal": left_normal,
                "pesquera": left_pesquera,
                "patrulla": left_patrulla,
            },
            "rightInitialShips": {
                "normal": right_normal,
                "pesquera": right_pesquera,
                "patrulla": right_patrulla,
            },
            "fairnessW": fairness_w,
            "signInterval": sign_interval,
            "rrQuantum": rr_quantum,
            "visibleQueueSlots": visible_queue,
        }

        self.callback_iniciar(config)

    # ======================================================
    # DESTRUCCIÓN DE LA VISTA
    # ======================================================

    def destroy(self):
        try:
            self.canvas.unbind_all("<MouseWheel>")
            self.canvas.unbind_all("<Button-4>")
            self.canvas.unbind_all("<Button-5>")
        except Exception:
            pass

        self.container.destroy()
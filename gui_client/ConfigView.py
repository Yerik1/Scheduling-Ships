import tkinter as tk
from tkinter import ttk, messagebox
MAX_QUEUE_SIZE = 100

class ConfigView:
    def __init__(self, parent, callback_iniciar):
        self.parent = parent
        self.callback_iniciar = callback_iniciar
        self.widgets = []

        # Título
        title = tk.Label(parent, text="Configuración del Canal", font=("Arial", 16, "bold"), fg="navy")
        title.pack(pady=20)
        self.widgets.append(title)

        # Frame para configuración general
        frame_general = tk.Frame(parent, bg="lightblue", padx=20, pady=10)
        frame_general.pack(pady=10, fill=tk.X)
        self.widgets.append(frame_general)

        # --- CAMPOS EXISTENTES ---
        tk.Label(frame_general, text="Largo del Canal (unidades):", bg="lightblue", font=("Arial", 10, "bold")).grid(row=0, column=0, sticky=tk.W, pady=5)
        self.entry_largo = tk.Entry(frame_general, width=10)
        self.entry_largo.insert(0, "6")
        self.entry_largo.grid(row=0, column=1, pady=5)
        self.widgets.extend([frame_general.children['!label'], self.entry_largo])

        tk.Label(frame_general, text=f"Tamaño Máximo interno de Cola: {MAX_QUEUE_SIZE}):", bg="lightblue", font=("Arial", 10, "bold")).grid(row=1, column=0, sticky=tk.W, pady=5)
        self.entry_queue_size = tk.Entry(frame_general, width=10)
        self.entry_queue_size.insert(0, "4")
        self.entry_queue_size.grid(row=1, column=1, pady=5)
        self.widgets.append(self.entry_queue_size)

        tk.Label(
            frame_general,
            text="Barcos visibles por cola (1-4):",
            bg="lightblue",
            font=("Arial", 10, "bold")
        ).grid(row=2, column=0, sticky=tk.W, pady=5)

        self.entry_visible_queue = tk.Entry(frame_general, width=10)
        self.entry_visible_queue.insert(0, "4")
        self.entry_visible_queue.grid(row=2, column=1, pady=5)

        tk.Label(frame_general, text="Algoritmo de Scheduling:", bg="lightblue", font=("Arial", 10, "bold")).grid(row=3, column=0, sticky=tk.W, pady=5)
        self.combo_algo = ttk.Combobox(frame_general, values=["FCFS", "SJF", "Round Robin", "Priority", "STRN", "EDF"], width=15)
        self.combo_algo.current(0)
        self.combo_algo.grid(row=3, column=1, pady=5)
        self.combo_algo.bind('<<ComboboxSelected>>', self._update_fields)
        self.widgets.append(self.combo_algo)

        tk.Label(frame_general, text="Control de Flujo:", bg="lightblue", font=("Arial", 10, "bold")).grid(row=4, column=0, sticky=tk.W, pady=5)
        self.combo_flujo = ttk.Combobox(frame_general, values=["Equidad", "Letrero", "Tico"], width=15)
        self.combo_flujo.current(1)
        self.combo_flujo.grid(row=4, column=1, pady=5)
        self.combo_flujo.bind('<<ComboboxSelected>>', self._update_fields)
        self.widgets.append(self.combo_flujo)

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
        self.widgets.append(self.combo_modo)

        # --- CAMPOS DINÁMICOS ---
        
        # Fairness W (Equidad)
        tk.Label(frame_general, text="Fairness W:", bg="lightblue", font=("Arial", 10, "bold")).grid(row=5, column=0, sticky=tk.W, pady=5)
        self.entry_fairness_w = tk.Entry(frame_general, width=10)
        self.entry_fairness_w.insert(0, "1")
        self.entry_fairness_w.grid(row=6, column=1, pady=5)
        self.widgets.append(self.entry_fairness_w)

        # Sign Interval (Letrero)
        tk.Label(frame_general, text="Sign Interval (ms):", bg="lightblue", font=("Arial", 10, "bold")).grid(row=6, column=0, sticky=tk.W, pady=5)
        self.entry_sign_interval = tk.Entry(frame_general, width=10)
        self.entry_sign_interval.insert(0, "1000")
        self.entry_sign_interval.grid(row=7, column=1, pady=5)
        self.widgets.append(self.entry_sign_interval)

        # RR Quantum (Round Robin)
        tk.Label(frame_general, text="RR Quantum:", bg="lightblue", font=("Arial", 10, "bold")).grid(row=7, column=0, sticky=tk.W, pady=5)
        self.entry_rr_quantum = tk.Entry(frame_general, width=10)
        self.entry_rr_quantum.insert(0, "10")
        self.entry_rr_quantum.grid(row=7, column=1, pady=5)
        self.widgets.append(self.entry_rr_quantum)


        # Frame para barcos iniciales
        frame_barcos = tk.Frame(parent, bg="lightgreen", padx=20, pady=10)
        frame_barcos.pack(pady=10, fill=tk.X)
        self.widgets.append(frame_barcos)

        tk.Label(frame_barcos, text="Barcos Iniciales", font=("Arial", 12, "bold"), bg="lightgreen", fg="darkgreen").pack(pady=10)

        # Subframes para izquierda y derecha
        subframe = tk.Frame(frame_barcos, bg="lightgreen")
        subframe.pack()

        # Cola Izquierda
        frame_izq = tk.Frame(subframe, bg="lightyellow", padx=10, pady=10)
        frame_izq.pack(side=tk.LEFT, padx=20)
        self.widgets.append(frame_izq)

        tk.Label(frame_izq, text="Cola Izquierda", font=("Arial", 11, "bold"), bg="lightyellow", fg="orange").pack(pady=5)

        tk.Label(frame_izq, text="Normal:", bg="lightyellow").pack(anchor=tk.W)
        self.entry_left_normal = tk.Entry(frame_izq, width=5)
        self.entry_left_normal.insert(0, "0")
        self.entry_left_normal.pack()
        self.widgets.extend([frame_izq.children['!label'], self.entry_left_normal])

        tk.Label(frame_izq, text="Pesquera:", bg="lightyellow").pack(anchor=tk.W)
        self.entry_left_pesquera = tk.Entry(frame_izq, width=5)
        self.entry_left_pesquera.insert(0, "0")
        self.entry_left_pesquera.pack()
        self.widgets.extend([frame_izq.children['!label2'], self.entry_left_pesquera])

        tk.Label(frame_izq, text="Patrulla:", bg="lightyellow").pack(anchor=tk.W)
        self.entry_left_patrulla = tk.Entry(frame_izq, width=5)
        self.entry_left_patrulla.insert(0, "0")
        self.entry_left_patrulla.pack()
        self.widgets.extend([frame_izq.children['!label3'], self.entry_left_patrulla])

        # Cola Derecha
        frame_der = tk.Frame(subframe, bg="lightcyan", padx=10, pady=10)
        frame_der.pack(side=tk.LEFT, padx=20)
        self.widgets.append(frame_der)

        tk.Label(frame_der, text="Cola Derecha", font=("Arial", 11, "bold"), bg="lightcyan", fg="blue").pack(pady=5)

        tk.Label(frame_der, text="Normal:", bg="lightcyan").pack(anchor=tk.W)
        self.entry_right_normal = tk.Entry(frame_der, width=5)
        self.entry_right_normal.insert(0, "0")
        self.entry_right_normal.pack()
        self.widgets.extend([frame_der.children['!label'], self.entry_right_normal])

        tk.Label(frame_der, text="Pesquera:", bg="lightcyan").pack(anchor=tk.W)
        self.entry_right_pesquera = tk.Entry(frame_der, width=5)
        self.entry_right_pesquera.insert(0, "0")
        self.entry_right_pesquera.pack()
        self.widgets.extend([frame_der.children['!label2'], self.entry_right_pesquera])

        tk.Label(frame_der, text="Patrulla:", bg="lightcyan").pack(anchor=tk.W)
        self.entry_right_patrulla = tk.Entry(frame_der, width=5)
        self.entry_right_patrulla.insert(0, "0")
        self.entry_right_patrulla.pack()
        self.widgets.extend([frame_der.children['!label3'], self.entry_right_patrulla])

        # Inicializar estados
        self._update_fields()

        # Botón
        button = tk.Button(parent, text="Establecer Configuración", command=self.enviar_datos, bg="green", fg="white", font=("Arial", 12, "bold"), padx=20, pady=10)
        button.pack(pady=20)
        self.widgets.append(button)

    def _update_fields(self, event=None):
        scheduler = self.combo_algo.get()
        flow = self.combo_flujo.get()
        
        # Reset de estados (Habilitar todo primero)
        fields = [self.entry_fairness_w, self.entry_sign_interval, self.entry_rr_quantum, ]
        
        for f in fields:
            f.config(state='normal', bg="white")

        # Lógica de deshabilitación visual
        if scheduler != "Round Robin":
            self._disable_entry(self.entry_rr_quantum)
        
        if flow != "Letrero":
            self._disable_entry(self.entry_sign_interval)
        
        if flow != "Equidad":
            self._disable_entry(self.entry_fairness_w)
    
    def _disable_entry(self, entry):
        """Helper para deshabilitar y cambiar color visualmente"""
        entry.config(state='disabled', disabledbackground="#e0e0e0")

    def enviar_datos(self):
        scheduler = self.combo_algo.get()
        flow = self.combo_flujo.get()

        # Validaciones
        try:
            queue_size = int(self.entry_queue_size.get())
            visible_queue = int(self.entry_visible_queue.get())
            if queue_size <= 0 or queue_size >  MAX_QUEUE_SIZE:
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

            left_total = (
                    int(self.entry_left_normal.get()) +
                    int(self.entry_left_pesquera.get()) +
                    int(self.entry_left_patrulla.get())
            )

            right_total = (
                    int(self.entry_right_normal.get()) +
                    int(self.entry_right_pesquera.get()) +
                    int(self.entry_right_patrulla.get())
            )

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
                    messagebox.showerror("Error de Validación", "W debe ser mayor a 0 y menor al tamaño de la cola.")
                    return
        except ValueError:
            messagebox.showerror("Error de Validación", "Los valores numéricos deben ser enteros válidos.")
            return

        config = {
            "canalLength": int(self.entry_largo.get()),
            "queueSize": int(self.entry_queue_size.get()),
            "schedulerType": self.combo_algo.get(),
            "flowType": self.combo_flujo.get(),
            "generationMode": self.combo_modo.get(),
            "leftInitialShips": {
                "normal": int(self.entry_left_normal.get()),
                "pesquera": int(self.entry_left_pesquera.get()),
                "patrulla": int(self.entry_left_patrulla.get())
            },
            "rightInitialShips": {
                "normal": int(self.entry_right_normal.get()),
                "pesquera": int(self.entry_right_pesquera.get()),
                "patrulla": int(self.entry_right_patrulla.get())
            },
            "fairnessW": int(self.entry_fairness_w.get()) if flow == "Equidad" else 0,
            "signInterval": int(self.entry_sign_interval.get()) if flow == "Letrero" else 0,
            "rrQuantum": int(self.entry_rr_quantum.get()) if scheduler == "Round Robin" else 0,
            "visibleQueueSlots": visible_queue,
        }
        self.callback_iniciar(config)

    def destroy(self):
        for widget in self.widgets:
            widget.destroy()
        self.widgets = []

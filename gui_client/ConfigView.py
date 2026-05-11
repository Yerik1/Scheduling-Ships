import tkinter as tk
from tkinter import ttk, messagebox

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

        tk.Label(frame_general, text="Largo del Canal (unidades):", bg="lightblue", font=("Arial", 10, "bold")).grid(row=0, column=0, sticky=tk.W, pady=5)
        self.entry_largo = tk.Entry(frame_general, width=10)
        self.entry_largo.insert(0, "10")
        self.entry_largo.grid(row=0, column=1, pady=5)
        self.widgets.extend([frame_general.children['!label'], self.entry_largo])

        tk.Label(frame_general, text="Tamaño Máximo de Cola(Máximo 4):", bg="lightblue", font=("Arial", 10, "bold")).grid(row=1, column=0, sticky=tk.W, pady=5)
        self.entry_queue_size = tk.Entry(frame_general, width=10)
        self.entry_queue_size.insert(0, "4")
        self.entry_queue_size.grid(row=1, column=1, pady=5)
        self.widgets.append(self.entry_queue_size)

        tk.Label(frame_general, text="Algoritmo de Scheduling:", bg="lightblue", font=("Arial", 10, "bold")).grid(row=2, column=0, sticky=tk.W, pady=5)
        self.combo_algo = ttk.Combobox(frame_general, values=["FCFS", "SJF", "Round Robin", "Priority"], width=15)
        self.combo_algo.current(0)
        self.combo_algo.grid(row=2, column=1, pady=5)
        self.combo_algo.bind('<<ComboboxSelected>>', self._update_fields)
        self.widgets.append(self.combo_algo)

        tk.Label(frame_general, text="Control de Flujo:", bg="lightblue", font=("Arial", 10, "bold")).grid(row=3, column=0, sticky=tk.W, pady=5)
        self.combo_flujo = ttk.Combobox(frame_general, values=["Equidad", "Letrero", "Tico"], width=15)
        self.combo_flujo.current(1)
        self.combo_flujo.grid(row=3, column=1, pady=5)
        self.combo_flujo.bind('<<ComboboxSelected>>', self._update_fields)
        self.widgets.append(self.combo_flujo)

        tk.Label(frame_general, text="Fairness W:", bg="lightblue", font=("Arial", 10, "bold")).grid(row=4, column=0, sticky=tk.W, pady=5)
        self.entry_fairness_w = tk.Entry(frame_general, width=10)
        self.entry_fairness_w.insert(0, "1")
        self.entry_fairness_w.grid(row=4, column=1, pady=5)
        self.widgets.append(self.entry_fairness_w)

        tk.Label(frame_general, text="Sign Interval (ms):", bg="lightblue", font=("Arial", 10, "bold")).grid(row=5, column=0, sticky=tk.W, pady=5)
        self.entry_sign_interval = tk.Entry(frame_general, width=10)
        self.entry_sign_interval.insert(0, "1000")
        self.entry_sign_interval.grid(row=5, column=1, pady=5)
        self.widgets.append(self.entry_sign_interval)

        tk.Label(frame_general, text="RR Quantum:", bg="lightblue", font=("Arial", 10, "bold")).grid(row=6, column=0, sticky=tk.W, pady=5)
        self.entry_rr_quantum = tk.Entry(frame_general, width=10)
        self.entry_rr_quantum.insert(0, "10")
        self.entry_rr_quantum.grid(row=6, column=1, pady=5)
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
        
        # Por defecto, habilitar todos
        self.entry_fairness_w.config(state='normal')
        self.entry_sign_interval.config(state='normal')
        self.entry_rr_quantum.config(state='normal')

        # Si scheduler no es Round Robin, deshabilitar Quantum
        if scheduler != "Round Robin":
            self.entry_rr_quantum.config(state='disabled')
        
        # Si flow es Letrero, W
        if flow == "Letrero":
            self.entry_fairness_w.config(state='disabled')
        
        # Si flow es Equidad, Sign Interval
        if flow == "Equidad":
            self.entry_sign_interval.config(state='disabled')

    def enviar_datos(self):
        scheduler = self.combo_algo.get()
        flow = self.combo_flujo.get()

        # Validaciones
        try:
            queue_size = int(self.entry_queue_size.get())
            if queue_size <= 0 or queue_size > 4:
                messagebox.showerror("Error de Validación", "El tamaño de la cola debe ser entre 1 y 4.")
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
            "rrQuantum": int(self.entry_rr_quantum.get()) if scheduler == "Round Robin" else 0
        }
        self.callback_iniciar(config)

    def destroy(self):
        for widget in self.widgets:
            widget.destroy()
        self.widgets = []

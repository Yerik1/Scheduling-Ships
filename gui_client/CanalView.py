import tkinter as tk
from tkinter import ttk

class CanalView:
    def __init__(self, root, modelo_canal):
        self.root = root
        self.modelo = modelo_canal

        self.control_frame = tk.Frame(root, bg="lightsteelblue", padx=10, pady=10)
        self.control_frame.pack(fill=tk.X)

        tk.Label(self.control_frame, text="Tipo de barco a generar:", bg="lightsteelblue", font=("Arial", 10, "bold")).grid(row=0, column=0, sticky=tk.W)
        self.tipo_combo = ttk.Combobox(self.control_frame, values=["Normal", "Pesquera", "Patrulla"], width=12, state="readonly")
        self.tipo_combo.current(0)
        self.tipo_combo.grid(row=0, column=1, padx=6)

        tk.Label(self.control_frame, text="Presione L para generar desde IZQUIERDA, R para generar desde DERECHA", bg="lightsteelblue", font=("Arial", 10)).grid(row=0, column=2, padx=20)
        tk.Label(self.control_frame, text="Tecla W para cerrar la aplicación.", bg="lightsteelblue", font=("Arial", 10)).grid(row=1, column=0, columnspan=3, pady=(6, 0), sticky=tk.W)

        self.canvas = tk.Canvas(root, width=1000, height=500, bg="skyblue")
        self.canvas.pack(pady=(0, 10))
        
        # Dibujar Canal
        self.canal_rect = self.canvas.create_rectangle(250, 200, 750, 300, fill="deepskyblue", outline="blue", width=3)
        self.canvas.create_text(500, 170, text="CANAL", font=("Arial", 14, "bold"), fill="navy")
        
        # Agujas en los extremos del canal (ocultas hasta activarse)
        self.aguja_izq_line = self.canvas.create_line(250, 200, 250, 300, fill="red", width=8, state="hidden")
        self.aguja_der_line = self.canvas.create_line(750, 200, 750, 300, fill="red", width=8, state="hidden")

        # Letrero de sentido
        self.letrero_indicador = self.canvas.create_oval(480, 80, 520, 120, fill="red", outline="black", width=2)
        self.texto_letrero = self.canvas.create_text(500, 140, text="Sentido: IZQ", font=("Arial", 10, "bold"), fill="black")
        self.texto_agujas = self.canvas.create_text(500, 485, text="Agujas: IZQ=ARRIBA  DER=ARRIBA", font=("Arial", 10, "bold"), fill="black")

    def actualizar_barcos(self, cola_izq, cola_der, barco_canal):
        self.canvas.delete("barco_tag")
        
        # --- 1. DIBUJAR COLA IZQUIERDA ---
        for i, b_id in enumerate(cola_izq):
            x_cola = 200 - (i * 50)
            tipo = self.get_tipo_from_id(b_id)
            self.dibujar_barco(x_cola, 230, tipo, b_id) 

        # --- 2. DIBUJAR COLA DERECHA ---
        for i, b_id in enumerate(cola_der):
            x_cola = 800 + (i * 50)
            tipo = self.get_tipo_from_id(b_id)
            self.dibujar_barco(x_cola, 230, tipo, b_id)

        # --- 3. DIBUJAR BARCO EN CANAL ---
        if barco_canal:
            ancho_visual = 500 
            x_pos = 250 + (barco_canal['pos'] * (ancho_visual / self.modelo.largo))
            self.dibujar_barco(x_pos, 230, barco_canal['tipo'], barco_canal['id'])

        self.actualizar_agujas(self.modelo.agujas_activas)
    
    def get_tipo_from_id(self, b_id):
        if b_id.endswith('N'):
            return 'N'
        elif b_id.endswith('F'):
            return 'F'
        elif b_id.endswith('P'):
            return 'P'
        else:
            return 'N'
    
    def actualizar_letrero(self, sentido):
        # sentido viene como "IZQUIERDA" o "DERECHA" desde el modelo
        # IZQUIERDA: barcos de derecha moviéndose a izquierda -> tránsito derecha a izquierda
        # DERECHA: barcos de izquierda moviéndose a derecha -> tránsito izquierda a derecha
        color = "blue" if sentido == "IZQUIERDA" else "green"
        direccion = "DERECHA A IZQUIERDA" if sentido == "IZQUIERDA" else "IZQUIERDA A DERECHA"
        
        if self.modelo.flow_actual == "Letrero":
            self.canvas.itemconfig(self.letrero_indicador, fill=color, state="normal")
            self.canvas.itemconfig(self.texto_letrero, text=f"DIRECCIÓN: {direccion}", state="normal")
        else:
            self.canvas.itemconfig(self.letrero_indicador, state="hidden")
            self.canvas.itemconfig(self.texto_letrero, state="hidden")

    def actualizar_agujas(self, activas):
        if activas:
            self.canvas.itemconfig(self.aguja_izq_line, fill="red", state="normal")
            self.canvas.itemconfig(self.aguja_der_line, fill="red", state="normal")
            estado = "BAJADAS"
        else:
            self.canvas.itemconfig(self.aguja_izq_line, state="hidden")
            self.canvas.itemconfig(self.aguja_der_line, state="hidden")
            estado = "ARRIBA"

        self.canvas.itemconfig(self.texto_agujas, text=f"Agujas: {estado}")
    
    def get_tipo_generacion(self):
        texto = self.tipo_combo.get()
        return {
            "Normal": "N",
            "Pesquera": "F",
            "Patrulla": "P"
        }.get(texto, "N")

    def dibujar_barco(self, x, y, tipo, b_id):
        colores = {
            'N': "silver",    # Normal
            'F': "limegreen",   # Pesquera
            'P': "crimson"      # Patrulla
        }
        color = colores.get(tipo, "gray")
        
        # Dibujar el casco del barco
        self.canvas.create_polygon(x, y, x+50, y, x+40, y+20, x+10, y+20, 
                                fill=color, outline="black", width=2, tags="barco_tag")
        
        # Mostrar el ID del barco encima
        self.canvas.create_text(x+25, y+10, text=str(b_id), fill="black", font=("Arial", 8, "bold"), tags="barco_tag")
        
        # Añadir cabina si es Patrulla
        if tipo == 'P':
            self.canvas.create_rectangle(x+20, y-15, x+30, y, fill="gold", outline="black", tags="barco_tag")
import serial
import random
import platform
import os
import json
import tkinter as tk
from model import CanalModelo
from ConfigView import ConfigView
from CanalView import CanalView

class CanalController:
    def __init__(self, root):
        self.root = root
        self.root.title("Scheduling Ships - TEC")
        self.simulacion = False
        self.modelo = CanalModelo(largo=100)
        self.vista = None
        self.ser = None
        self.id_counter = 0
        self.sim_time = 0
        self.barco_pos = 0
        self.flow_type = "Equidad"  # default
        self.config_view = None

        # Intentar cargar configuración desde canal.config
        config_file = "canal.config"
        if os.path.exists(config_file):
            try:
                with open(config_file, 'r') as f:
                    config = json.load(f)
                self.iniciar_sistema(config)
            except Exception as e:
                print(f"Error cargando canal.config: {e}")
                self.config_view = ConfigView(self.root, self.iniciar_sistema)
        else:
            # Mostrar la vista de configuración
            self.config_view = ConfigView(self.root, self.iniciar_sistema)

    def iniciar_sistema(self, config):
        # Destruir la vista de configuración
        if self.config_view:
            self.config_view.destroy()

        # Aplicar configuración
        self.modelo.largo = config["canalLength"]
        self.queue_size = config["queueSize"]
        self.flow_type = config["flowType"]

        # Generar barcos iniciales
        izq_ids = []
        id_counter = 0
        for tipo, count in config["leftInitialShips"].items():
            tipo_code = tipo[0].upper()  # N, P, F
            for i in range(count):
                izq_ids.append(f"L{id_counter}{tipo_code}")
                id_counter += 1

        der_ids = []
        for tipo, count in config["rightInitialShips"].items():
            tipo_code = tipo[0].upper()
            for i in range(count):
                der_ids.append(f"R{id_counter}{tipo_code}")
                id_counter += 1

        self.modelo.actualizar_colas(izq_ids, der_ids)

        # Intentar conexión Serial
        try:
            puerto = 'COM3' if platform.system() == 'Windows' else '/dev/ttyUSB0'
            self.ser = serial.Serial(puerto, 115200, timeout=0.1)
            msg = f"CONF:{config['canalLength']}:{config['queueSize']}:{config['schedulerType']}:{config['flowType']}:{config['fairnessW']}:{config['signInterval']}:{config['rrQuantum']}\n"
            self.ser.write(msg.encode())
        except Exception:
            self.simulacion = True
            print("Iniciando en modo simulación.")
            # Configurar simulación de test RR con Equidad W=1
            self.estado_index = 0
            self.estados_simulacion =  [
                # Estado inicial
                "STATE:left:L0N,L1N,L2N,L3N;right:R0N,R1N,R2N,R3N;canal:;sign:L;needle:0;flow:Equidad",
                # Barco R0N entra y se mueve
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@10#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@9#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@8#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@7#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@6#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@5#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@4#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@3#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@2#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@1#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:R0N@0#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R1N,R2N,R3N;canal:;sign:L;needle:0;flow:Equidad",
                # Barco R1N
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@10#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@9#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@8#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@7#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@6#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@5#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@4#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@3#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@2#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@1#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:R1N@0#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R2N,R3N;canal:;sign:L;needle:0;flow:Equidad",
                # Barco R2N
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@10#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@9#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@8#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@7#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@6#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@5#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@4#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@3#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@2#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@1#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:R2N@0#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:R3N;canal:;sign:L;needle:0;flow:Equidad",
                # Barco R3N
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@10#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@9#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@8#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@7#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@6#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@5#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@4#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@3#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@2#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@1#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:R3N@0#N;sign:L;needle:0;flow:Equidad",
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:;sign:L;needle:0;flow:Equidad",
                # Cambiar sentido a DERECHA
                "STATE:left:L0N,L1N,L2N,L3N;right:;canal:;sign:R;needle:0;flow:Equidad",
                # Barco L0N
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@0#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@1#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@2#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@3#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@4#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@5#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@6#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@7#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@8#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@9#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:L0N@10#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L1N,L2N,L3N;right:;canal:;sign:R;needle:0;flow:Equidad",
                # Barco L1N
                "STATE:left:L2N,L3N;right:;canal:L1N@0#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:L1N@1#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:L1N@2#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:L1N@3#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:L1N@4#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:L1N@5#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:L1N@6#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:L1N@7#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:L1N@8#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:L1N@9#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:L1N@10#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L2N,L3N;right:;canal:;sign:R;needle:0;flow:Equidad",
                # Barco L2N
                "STATE:left:L3N;right:;canal:L2N@0#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:L2N@1#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:L2N@2#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:L2N@3#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:L2N@4#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:L2N@5#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:L2N@6#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:L2N@7#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:L2N@8#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:L2N@9#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:L2N@10#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:L3N;right:;canal:;sign:R;needle:0;flow:Equidad",
                # Barco L3N
                "STATE:left:;right:;canal:L3N@0#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:L3N@1#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:L3N@2#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:L3N@3#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:L3N@4#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:L3N@5#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:L3N@6#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:L3N@7#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:L3N@8#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:L3N@9#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:L3N@10#N;sign:R;needle:0;flow:Equidad",
                "STATE:left:;right:;canal:;sign:R;needle:0;flow:Equidad",
            ]

        # Cargar la vista del canal
        self.vista = CanalView(self.root, self.modelo)
        self.root.bind('<Key>', self._on_key_press)
        self.root.focus_set()

        # Iniciar el loop único
        self.actualizar_loop()

    def actualizar_loop(self):
        if self.simulacion:
            if hasattr(self, 'estados_simulacion') and hasattr(self, 'estado_index'):
                if self.estado_index < len(self.estados_simulacion):
                    data = self.estados_simulacion[self.estado_index]
                    self.procesar_estado(data)
                    self.estado_index += 1
                else:
                    print("Simulación terminada: todos los barcos han cruzado el canal.")
                    self.root.quit()
            else:
                self.sim_time += 1
                self.barco_pos = (self.sim_time // 5) % (self.modelo.largo + 1)
                sentido = "IZQUIERDA" if self.barco_pos < self.modelo.largo // 2 else "DERECHA"
                sign = 'L' if sentido == "IZQUIERDA" else 'R'
                data = f"STATE:left:L0N,L1F,L2P;right:R0N,R1F;canal:L0N@{self.barco_pos}#N;sign:{sign};needle:0;flow:Equidad"
                self.procesar_estado(data)
        else:
            if hasattr(self, 'ser') and self.ser.in_waiting > 0:
                try:
                    linea = self.ser.readline().decode('utf-8').strip()
                    if linea.startswith("STATE:"):
                        self.procesar_estado(linea)
                except Exception as e:
                    print(f"Error de lectura: {e}")

        # Actualizar la interfaz
        if self.vista:
            self.vista.actualizar_letrero(self.modelo.sentido_actual)
            self.vista.actualizar_barcos(
                self.modelo.cola_izq, 
                self.modelo.cola_der, 
                self.modelo.barco_canal
            )
        
        self.root.after(200, self.actualizar_loop)

    def procesar_estado(self, data):
        try:
            payload = data.replace("STATE:", "").strip()
            secciones = payload.split(';')
            dict_datos = {}
            for s in secciones:
                if ':' in s:
                    clave, valor = s.split(':', 1)
                    dict_datos[clave] = valor

            # Actualizar colas
            izq = [i for i in dict_datos.get('left', '').split(',') if i]
            der = [i for i in dict_datos.get('right', '').split(',') if i]
            self.modelo.actualizar_colas(izq, der)

            # Actualizar canal
            canal_data = dict_datos.get('canal', '')
            if '@' in canal_data:
                datos, tipo = canal_data.split('#') if '#' in canal_data else (canal_data, 'N')
                ship_id, pos = datos.split('@')
                self.modelo.actualizar_canal(ship_id, int(pos), tipo)
            else:
                self.modelo.actualizar_canal(None, 0)

            # Actualizar agujas (estado único para ambos extremos)
            agujas_activas = False
            if 'needle' in dict_datos:
                raw = dict_datos['needle'].strip()
                if raw in ('1', '0'):
                    agujas_activas = raw == '1'
                else:
                    partes = raw.replace(',', ';').replace('=', ':').split(';')
                    for item in partes:
                        if ':' in item:
                            _, valor = item.split(':', 1)
                            if valor.strip() == '1':
                                agujas_activas = True
                                break
            elif 'needle_left' in dict_datos or 'needle_right' in dict_datos:
                if dict_datos.get('needle_left', '') == '1' or dict_datos.get('needle_right', '') == '1':
                    agujas_activas = True
            self.modelo.actualizar_agujas(agujas_activas)

            # Actualizar flow
            flow = dict_datos.get('flow', self.flow_type)
            self.modelo.flow_actual = flow

            # Letrero
            sign = dict_datos.get('sign', 'L')
            self.modelo.sentido_actual = "DERECHA" if sign == 'L' else "IZQUIERDA"

        except Exception as e:
            print(f"Error parseo: {e}")

    def generar_barco(self, lado):
        if not self.vista:
            return
        
        # Verificar que haya espacio en la cola (máximo self.queue_size barcos por cola)
        if lado == 'L' and len(self.modelo.cola_izq) >= self.queue_size:
            print("Cola izquierda llena, no se puede generar barco.")
            return
        if lado == 'R' and len(self.modelo.cola_der) >= self.queue_size:
            print("Cola derecha llena, no se puede generar barco.")
            return
        
        tipo = self.vista.get_tipo_generacion()
        if not tipo:
            return

        comando = f"GEN:{lado}:{tipo}\n"
        if not self.simulacion and self.ser:
            try:
                self.ser.write(comando.encode())
            except Exception as e:
                print(f"Error serial al generar barco: {e}")
        else:
            nuevo_id = f"{lado}{self.id_counter}{tipo}"
            self.id_counter += 1
            self.modelo.agregar_barco_cola(lado, nuevo_id)

        print(f"Generar barco -> lado={lado}, tipo={tipo}, comando={comando.strip()}")

    def _on_key_press(self, event):
        key = getattr(event, 'keysym', '').lower()
        if key == 'l':
            self.generar_barco('L')
        elif key == 'r':
            self.generar_barco('R')
        elif key == 'w':
            self.cerrar_programa()

    def cerrar_programa(self, event=None):
        if not self.simulacion and hasattr(self, 'ser'):
            self.ser.close()
        self.root.destroy()

import serial
import platform
import os
import json
import time
from model import CanalModelo
from ConfigView import ConfigView
from CanalView import CanalView

CANAL_LENGTH_FIXED = 6
UI_REFRESH_MS = 50

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
        self.modelo.largo = CANAL_LENGTH_FIXED
        self.queue_size = config["queueSize"]
        self.flow_type = config["flowType"]
        self.generation_mode = config.get("generationMode", "Fijo")

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
            puerto = 'COM4' if platform.system() == 'Windows' else '/dev/ttyUSB0'
            self.ser = serial.Serial(
                puerto,
                115200,
                timeout=0.02,
                write_timeout=3,
                rtscts=False,
                dsrdtr=False,
                xonxoff=False
            )


            time.sleep(1.5)

            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()

            if not self._esperar_esp_ready():
                print("No se recibio UI_READY desde la ESP.")
                self.simulacion = True
                return

            comandos = [
                f"CFG_LEN:{config['canalLength']}",
                f"CFG_Q:{config['queueSize']}",
                f"CFG_S:{config['schedulerType']}",
                f"CFG_F:{config['flowType']}",
                f"CFG_W:{config['fairnessW']}",
                f"CFG_SIGN:{config['signInterval']}",
                f"CFG_RR:{config['rrQuantum']}",
                f"CFG_MODE:{config.get('generationMode', 'Fijo')}",
                f"CFG_BURST:{config['burstTime']}",
                f"CFG_DEADLINE:{config['deadline']}",
            ]

            for cmd in comandos:
                if not self._send_line(cmd):
                    print("No se pudo enviar configuración completa.")
                    self.simulacion = True
                    return

                if not self._read_until_ack("ACK:CFG"):
                    print("No se recibio ACK:CFG.")
                    self.simulacion = True
                    return

            time.sleep(0.1)

            if not self._enviar_barcos_iniciales(config):
                print("No se pudieron enviar barcos iniciales.")
                self.simulacion = True
                return

            time.sleep(0.1)

            if not self._send_line("START"):
                self.simulacion = True
                return

            if not self._read_until_ack("ACK:START"):
                print("No se recibio ACK:START.")
                self.simulacion = True
                return

            self.simulacion = False
            print("Interfaz conectada a ESP32.")

        except Exception as e:
            self.simulacion = True
            print(f"Iniciando en modo simulación. Error serial: {e}")


        # Cargar la vista del canal
        self.vista = CanalView(self.root, self.modelo)
        self.root.bind('<Key>', self._on_key_press)
        self.root.focus_set()

        # Iniciar el loop único
        self.actualizar_loop()

    def actualizar_loop(self):
        latest_state = None

        if not self.simulacion and self.ser:
            try:
                # Leer todo lo disponible para no quedarse atrasado
                while self.ser.in_waiting > 0:
                    linea = self.ser.readline().decode("utf-8", errors="ignore").strip()

                    if linea.startswith("STATE:"):
                        latest_state = linea
                    elif linea:
                        print("RX:", linea)

                # Procesar solo el ultimo STATE recibido
                if latest_state:
                    self.procesar_estado(latest_state)

            except Exception as e:
                print(f"Error de lectura: {e}")

        if self.vista:
            self.vista.actualizar_letrero(self.modelo.sentido_actual)
            self.vista.actualizar_barcos(
                self.modelo.cola_izq,
                self.modelo.cola_der,
                self.modelo.barcos_canal
            )

        self.root.after(UI_REFRESH_MS, self.actualizar_loop)

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
            barcos_canal = []

            if canal_data:
                items = [item for item in canal_data.split(',') if item]

                for item in items:
                    try:
                        if '#' in item:
                            datos, tipo = item.split('#', 1)
                        else:
                            datos, tipo = item, 'N'

                        ship_id, pos = datos.split('@', 1)

                        barcos_canal.append({
                            "id": ship_id,
                            "pos": int(pos),
                            "tipo": tipo
                        })

                    except ValueError:
                        print(f"Advertencia: no se pudo parsear barco en canal: {item}")

            self.modelo.actualizar_canal(barcos_canal)

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

        if self.generation_mode == "Fijo":
            print("Modo fijo activo: no se pueden generar barcos durante la ejecución.")
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
            if not self._send_line(f"GEN:{lado}:{tipo}"):
                print("Error serial al generar barco dinámico.")
        else:
            nuevo_id = f"{lado}{self.id_counter}{tipo}"
            self.id_counter += 1
            self.modelo.agregar_barco_cola(lado, nuevo_id)

        print(f"Generar barco -> lado={lado}, tipo={tipo}, comando={comando.strip()}")

    def _enviar_barcos_iniciales(self, config):
        import time

        tipo_map = {
            "normal": "N",
            "pesquera": "F",
            "patrulla": "P"
        }

        for tipo, count in config["leftInitialShips"].items():
            tipo_code = tipo_map.get(tipo, "N")

            for _ in range(count):
                if not self._send_line(f"GEN:L:{tipo_code}"):
                    return False

                if not self._read_until_ack("ACK:GEN"):
                    return False

                time.sleep(0.05)

        for tipo, count in config["rightInitialShips"].items():
            tipo_code = tipo_map.get(tipo, "N")

            for _ in range(count):
                if not self._send_line(f"GEN:R:{tipo_code}"):
                    return False

                if not self._read_until_ack("ACK:GEN"):
                    return False

                time.sleep(0.05)

        return True

    def _on_key_press(self, event):
        key = getattr(event, 'keysym', '').lower()
        if key == 'l':
            self.generar_barco('L')
        elif key == 'r':
            self.generar_barco('R')
        elif key == 'w':
            self.cerrar_programa()

    def _esperar_esp_ready(self, timeout_s=8):
        import time

        start = time.time()
        last_ping = 0

        while time.time() - start < timeout_s:
            now = time.time()

            if now - last_ping >= 0.5:
                if not self._send_line("PING"):
                    return False
                last_ping = now

            try:
                linea = self.ser.readline().decode("utf-8", errors="ignore").strip()

                if linea:
                    print("RX:", linea)

                if linea == "ACK:PING":
                    return True

            except Exception as e:
                print(f"Error esperando ACK:PING: {e}")
                return False

        return False

    def _send_line(self, line):
        if not self.ser or not self.ser.is_open:
            print("Serial no disponible.")
            return False

        try:
            msg = line.strip() + "\n"
            data = msg.encode("utf-8")

            print("TX:", msg.strip())

            written = self.ser.write(data)

            if written != len(data):
                print(f"Advertencia: solo se escribieron {written}/{len(data)} bytes")
                return False

            return True

        except Exception as e:
            print(f"Error enviando linea serial '{line}': {e}")
            return False

    def _read_until_ack(self, expected, timeout_s=3):
        import time

        start = time.time()

        while time.time() - start < timeout_s:
            try:
                linea = self.ser.readline().decode("utf-8", errors="ignore").strip()

                if linea:
                    print("RX:", linea)

                if linea == expected:
                    return True

            except Exception as e:
                print(f"Error esperando {expected}: {e}")
                return False

        return False

    def cerrar_programa(self, event=None):
        if not self.simulacion and hasattr(self, 'ser'):
            self.ser.close()
        self.root.destroy()

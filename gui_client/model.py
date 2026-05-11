class Barco:
    def __init__(self, id, tipo, sentido, velocidad):
        self.id = id
        self.tipo = tipo  # Normal, Pesquero, Patrulla
        self.sentido = sentido  # 'izq_der' o 'der_izq'
        self.posicion_x = 0
        self.velocidad = velocidad

class CanalModelo:
    def __init__(self, largo):
        self.largo = largo
        self.cola_izq = []
        self.cola_der = []
        self.barco_canal = None
        self.sentido_actual = "IZQUIERDA"
        self.agujas_activas = False
        self.flow_actual = "Equidad"

    def actualizar_colas(self, izq_ids, der_ids):
        self.cola_izq = izq_ids
        self.cola_der = der_ids

    def actualizar_canal(self, ship_id, posicion, tipo='N'):
        if ship_id:
            self.barco_canal = {
                "id": ship_id,
                "pos": posicion,
                "tipo": tipo
            }
        else:
            self.barco_canal = None

    def agregar_barco_cola(self, lado, barco_id):
        if lado == 'L':
            self.cola_izq.append(barco_id)
        elif lado == 'R':
            self.cola_der.append(barco_id)

    def actualizar_agujas(self, activas):
        self.agujas_activas = activas

    def get_tipo_from_id(self, b_id):
        if b_id.endswith('N'):
            return 'N'
        elif b_id.endswith('F'):
            return 'F'
        elif b_id.endswith('P'):
            return 'P'
        else:
            return 'N'

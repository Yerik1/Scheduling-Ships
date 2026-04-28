# Scheduling Ships

Proyecto del curso **Principios de Sistemas Operativos**.

El objetivo general es simular barcos como tareas/hilos, donde cada barco representa un proceso/task.  
De momento, este avance implementa:

- Definición de barcos (`Ship`).
- Creación de barcos mediante `ShipFactory`.
- Asociación de cada barco con una task de FreeRTOS.
- Prueba básica en ESP32-C6 usando ESP-IDF.

---

## Requisitos

Para compilar y ejecutar el proyecto se necesita:

- ESP-IDF instalado.
- Toolchain para ESP32-C6.
- Python configurado por ESP-IDF.
- Una placa ESP32-C6.
- Cable USB para flashear y monitorear la placa.

No es necesario usar CLion. El proyecto puede compilarse directamente desde la terminal de ESP-IDF.

---

## Estructura actual del proyecto

```text
Scheduling-Ships/
│
├── CMakeLists.txt
├── sdkconfig
│
└── main/
    ├── CMakeLists.txt
    ├── main.c
    │
    ├── common/
    │   └── types.h
    │
    ├── ships/
    │   ├── ship.h
    │   ├── ship.c
    │   ├── ship_factory.h
    │   └── ship_factory.c
    │
    └── tasks/
        ├── ship_task.h
        └── ship_task.c
```

---

## Configurar el entorno

Abrir una terminal de ESP-IDF.

En Windows, normalmente se puede usar:

```text
ESP-IDF PowerShell
```

o

```text
ESP-IDF Command Prompt
```

Luego entrar a la carpeta raíz del proyecto:

```bash
cd ruta/al/proyecto/Scheduling-Ships
```

---

## Configurar el target

Este proyecto está pensado para **ESP32-C6**, por lo que se debe configurar el target así:

```bash
idf.py set-target esp32c6
```

Este comando solo es necesario la primera vez o cuando se cambia de chip.

---

## Compilar el proyecto

Para compilar:

```bash
idf.py build
```

Si la compilación funciona correctamente, debería aparecer algo parecido a:

```text
Project build complete.
```

También pueden aparecer mensajes como:

```text
Successfully created esp32c6 image.
```

---

## Flashear la ESP32-C6

Conectar la placa por USB.

Luego ejecutar:

```bash
idf.py flash
```

Si hay varias placas o el puerto no se detecta automáticamente, indicar el puerto manualmente.

En Windows puede ser algo como:

```bash
idf.py -p COM3 flash
```

Cambiar `COM3` por el puerto correspondiente.

---

## Ver la salida por monitor serial

Después de flashear, abrir el monitor serial:

```bash
idf.py monitor
```

También se puede flashear y abrir el monitor en un solo comando:

```bash
idf.py flash monitor
```

O indicando el puerto:

```bash
idf.py -p COM3 flash monitor
```

Para salir del monitor:

```text
Ctrl + ]
```

---

## Salida esperada

Al ejecutar el proyecto, deberían verse mensajes similares a estos:

```text
Scheduling Ships iniciado

Task creada para barco 0 con nombre Ship_0
Task creada para barco 1 con nombre Ship_1
Task creada para barco 2 con nombre Ship_2

[Ship_0] Barco creado | ID: 0 | Tipo: NORMAL | Origen: LEFT | Destino: RIGHT | Estado: READY
[Ship_1] Barco creado | ID: 1 | Tipo: FISHING | Origen: RIGHT | Destino: LEFT | Estado: READY
[Ship_2] Barco creado | ID: 2 | Tipo: PATROL | Origen: LEFT | Destino: RIGHT | Estado: READY

[Ship_0] Barco 0 cambio a estado RUNNING
[Ship_1] Barco 1 cambio a estado RUNNING
[Ship_2] Barco 2 cambio a estado RUNNING

[Ship_0] Barco 0 avanzando | Posicion: 1 | Tiempo restante: ...
[Ship_1] Barco 1 avanzando | Posicion: 2 | Tiempo restante: ...
[Ship_2] Barco 2 avanzando | Posicion: 3 | Tiempo restante: ...

[Ship_0] Barco 0 finalizo | Estado: FINISHED
[Ship_1] Barco 1 finalizo | Estado: FINISHED
[Ship_2] Barco 2 finalizo | Estado: FINISHED
```

El orden exacto de los mensajes puede cambiar, porque cada barco corre como una task independiente de FreeRTOS.

Eso es normal y esperado.

---

## Qué demuestra este avance

Este avance demuestra que:

1. Cada barco se representa mediante un `struct Ship`.
2. Los barcos se crean usando `ShipFactory`.
3. Cada barco se asocia a una task de FreeRTOS.
4. Cada task ejecuta su propio flujo.
5. Cada barco puede cambiar de estado y simular avance.

---

## Comandos útiles

Limpiar completamente la compilación:

```bash
idf.py fullclean
```

Volver a compilar:

```bash
idf.py build
```

Compilar, flashear y monitorear:

```bash
idf.py flash monitor
```

Configurar nuevamente el target:

```bash
idf.py set-target esp32c6
```

---

## Notas importantes

- No se debe llamar manualmente a `vTaskStartScheduler()`.
- En ESP-IDF, el punto de entrada del programa es `app_main()`, no `main()`.
- FreeRTOS ya viene incluido con ESP-IDF.
- Las prioridades usadas por FreeRTOS no representan todavía los algoritmos de calendarización del proyecto.
- La calendarización propia del proyecto se implementará después sobre las colas y el canal.

---

## Estado actual

Actualmente el proyecto está en una etapa inicial enfocada en:

```text
Ship → ShipFactory → ShipTask → FreeRTOS Task
```

Próximos pasos sugeridos:

- Implementar colas izquierda y derecha.
- Implementar canal.
- Implementar calendarizadores.
- Conectar tasks con el scheduler propio del proyecto.
- Agregar políticas de flujo: Letrero, Equidad y Tico.
cmake_minimum_required(VERSION 3.12)
project(ShipVisualizer C)

# --- Gestión de Dependencias (Raylib) ---
include(FetchContent)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        5.0
)

# Configurar Raylib antes de descargar (para que no compile ejemplos pesados)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

# --- Detección Automática de Archivos ---
file(GLOB SOURCES "src/*.c")

# --- Configuración del Ejecutable ---

if(NOT SOURCES)
    message(FATAL_ERROR "No se encontraron archivos fuente en src/. Revisa la estructura de carpetas.")
endif()

add_executable(ship_gui main.c ${SOURCES})

# --- Inclusión y Linkeo ---
target_include_directories(ship_gui PRIVATE include)
target_link_libraries(ship_gui raylib m pthread dl rt X11)

# Mensaje de éxito
message(STATUS "Raylib configurado para ${CMAKE_SYSTEM_NAME}")
message(STATUS "Archivos fuente detectados: ${SRC_COUNT}")

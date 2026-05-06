#!/bin/bash

# Colores para la terminal
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}--- Iniciando Ship Visualizer (Raylib Client) ---${NC}"

# 1. Crear carpeta build si no existe
if [ ! -d "build" ]; then
    echo -e "${GREEN}[1/3] Creando directorio de compilación...${NC}"
    mkdir build
fi

cd build

# 2. Configurar con CMake
echo -e "${GREEN}[2/3] Configurando proyecto con CMake...${NC}"
# Usamos -Wno-dev para silenciar advertencias de dependencias externas
cmake .. -Wno-dev

# 3. Compilar
echo -e "${GREEN}[3/3] Compilando binario...${NC}"
make

# 4. Ejecutar si la compilación fue exitosa
if [ $? -eq 0 ]; then
    echo -e "${BLUE}--- Ejecutando Interfaz Gráfica ---${NC}"
    ./ship_gui
else
    echo -e "${RED}Error: La compilación falló.${NC}"
    exit 1
fi

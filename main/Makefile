SHELL := /bin/bash

# =========================
# Configuración general
# =========================

IDF_PATH ?= $(HOME)/esp/esp-idf
TARGET ?= esp32c6
PORT ?= /dev/ttyACM0
BAUD ?= 115200

PYTHON ?= python3
GUI_DIR := gui_client
GUI_VENV := $(GUI_DIR)/.venv
GUI_PY := $(GUI_VENV)/bin/python

# =========================
# Función auxiliar ESP-IDF
# =========================

define IDF_CMD
	@bash -lc ' \
		if [ -f "$(IDF_PATH)/export.sh" ]; then \
			source "$(IDF_PATH)/export.sh" >/dev/null; \
		fi; \
		idf.py $(1) \
	'
endef

# =========================
# Targets principales
# =========================

.PHONY: help
help:
	@echo ""
	@echo "Comandos disponibles:"
	@echo "  make setup-target      Configura el target ESP32-C6"
	@echo "  make build             Compila el firmware"
	@echo "  make flash             Flashea la ESP32-C6"
	@echo "  make monitor           Abre monitor serial"
	@echo "  make flash-monitor     Flashea y abre monitor"
	@echo "  make menuconfig        Abre configuración ESP-IDF"
	@echo "  make clean             Limpieza normal"
	@echo "  make fullclean         Limpieza completa"
	@echo "  make gui-setup         Crea entorno Python de la GUI"
	@echo "  make gui               Ejecuta la interfaz gráfica"
	@echo "  make ports             Lista puertos seriales disponibles"
	@echo ""
	@echo "Variables opcionales:"
	@echo "  PORT=/dev/ttyACM0"
	@echo "  IDF_PATH=$(HOME)/esp/esp-idf"
	@echo ""

.PHONY: setup-target
setup-target:
	$(call IDF_CMD,set-target $(TARGET))

.PHONY: build
build:
	$(call IDF_CMD,build)

.PHONY: flash
flash:
	$(call IDF_CMD,-p $(PORT) flash)

.PHONY: monitor
monitor:
	$(call IDF_CMD,-p $(PORT) monitor)

.PHONY: flash-monitor
flash-monitor:
	$(call IDF_CMD,-p $(PORT) flash monitor)

.PHONY: menuconfig
menuconfig:
	$(call IDF_CMD,menuconfig)

.PHONY: clean
clean:
	$(call IDF_CMD,clean)

.PHONY: fullclean
fullclean:
	$(call IDF_CMD,fullclean)

# =========================
# GUI Python
# =========================

.PHONY: gui-setup
gui-setup:
	$(PYTHON) -m venv $(GUI_VENV)
	$(GUI_PY) -m pip install --upgrade pip
	$(GUI_PY) -m pip install -r $(GUI_DIR)/requirements.txt

.PHONY: gui
gui:
	cd $(GUI_DIR) && .venv/bin/python main.py

.PHONY: ports
ports:
	@$(PYTHON) -m serial.tools.list_ports
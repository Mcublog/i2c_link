# Каталоги сборки: прошивка (arm-none-eabi) и хост (нативный компилятор)
BUILD_DIR_FW   = build/fw
BUILD_DIR_HOST = build/host

MAKEFLAGS += --no-print-directory

JFLAGS := -j$(shell nproc)

# Тулчейны; можно переопределить:
#   make firmwares TOOLCHAIN_CC=/opt/arm/bin/arm-none-eabi-gcc
TOOLCHAIN_CC  ?= arm-none-eabi-gcc
TOOLCHAIN_CXX ?= arm-none-eabi-g++
HOST_CC       ?= gcc
HOST_CXX      ?= g++

.PHONY: all configure firmwares emuls utils utest hwtests clean

# Правило по умолчанию
all:
	$(MAKE) firmwares
	$(MAKE) emuls
	$(MAKE) utils
	$(MAKE) utest

# Принудительная переконфигурация обеих сборок
configure:
	cmake -S . -B $(BUILD_DIR_FW) -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER=$(TOOLCHAIN_CC) \
		-DCMAKE_CXX_COMPILER=$(TOOLCHAIN_CXX)
	cmake -S . -B $(BUILD_DIR_HOST) -DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_C_COMPILER=$(HOST_CC) \
		-DCMAKE_CXX_COMPILER=$(HOST_CXX)

$(BUILD_DIR_FW)/CMakeCache.txt:
	cmake -S . -B $(BUILD_DIR_FW) -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER=$(TOOLCHAIN_CC) \
		-DCMAKE_CXX_COMPILER=$(TOOLCHAIN_CXX)

$(BUILD_DIR_HOST)/CMakeCache.txt:
	cmake -S . -B $(BUILD_DIR_HOST) -DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_C_COMPILER=$(HOST_CC) \
		-DCMAKE_CXX_COMPILER=$(HOST_CXX)

prepare_fw: $(BUILD_DIR_FW)/CMakeCache.txt
prepare_host: $(BUILD_DIR_HOST)/CMakeCache.txt

firmwares: prepare_fw
	$(MAKE) -C $(BUILD_DIR_FW) fntester_pcb $(JFLAGS)

emuls: prepare_host
	$(MAKE) -C $(BUILD_DIR_HOST) emu $(JFLAGS)

utils: prepare_host
	$(MAKE) -C $(BUILD_DIR_HOST) hello i2c-link-adapter $(JFLAGS)

utest: prepare_host
	$(MAKE) -C $(BUILD_DIR_HOST) utest_all $(JFLAGS)
	ctest --test-dir $(BUILD_DIR_HOST) --output-on-failure

# hwtest: тесты на железе (в сборку не входят)
hwtests:
	@echo "hwtest skipped (not configured; see CMakeLists.txt)"

# Правило для очистки
clean:
	rm -rf build
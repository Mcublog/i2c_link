# Директория для сборки
BUILD_DIR = build

MAKEFLAGS += --no-print-directory

JFLAGS := -j$(shell nproc)

# Правило по умолчанию
all:
	$(MAKE) firmwares
	$(MAKE) emuls
	$(MAKE) utils
	$(MAKE) utest

# Правило для генерации Makefile с помощью CMake и сборки
prepare:
	@if [ -d $(BUILD_DIR) ]; then \
		echo "directory exists"; \
		exit 0; \
	fi

	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER=arm-none-eabi-gcc \
	-DCMAKE_CXX_COMPILER=arm-none-eabi-g++ ..

firmwares: prepare
	cd $(BUILD_DIR) && make fntester_pcb $(JFLAGS)

emuls: prepare
	cd $(BUILD_DIR) && make emu $(JFLAGS)

utils: prepare
	cd $(BUILD_DIR) && make hello $(JFLAGS)

utest_build: prepare
	cd $(BUILD_DIR) && make utest_all $(JFLAGS)

utest: utest_build
	cd $(BUILD_DIR) && ctest --output-on-failure

# hwtest: тесты на железе (не настроены в шаблоне, выключены по умолчанию)
hwtests:
	@echo "hwtest skipped (not configured; add_subdirectory(hwtest) закомментирован)"

# Правило для очистки
clean:
	rm -rf $(BUILD_DIR)

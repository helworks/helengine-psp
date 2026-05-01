HELENGINE_CORE_CPP_ROOT ?=

BUILD_DIR := build
TARGET_PBP := $(BUILD_DIR)/EBOOT.PBP
CMAKE_ARGS :=

ifneq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CMAKE_ARGS += -DHELENGINE_CORE_CPP_ROOT=$(HELENGINE_CORE_CPP_ROOT)
endif

.PHONY: all clean

all: $(TARGET_PBP)

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && psp-cmake $(CMAKE_ARGS) ..

$(TARGET_PBP): $(BUILD_DIR)/CMakeCache.txt
	$(MAKE) -C $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)

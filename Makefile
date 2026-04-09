TOOLCHAIN ?= arm-none-eabi-
CUSTOM_COMPILER_OPTIONS ?=
PATCH_LOAD_ADDR ?= 0x80000000
PATCH_NAME ?= patch
PATCH_IMPL_SRC ?= source/PatchImpl.cpp

CC = $(TOOLCHAIN)gcc
CXX = $(TOOLCHAIN)g++
AS = $(TOOLCHAIN)gcc
LD = $(CXX)
OBJCOPY = $(TOOLCHAIN)objcopy

BUILD_DIR := build

COMMON_FLAGS := -Wall -Wextra -Werror -fno-builtin -fno-common -ffunction-sections -fdata-sections \
                -fsingle-precision-constant -g -mfloat-abi=hard -mthumb \
                -fstack-usage -specs=nano.specs -Wdouble-promotion -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 \
                -mfp16-format=ieee -O3 -DNDEBUG

CFLAGS := $(COMMON_FLAGS) -std=c11 $(CUSTOM_COMPILER_OPTIONS)
CXXFLAGS := $(COMMON_FLAGS) -std=c++20 -includecstdint -felide-constructors -Wno-psabi -fno-exceptions -fno-rtti $(CUSTOM_COMPILER_OPTIONS)

INCLUDES := -I source

SRC_C := $(wildcard internal/*.c)
SRC_CPP := $(filter-out source/PatchImpl.cpp,$(wildcard source/*.cpp))
SRC_CPP += $(PATCH_IMPL_SRC)
SRC_CPP += $(wildcard internal/*.cpp)
OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC_C)) $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRC_CPP))

LDFLAGS = -nostartfiles --specs=nosys.specs -Wl,-gc-sections -T internal/patch_imx.ld \
           -Wl,--defsym=PATCH_LOAD_ADDR=$(PATCH_LOAD_ADDR) -Wl,--defsym=end=__patch_bss_end

LIBS := -Wl,--start-group -lstdc++ -lc -lm -lgcc -Wl,--end-group

ifeq ($(OS),Windows_NT)
	# Windows specific variables
    RM = rmdir /s /q
    MKDIR = mkdir
    PATCH_TIMESTAMP := $(shell powershell -Command "Get-Date -Format 'yyyyMMdd_HHmmss'")
    FIX_PATH = $(subst /,\,$1)
    NULL_DEVICE = NUL
else
    # Linux/OSX specific variables
    RM = rm -rf
    MKDIR = mkdir -p
    PATCH_TIMESTAMP := $(shell date +"%Y%m%d_%H%M%S")
    FIX_PATH = $1
    NULL_DEVICE = /dev/null
endif

PATCH_ELF := $(BUILD_DIR)/$(PATCH_NAME).elf
PATCH_BIN ?= $(BUILD_DIR)/$(PATCH_NAME)_$(PATCH_TIMESTAMP).endl

all: $(PATCH_BIN)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@$(MKDIR) $(call FIX_PATH,$(dir $@)) 2>$(NULL_DEVICE) || exit 0
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@$(MKDIR) $(call FIX_PATH,$(dir $@)) 2>$(NULL_DEVICE) || exit 0
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(PATCH_ELF): $(OBJ) internal/patch_imx.ld | $(BUILD_DIR)
	$(LD) $(CXXFLAGS) $(OBJ) -o $@ $(LDFLAGS) $(LIBS)

$(PATCH_BIN): $(PATCH_ELF) | $(BUILD_DIR)
	@$(MKDIR) $(call FIX_PATH,$(dir $@)) 2>$(NULL_DEVICE) || exit 0
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR):
	@$(MKDIR) $(call FIX_PATH,$(BUILD_DIR)) 2>$(NULL_DEVICE) || exit 0

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(BUILD_DIR) $(RM) $(BUILD_DIR)
else
	$(RM) $(BUILD_DIR)
endif

.PHONY: all clean

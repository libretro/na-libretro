ifndef ANDROID_NDK_ROOT
ifneq ($(MAKECMDGOALS),clean)
$(error ANDROID_NDK_ROOT is not set. Point it at your Android NDK install)
endif
endif

ifndef platform
ifneq ($(MAKECMDGOALS),clean)
$(error platform is not set. Use platform=android-x86_64 or platform=arm)
endif
endif

HOST_TAG  ?= linux-x86_64
MIN_API   ?= 21
TOOLCHAIN := $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/$(HOST_TAG)/bin

CORENAME ?= na
SRC      := core.c
TARGET   := $(CORENAME)_libretro_android.so

CFLAGS  := -shared -fPIC -O2 -Wall -Wextra

ifneq ($(MAKECMDGOALS),clean)
ifeq ($(platform),android-x86_64)
    CC := $(TOOLCHAIN)/x86_64-linux-android$(MIN_API)-clang
else ifeq ($(platform),android-x86)
    CC := $(TOOLCHAIN)/i686-linux-android$(MIN_API)-clang
else ifeq ($(platform),android-arm)
    CC := $(TOOLCHAIN)/armv7a-linux-androideabi$(MIN_API)-clang
else ifeq ($(platform),android-arm64)
    CC := $(TOOLCHAIN)/aarch64-linux-android$(MIN_API)-clang
else
    $(error Unsupported platform '$(platform)'. Use platform=android-x86_64 or platform=arm)
endif
endif
# ------------------------------------------------------------------------

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	rm -f $(CORENAME)_libretro_android.so

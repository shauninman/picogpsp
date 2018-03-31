LOCAL_PATH := $(call my-dir)

CORE_CFLAGS  :=
CORE_LDLIBS  :=
CPU_ARCH     :=
HAVE_DYNAREC :=
SOURCES_C    :=
SOURCES_ASM  :=

CORE_DIR     := ..

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
   CORE_CFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

ifeq ($(TARGET_ARCH),arm)
   CORE_CFLAGS += -DARM_ARCH -DARM_MEMORY_DYNAREC
   CPU_ARCH := arm
   HAVE_DYNAREC := 1
endif

ifeq ($(TARGET_ARCH),x86)
   CORE_CFLAGS += -DHAVE_MMAP
   CPU_ARCH := x86_32
   HAVE_DYNAREC := 1
endif

ifeq ($(HAVE_DYNAREC),1)
  CORE_CFLAGS += -DHAVE_DYNAREC
  CORE_LDLIBS += -Wl,--no-warn-shared-textrel
endif

include $(CORE_DIR)/Makefile.common

CORE_CFLAGS += -DINLINE=inline -D__LIBRETRO__ -DFRONTEND_SUPPORTS_RGB565 $(INCFLAGS)

include $(CLEAR_VARS)
LOCAL_MODULE    := retro
LOCAL_SRC_FILES := $(SOURCES_C) $(SOURCES_ASM)
LOCAL_CFLAGS    := $(CORE_CFLAGS)
LOCAL_LDLIBS    := $(CORE_LDLIBS)
LOCAL_ARM_MODE  := arm
include $(BUILD_SHARED_LIBRARY)

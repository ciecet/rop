$(call begin-target, rop, js)
    LOCAL_JS_DIR := $(LOCAL_PATH)/js
$(end-target)

$(call begin-target, rop, nodejs)
    LOCAL_SRCS := $(shell find $(LOCAL_PATH)/node -type f -name *.js)
    LOCAL_SRCS += $(LOCAL_PATH)/js/rop_base.js
$(end-target)

$(call begin-target, rop, sharedlib)
    ifndef AL_CF_ROP_USE_CPP_RUNTIME
        LOCAL_LABELS := OPTIONAL
    endif

    LOCAL_SRCS := $(wildcard $(patsubst %, $(LOCAL_PATH)/%/*.c, cpp cpp/ed))
    LOCAL_SRCS += $(wildcard $(patsubst %, $(LOCAL_PATH)/%/*.cpp, cpp cpp/ed))
    LOCAL_CFLAGS := -I$(LOCAL_PATH)/cpp -I$(LOCAL_PATH)/cpp/ed

    ifdef AL_CF_ROP_USE_REMOTEMAP
        LOCAL_SRCS += $(wildcard $(LOCAL_PATH)/remotemap/cpp/*.cpp)
        LOCAL_CFLAGS += -I$(LOCAL_PATH)/remotemap/cpp
        LOCAL_CFLAGS += -I$(TARGET_BIN_DIR)/rop/cpp
        LOCAL_OBJDEPS := $(TARGET_ROP)
    endif

    ifdef AL_CF_ROP_DEBUG
        LOCAL_CFLAGS += -DROP_DEBUG=1
    else
        LOCAL_CFLAGS += -DROP_DEBUG=0
    endif

    ifdef AL_CF_ROP_USE_EVENTFD
        LOCAL_CFLAGS += -DROP_USE_EVENTFD=1
    else
        LOCAL_CFLAGS += -DROP_USE_EVENTFD=0
    endif

    ifdef AL_CF_ANDROID_TARGET_OPENSSL
        LOCAL_CFLAGS += -I$(BUILD_ROOT)/android/$(AL_CF_ANDROID_INCLUDE_OPENSSL)
        LOCAL_TMP := $(TARGET_BIN_DIR)/android/target/product/alticaptor/obj/SHARED_LIBRARIES
        LOCAL_LDFLAGS += $(LOCAL_TMP)/libssl_intermediates/LINKED/libssl.so
        LOCAL_LDFLAGS += $(LOCAL_TMP)/libcrypto_intermediates/LINKED/libcrypto.so
    else
        LOCAL_LDFLAGS += -lrt -lssl -lcrypto -lpthread
    endif

    LOCAL_CXXFLAGS := $(LOCAL_CFLAGS)
$(end-target)

$(call begin-target, remotemap, executable)
    ifndef AL_CF_ROP_USE_REMOTEMAP
        LOCAL_LABELS := OPTIONAL
    endif

    LOCAL_SRCS := $(wildcard $(LOCAL_PATH)/remotemap/server/*.cpp)
    LOCAL_CXXFLAGS := -I$(LOCAL_PATH)/cpp -I$(LOCAL_PATH)/cpp/ed
    LOCAL_CXXFLAGS += -I$(TARGET_BIN_DIR)/rop/cpp
    LOCAL_SHAREDLIBS += rop
    LOCAL_OBJDEPS := $(TARGET_ROP)

    ifdef AL_CF_ROP_DEBUG
        LOCAL_CXXFLAGS += -DROP_DEBUG=1
    else
        LOCAL_CXXFLAGS += -DROP_DEBUG=0
    endif
$(end-target)

# NOTE: cannot modify LOCAL_LABELS in case of js
ifdef AL_CF_ROP_USE_REMOTEMAP
$(call begin-target, remotemap, js)
    LOCAL_JS_DIR := $(LOCAL_PATH)/remotemap/js
$(end-target)
endif

$(call begin-target, remotemap, nodejs)
    ifndef AL_CF_ROP_USE_REMOTEMAP
        LOCAL_LABELS := OPTIONAL
    endif
    LOCAL_SRCS := $(shell find $(LOCAL_PATH)/remotemap/node -type f -name *.js)
$(end-target)

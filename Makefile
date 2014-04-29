#-----------------------------------------------------------------
#          Makefile for ROP Event Driver
#-----------------------------------------------------------------

BUILD_ROOT      = ../..

include $(BUILD_ROOT)/etc/common.mk
include $(MAKEFILE_ROOT)/module.mk

PACKAGE         = rop
CHILD_DIRS      =
EXPORT_FILES    =
INCLUDE_DIRS    += cpp cpp/ed
SRC_DIRS        += cpp/ed

ifdef AL_CF_ROP_USE_CPP_RUNTIME
SRC_DIRS += cpp
endif

ifdef AL_CF_ROP_DEBUG
DEFINES += ROP_DEBUG=1
else
DEFINES += ROP_DEBUG=0
endif

ifdef AL_CF_ROP_USE_EVENTFD
DEFINES += ROP_USE_EVENTFD=1
else
DEFINES += ROP_USE_EVENTFD=0
endif

DEFINES += ROP_THREAD_LOCAL_IMPL=$(AL_CF_ROP_THREAD_LOCAL_IMPL)

target  : export_header classes all 

ifdef AL_CF_ROP_USE_MANGLED_ED_MZ
ED_MANGLE_PREFIX = MZ
ED_SRCS = cpp/HttpReactor.cpp cpp/HttpTransport.cpp cpp/SocketTransport.cpp cpp/ed-mz/EventDriver.cpp
ED_MANGLED_DIR = $(TARGET_SRC_DIR)/ed-$(ED_MANGLE_PREFIX)
$(shell mkdir -p $(ED_MANGLED_DIR))
$(shell for src in $(ED_SRCS); do cp -u $$src $(ED_MANGLED_DIR)/$(ED_MANGLE_PREFIX)$$(basename $$src); done)
SRC_DIRS += $(ED_MANGLED_DIR)
$(TARGET_OBJ_DIR)/$(ED_MANGLE_PREFIX)%$(OBJ_EXT) : INCLUDE_DIRS := cpp/ed-mz $(INCLUDE_DIRS) $(EXTERNAL_INCLUDES)
$(TARGET_OBJ_DIR)/$(ED_MANGLE_PREFIX)%$(OBJ_EXT) : DEFINES += ED_MANGLE_PREFIX=MZ
endif

include $(MAKEFILE_ROOT)/java.mk
include $(MAKEFILE_ROOT)/jni.mk
include $(MAKEFILE_ROOT)/rule.mk

all     : $(TARGET_OBJ_DIR) $(TARGET_OBJS) $(TARGET_LIB_DIR) $(TARGET_LIB) 

clean   : clean_classes
	@ $(RM) $(TARGET_OBJ_DIR) $(TARGET_LIB)

SRCS = $(shell find java -name \*.java)
test:
	mkdir -p out
	javac -d out $(SRCS)

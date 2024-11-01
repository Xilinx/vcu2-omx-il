THIS.core:=$(call get-my-dir)

include $(THIS.core)/core_version.mk
LIB_OMX_CORE_NAME:=libOMX.allegro.core.so
LIB_OMX_CORE:=$(BIN)/$(LIB_OMX_CORE_NAME)

OMX_CORE_SRCS:=\
               $(THIS.core)/omx_core.cpp\

OMX_CORE_OBJ:=$(OMX_CORE_SRCS:%=$(BIN)/%.o)
OMX_CORE_OBJ+=$(UTILITY_SRCS:%=$(BIN)/%.o)

OMX_CORE_CFLAGS:=$(DEFAULT_CFLAGS)
OMX_CORE_CFLAGS+=-fPIC
OMX_CORE_CFLAGS+=-pthread
OMX_CORE_LDFLAGS:=$(DEFAULT_LDFLAGS)
OMX_CORE_LDFLAGS+=-ldl
OMX_CORE_LDFLAGS+=-lpthread

$(LIB_OMX_CORE): $(OMX_CORE_OBJ)
$(LIB_OMX_CORE): CFLAGS:=$(OMX_CORE_CFLAGS)
$(LIB_OMX_CORE): LDFLAGS:=$(OMX_CORE_LDFLAGS)
$(LIB_OMX_CORE): MAJOR:=$(CORE_MAJOR)
$(LIB_OMX_CORE): VERSION:=$(CORE_VERSION)

core: $(LIB_OMX_CORE)

.PHONY:core
TARGETS+=core

UNITTESTS+=$(OMX_CORE_SRCS)
UNITTESTS+=$(shell find $(THIS.core)/unittests -name "*.cpp")

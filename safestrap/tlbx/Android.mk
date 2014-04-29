LOCAL_PATH:= system/core/toolbox/
include $(CLEAR_VARS)

SS_TOOLS := \
	stop \
	getprop \
	setprop


LOCAL_SRC_FILES := \
	dynarray.c \
	toolbox.c \
	$(patsubst %,%.c,$(SS_TOOLS))

LOCAL_C_INCLUDES := bionic/libc/bionic

LOCAL_STATIC_LIBRARIES := \
	libcutils \
	liblog \
	libc

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_OUT)/../2nd-init-files
LOCAL_MODULE := tlbx

LOCAL_FORCE_STATIC_EXECUTABLE := true

# Including this will define $(intermediates).
#
include $(BUILD_EXECUTABLE)

$(LOCAL_PATH)/toolbox.c: $(intermediates)/tools.h

TOOLS_H := $(intermediates)/tools.h
$(TOOLS_H): PRIVATE_TOOLS := $(SS_TOOLS)
$(TOOLS_H): PRIVATE_CUSTOM_TOOL = echo "/* file generated automatically */" > $@ ; for t in $(PRIVATE_TOOLS) ; do echo "TOOL($$t)" >> $@ ; done
$(TOOLS_H): $(LOCAL_PATH)/Android.mk
$(TOOLS_H):
	$(transform-generated-source)



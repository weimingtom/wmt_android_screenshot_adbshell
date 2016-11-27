LOCAL_PATH := $(call my-dir)

#===================================
include $(CLEAR_VARS)

LOCAL_MODULE := gui

LOCAL_SRC_FILES := fake_libgui.cpp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH) 
LOCAL_C_INCLUDES := $(LOCAL_PATH) 

LOCAL_LDLIBS := -lGLESv2 -llog -lz
LOCAL_EXPORT_LDLIBS := -lGLESv2 -llog -lz

#LOCAL_WHOLE_STATIC_LIBRARIES := 

LOCAL_CFLAGS := 
LOCAL_CFLAGS += -fPIE
LOCAL_LDFLAGS += -fPIE
#LOCAL_EXPORT_CFLAGS := 

include $(BUILD_SHARED_LIBRARY)
#===================================
include $(CLEAR_VARS)

LOCAL_MODULE := binder

LOCAL_SRC_FILES := fake_libbinder.cpp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH) 
LOCAL_C_INCLUDES := $(LOCAL_PATH) 

LOCAL_LDLIBS := -lGLESv2 -llog -lz
LOCAL_EXPORT_LDLIBS := -lGLESv2 -llog -lz

#LOCAL_WHOLE_STATIC_LIBRARIES := 

LOCAL_CFLAGS := 
LOCAL_CFLAGS += -fPIE
LOCAL_LDFLAGS += -fPIE
#LOCAL_EXPORT_CFLAGS := 

include $(BUILD_SHARED_LIBRARY)
#===================================
include $(CLEAR_VARS)

LOCAL_MODULE := mycap

LOCAL_SRC_FILES := mycap.cpp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH) 
LOCAL_C_INCLUDES := $(LOCAL_PATH) 

LOCAL_LDLIBS := -lGLESv2 -llog -lz
LOCAL_EXPORT_LDLIBS := -lGLESv2 -llog -lz

#LOCAL_WHOLE_STATIC_LIBRARIES := 

LOCAL_CFLAGS := 
LOCAL_CFLAGS += -pie -fPIE -Xlinker -rpath=/system/lib
LOCAL_LDFLAGS += -pie -fPIE -Xlinker -rpath=/system/lib

#LOCAL_EXPORT_CFLAGS := 

include $(BUILD_EXECUTABLE)
#===================================
include $(CLEAR_VARS)

LOCAL_MODULE := mycap-ics

LOCAL_SRC_FILES := mycap.cpp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH) 
LOCAL_C_INCLUDES := $(LOCAL_PATH) 

LOCAL_LDLIBS := -lGLESv2 -llog -lz
LOCAL_EXPORT_LDLIBS := -lGLESv2 -llog -lz

#LOCAL_WHOLE_STATIC_LIBRARIES := 

LOCAL_CFLAGS := -DTARGET_ICS
LOCAL_CFLAGS += -pie -fPIE -Xlinker -rpath=/system/lib
LOCAL_LDFLAGS += -pie -fPIE -Xlinker -rpath=/system/lib

#LOCAL_EXPORT_CFLAGS := 
LOCAL_SHARED_LIBRARIES := gui binder

include $(BUILD_EXECUTABLE)
#===================================
include $(CLEAR_VARS)

LOCAL_MODULE := mycap-jb

LOCAL_SRC_FILES := mycap.cpp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH) 
LOCAL_C_INCLUDES := $(LOCAL_PATH) 

LOCAL_LDLIBS := -lGLESv2 -llog -lz
LOCAL_EXPORT_LDLIBS := -lGLESv2 -llog -lz

#LOCAL_WHOLE_STATIC_LIBRARIES := 

LOCAL_CFLAGS := -DTARGET_JB
LOCAL_CFLAGS += -pie -fPIE -Xlinker -rpath=/system/lib
LOCAL_LDFLAGS += -pie -fPIE -Xlinker -rpath=/system/lib

#LOCAL_EXPORT_CFLAGS := 
LOCAL_SHARED_LIBRARIES := gui binder

include $(BUILD_EXECUTABLE)
#===================================
include $(CLEAR_VARS)

LOCAL_MODULE := mysys

LOCAL_SRC_FILES := mysys.cpp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH) 
LOCAL_C_INCLUDES := $(LOCAL_PATH) 

LOCAL_LDLIBS := -lGLESv2 -llog -lz
LOCAL_EXPORT_LDLIBS := -lGLESv2 -llog -lz

LOCAL_SHARED_LIBRARIES :=

include $(BUILD_EXECUTABLE)
#===================================
include $(CLEAR_VARS)

LOCAL_MODULE := myjni

LOCAL_SRC_FILES := myjni.c

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH) 
LOCAL_C_INCLUDES := $(LOCAL_PATH) 

LOCAL_LDLIBS := -lGLESv2 -llog -lz
LOCAL_EXPORT_LDLIBS := -lGLESv2 -llog -lz

LOCAL_SHARED_LIBRARIES :=

include $(BUILD_SHARED_LIBRARY)
#===================================

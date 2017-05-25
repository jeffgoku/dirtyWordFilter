LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := bong
LOCAL_SRC_FILES := ../../wordFilter.cpp

LOCAL_C_INCLUDES := ../..
LOCAL_CPPFLAGS := -std=c++11

include $(BUILD_SHARED_LIBRARY)

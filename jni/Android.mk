LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true
TARGET_ARCH_ABI := armeabi-v7a

LOCAL_MODULE := vorbis-jni
LOCAL_SRC_FILES := bitwise.c framing.c analysis.c bitrate.c block.c codebook.c envelope.c floor0.c floor1.c info.c lookup.c lpc.c lsp.c mapping0.c mdct.c psy.c registry.c res0.c sharedbook.c smallft.c synthesis.c tone.c vorbisenc.c vorbisfile.c window.c jnilibvorbis.c  
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)
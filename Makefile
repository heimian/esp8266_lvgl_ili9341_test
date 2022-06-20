#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
PROJECT_NAME := tft_test

CFLAGS += -DLV_CONF_INCLUDE_SIMPLE

INCLUDES += -I app/lv_examples

EXTRA_COMPONENT_DIRS = $(IDF_PATH)/examples/common_components/protocol_examples_common

include $(IDF_PATH)/make/project.mk

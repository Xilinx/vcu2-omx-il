THIS.module_codec:=$(call get-my-dir)

MODULE_CODEC_SRCS+=\
                    $(THIS.module_codec)/convert_module_soft.cpp\
                    $(THIS.module_codec)/convert_module_soft_avc.cpp\
                    $(THIS.module_codec)/convert_module_soft_hevc.cpp\
                    $(THIS.module_codec)/settings_interface.cpp\
                    $(THIS.module_codec)/settings_checks.cpp\
                    $(THIS.module_codec)/settings_codec_itu.cpp\
                    $(THIS.module_codec)/settings_codec_avc.cpp\
                    $(THIS.module_codec)/settings_codec_hevc.cpp\
                    $(THIS.module_codec)/settings_dummy.cpp\
                    $(THIS.module_codec)/module_interface.cpp\
                    $(THIS.module_codec)/module_dummy.cpp\
                    $(THIS.module_codec)/buffer_handle_interface.cpp\

    MODULE_CODEC_SRCS+= $(THIS.module_codec)/convert_module_soft_mjpeg.cpp

UNITTESTS+=$(shell find $(THIS.module_codec)/unittests -name "*.cpp")
UNITTESTS+=$(MODULE_CODEC_SRCS)

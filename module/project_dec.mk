THIS.module_dec:=$(call get-my-dir)

MODULE_DEC_SRCS+=\
                 $(THIS.module_dec)/settings_dec_avc.cpp\
                 $(THIS.module_dec)/settings_dec_hevc.cpp\
                 $(THIS.module_dec)/settings_dec_itu.cpp\
                 $(THIS.module_dec)/convert_module_soft_dec.cpp\
                 $(THIS.module_dec)/module_dec.cpp\
                 $(THIS.module_dec)/device_dec_interface.cpp\


ifeq ($(ENABLE_OMX_MCU), 1)
endif

ifeq ($(ENABLE_VCU),0)
endif

# TODO: SINCE THE UNIFDEF DOESN'T EXIST, THIS ifeq SHOULD NOT BE REMOVED NOR ITS CONTENT
  MODULE_DEC_SRCS+=$(THIS.module_dec)/device_dec_hardware_riscv.cpp

    MODULE_DEC_SRCS+=$(THIS.module_dec)/settings_dec_mjpeg.cpp

UNITTESTS+=$(MODULE_DEC_SRCS)

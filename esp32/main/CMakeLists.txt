idf_component_register(SRCS "main.c;usb_esp32sx.c;../../common/device.c"
                    INCLUDE_DIRS "../../common")

# No high speed on esp32
target_compile_definitions(${COMPONENT_LIB} PRIVATE NO_HIGH_SPEED)

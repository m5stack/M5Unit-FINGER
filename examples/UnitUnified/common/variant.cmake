# Map the Kconfig "Target unit / board" choice (see common/Kconfig.variant) to the source-level
# USING_* macro that both the Arduino and ESP-IDF builds use. Include this from an example's
# main/CMakeLists.txt *after* idf_component_register() (it needs ${COMPONENT_LIB}).
# Default (nothing selected): UnitFinger (UART / GROVE).
if(CONFIG_EXAMPLE_USING_HAT_FINGER)
    set(M5UNIT_VARIANT USING_HAT_FINGER)
elseif(CONFIG_EXAMPLE_USING_FACES_FINGER)
    set(M5UNIT_VARIANT USING_FACES_FINGER)
else()
    set(M5UNIT_VARIANT USING_UNIT_FINGER)
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${M5UNIT_VARIANT})

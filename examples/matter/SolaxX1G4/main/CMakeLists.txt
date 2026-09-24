idf_component_register(SRCS "solax.cpp" "modbus.cpp" "status_led.cpp" "electrical_power_measurement_delegate.cpp" "main.cpp"
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

idf_component_register(SRCS "kronoterm.cpp" "water_heater_delegates.cpp" "modbus.cpp" "status_led.cpp" "electrical_power_measurement_delegate.cpp" "main.cpp"
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

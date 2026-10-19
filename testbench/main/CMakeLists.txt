idf_component_register(
  SRCS "testbench.cpp" "../../blockcode/firmware/main/hal/platform.cpp" "../../blockcode/firmware/main/hal/motors.cpp"
  INCLUDE_DIRS "." "../../blockcode/firmware/main"
  REQUIRES u8g2 esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_driver_uart esp_adc esp_timer
           esp_wifi esp_netif esp_event nvs_flash esp_rom vfs
)

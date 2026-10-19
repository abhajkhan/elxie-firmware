idf_component_register(
  SRCS
    "main.cpp" "app.cpp" "robot_host.cpp"
    "hal/platform.cpp" "hal/motors.cpp" "hal/sensor_hub.cpp"
    "services/motion.cpp" "services/sound.cpp" "services/lights.cpp" "services/face.cpp"
    "services/remote_link.cpp" "services/storage.cpp"
    "link/protocol.cpp" "link/link.cpp" "link/ble_transport.cpp" "link/uart_transport.cpp"
  INCLUDE_DIRS "."
  REQUIRES elx_vm u8g2 esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_driver_uart
           esp_adc esp_timer esp_wifi esp_netif esp_event nvs_flash bt vfs
)

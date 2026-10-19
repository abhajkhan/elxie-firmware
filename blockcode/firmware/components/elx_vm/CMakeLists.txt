# Portable bytecode VM (no ESP-IDF dependencies). Also built for PC tests and WebAssembly.
idf_component_register(SRCS "vm.cpp" "program.cpp"
                       INCLUDE_DIRS "include")

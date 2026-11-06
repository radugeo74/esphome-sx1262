idf_component_register(SRCS "main.cpp" "kamwater_nvs_checkpoint.cpp"
    INCLUDE_DIRS "." "../../portable_cpp" "../../generated" "../../../../components/wmbus_native_sx126x"
    REQUIRES log esp_system heap mbedtls nvs_flash)

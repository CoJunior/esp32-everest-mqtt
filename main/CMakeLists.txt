idf_component_register(
     SRCS "main.c"
    INCLUDE_DIRS "."
    EMBED_FILES "web/index.html"
    REQUIRES
        esp_http_server
        esp_netif
        esp_wifi
        esp_event
        mqtt
        nvs_flash
)
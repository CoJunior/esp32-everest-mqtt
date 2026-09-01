#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

/* ============================================================
 * CONFIGURAÇÃO WI-FI
 * ============================================================ */

#define WIFI_SSID      "Nome_da_rede"
#define WIFI_PASSWORD  "Senha_da_rede"

/*
 * IP do computador Ubuntu onde estão:
 * - Mosquitto
 * - EVerest
 *
 * NÃO usar localhost.
 */

#define MQTT_BROKER_URI "mqtt://192.168.1.40:1883"

/* ============================================================
 * TÓPICOS MQTT DO ESP32
 * ============================================================ */

#define MQTT_TOPIC_ESP32_STATUS \
    "esp32/ev/status"

#define MQTT_TOPIC_ESP32_COMMAND \
    "esp32/ev/comando"

#define MQTT_TOPIC_ESP32_FAULT \
    "esp32/ev/falha"

/* ============================================================
 * TÓPICOS MQTT DO EVEREST
 * ============================================================ */

#define MQTT_TOPIC_EVEREST_ENABLE \
    "everest_external/nodered/1/carsim/cmd/enable"

#define MQTT_TOPIC_EVEREST_SESSION \
    "everest_external/nodered/1/carsim/cmd/execute_charging_session"

#define MQTT_TOPIC_EVEREST_SESSION_INFO \
    "everest_api/connector_1/var/session_info"

/* ============================================================
 * CONFIGURAÇÃO GERAL
 * ============================================================ */

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILURE_BIT   BIT1
#define MAXIMUM_RETRY      10

#define MQTT_DATA_BUFFER_SIZE 2048

static const char *TAG = "ESP32_EVEREST";

static EventGroupHandle_t wifi_event_group;

static int wifi_retry_count = 0;

static esp_mqtt_client_handle_t mqtt_client = NULL;

static bool mqtt_connected = false;
static bool mqtt_simulated_failure = false;
static bool mqtt_service_started = false;
static bool vehicle_simulated_failure = false;

/* ============================================================
 * ESTADO RECEBIDO DO EVEREST
 * ============================================================ */

static char ev_state[32] = "Unknown";

static double charged_energy_wh = 0.0;

static double latest_total_w = 0.0;

static int charging_duration_s = 0;

static bool permanent_fault = false;

/* ============================================================
 * DECLARAÇÕES
 * ============================================================ */

static void start_mqtt(void);

static void everest_connect_vehicle(void);

static void everest_disconnect_vehicle(void);

static void publish_esp32_status(
    const char *estado
);

static void publish_vehicle_fault_status(void);

static void process_everest_session_info(
    const char *json
);

/* ============================================================
 * SERVIDOR WEB DO ESP32
 * ============================================================ */

static httpd_handle_t web_server = NULL;
static char web_html_buffer[6144];

/* ------------------------------------------------------------
 * Redirecionar para a página principal
 * ------------------------------------------------------------ */

static esp_err_t web_redirect_home(
    httpd_req_t *req
)
{
    httpd_resp_set_status(
        req,
        "303 See Other"
    );

    httpd_resp_set_hdr(
        req,
        "Location",
        "/"
    );

    return httpd_resp_send(
        req,
        NULL,
        0
    );
}

/* ------------------------------------------------------------
 * Página principal
 * ------------------------------------------------------------ */

static esp_err_t web_root_handler(
    httpd_req_t *req
)
{
    const char *mqtt_status;
    const char *mqtt_class;
    const char *mqtt_failure_status;
    const char *vehicle_failure_status;
    const char *vehicle_failure_class;

    if (mqtt_simulated_failure)
    {
        mqtt_status = "FALHA SIMULADA";
        mqtt_class = "danger";
        mqtt_failure_status = "Ativa";
    }
    else if (mqtt_connected)
    {
        mqtt_status = "CONECTADO";
        mqtt_class = "success";
        mqtt_failure_status = "Nenhuma";
    }
    else
    {
        mqtt_status = "RECONECTANDO";
        mqtt_class = "warning";
        mqtt_failure_status = "Nenhuma";
    }

    if (vehicle_simulated_failure)
    {
        vehicle_failure_status = "FALHA ATIVA";
        vehicle_failure_class = "danger";
    }
    else
    {
        vehicle_failure_status = "NORMAL";
        vehicle_failure_class = "success";
    }

    snprintf(
        web_html_buffer,
        sizeof(web_html_buffer),
        "<!DOCTYPE html>"
        "<html lang=\"pt-BR\">"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "<meta http-equiv=\"refresh\" content=\"3\">"
        "<title>ESP32 + EVerest</title>"
        "<style>"
        "body{font-family:Arial,sans-serif;background:#f4f6f8;margin:0;padding:20px;"
        "display:flex;justify-content:center;align-items:center;min-height:100vh;box-sizing:border-box;}"
        ".container{background:white;width:100%%;max-width:680px;padding:30px;"
        "border-radius:12px;box-shadow:0 4px 15px rgba(0,0,0,.15);text-align:center;}"
        "h1{margin:0 0 8px;}"
        ".subtitle{margin:0 0 24px;color:#555;}"
        ".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin:20px 0;}"
        ".card{background:#f7f8fa;padding:16px;border-radius:9px;text-align:left;}"
        ".label{font-size:13px;color:#666;margin-bottom:7px;}"
        ".value{font-weight:bold;font-size:17px;word-break:break-word;}"
        ".success{color:#1b7f3a;}"
        ".danger{color:#b42318;}"
        ".warning{color:#a15c00;}"
        ".actions{margin-top:25px;padding-top:20px;border-top:1px solid #ddd;}"
        ".btn{display:block;text-decoration:none;padding:14px;margin:12px 0;border-radius:8px;"
        "font-weight:bold;transition:.2s;}"
        ".btn-danger{background:#b42318;color:white;}"
        ".btn-danger:hover{background:#8f1c13;}"
        ".btn-success{background:#1b7f3a;color:white;}"
        ".btn-success:hover{background:#13652d;}"
        ".note{font-size:13px;color:#666;line-height:1.5;margin-top:18px;}"
        "@media(max-width:560px){.grid{grid-template-columns:1fr;}.container{padding:20px;}}"
        "</style>"
        "</head>"
        "<body>"
        "<div class=\"container\">"
        "<h1>ESP32 + EVerest</h1>"
        "<p class=\"subtitle\">Interface de simulação de cenários</p>"
        "<div class=\"grid\">"
        "<div class=\"card\"><div class=\"label\">Wi-Fi</div>"
        "<div class=\"value success\">CONECTADO</div></div>"
        "<div class=\"card\"><div class=\"label\">Servidor HTTP</div>"
        "<div class=\"value success\">ATIVO</div></div>"
        "<div class=\"card\"><div class=\"label\">MQTT do ESP32</div>"
        "<div class=\"value %s\">%s</div></div>"
        "<div class=\"card\"><div class=\"label\">Estado EVerest</div>"
        "<div class=\"value\">%s</div></div>"
        "<div class=\"card\"><div class=\"label\">Falha MQTT simulada</div>"
        "<div class=\"value\">%s</div></div>"
        "<div class=\"card\"><div class=\"label\">Falha do veículo simulada</div>"
        "<div class=\"value %s\">%s</div></div>"
        "<div class=\"card\"><div class=\"label\">Potência EVerest</div>"
        "<div class=\"value\">%.2f W</div></div>"
        "<div class=\"card\"><div class=\"label\">Falha permanente EVerest</div>"
        "<div class=\"value\">%s</div></div>"
        "</div>"
        "<div class=\"actions\">"
        "<h2>Cenário MQTT</h2>"
        "<a class=\"btn btn-danger\" href=\"/mqtt/falha\">SIMULAR FALHA MQTT</a>"
        "<a class=\"btn btn-success\" href=\"/mqtt/restaurar\">RESTAURAR MQTT</a>"
        "</div>"
        "<p class=\"note\">A falha MQTT interrompe somente a comunicação deste ESP32. "
        "O broker Mosquitto e o EVerest permanecem em execução.</p>"
        "<div class=\"actions\">"
        "<h2>Cenário do Veículo</h2>"
        "<a class=\"btn btn-danger\" href=\"/veiculo/falha\">SIMULAR FALHA DO VEÍCULO</a>"
        "<a class=\"btn btn-success\" href=\"/veiculo/restaurar\">RESTAURAR VEÍCULO</a>"
        "</div>"
        "<p class=\"note\">Esta é uma falha injetada pelo simulador ESP32 e não altera "
        "o campo permanent_fault real recebido do EVerest.</p>"
        "</div>"
        "</body>"
        "</html>",
        mqtt_class,
        mqtt_status,
        ev_state,
        mqtt_failure_status,
        vehicle_failure_class,
        vehicle_failure_status,
        latest_total_w,
        permanent_fault ? "SIM" : "NAO"
    );

    httpd_resp_set_type(
        req,
        "text/html; charset=utf-8"
    );

    return httpd_resp_send(
        req,
        web_html_buffer,
        HTTPD_RESP_USE_STRLEN
    );
}

/* ------------------------------------------------------------
 * Simular falha MQTT do ESP32
 * ------------------------------------------------------------ */

static esp_err_t web_mqtt_failure_handler(
    httpd_req_t *req
)
{
    ESP_LOGW(
        TAG,
        "============================================"
    );

    ESP_LOGW(
        TAG,
        "SIMULACAO DE FALHA MQTT SOLICITADA PELA WEB"
    );

    if (
        mqtt_client != NULL &&
        mqtt_service_started &&
        !mqtt_simulated_failure
    )
    {
        mqtt_simulated_failure = true;
        mqtt_connected = false;

        esp_err_t result =
            esp_mqtt_client_stop(
                mqtt_client
            );

        if (result == ESP_OK)
        {
            mqtt_service_started = false;

            ESP_LOGW(
                TAG,
                "Cliente MQTT do ESP32 foi interrompido."
            );
        }
        else
        {
            mqtt_simulated_failure = false;

            ESP_LOGE(
                TAG,
                "Falha ao interromper MQTT: %s",
                esp_err_to_name(result)
            );
        }
    }
    else
    {
        ESP_LOGW(
            TAG,
            "MQTT já está parado ou a falha já está ativa."
        );
    }

    ESP_LOGW(
        TAG,
        "============================================"
    );

    return web_redirect_home(
        req
    );
}

/* ------------------------------------------------------------
 * Restaurar MQTT do ESP32
 * ------------------------------------------------------------ */

static esp_err_t web_mqtt_restore_handler(
    httpd_req_t *req
)
{
    ESP_LOGI(
        TAG,
        "============================================"
    );

    ESP_LOGI(
        TAG,
        "RESTAURACAO DO MQTT SOLICITADA PELA WEB"
    );

    if (
        mqtt_client != NULL &&
        !mqtt_service_started
    )
    {
        esp_err_t result =
            esp_mqtt_client_start(
                mqtt_client
            );

        if (result == ESP_OK)
        {
            mqtt_service_started = true;
            mqtt_simulated_failure = false;

            ESP_LOGI(
                TAG,
                "Cliente MQTT reiniciado. Aguardando broker..."
            );
        }
        else
        {
            ESP_LOGE(
                TAG,
                "Falha ao reiniciar MQTT: %s",
                esp_err_to_name(result)
            );
        }
    }
    else if (mqtt_service_started)
    {
        mqtt_simulated_failure = false;

        ESP_LOGI(
            TAG,
            "Cliente MQTT já está em execução."
        );
    }
    else
    {
        ESP_LOGE(
            TAG,
            "Cliente MQTT não está disponível."
        );
    }

    ESP_LOGI(
        TAG,
        "============================================"
    );

    return web_redirect_home(
        req
    );
}

/* ------------------------------------------------------------
 * Publicar estado da falha simulada do veículo
 * ------------------------------------------------------------ */

static void publish_vehicle_fault_status(void)
{
    if (mqtt_client == NULL || !mqtt_connected)
    {
        ESP_LOGW(
            TAG,
            "Não foi possível publicar falha do veículo: MQTT desconectado."
        );

        return;
    }

    const char *message = vehicle_simulated_failure
        ? "{\"origem\":\"ESP32\",\"tipo\":\"falha_veiculo_simulada\",\"ativa\":true}"
        : "{\"origem\":\"ESP32\",\"tipo\":\"falha_veiculo_simulada\",\"ativa\":false}";

    int message_id = esp_mqtt_client_publish(
        mqtt_client,
        MQTT_TOPIC_ESP32_FAULT,
        message,
        0,
        1,
        1
    );

    ESP_LOGI(
        TAG,
        "Estado da falha simulada do veículo publicado: %s | ID=%d",
        vehicle_simulated_failure ? "ATIVA" : "NORMAL",
        message_id
    );
}

/* ------------------------------------------------------------
 * Simular falha do veículo
 * ------------------------------------------------------------ */

static esp_err_t web_vehicle_failure_handler(
    httpd_req_t *req
)
{
    ESP_LOGW(TAG, "============================================");
    ESP_LOGW(TAG, "SIMULACAO DE FALHA DO VEICULO SOLICITADA PELA WEB");

    if (!mqtt_connected || mqtt_simulated_failure)
    {
        ESP_LOGE(
            TAG,
            "Falha do veículo não ativada: MQTT do ESP32 precisa estar conectado."
        );

        return web_redirect_home(req);
    }

    vehicle_simulated_failure = true;
    publish_vehicle_fault_status();
    publish_esp32_status("falha_veiculo_simulada");

    ESP_LOGW(TAG, "Falha simulada do veículo: ATIVA");
    ESP_LOGW(TAG, "============================================");

    return web_redirect_home(req);
}

/* ------------------------------------------------------------
 * Restaurar veículo
 * ------------------------------------------------------------ */

static esp_err_t web_vehicle_restore_handler(
    httpd_req_t *req
)
{
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "RESTAURACAO DO VEICULO SOLICITADA PELA WEB");

    vehicle_simulated_failure = false;

    if (mqtt_connected)
    {
        publish_vehicle_fault_status();
        publish_esp32_status("veiculo_normal");
    }
    else
    {
        ESP_LOGW(
            TAG,
            "Veículo restaurado localmente; estado será publicado quando o MQTT reconectar."
        );
    }

    ESP_LOGI(TAG, "Falha simulada do veículo: NORMAL");
    ESP_LOGI(TAG, "============================================");

    return web_redirect_home(req);
}

/* ------------------------------------------------------------
 * Iniciar servidor HTTP
 * ------------------------------------------------------------ */

static void start_web_server(void)
{
    if (web_server != NULL)
    {
        ESP_LOGW(
            TAG,
            "Servidor Web já está iniciado."
        );

        return;
    }

    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    config.server_port = 80;
    config.max_uri_handlers = 10;

    ESP_LOGI(
        TAG,
        "Iniciando servidor Web..."
    );

    esp_err_t result =
        httpd_start(
            &web_server,
            &config
        );

    if (result != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Falha ao iniciar servidor Web: %s",
            esp_err_to_name(result)
        );

        web_server = NULL;

        return;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = web_root_handler,
        .user_ctx = NULL
    };

    httpd_uri_t mqtt_failure_uri = {
        .uri = "/mqtt/falha",
        .method = HTTP_GET,
        .handler = web_mqtt_failure_handler,
        .user_ctx = NULL
    };

    httpd_uri_t mqtt_restore_uri = {
        .uri = "/mqtt/restaurar",
        .method = HTTP_GET,
        .handler = web_mqtt_restore_handler,
        .user_ctx = NULL
    };

    httpd_uri_t vehicle_failure_uri = {
        .uri = "/veiculo/falha",
        .method = HTTP_GET,
        .handler = web_vehicle_failure_handler,
        .user_ctx = NULL
    };

    httpd_uri_t vehicle_restore_uri = {
        .uri = "/veiculo/restaurar",
        .method = HTTP_GET,
        .handler = web_vehicle_restore_handler,
        .user_ctx = NULL
    };

    result =
        httpd_register_uri_handler(
            web_server,
            &root_uri
        );

    if (result == ESP_OK)
    {
        result =
            httpd_register_uri_handler(
                web_server,
                &mqtt_failure_uri
            );
    }

    if (result == ESP_OK)
    {
        result =
            httpd_register_uri_handler(
                web_server,
                &mqtt_restore_uri
            );
    }

    if (result == ESP_OK)
    {
        result =
            httpd_register_uri_handler(
                web_server,
                &vehicle_failure_uri
            );
    }

    if (result == ESP_OK)
    {
        result =
            httpd_register_uri_handler(
                web_server,
                &vehicle_restore_uri
            );
    }

    if (result != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Falha ao registrar rotas HTTP: %s",
            esp_err_to_name(result)
        );

        httpd_stop(
            web_server
        );

        web_server = NULL;

        return;
    }

    ESP_LOGI(
        TAG,
        "============================================"
    );

    ESP_LOGI(
        TAG,
        "SERVIDOR WEB ATIVO"
    );

    ESP_LOGI(
        TAG,
        "Cenarios MQTT e Veiculo disponiveis na interface Web"
    );

    ESP_LOGI(
        TAG,
        "Porta HTTP: 80"
    );

    ESP_LOGI(
        TAG,
        "============================================"
    );
}

/* ============================================================
 * COMPARAR TÓPICO MQTT
 * ============================================================ */

static bool mqtt_topic_equals(
    esp_mqtt_event_handle_t event,
    const char *expected_topic
)
{
    size_t expected_length =
        strlen(expected_topic);

    if (event->topic == NULL)
    {
        return false;
    }

    if (
        event->topic_len !=
        expected_length
    )
    {
        return false;
    }

    return strncmp(
        event->topic,
        expected_topic,
        event->topic_len
    ) == 0;
}

/* ============================================================
 * PEGAR A ÚLTIMA STRING DE UMA CHAVE JSON
 *
 * Isso é importante porque session_info possui dois "state":
 *
 * "active_enable_disable_source": {
 *     "state":"Unassigned"
 * }
 *
 * ...
 *
 * "state":"Charging"
 *
 * Queremos o último.
 * ============================================================ */

static bool json_get_last_string(
    const char *json,
    const char *key,
    char *output,
    size_t output_size
)
{
    char pattern[64];

    snprintf(
        pattern,
        sizeof(pattern),
        "\"%s\":\"",
        key
    );

    const char *current = json;
    const char *last_match = NULL;

    while (1)
    {
        const char *match =
            strstr(
                current,
                pattern
            );

        if (match == NULL)
        {
            break;
        }

        last_match = match;

        current =
            match + strlen(pattern);
    }

    if (last_match == NULL)
    {
        return false;
    }

    const char *start =
        last_match +
        strlen(pattern);

    const char *end =
        strchr(
            start,
            '"'
        );

    if (end == NULL)
    {
        return false;
    }

    size_t length =
        end - start;

    if (length >= output_size)
    {
        length =
            output_size - 1;
    }

    memcpy(
        output,
        start,
        length
    );

    output[length] = '\0';

    return true;
}

/* ============================================================
 * EXTRAIR NÚMERO DO JSON
 * ============================================================ */

static bool json_get_number(
    const char *json,
    const char *key,
    double *value
)
{
    char pattern[64];

    snprintf(
        pattern,
        sizeof(pattern),
        "\"%s\":",
        key
    );

    const char *start =
        strstr(
            json,
            pattern
        );

    if (start == NULL)
    {
        return false;
    }

    start +=
        strlen(pattern);

    char *end_pointer;

    double parsed_value =
        strtod(
            start,
            &end_pointer
        );

    if (
        start ==
        end_pointer
    )
    {
        return false;
    }

    *value =
        parsed_value;

    return true;
}

/* ============================================================
 * EXTRAIR BOOLEAN DO JSON
 * ============================================================ */

static bool json_get_bool(
    const char *json,
    const char *key,
    bool *value
)
{
    char pattern[64];

    snprintf(
        pattern,
        sizeof(pattern),
        "\"%s\":",
        key
    );

    const char *start =
        strstr(
            json,
            pattern
        );

    if (start == NULL)
    {
        return false;
    }

    start +=
        strlen(pattern);

    if (
        strncmp(
            start,
            "true",
            4
        ) == 0
    )
    {
        *value = true;

        return true;
    }

    if (
        strncmp(
            start,
            "false",
            5
        ) == 0
    )
    {
        *value = false;

        return true;
    }

    return false;
}

/* ============================================================
 * PROCESSAR SESSION_INFO
 * ============================================================ */

static void process_everest_session_info(
    const char *json
)
{
    double duration = 0.0;

    json_get_last_string(
        json,
        "state",
        ev_state,
        sizeof(ev_state)
    );

    json_get_number(
        json,
        "charged_energy_wh",
        &charged_energy_wh
    );

    json_get_number(
        json,
        "latest_total_w",
        &latest_total_w
    );

    if (
        json_get_number(
            json,
            "charging_duration_s",
            &duration
        )
    )
    {
        charging_duration_s =
            (int) duration;
    }

    json_get_bool(
        json,
        "permanent_fault",
        &permanent_fault
    );

    ESP_LOGI(
        TAG,
        ""
    );

    ESP_LOGI(
        TAG,
        "============================================"
    );

    ESP_LOGI(
        TAG,
        "        DADOS RECEBIDOS DO EVEREST"
    );

    ESP_LOGI(
        TAG,
        "============================================"
    );

    ESP_LOGI(
        TAG,
        "Estado              : %s",
        ev_state
    );

    ESP_LOGI(
        TAG,
        "Potencia            : %.2f W",
        latest_total_w
    );

    ESP_LOGI(
        TAG,
        "Potencia            : %.3f kW",
        latest_total_w / 1000.0
    );

    ESP_LOGI(
        TAG,
        "Energia carregada   : %.2f Wh",
        charged_energy_wh
    );

    ESP_LOGI(
        TAG,
        "Tempo de carga      : %d s",
        charging_duration_s
    );

    ESP_LOGI(
        TAG,
        "Falha permanente    : %s",
        permanent_fault
            ? "SIM"
            : "NAO"
    );

    ESP_LOGI(
        TAG,
        "============================================"
    );

    if (
        strcmp(
            ev_state,
            "Charging"
        ) == 0
    )
    {
        ESP_LOGI(
            TAG,
            "VEICULO CARREGANDO"
        );
    }

    else if (
        strcmp(
            ev_state,
            "Unplugged"
        ) == 0
    )
    {
        ESP_LOGI(
            TAG,
            "VEICULO DESCONECTADO"
        );
    }

    else
    {
        ESP_LOGI(
            TAG,
            "Estado EVSE: %s",
            ev_state
        );
    }

    ESP_LOGI(
        TAG,
        "============================================"
    );
}

/* ============================================================
 * PUBLICAR STATUS DO ESP32
 * ============================================================ */

static void publish_esp32_status(
    const char *estado
)
{
    if (mqtt_client == NULL)
    {
        return;
    }

    char message[256];

    snprintf(
        message,
        sizeof(message),
        "{"
        "\"dispositivo\":\"ESP32\","
        "\"projeto\":\"EVerest\","
        "\"estado\":\"%s\","
        "\"soc\":45"
        "}",
        estado
    );

    int message_id =
        esp_mqtt_client_publish(
            mqtt_client,
            MQTT_TOPIC_ESP32_STATUS,
            message,
            0,
            1,
            0
        );

    ESP_LOGI(
        TAG,
        "Status ESP32 publicado: %s | ID=%d",
        estado,
        message_id
    );
}

/* ============================================================
 * CONECTAR VEÍCULO VIRTUAL
 * ============================================================ */

static void everest_connect_vehicle(void)
{
    ESP_LOGI(
        TAG,
        "============================================"
    );

    ESP_LOGI(
        TAG,
        "Enviando CONECTAR para o EVerest..."
    );

    int enable_id =
        esp_mqtt_client_publish(
            mqtt_client,
            MQTT_TOPIC_EVEREST_ENABLE,
            "true",
            0,
            1,
            0
        );

    ESP_LOGI(
        TAG,
        "EvManager ENABLE enviado. ID=%d",
        enable_id
    );

    const char *charging_command =
        "sleep 1;"
        "iec_wait_pwr_ready;"
        "sleep 1;"
        "draw_power_regulated 16,3";

    int command_id =
        esp_mqtt_client_publish(
            mqtt_client,
            MQTT_TOPIC_EVEREST_SESSION,
            charging_command,
            0,
            1,
            0
        );

    ESP_LOGI(
        TAG,
        "Comando de carga enviado. ID=%d",
        command_id
    );

    publish_esp32_status(
        "comando_conectar_enviado"
    );

    ESP_LOGI(
        TAG,
        "============================================"
    );
}

/* ============================================================
 * DESCONECTAR VEÍCULO VIRTUAL
 * ============================================================ */

static void everest_disconnect_vehicle(void)
{
    ESP_LOGI(
        TAG,
        "============================================"
    );

    ESP_LOGI(
        TAG,
        "Enviando DESCONECTAR para o EVerest..."
    );

    int command_id =
        esp_mqtt_client_publish(
            mqtt_client,
            MQTT_TOPIC_EVEREST_SESSION,
            "unplug",
            0,
            1,
            0
        );

    ESP_LOGI(
        TAG,
        "Comando UNPLUG enviado. ID=%d",
        command_id
    );

    publish_esp32_status(
        "comando_desconectar_enviado"
    );

    ESP_LOGI(
        TAG,
        "============================================"
    );
}

/* ============================================================
 * EVENTOS WI-FI
 * ============================================================ */

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    if (
        event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START
    )
    {
        ESP_LOGI(
            TAG,
            "Tentando conectar ao Wi-Fi..."
        );

        esp_wifi_connect();
    }

    else if (
        event_base == WIFI_EVENT &&
        event_id ==
            WIFI_EVENT_STA_DISCONNECTED
    )
    {
        if (
            wifi_retry_count <
            MAXIMUM_RETRY
        )
        {
            wifi_retry_count++;

            ESP_LOGW(
                TAG,
                "Wi-Fi desconectado. Tentativa %d de %d",
                wifi_retry_count,
                MAXIMUM_RETRY
            );

            esp_wifi_connect();
        }
        else
        {
            xEventGroupSetBits(
                wifi_event_group,
                WIFI_FAILURE_BIT
            );
        }
    }

    else if (
        event_base == IP_EVENT &&
        event_id ==
            IP_EVENT_STA_GOT_IP
    )
    {
        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)
            event_data;

        ESP_LOGI(
            TAG,
            "Wi-Fi conectado. IP do ESP32: " IPSTR,
            IP2STR(
                &event->ip_info.ip
            )
        );

        wifi_retry_count = 0;

        xEventGroupSetBits(
            wifi_event_group,
            WIFI_CONNECTED_BIT
        );
    }
}

/* ============================================================
 * INICIAR WI-FI
 * ============================================================ */

static void wifi_init(void)
{
    wifi_event_group =
        xEventGroupCreate();

    ESP_ERROR_CHECK(
        esp_netif_init()
    );

    ESP_ERROR_CHECK(
        esp_event_loop_create_default()
    );

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(
            &wifi_init_config
        )
    );

    esp_event_handler_instance_t
        wifi_handler;

    esp_event_handler_instance_t
        ip_handler;

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            &wifi_handler
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            &ip_handler
        )
    );

    wifi_config_t station_config = {
        .sta = {
            .ssid =
                WIFI_SSID,

            .password =
                WIFI_PASSWORD,

            .threshold.authmode =
                WIFI_AUTH_WPA2_PSK
        }
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(
            WIFI_MODE_STA
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &station_config
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );

    EventBits_t bits =
        xEventGroupWaitBits(
            wifi_event_group,
            WIFI_CONNECTED_BIT |
            WIFI_FAILURE_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );

    if (
        bits &
        WIFI_CONNECTED_BIT
    )
    {
        ESP_LOGI(
            TAG,
            "Conexão Wi-Fi concluída."
        );

        start_mqtt();

        start_web_server();
    }
    else
    {
        ESP_LOGE(
            TAG,
            "Falha ao conectar ao Wi-Fi."
        );
    }
}

/* ============================================================
 * EVENTOS MQTT
 * ============================================================ */

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
)
{
    esp_mqtt_event_handle_t event =
        (esp_mqtt_event_handle_t)
        event_data;

    switch (
        (esp_mqtt_event_id_t)
        event_id
    )
    {
        case MQTT_EVENT_CONNECTED:
        {
            mqtt_connected = true;
            mqtt_simulated_failure = false;

            ESP_LOGI(
                TAG,
                "Conectado ao broker MQTT."
            );

            int command_sub_id =
                esp_mqtt_client_subscribe(
                    event->client,
                    MQTT_TOPIC_ESP32_COMMAND,
                    1
                );

            ESP_LOGI(
                TAG,
                "Inscrito em %s. ID=%d",
                MQTT_TOPIC_ESP32_COMMAND,
                command_sub_id
            );

            int everest_sub_id =
                esp_mqtt_client_subscribe(
                    event->client,
                    MQTT_TOPIC_EVEREST_SESSION_INFO,
                    1
                );

            ESP_LOGI(
                TAG,
                "Inscrito no session_info do EVerest. ID=%d",
                everest_sub_id
            );

            publish_esp32_status(
                "online"
            );

            publish_vehicle_fault_status();

            break;
        }

        case MQTT_EVENT_SUBSCRIBED:
        {
            ESP_LOGI(
                TAG,
                "Inscrição MQTT confirmada. ID=%d",
                event->msg_id
            );

            break;
        }

        case MQTT_EVENT_PUBLISHED:
        {
            ESP_LOGI(
                TAG,
                "Publicação confirmada. ID=%d",
                event->msg_id
            );

            break;
        }

        case MQTT_EVENT_DATA:
        {
            /* --------------------------------------------
             * COMANDOS RECEBIDOS PELO ESP32
             * -------------------------------------------- */

            if (
                mqtt_topic_equals(
                    event,
                    MQTT_TOPIC_ESP32_COMMAND
                )
            )
            {
                ESP_LOGI(
                    TAG,
                    "============================================"
                );

                ESP_LOGI(
                    TAG,
                    "Comando MQTT recebido!"
                );

                printf(
                    "Topico: %.*s\n",
                    event->topic_len,
                    event->topic
                );

                printf(
                    "Mensagem: %.*s\n",
                    event->data_len,
                    event->data
                );

                if (
                    event->data_len ==
                    strlen("CONECTAR") &&

                    strncmp(
                        event->data,
                        "CONECTAR",
                        event->data_len
                    ) == 0
                )
                {
                    ESP_LOGI(
                        TAG,
                        "COMANDO RECONHECIDO: CONECTAR"
                    );

                    everest_connect_vehicle();
                }

                else if (
                    event->data_len ==
                    strlen("DESCONECTAR") &&

                    strncmp(
                        event->data,
                        "DESCONECTAR",
                        event->data_len
                    ) == 0
                )
                {
                    ESP_LOGI(
                        TAG,
                        "COMANDO RECONHECIDO: DESCONECTAR"
                    );

                    everest_disconnect_vehicle();
                }

                else
                {
                    ESP_LOGW(
                        TAG,
                        "Comando desconhecido."
                    );
                }

                break;
            }

            /* --------------------------------------------
             * SESSION_INFO DO EVEREST
             * -------------------------------------------- */

            if (
                mqtt_topic_equals(
                    event,
                    MQTT_TOPIC_EVEREST_SESSION_INFO
                )
            )
            {
                if (
                    event->current_data_offset != 0 ||
                    event->data_len !=
                    event->total_data_len
                )
                {
                    ESP_LOGW(
                        TAG,
                        "session_info recebido fragmentado."
                    );

                    break;
                }

                if (
                    event->data_len >=
                    MQTT_DATA_BUFFER_SIZE
                )
                {
                    ESP_LOGE(
                        TAG,
                        "session_info maior que o buffer."
                    );

                    break;
                }

                char json_buffer[
                    MQTT_DATA_BUFFER_SIZE
                ];

                memcpy(
                    json_buffer,
                    event->data,
                    event->data_len
                );

                json_buffer[
                    event->data_len
                ] = '\0';

                process_everest_session_info(
                    json_buffer
                );

                break;
            }

            break;
        }

        case MQTT_EVENT_DISCONNECTED:
        {
            mqtt_connected = false;

            ESP_LOGW(
                TAG,
                "MQTT desconectado."
            );

            break;
        }

        case MQTT_EVENT_ERROR:
        {
            ESP_LOGE(
                TAG,
                "Erro MQTT."
            );

            break;
        }

        default:
            break;
    }
}

/* ============================================================
 * INICIAR MQTT
 * ============================================================ */

static void start_mqtt(void)
{
    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri =
            MQTT_BROKER_URI
    };

    mqtt_client =
        esp_mqtt_client_init(
            &mqtt_config
        );

    if (
        mqtt_client == NULL
    )
    {
        ESP_LOGE(
            TAG,
            "Falha ao criar cliente MQTT."
        );

        return;
    }

    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(
            mqtt_client,
            ESP_EVENT_ANY_ID,
            mqtt_event_handler,
            NULL
        )
    );

    esp_err_t start_result =
        esp_mqtt_client_start(
            mqtt_client
        );

    if (start_result != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Falha ao iniciar cliente MQTT: %s",
            esp_err_to_name(start_result)
        );

        return;
    }

    mqtt_service_started = true;

    ESP_LOGI(
        TAG,
        "Cliente MQTT iniciado."
    );
}

/* ============================================================
 * FUNÇÃO PRINCIPAL
 * ============================================================ */

void app_main(void)
{
    esp_err_t result =
        nvs_flash_init();

    if (
        result ==
            ESP_ERR_NVS_NO_FREE_PAGES ||

        result ==
            ESP_ERR_NVS_NEW_VERSION_FOUND
    )
    {
        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );

        ESP_ERROR_CHECK(
            nvs_flash_init()
        );
    }

    ESP_LOGI(
        TAG,
        "============================================"
    );

    ESP_LOGI(
        TAG,
        "ESP32 + EVerest EV Controller"
    );

    ESP_LOGI(
        TAG,
        "Controle + Telemetria"
    );

    ESP_LOGI(
        TAG,
        "============================================"
    );

    wifi_init();
}
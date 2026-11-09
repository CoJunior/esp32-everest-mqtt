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

#define WIFI_SSID      "Bissau-Guiné"
#define WIFI_PASSWORD  "Bedagra84"

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

#define MQTT_TOPIC_EVEREST_POWERMETER \
    "everest_api/connector_1/var/powermeter"

#define MQTT_TOPIC_CONTROL_PILOT \
    "esp32/ev/control_pilot"
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

typedef enum
{
    VEHICLE_FAULT_NONE,
    VEHICLE_FAULT_GENERIC
} vehicle_fault_t;

static vehicle_fault_t vehicle_fault =
    VEHICLE_FAULT_NONE;


/* ============================================================
 * MÁQUINA DE ESTADOS IEC 61851 - VEÍCULO SIMULADO
 * ============================================================ */


typedef enum
{
    IEC_STATE_A1,
    IEC_STATE_B1,
    IEC_STATE_B2,
    IEC_STATE_C1,
    IEC_STATE_C2,
    IEC_STATE_D1,
    IEC_STATE_D2,
    IEC_STATE_E,
    IEC_STATE_F
} iec_state_t;

/*
 * Estado inicial:
 * A1 = veículo não conectado
 */
static iec_state_t iec_state = IEC_STATE_A1;

/*
 * Control Pilot simulado
 * Duty cycle PWM informado pelo EVSE.
 */
static float iec_pwm_duty = 50.0f;

static const char *iec_state_to_string(iec_state_t state)
{
    switch (state)
    {
        case IEC_STATE_A1:
            return "A1";

        case IEC_STATE_B1:
            return "B1";

        case IEC_STATE_B2:
            return "B2";

        case IEC_STATE_C1:
            return "C1";

        case IEC_STATE_C2:
            return "C2";

        case IEC_STATE_D1:
            return "D1";

        case IEC_STATE_D2:
            return "D2";

        case IEC_STATE_E:
            return "E";

        case IEC_STATE_F:
            return "F";

        default:
            return "UNKNOWN";
    }
}


static float iec_pwm_to_current(float duty_cycle)
{
    /*
     * IEC 61851 - conversao simplificada do duty cycle
     * do Control Pilot para corrente maxima indicada.
     */

    if (duty_cycle < 8.0f)
    {
        return 0.0f;
    }

    if (duty_cycle < 10.0f)
    {
        return 6.0f;
    }

    if (duty_cycle <= 85.0f)
    {
        return duty_cycle * 0.6f;
    }

    if (duty_cycle <= 96.0f)
    {
        return (duty_cycle - 64.0f) * 2.5f;
    }

    if (duty_cycle <= 97.0f)
    {
        return 80.0f;
    }

    return 0.0f;
}

static void publish_control_pilot(void);

static void iec_set_state(iec_state_t new_state)
{
    if (iec_state == new_state)
    {
        return;
    }

    ESP_LOGI(
        TAG,
        "IEC 61851: %s -> %s",
        iec_state_to_string(iec_state),
        iec_state_to_string(new_state)
    );

    iec_state = new_state;

    publish_control_pilot();
}

/* ============================================================
 * ESTADO RECEBIDO DO EVEREST
 * ============================================================ */

static char ev_state[32] = "Unknown";

static double charged_energy_wh = 0.0;

static double latest_total_w = 0.0;

/* Correntes medidas pelo powermeter do EVerest */
static double measured_current_l1 = 0.0;
static double measured_current_l2 = 0.0;
static double measured_current_l3 = 0.0;
static double measured_current_n  = 0.0;

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

extern const unsigned char web_index_html_start[]
    asm("_binary_index_html_start");

extern const unsigned char web_index_html_end[]
    asm("_binary_index_html_end");

static esp_err_t web_root_handler(httpd_req_t *req)
{
    const size_t tamanho_html =
        web_index_html_end - web_index_html_start;

    httpd_resp_set_type(
        req,
        "text/html; charset=utf-8"
    );

    httpd_resp_set_hdr(
        req,
        "Cache-Control",
        "no-store"
    );

    return httpd_resp_send(
        req,
        (const char *)web_index_html_start,
        tamanho_html
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

    const char *cenario =
    vehicle_fault == VEHICLE_FAULT_GENERIC
        ? "generica"
        : "nenhuma";

    char message[256];

    snprintf(
        message,
        sizeof(message),
        "{\"origem\":\"ESP32\","
        "\"tipo\":\"falha_veiculo_simulada\","
        "\"cenario\":\"%s\","
        "\"ativa\":%s}",
        cenario,
        vehicle_simulated_failure ? "true" : "false"
);
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
 * Publicar dados simulados do Control Pilot
 * ------------------------------------------------------------ */

static void publish_control_pilot(void)
{
    if (mqtt_client == NULL || !mqtt_connected)
    {
        ESP_LOGW(
            TAG,
            "Não foi possível publicar Control Pilot: MQTT desconectado."
        );

        return;
    }

    float corrente_maxima =
        iec_pwm_to_current(iec_pwm_duty);

    char message[256];

    snprintf(
        message,
        sizeof(message),
        "{"
        "\"estado\":\"%s\","
        "\"pwm_duty\":%.1f,"
        "\"corrente_maxima_a\":%.1f"
        "}",
        iec_state_to_string(iec_state),
        iec_pwm_duty,
        corrente_maxima
    );

    int message_id =
        esp_mqtt_client_publish(
            mqtt_client,
            MQTT_TOPIC_CONTROL_PILOT,
            message,
            0,
            1,
            1
        );

    ESP_LOGI(
        TAG,
        "Control Pilot publicado: %s | ID=%d",
        message,
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
    vehicle_fault = VEHICLE_FAULT_GENERIC;
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
    vehicle_fault = VEHICLE_FAULT_NONE;

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

/* ============================================================
 * API HTTP - STATUS DO ESP32
 * ============================================================ */

static esp_err_t web_api_status_handler(httpd_req_t *req)
{
    char resposta[1024];

    double corrente_maxima_indicada =
        iec_pwm_to_current(iec_pwm_duty);

    double maior_corrente_medida = measured_current_l1;

    if (measured_current_l2 > maior_corrente_medida)
    {
        maior_corrente_medida = measured_current_l2;
    }

    if (measured_current_l3 > maior_corrente_medida)
    {
        maior_corrente_medida = measured_current_l3;
    }

    int tamanho = snprintf(
        resposta,
        sizeof(resposta),
        "{"
        "\"mqtt_conectado\":%s,"
        "\"falha_mqtt_simulada\":%s,"
        "\"falha_veiculo_simulada\":%s,"
        "\"estado_everest\":\"%s\","
        "\"potencia_w\":%.2f,"
        "\"falha_permanente\":%s,"
        "\"potencia_kw\":%.3f,"
        "\"energia_carregada_wh\":%.2f,"
        "\"tempo_carregamento_s\":%d,"
        "\"estado_iec\":\"%s\","
        "\"pwm_control_pilot\":%.1f,"
        "\"corrente_maxima_a\":%.2f,"
        "\"corrente_l1_a\":%.2f,"
        "\"corrente_l2_a\":%.2f,"
        "\"corrente_l3_a\":%.2f,"
        "\"corrente_n_a\":%.2f,"
        "\"maior_corrente_medida_a\":%.2f,"
        "\"corrente_dentro_limite\":%s"
        "}",
        mqtt_connected ? "true" : "false",
        mqtt_simulated_failure ? "true" : "false",
        vehicle_simulated_failure ? "true" : "false",
        ev_state,
        latest_total_w,
        permanent_fault ? "true" : "false",
        latest_total_w / 1000.0,
        charged_energy_wh,
        charging_duration_s,
        iec_state_to_string(iec_state),
        iec_pwm_duty,
        corrente_maxima_indicada,
        measured_current_l1,
        measured_current_l2,
        measured_current_l3,
        measured_current_n,
        maior_corrente_medida,
        maior_corrente_medida <= corrente_maxima_indicada
            ? "true"
            : "false"
    );

    if (tamanho < 0 || tamanho >= sizeof(resposta))
    {
        ESP_LOGE(TAG, "Resposta JSON excedeu o buffer");

        return httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Erro ao gerar status"
        );
    }

    httpd_resp_set_type(
        req,
        "application/json; charset=utf-8"
    );

    httpd_resp_set_hdr(
        req,
        "Cache-Control",
        "no-store"
    );

    return httpd_resp_send(
        req,
        resposta,
        HTTPD_RESP_USE_STRLEN
    );
}

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

    httpd_uri_t api_status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = web_api_status_handler,
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

    if (result == ESP_OK)
    {
        result =
            httpd_register_uri_handler(
                web_server,
                &api_status_uri
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
 * EXTRAIR CORRENTE DO POWERMETER
 * ============================================================ */

static bool json_get_current_phase(
    const char *json,
    const char *phase,
    double *value
)
{
    const char *current_object =
        strstr(
            json,
            "\"current_A\":"
        );

    if (current_object == NULL)
    {
        return false;
    }

    return json_get_number(
        current_object,
        phase,
        value
    );
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

    if (
    strcmp(ev_state, "Preparing") == 0 &&
    iec_state == IEC_STATE_B1
)
{
    iec_set_state(IEC_STATE_B2);
}

    if (
    strcmp(ev_state, "Charging") == 0 &&
    iec_state == IEC_STATE_B2
)
{
    iec_set_state(IEC_STATE_C2);
}

     if (
    strcmp(ev_state, "Finished") == 0 &&
    iec_state == IEC_STATE_C2
)
{
    iec_set_state(IEC_STATE_B2);
}

if (
    strcmp(ev_state, "Unplugged") == 0 &&
    iec_state == IEC_STATE_B2
)
{
    iec_set_state(IEC_STATE_A1);
}

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
 * PROCESSAR POWERMETER DO EVEREST
 * ============================================================ */

static void process_everest_powermeter(
    const char *json
)
{
    json_get_current_phase(
        json,
        "L1",
        &measured_current_l1
    );

    json_get_current_phase(
        json,
        "L2",
        &measured_current_l2
    );

    json_get_current_phase(
        json,
        "L3",
        &measured_current_l3
    );

    json_get_current_phase(
        json,
        "N",
        &measured_current_n
    );

    double current_limit =
        iec_pwm_to_current(
            iec_pwm_duty
        );

    double max_measured_current =
        measured_current_l1;

    if (
        measured_current_l2 >
        max_measured_current
    )
    {
        max_measured_current =
            measured_current_l2;
    }

    if (
        measured_current_l3 >
        max_measured_current
    )
    {
        max_measured_current =
            measured_current_l3;
    }

    ESP_LOGI(
        TAG,
        ""
    );

    ESP_LOGI(
        TAG,
        "========== CONTROL PILOT / POWERMETER =========="
    );

    ESP_LOGI(
        TAG,
        "Estado IEC               : %s",
        iec_state_to_string(iec_state)
    );

    ESP_LOGI(
        TAG,
        "PWM Control Pilot        : %.1f %%",
        iec_pwm_duty
    );

    ESP_LOGI(
        TAG,
        "Corrente maxima indicada : %.2f A",
        current_limit
    );

    ESP_LOGI(
        TAG,
        "Corrente medida L1       : %.2f A",
        measured_current_l1
    );

    ESP_LOGI(
        TAG,
        "Corrente medida L2       : %.2f A",
        measured_current_l2
    );

    ESP_LOGI(
        TAG,
        "Corrente medida L3       : %.2f A",
        measured_current_l3
    );

    ESP_LOGI(
        TAG,
        "Corrente medida N        : %.2f A",
        measured_current_n
    );

    ESP_LOGI(
        TAG,
        "Maior corrente medida    : %.2f A",
        max_measured_current
    );

    ESP_LOGI(
        TAG,
        "Situacao                 : %s",
        max_measured_current <= current_limit
            ? "DENTRO DO LIMITE"
            : "ACIMA DO LIMITE"
    );

    ESP_LOGI(
        TAG,
        "================================================"
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

    iec_set_state(IEC_STATE_B1);

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

            int powermeter_sub_id =
                   esp_mqtt_client_subscribe(
                   event->client,
                    MQTT_TOPIC_EVEREST_POWERMETER,
                    1
            );

            ESP_LOGI(
                TAG,
                 "Inscrito no powermeter do EVerest. ID=%d",
                  powermeter_sub_id
           );

            publish_esp32_status(
                "online"
            );

            publish_vehicle_fault_status();

            publish_control_pilot();

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

            /* --------------------------------------------
             * POWERMETER DO EVEREST
             * -------------------------------------------- */

            if (
                mqtt_topic_equals(
                    event,
                    MQTT_TOPIC_EVEREST_POWERMETER
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
                        "powermeter recebido fragmentado."
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
                        "powermeter maior que o buffer."
                    );

                    break;
                }

                char powermeter_buffer[
                    MQTT_DATA_BUFFER_SIZE
                ];

                memcpy(
                    powermeter_buffer,
                    event->data,
                    event->data_len
                );

                powermeter_buffer[
                    event->data_len
                ] = '\0';

               process_everest_powermeter(
                   powermeter_buffer
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

   ESP_LOGI(
    TAG,
    "Estado IEC 61851 inicial : %s",
    iec_state_to_string(iec_state)
);

/* Teste da conversao PWM -> corrente maxima IEC 61851 */

   ESP_LOGI(
    TAG,
    "Control Pilot PWM       : %.1f %%",
    iec_pwm_duty
);

ESP_LOGI(
    TAG,
    "Corrente maxima indicada: %.1f A",
    iec_pwm_to_current(iec_pwm_duty)
);

    wifi_init();
}
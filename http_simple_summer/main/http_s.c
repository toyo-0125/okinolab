#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include "esp_check.h"
#include "protocol_examples_utils.h"
#include <sys/param.h>
#include "esp_timer.h"
#include <stdbool.h>


#include "temprature.h"
#include "cpu_usage.h"
#include "http_s.h"

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (64)

static const char *TAG = "http_server";
static int request_count = 0;
static httpd_handle_t server = NULL;

bool http_server_is_running(void)
{
    return server != NULL;
}

wifi_ap_record_t ap_info;

/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN], dec_param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN] = {0};
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
                example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
                example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
                example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Hello World!"
};

/* An HTTP POST handler */
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

/* An HTTP_ANY handler */
static esp_err_t any_handler(httpd_req_t *req)
{
    /* Send response with body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t any = {
    .uri       = "/any",
    .method    = HTTP_ANY,
    .handler   = any_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Hello World!"
};


//making point
static esp_err_t status_get_handler(httpd_req_t *req)
{
    request_count++;

    httpd_resp_set_type(req, "text/html");

    httpd_resp_sendstr_chunk(req,
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<title>ESP32 Attack Monitor</title>"
        "</head>"
        "<body>");

    httpd_resp_sendstr_chunk(req,
        "<h1>ESP32 Attack Monitor</h1>"

        "<p>Free Heap : <span id=\"free_heap\">--</span> bytes</p>"
        "<p>Uptime : <span id=\"uptime\">--:--:--</span></p>"
        "<p>Wi-Fi RSSI : <span id=\"rssi\">--</span> dBm</p>"
       
        "<p>CPU Temp : <span id=\"cpu_temp\">--</span> ℃</p>"
        "<p>CPU Usage : <span id=\"cpu_total\">--</span> %</p>"
        "<p>Core 0 Usage : <span id=\"cpu_core0\">--</span> %</p>"
        "<p>Core 1 Usage : <span id=\"cpu_core1\">--</span> %</p>"
    );

    httpd_resp_sendstr_chunk(req,
        "<script>"

        "async function update(){"

        "try{"

        "const res = await fetch('/data');"
        "const data = await res.json();"

        "document.getElementById('free_heap').textContent=data.free_heap;"

        "const h=Math.floor(data.uptime_sec/3600);"
        "const m=Math.floor((data.uptime_sec%3600)/60);"
        "const s=data.uptime_sec%60;"

        "document.getElementById('uptime').textContent="
        "String(h).padStart(2,'0')+':'+" 
        "String(m).padStart(2,'0')+':'+" 
        "String(s).padStart(2,'0');"

        "document.getElementById('rssi').textContent=data.rssi;"

        "document.getElementById('cpu_temp').textContent=data.cpu_temp.toFixed(1);"
        "document.getElementById('cpu_total').textContent=data.cpu_total.toFixed(1);"
        "document.getElementById('cpu_core0').textContent=data.cpu_core0.toFixed(1);"
        "document.getElementById('cpu_core1').textContent=data.cpu_core1.toFixed(1);"

        "}catch(e){"
        "console.log(e);"
        "}"

        "}"

        "update();"
        "setInterval(update,1000);"

        "</script>"
    );

    httpd_resp_sendstr_chunk(req,
        "</body>"
        "</html>"
    );

    /* 終了 */
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}


static esp_err_t data_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "data handler");

    /*
     * 現在の空きヒープメモリを取得
     */
    size_t free_heap = esp_get_free_heap_size();


    /*
     * Wi-Fi RSSIを取得
     */
    wifi_ap_record_t ap_info;

    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ap_info.rssi = 0;
    }

    /*
     * ESP32起動後の経過時間を秒単位で取得
     */
    int64_t uptime_us = esp_timer_get_time();
    int64_t uptime_sec = uptime_us / 1000000;


    /*
     * CPU内部温度を取得
     */
    float cpu_temp = get_cpu_temprature();

    /*
     * JSON文字列を作成
     */
    char response[4096];

     esp_netif_ip_info_t ip_info;
     esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

     if (netif != NULL &&
        esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        memset(&ip_info, 0, sizeof(ip_info));
     }

     snprintf(
        response,
        sizeof(response),

        "{"
        "\"free_heap\":%u,"
        "\"uptime_sec\":%lld,"
        "\"rssi\":%d,"
        "\"cpu_temp\":%.1f,"
        "\"cpu_total\":%.1f,"
        "\"cpu_core0\":%.1f,"
        "\"cpu_core1\":%.1f"
        "}",

        (unsigned int)free_heap,
        (long long)uptime_sec,
        ap_info.rssi,
        cpu_temp,
        cpu_usage_get_total(),
        cpu_usage_get_core0(),
        cpu_usage_get_core1()
    );


    /*
     * JSONとしてレスポンスを返す
     */
    httpd_resp_set_type(
        req,
        "application/json"
    );

    httpd_resp_send(
        req,
        response,
        HTTPD_RESP_USE_STRLEN
    );

    return ESP_OK;
}


static const httpd_uri_t status = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_get_handler,
};

static const httpd_uri_t data = {
    .uri = "/data",
    .method = HTTP_GET,
    .handler = data_get_handler,
};

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */
static esp_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf;
    int ret;

    if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (buf == '0') {
        /* URI handlers can be unregistered using the uri string */
        ESP_LOGI(TAG, "Unregistering /hello and /echo URIs");
        httpd_unregister_uri(req->handle, "/hello");
        httpd_unregister_uri(req->handle, "/echo");
        /* Register the custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    else {
        ESP_LOGI(TAG, "Registering /hello and /echo URIs");
        httpd_register_uri_handler(req->handle, &hello);
        httpd_register_uri_handler(req->handle, &echo);
        /* Unregister custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, NULL);
    }

    /* Respond with empty body */
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t ctrl = {
    .uri       = "/ctrl",
    .method    = HTTP_PUT,
    .handler   = ctrl_put_handler,
    .user_ctx  = NULL
};



static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#if CONFIG_IDF_TARGET_LINUX
    // Setting port as 8001 when building for Linux. Port 80 can be used only by a privileged user in linux.
    // So when a unprivileged user tries to run the application, it throws bind error and the server is not started.
    // Port 8001 can be used by an unprivileged user as well. So the application will not throw bind error and the
    // server will be started.
    config.server_port = 8001;
#endif // !CONFIG_IDF_TARGET_LINUX
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &status);
        httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &data);
        httpd_register_uri_handler(server, &ctrl);
        httpd_register_uri_handler(server, &any);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


void http_server_start(void)
{
    // イベントハンドラ登録
    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &connect_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            WIFI_EVENT_STA_DISCONNECTED,
            &disconnect_handler,
            NULL
        )
    );

    // 最初の起動
    server = start_webserver();
}

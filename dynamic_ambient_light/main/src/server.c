#include "server.h"
#include "camera.h"
#include "esp_camera.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "led_esp_now.h"
#include "led_logic.h"
#include <string.h>
#include "esp_timer.h"

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace; boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
volatile bool lights_on;
volatile bool is_streaming;

static led_struct led_points_data;
static led_lut out_lut_data;
led_struct *led_points = &led_points_data;
led_lut *out_lut = &out_lut_data;

static TaskHandle_t ambilight_task_handle = NULL;
static volatile bool ambilight_running = false;
extern led_lut ambilight_lut;

void ambilight_task (void *pvParameters)
{
    ambilight_running = true;
    int64_t t0 = esp_timer_get_time();
    int frames = 0;
    
    //ESP_LOGI("AMBILIGHT", "Watek ambilight wystartowal");
    output_rgb_struct colour_results;

    while (lights_on)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb)
        {
            calculate_colours(out_lut, fb, &colour_results);
            send_led_colours(&colour_results);
            esp_camera_fb_return(fb);
            frames++;
        }
            if (esp_timer_get_time() - t0 >= 1000000)
            {
                ESP_LOGI("FPS", "%d fps", frames);
                frames = 0;
                t0 = esp_timer_get_time();
            }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    for (int i = 0; i < ZONES_NUMBER; i++)
    {
        colour_results.rgb[i].r = 0;
        colour_results.rgb[i].g = 0;
        colour_results.rgb[i].b = 0;
        
    }
    send_led_colours(&colour_results);

    ambilight_task_handle = NULL;
    ambilight_running = false;
    vTaskDelete(NULL);
}

esp_err_t file_server_get_handler(httpd_req_t *req)
{
    char filepath[600];

    // Mapowanie adresu z przeglądarki na adres LittleFS
    if (strcmp(req->uri, "/") == 0)
    {
        strcpy(filepath, "/web/index.html");
    } else 
    {
        snprintf(filepath, sizeof(filepath), "/web%s", req->uri);
    }

    // Wykrywanie typu pliku
    if (strstr(filepath, ".html"))
    {
        httpd_resp_set_type(req, "text/html");
    } else if (strstr(filepath, ".css"))
    {
        httpd_resp_set_type(req, "text/css");
    }  else if (strstr(filepath, ".js"))
    {
        httpd_resp_set_type(req, "application/javascript");
    }


    FILE *f = fopen(filepath, "r");
    if (f == NULL)
    {
        //ESP_LOGE(HTTP, "Nie znaleziono pliku %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Brak pliku");
        return ESP_FAIL;
    }

    char chunk[512];
    size_t read_bytes = 1;
    while (read_bytes > 0)
    {
        read_bytes = fread(chunk, 1, sizeof(chunk), f);
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK)
        {
            fclose(f);
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t save_markers_handler(httpd_req_t *req)
{
    char buf[100];
    
    int remaining = req->content_len;
    int received = 0;

    if (remaining >= sizeof(buf))
    {
        //ESP_LOGE("MARKERS", "Marker's data size to large for given buffer!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Rozmiar danych z markerów za duży");
        return ESP_FAIL;
    }

    // Odczyt danych z POST
    while (received < remaining)
    {
        int ret = httpd_req_recv(req, buf + received, remaining - received);
        if (ret <= 0)
        {
            //ESP_LOGE("MARKERS", "Error while reading POST data");
            return ESP_FAIL;
        }
        received += ret;

    }
    // Zakończenie ciągu z JSON null-terminatorem
    buf[received] = '\0';

    FILE *f = fopen("/web/markers.json", "w");
    if (f == NULL)
    {
        //ESP_LOGE("MARKERS", "Error while creating markers.json file");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Błąd zapisu pliku z danymi z markerów");
        return ESP_FAIL;
    }
    fprintf(f, "%s", buf);
    fclose(f);
    //ESP_LOGI("MARKERS", "Poprawnie zapisano dane z markerów w pamięci flash ESP32-S3. Dane: %s", buf);
    httpd_resp_sendstr(req, "Zapisano pozycje markerów");
    return ESP_OK;
}

static esp_err_t lights_handler(httpd_req_t *req)
{
    char buf[8];
    int ret;
    int remaining = req->content_len;
    if(remaining >= sizeof(buf))
    {
        //ESP_LOGE("LIGHTS", "Lights' on signal size to large for given buffer!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Błąd przy przesyłaniu sygnału włączenia diod");
        return ESP_FAIL;
    }
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0)
    {
        //ESP_LOGE("LIGHTS", "Error while reading lights' on signal");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    if (!strcmp(buf, "ON"))
    {
        calculate_led_points(led_points);
        calculate_zones_LUT(led_points, out_lut);
        //ESP_LOGI("LED_ZONES", "Obliczono strefy poboru probek koloru");
        if (is_streaming)
        {
            lights_on = true;
            int wait_ms = 0;
            while (is_streaming && wait_ms < 1500)
            {
                vTaskDelay(pdMS_TO_TICKS(10));
                wait_ms += 10;
            }
        }
        
        if (!ambilight_running)
        { 
            init_camera_raw();
            lights_on = true;
            xTaskCreatePinnedToCore(
                ambilight_task,
                "ambilight_task",
                4096,
                NULL,
                5,
                &ambilight_task_handle,
                1
            );
            //ESP_LOGI("LIGHTS", "Podświetlenie włączone");
        }
        else
        {
            //ESP_LOGW("LIGHTS", "Watek ambilight juz dziala");
        }
    }
    else if (!strcmp(buf, "OFF"))
    {
        lights_on = false;
        int wait_ms = 0;
        while (ambilight_task_handle != NULL && wait_ms < 500)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_ms += 10;
        }
    }
    else
    {
        ESP_LOGE("LIGHTS","Odebrano nieznaną komendę sterującą");
    }
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t set_colour_handler(httpd_req_t *req)
{
    if (ambilight_task_handle != NULL)
    {
        lights_on = false;
        int wait_ms = 0;
        while (ambilight_task_handle != NULL && wait_ms < 500)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_ms += 10;
        }
    }

    char buf[100];
    
    int remaining = req->content_len;
    int received = 0;

    if (remaining >= sizeof(buf))
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Rozmiar danych z kolorem za duży");
        return ESP_FAIL;
    }

    // Odczyt danych z POST
    while (received < remaining)
    {
        int ret = httpd_req_recv(req, buf + received, remaining - received);
        if (ret <= 0)
        {
            return ESP_FAIL;
        }
        received += ret;

    }
    // Zakończenie ciągu z JSON null-terminatorem
    buf[received] = '\0';

    int r = 0;
    int g = 0;
    int b = 0;

    int parsed = sscanf(buf, " [%d, %d, %d] ", &r, &g, &b);

    if (parsed != 3)
    {
        //ESP_LOGE("COLOUR", "Błąd parsowania koloru RGB: %s", buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Niepoprawny format JSON");
        return ESP_FAIL;
    }

    output_rgb_struct colour_to_set;
    for (int i = 0; i < ZONES_NUMBER; i++)
    {
        colour_to_set.rgb[i].r = (uint8_t)r;
        colour_to_set.rgb[i].g = (uint8_t)g;
        colour_to_set.rgb[i].b = (uint8_t)b;
    }
    send_led_colours(&colour_to_set);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;

}

esp_err_t stream_get_handler(httpd_req_t *req)
{
    lights_on = false;
    for (int i = 0; i < 300 && ambilight_running; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (ambilight_running)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Ambilight nie zatrzymał się");
        return ESP_FAIL;
    }
    init_camera_jpeg();

    camera_fb_t* fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t* _jpg_buf = NULL;
    char part_buf[64];

    // Ustawienie nagłówka z informacją o strumieniu MJPEG
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK)
    {
        ESP_LOGE("MJPEG", "Nie udało się ustawić informacji o MJPEG w nagłówku");
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    //ESP_LOGI("STREAM", "Strumieniowanie rozpoczęte");
    is_streaming = true;
    while(true)
    {
        fb = esp_camera_fb_get();
        if(!fb)
        {
            //ESP_LOGE("STREAM", "Błąd pobierania klatki");
            res = ESP_FAIL;
            break;
        }
        //ESP_LOGI("STREAM", "Wysyłam klatkę o rozmiarze: %zu bajtów", fb->len);
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;

        // Nagłówek zdjęcia
        if (res == ESP_OK)
        {
            size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        // Zdjęcie
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char*)_jpg_buf, _jpg_buf_len);
        }
        // Granica ramki
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }

        // Zwolnienie bufora
        esp_camera_fb_return(fb);

        vTaskDelay(pdMS_TO_TICKS(40));

        if(res != ESP_OK || lights_on == true)
        {
            break;
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);
    is_streaming = false;
    //ESP_LOGI("STREAM", "Strumień wideo zakończony");
    return res;
}

esp_err_t start_webserver(void)
{
    // Domyślna konfiguracja serwera HTTP
    httpd_config_t config_main = HTTPD_DEFAULT_CONFIG();
    config_main.server_port = 80;
    config_main.ctrl_port = 32768;
    config_main.uri_match_fn = httpd_uri_match_wildcard;
    
    httpd_handle_t server_main = NULL;

    // Uruchomienie serwera
    if (httpd_start(&server_main, &config_main) == ESP_OK)
    {
        httpd_uri_t save_markers_uri =
        {
            .uri = "/save_markers_position",
            .method = HTTP_POST,
            .handler = save_markers_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_main, &save_markers_uri);
        
        httpd_uri_t colour_pick_uri =
        {
            .uri = "/set_colour",
            .method = HTTP_POST,
            .handler = set_colour_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_main, &colour_pick_uri);

        httpd_uri_t turn_lights_uri =
        {
            .uri = "/turn_lights",
            .method = HTTP_POST,
            .handler = lights_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_main, &turn_lights_uri);
        
        httpd_uri_t file_server_uri = 
        {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = file_server_get_handler,
            .user_ctx = NULL,
        }; 
        httpd_register_uri_handler(server_main, &file_server_uri);
        
        //ESP_LOGI(HTTP, "Pomyślnie uruchomiono główny serwer HTTP.");
    }

    httpd_config_t config_stream = HTTPD_DEFAULT_CONFIG();
    config_stream.server_port = 81;
    config_stream.ctrl_port = 32769;
    config_stream.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server_stream;

    if(httpd_start(&server_stream, &config_stream) == ESP_OK)
    {
        httpd_uri_t stream_uri = 
        {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = stream_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server_stream, &stream_uri);
        //ESP_LOGI(HTTP, "Pomyślnie uruchomiono serwer HTTP do obsługi MJPEG.");
    }

    //ESP_LOGE(HTTP, "Serwer HTTP nie został uruchomiony.");
    return ESP_OK;
}
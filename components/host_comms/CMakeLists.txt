idf_component_register(SRCS "http_client.c" "message_handler.c" "wifi_interface.c" "nxjson.c" "context_dictionary.c" "http_server.c" "http_utils.c" "template_renderer.c"
                       REQUIRES esp_netif esp_wifi esp-tls nvs_flash esp_http_server wpa_supplicant
                       INCLUDE_DIRS "." ".." "../bsp" "../controller"
                       EMBED_TXTFILES server_root_cert_stafford.pem
                       EMBED_TXTFILES server_root_cert_rohde.pem
                       EMBED_TXTFILES web_pages/home.html
                       EMBED_TXTFILES web_pages/diagnostics.html
                       EMBED_TXTFILES web_pages/scan.html
                       EMBED_TXTFILES web_pages/connect.html
                       EMBED_TXTFILES web_pages/connect_request_ssid.html
                       EMBED_TXTFILES web_pages/connect_result.html
                       EMBED_TXTFILES web_pages/base.css
                       EMBED_TXTFILES web_pages/jquery.js
                       EMBED_TXTFILES web_pages/spin.js
                       EMBED_TXTFILES web_pages/wifi_0_locked.png
                       EMBED_TXTFILES web_pages/wifi_1_locked.png
                       EMBED_TXTFILES web_pages/wifi_2_locked.png
                       EMBED_TXTFILES web_pages/wifi_3_locked.png
                       EMBED_TXTFILES web_pages/wifi_4_locked.png
                       EMBED_TXTFILES web_pages/wifi_0_unlocked.png
                       EMBED_TXTFILES web_pages/wifi_1_unlocked.png
                       EMBED_TXTFILES web_pages/wifi_2_unlocked.png
                       EMBED_TXTFILES web_pages/wifi_3_unlocked.png
                      EMBED_TXTFILES web_pages/wifi_4_unlocked.png)



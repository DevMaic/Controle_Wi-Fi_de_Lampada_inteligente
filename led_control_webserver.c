/**
 * AULA IoT - Embarcatech - Ricardo Prates - 004 - Webserver Raspberry Pi Pico w - wlan
 *
 * Material de suporte
 * 
 * https://www.raspberrypi.com/documentation/pico-sdk/networking.html#group_pico_cyw43_arch_1ga33cca1c95fc0d7512e7fef4a59fd7475 
 */

#include <stdio.h>               // Biblioteca padrão para entrada e saída
#include <string.h>              // Biblioteca manipular strings
#include <stdlib.h>              // funções para realizar várias operações, incluindo alocação de memória dinâmica (malloc)

#include "pico/stdlib.h"         // Biblioteca da Raspberry Pi Pico para funções padrão (GPIO, temporização, etc.)
#include "hardware/adc.h"        // Biblioteca da Raspberry Pi Pico para manipulação do conversor ADC
#include "hardware/pwm.h"         // Biblioteca da Raspberry Pi Pico para manipulação de PWM (modulação por largura de pulso)
#include "pico/cyw43_arch.h"     // Biblioteca para arquitetura Wi-Fi da Pico com CYW43  

#include "lwip/pbuf.h"           // Lightweight IP stack - manipulação de buffers de pacotes de rede
#include "lwip/tcp.h"            // Lightweight IP stack - fornece funções e estruturas para trabalhar com o protocolo TCP
#include "lwip/netif.h"          // Lightweight IP stack - fornece funções e estruturas para trabalhar com interfaces de rede (netif)

// Credenciais WIFI - Tome cuidado se publicar no github!
#define WIFI_SSID "SEU_SSID"
#define WIFI_PASSWORD "SUA_SENHA"
#include "senhas.c"

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN   // GPIO do CI CYW43
#define LED_BLUE_PIN 12                 // GPIO12 - LED azul
#define LED_GREEN_PIN 11                // GPIO11 - LED verde
#define LED_RED_PIN 13                  // GPIO13 - LED vermelho

// Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
void gpio_led_bitdog(int);

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// Leitura da temperatura interna
float temp_read(void);

// Tratamento do request do usuário
void user_request(char **request);

// Função principal
int main()
{
    //Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();

    // Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
    gpio_led_bitdog(11);
    gpio_led_bitdog(12);
    gpio_led_bitdog(13);

    //Inicializa a arquitetura do cyw43
    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    // GPIO do CI CYW43 em nível baixo
    cyw43_arch_gpio_put(LED_PIN, 0);

    // Ativa o Wi-Fi no modo Station, de modo a que possam ser feitas ligações a outros pontos de acesso Wi-Fi.
    cyw43_arch_enable_sta_mode();

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
    }
    printf("Conectado ao Wi-Fi\n");

    // Caso seja a interface de rede padrão - imprimir o IP do dispositivo.
    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP - cria novos PCBs TCP. É o primeiro passo para estabelecer uma conexão TCP.
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    //vincula um PCB (Protocol Control Block) TCP a um endereço IP e porta específicos.
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    // Coloca um PCB (Protocol Control Block) TCP em modo de escuta, permitindo que ele aceite conexões de entrada.
    server = tcp_listen(server);

    // Define uma função de callback para aceitar conexões TCP de entrada. É um passo importante na configuração de servidores TCP.
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    // Inicializa o conversor ADC
    adc_init();
    adc_set_temp_sensor_enabled(true);

    while (true)
    {
        /* 
        * Efetuar o processamento exigido pelo cyw43_driver ou pela stack TCP/IP.
        * Este método deve ser chamado periodicamente a partir do ciclo principal 
        * quando se utiliza um estilo de sondagem pico_cyw43_arch 
        */
        cyw43_arch_poll(); // Necessário para manter o Wi-Fi ativo
        sleep_ms(100);      // Reduz o uso da CPU
    }

    //Desligar a arquitetura CYW43.
    cyw43_arch_deinit();
    return 0;
}

// -------------------------------------- Funções ---------------------------------

// Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
void gpio_led_bitdog(int PIN){
    gpio_set_function(PIN, GPIO_FUNC_PWM); //habilitar o pino GPIO como PWM
    uint slice = pwm_gpio_to_slice_num(PIN); //obter o canal PWM da GPIO
    pwm_set_clkdiv(slice, 38.1469); //define o divisor de clock do PWM
    pwm_set_wrap(slice, 65535); //definir o valor de wrap
    pwm_set_enabled(slice, true); //habilita o pwm no slice correspondente
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Tratamento do request do usuário - digite aqui
void user_request(char **request){
    static int ledIntensity = 65535; // Variável estática para armazenar a intensidade do LED
    static bool ledState = false;

    if (strstr(*request, "GET /more_brightness") != NULL)
    {
        if(ledIntensity <= 65535-65535/10 && ledState == true) {
            ledIntensity += 65535/10; // Aumenta a intensidade do LED
            pwm_set_gpio_level(13, ledIntensity); // 0 graus
            pwm_set_gpio_level(12, ledIntensity); // 0 graus
            pwm_set_gpio_level(11, ledIntensity); // 0 graus
            printf("Intensidade do LED: %d\n", ledIntensity);
        }
    }
    else if (strstr(*request, "GET /less_brightness") != NULL)
    {
        if(ledIntensity >= 65535/10 && ledState == true) {
            ledIntensity -= 65400/10; // Diminui a intensidade do LED
            pwm_set_gpio_level(13, ledIntensity); // 0 graus
            pwm_set_gpio_level(12, ledIntensity); // 0 graus
            pwm_set_gpio_level(11, ledIntensity); // 0 graus
            printf("Intensidade do LED: %d\n", ledIntensity);
        }
    }
    else if (strstr(*request, "GET /light_on") != NULL)
    {
        if(ledState == false) {
            pwm_set_gpio_level(13, 65535); // 90 graus
            pwm_set_gpio_level(12, 63535); // 90 graus
            pwm_set_gpio_level(11, 65535); // 90 graus
            ledState = true; // Atualiza o estado do LED
            ledIntensity = 65535; // Reseta a intensidade do LED
            printf("Intensidade do LED: %d\n", ledIntensity);
        }
    }
    else if (strstr(*request, "GET /light_off") != NULL)
    {
        if(ledState == true) {
            pwm_set_gpio_level(13, 0); // 90 graus
            pwm_set_gpio_level(12, 0); // 90 graus
            pwm_set_gpio_level(11, 0); // 90 graus
            ledState = false; // Atualiza o estado do LED
            ledIntensity = 0; // Reseta a intensidade do LED
            printf("Intensidade do LED: %d\n", ledIntensity);
        }
    }
};

// Leitura da temperatura interna
float temp_read(void){
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;
        return temperature;
}

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Alocação do request na memória dinámica
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Tratamento de request - Controle dos LEDs
    user_request(&request);
    
    // Leitura da temperatura interna
    float temperature = temp_read();

    // Cria a resposta HTML
    char html[1024];

    // Instruções html do webserver
    snprintf(html, sizeof(html), // Formatar uma string e armazená-la em um buffer de caracteres
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title>EmbarcaTech - Smart Light</title>\n"
             "<style>\n"
             "body { background-color: #b5e5fb; font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
             "h1 { font-size: 64px; margin-bottom: 30px; }\n"
             "button { background-color: LightGray; font-size: 36px; margin: 10px; padding: 20px 40px; border-radius: 10px; }\n"
             ".temperature { font-size: 48px; margin-top: 30px; color: #333; }\n"
             ".buttons { display: flex; justify-content: center; margin-top: 30px;}\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"
             "<h1>EmbarcaTech - Smart Light</h1>\n"
             "<form action=\"./light_on\"><button>Ligar luz</button></form>\n"
             "<form action=\"./light_off\"><button>Desligar luz</button></form>\n"
             "<div class=\"buttons\">\n"
                "<form action=\"./less_brightness\"><button> - </button></form>\n"
                "<p class=\"temperature\">Intensidade</p>\n"
                "<form action=\"./more_brightness\"><button> + </button></form>\n"
             "<div>\n"
             "</body>\n"
             "</html>\n",
             temperature);

    // Escreve dados para envio (mas não os envia imediatamente).
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

    // Envia a mensagem
    tcp_output(tpcb);

    //libera memória alocada dinamicamente
    free(request);
    
    //libera um buffer de pacote (pbuf) que foi alocado anteriormente
    pbuf_free(p);

    return ERR_OK;
}


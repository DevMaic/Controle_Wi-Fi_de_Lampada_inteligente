#include <stdio.h>               // Biblioteca padrão para entrada e saída
#include <string.h>              // Biblioteca manipular strings
#include <stdlib.h>              // funções para realizar várias operações, incluindo alocação de memória dinâmica (malloc)

#include "hardware/i2c.h"
// #include "hardware/clocks.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

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
#define LED_PIN CYW43_WL_GPIO_LED_PIN   // GPIO do CI CYW43

ssd1306_t ssd;                                                // Inicializa a estrutura do display

void gpioPwmInit(int); // Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err); // Função de callback ao aceitar conexões TCP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err); // Função de callback para processar requisições HTTP
void user_request(char **request); // Tratamento do request do usuário

// Função principal
int main() {
    //Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
    gpioPwmInit(11);
    gpioPwmInit(12);
    gpioPwmInit(13);

    //Inicializa a arquitetura do cyw43
    while (cyw43_arch_init()) {
        ssd1306_draw_string(&ssd, "Falha ao inicializar Wi-Fi\n", 0, 0);
        sleep_ms(100);
        // return -1;
    }

    // GPIO do CI CYW43 em nível baixo
    cyw43_arch_gpio_put(LED_PIN, 0);

    // Ativa o Wi-Fi no modo Station, de modo a que possam ser feitas ligações a outros pontos de acesso Wi-Fi.
    cyw43_arch_enable_sta_mode();

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    ssd1306_draw_string(&ssd, "Conectando WiFi\n", 0, 0);
    ssd1306_send_data(&ssd); // Envia os dados para o display
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        ssd1306_draw_string(&ssd, "Falha no WiFi\n", 0, 0);
        ssd1306_send_data(&ssd); // Envia os dados para o display
        sleep_ms(100);
        // return -1; // Encerra o programa se não conseguir conectar de primeira, péssimo para o usuário, mas Ricardo Exigiu
    }
    ssd1306_draw_string(&ssd, "WiFi conectado\n", 0, 12);
    ssd1306_send_data(&ssd); // Envia os dados para o display

    // Caso seja a interface de rede padrão - imprimir o IP do dispositivo.
    if (netif_default) {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr)); // Imprime o IP do dispositivo
        char str[50];
        sprintf(str, "IP%s\n", ipaddr_ntoa(&netif_default->ip_addr)); // Formata a string com o IP
        ssd1306_draw_string(&ssd, str, 0, 24); // Desenha a string no display
        ssd1306_send_data(&ssd); // Envia os dados para o display
    }

    // Configura o servidor TCP - cria novos PCBs TCP. É o primeiro passo para estabelecer uma conexão TCP.
    struct tcp_pcb *server = tcp_new();
    if (!server) {
        printf("Falha ao criar servidor TCP\n");
        ssd1306_draw_string(&ssd, "Falha ao criar servidor TCP\n", 0, 0);
        ssd1306_send_data(&ssd); // Envia os dados para o display
        return -1;
    }

    //vincula um PCB (Protocol Control Block) TCP a um endereço IP e porta específicos.
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Falha ao associar servidor TCP à porta 80\n");
        ssd1306_draw_string(&ssd, "Falha ao associar servidor TCP à porta 80\n", 0, 0);
        ssd1306_send_data(&ssd); // Envia os dados para o display
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

    while (true) {
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
void gpioPwmInit(int PIN) {
    gpio_set_function(PIN, GPIO_FUNC_PWM); //habilitar o pino GPIO como PWM
    uint slice = pwm_gpio_to_slice_num(PIN); //obter o canal PWM da GPIO
    pwm_set_clkdiv(slice, 38.1469); //define o divisor de clock do PWM
    pwm_set_wrap(slice, 65535); //definir o valor de wrap
    pwm_set_enabled(slice, true); //habilita o pwm no slice correspondente
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Tratamento do request do usuário - digite aqui
void user_request(char **request) {
    static int ledIntensity = 65535; // Variável estática para armazenar a intensidade do LED
    static bool ledState = false; // Variável estática para indicar se o led está ligado

    // Entra nesse if se o request for para aumentar a intensidade do LED
    if (strstr(*request, "GET /more_brightness") != NULL) {
        // Verifica se a intensidade do LED é menor que 90% do valor de wrap pra garantir que n daremos overflow no mesmo, e se o LED está ligado
        if(ledIntensity <= 65535-65535/10 && ledState == true) {
            ledIntensity += 65535/10; // Aumenta a intensidade do LED
            pwm_set_gpio_level(13, ledIntensity); // 0 graus
            pwm_set_gpio_level(12, ledIntensity); // 0 graus
            pwm_set_gpio_level(11, ledIntensity); // 0 graus
            printf("Intensidade do LED: %d\n", ledIntensity);
        }
    // Entra nesse if se o request for para diminuir a intensidade do LED
    } else if (strstr(*request, "GET /less_brightness") != NULL) {
        // Verifica se a intensidade do LED é maior que 10% do valor de wrap pra garantir que n teremos valores < 0 no mesmo, e se o LED está ligado
        if(ledIntensity >= 65535/10 && ledState == true) {
            ledIntensity -= 65535/10; // Diminui a intensidade do LED
            pwm_set_gpio_level(13, ledIntensity); // 0 graus
            pwm_set_gpio_level(12, ledIntensity); // 0 graus
            pwm_set_gpio_level(11, ledIntensity); // 0 graus
            printf("Intensidade do LED: %d\n", ledIntensity);
        }
    // Entra nesse if se o request for para ligar o LED
    } else if (strstr(*request, "GET /light_on") != NULL) {
        // Verifica se o LED está desligado para poder ligá-lo
        if(ledState == false) {
            pwm_set_gpio_level(13, 65535); // 90 graus
            pwm_set_gpio_level(12, 63535); // 90 graus
            pwm_set_gpio_level(11, 65535); // 90 graus
            ledState = true; // Atualiza o estado do LED
            ledIntensity = 65535; // Reseta a intensidade do LED
            printf("Intensidade do LED: %d\n", ledIntensity);
        }
    // Entra nesse if se o request for para desligar o LED
    } else if (strstr(*request, "GET /light_off") != NULL) {
        // Verifica se o LED está ligado para poder desligá-lo
        if(ledState == true) {
            pwm_set_gpio_level(13, 0); // 90 graus
            pwm_set_gpio_level(12, 0); // 90 graus
            pwm_set_gpio_level(11, 0); // 90 graus
            ledState = false; // Atualiza o estado do LED
            ledIntensity = 0; // Reseta a intensidade do LED
            printf("Intensidade do LED: %d\n", ledIntensity);
        }
    }

    // Exibe o nível de brilho atual da lâmpada
    ssd1306_draw_string(&ssd, "[", 5, 48);
    ssd1306_send_data(&ssd); // Envia os dados para o display
    for(int i = 0; i < 10; i++) { // Limpa o buffer do display
        // ledIntensity/65535.0) calcula a porcentagem atual de brilho
        // isso *10 indica quantos dos 10 níveis devemos desenhar
        // No que falta pra completar 10, entramos no else e desenhamos um espaço
        if(i<(int)((ledIntensity/65535.0)*10)) {
            // i*10+15 simboliza que desenharemos um - a cada 10pixels e tudo isso começando a 15 pixels da borda  
            ssd1306_draw_char(&ssd, '-', (i*10+15), 48);
            ssd1306_send_data(&ssd); // Envia os dados para o display
        } else {
            ssd1306_draw_char(&ssd, ' ', (i*10+15), 48);
            ssd1306_send_data(&ssd); // Envia os dados para o display
        }
    }
    ssd1306_draw_string(&ssd, "]", 115, 48); // Desenha a string no display
    ssd1306_send_data(&ssd);
};

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
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
        "body { background-color:rgb(116, 116, 116); font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
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
        "</html>\n"
    );

    
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY); // Escreve dados para envio (mas não os envia imediatamente).
    tcp_output(tpcb); // Envia a mensagem

    free(request); //libera memória alocada dinamicamente
    pbuf_free(p); //libera um buffer de pacote (pbuf) que foi alocado anteriormente

    return ERR_OK;
}

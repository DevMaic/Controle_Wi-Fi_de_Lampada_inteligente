<h1>
  <p align="center" width="100%">
    <img width="30%" src="https://softex.br/wp-content/uploads/2024/09/EmbarcaTech_logo_Azul-1030x428.png">
  </p>
</h1>

# ✨Tecnologias
Esse projeto foi desenvolvido com as seguintes tecnologias.
- Placa Raspberry Pi Pico W
- Raspberry Pi Pico SDK
- C/C++

# 💻Projeto
Projeto Desenvolvido durante a residência em microcontrolados e sistemas embarcados para estudantes de nível superior ofertado pela CEPEDI e SOFTEX, polo Juazeiro-BA, na Universidade Federal do Vale do São Francisco (UNIVASF), que tem como objetivo simular o controle de uma lâmpada inteligente por Wi-Fi utilizando a placa BitDogLab com Raspberry PI-Pico, e fortalecer o aprendizado sobre IOT e Wi-Fi na plataforma supracitada.

# 🚀Como rodar
### **Softwares Necessários**
1. **VS Code** com a extensão **Raspberry Pi Pico** instalada.
2. **CMake** e **Ninja** configurados.
3. **SDK do Raspberry Pi Pico** corretamente configurado.

### **Clonando o Repositório**
Para começar, clone o repositório no seu computador:
```bash
git clone https://github.com/DevMaic/Controle_Wi-Fi_de_Lampada_inteligente
cd Controle_Wi-Fi_de_Lampada_inteligente
```
---


### **Execução na Placa BitDogLab**
### **1. Substituindo nome e senha da rede Wi-Fi**
1. Substitua no cabeçalho do arquivo `led_control_webserver.c` o nome da sua rede Wi-Fi 2.4Ghz e a senha da mesma.
#### **2. Upload de Arquivo `led_control_webserver.uf2`**
1. Importe o projeto utilizando a extensão do VSCode, e o compile.
2. Abra a pasta `build` que será gerada na compilação.
3. Aperte o botão **BOOTSEL** no microcontrolador Raspberry Pi Pico W.
4. Ao mesmo tempo, aperte o botão de **Reset**..
5. Mova o arquivo `led_control_webserver.uf2` para a placa de desenvolvimento.
#### **4. Acompanhar Execução do Programa**
1. Assim que o código estiver na placa, acesse o endereço fornecido por meio da serial da BitDogLab.
2. Pressione os botões de ligar e desligar luz, os leds RGB da placa obedecerão esses comandos, gerando em conjunto a cor branca.
3. Enquanto ligados pressione os botões "+" e "-" intensidade para observar a mudança no PWM dos leds.
   
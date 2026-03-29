/* ==================================================================================

    Linguagem Arduino,
    Datalogger temperatura JA versão sem fio


    Autor: Sergio R Marchetti
    Data: 14/03/2026
    revisão 0
    

    Placa Heltec Wifi Lora 32 V3.2
    processador ESP32

    Etapas:
    1. testar sensor DS18B20            ok
    2. configurar Display               ok
    3. Fazer confirgurações WIFI - OTA  testar
    4. Configurar publicação banco      ok
    5. Avaliar publicar POST / MQTT
    6. testar Bateria                   ok                
    7. Testar deep sleep                ok
        7.1   Desligamento Radio LoRa   ok
        7.2   WiFi Estático             ok
        7.3   Fluxo Acordar -> Ler Sensor -> medir Bateria -> Conectar WiFi -> 
              -> Enviar dados -> checar Ota -> Dormir.

    8. verificar Lora

  =========================================================================== */

/*============================================================================
 *      Definições específicas de cada dispositivo
 *============================================================================*/
//const int dispositivo = 600;     //acrescenta ao sensor do dispositivo
#define DispositivoName "Teste"  //HostName e OTA

/* ========================================================================== */

/* ========================================================================== */
/* ---------------------------------- Bibliotecas --------------------------- */

#ifndef GPIO_IS_VALID_GPIO
#define GPIO_IS_VALID_GPIO(pin) ((pin) >= 0 && (pin) <= 48)
#endif
//#include <ArduinoOTA.h>
#include <OneWire.h>  //sensor DS18B20
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPUpdate.h>  //lib troca de OTA para atualizar o firmware via HTTP
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <HT_SSD1306Wire.h>
#include <Preferences.h>  //utilizada no lugar da EEPROM
#include <LoRaWan_APP.h>


/* ========================================================================== */

/* ========================================================================== */
/* --------------------------------Mapeamento Hardware----------------------- */

#define led 35  //GPIO ?
#define oneWire_pin 2

#define BAT_ADC_PIN 1    // Pino de leitura (ADC)
#define VEXT_CONTROL 37  // Pino que ativa o divisor resistivo e periféricos
//#define ds18b20 GPIO 45  //pino 6 lado esquerdo
// Configurações de Tempo (15 minutos = 15 * 60 * 10^6 microssegundos)
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 30  //900 = 15min

/* ========================================================================== */

/* ========================================================================== */
/* ------------------------- Instanciando objetos --------------------------- */

//WiFiClient client;             //para conecção com banco de dados
WebServer server(80);  // Cria o objeto 'server' na porta 80
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
// addr , freq , i2c group , resolution , rst
Preferences p;
OneWire ds(oneWire_pin);  // GPIO2 pino 13 da placa

/* ========================================================================== */

/* ========================================================================== */
/* -------------------------- Macros e Constantes --------------------------- */

#define DEBUG true  //Comentar para não imprimir serial

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINT2(x, y) Serial.print(x, y)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_BEGIN(x) Serial.begin(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINT2(x, y)
#define DEBUG_PRINTLN(x)
#define DEBUG_BEGIN(x)
#endif

//#define EEPROM_SIZE 64
//#define EEPROM_SSID_OFFSET 0
//#define EEPROM_PASSWORD_OFFSET 32
const String currentVersion = "1.0.4";
const char* servidorOTA = "http://10.0.0.11/firmware/v2.bin";

//para conexão com banco de dados
const char http_site[] = "http://192.168.15.19:3001/v2/gravasensor"; //"http://10.0.0.11/php/gravabanco.php";
const int http_port = 3001;

const float fatorVoltagem = 1.003367;
/* ========================================================================== */

/* ========================================================================== */
/* ------------------------- Variáveis Globais ------------------------------ */

//String ssid = "";
//String password = "";
String enderecoFormatado;
float temp;
float offSetDHT = 0.0;  //offSet do sensor DHT21
float bateria = 0.0;
byte addr[8];  //endereço do sensor DS18B20
// Configuração de IP Fixo (Otimiza conexão WiFi)
//IPAddress local_IP(192, 168, 15, 12);
//IPAddress gateway(192, 168, 15, 1);
//IPAddress subnet(255, 255, 255, 0);
/* ========================================================================== */

/* ========================================================================== */
/* ---------------------------- Protótipo das funções ----------------------- */

int enviaDados();  //Grava banco de dados

void accessPoint();   // Habilita como AccessPoint
void handleConfig();  // Recebe SSID e Senha
void handleRoot();    // Exibe página inicial

/* ========================================================================== */


int enviaDados() {
  //String date = dateTimeStr(time(NULL), -3);
  int ret = 0;
  HTTPClient http;
  WiFiClient client;

  if (WiFi.status() == WL_CONNECTED) {

    //envia versão atual junto com os dados, para controle de versão do firmware
    String dados_a_enviar = "{\"sensor\":\"" + enderecoFormatado + "\", \"valor\":" + String(temp) + 
      ", \"bateria\":" + String(bateria) + ", \"version\":\"" + currentVersion + "\"}";
    //"sensor=" + String(dispositivo) + "&valor=" + String(temp);  //+ "&date=" + date;
    DEBUG_PRINTLN(dados_a_enviar);
    http.begin(client, http_site);
    http.addHeader("Content-Type", "application/json");  // Definido para Json..

    int httpCode = http.POST(dados_a_enviar);

    if (httpCode == 200) {
      String response = http.getString();
      JsonDocument doc;
      ret = 1;
      deserializeJson(doc, response);

      if (doc["update"] == true) {
        String downloadUrl = doc["url"];
        DEBUG_PRINTLN("Nova versão detectada. Atualizando...");

        // O httpUpdate cuida do download e reinicialização automática
        httpUpdate.update(client, downloadUrl);
        display.drawString(60, 50, "Sucesso");
      }
    } else {
      display.drawString(60, 50, "Falha");
      // apagar para tratar quando tiver falha no envio
      ret = 1;
    }
    display.display();
  }
  http.end();  // libero recursos
  return ret;
}  //enviaDados

/*
void verificarAtualizacao(WiFiClient& client) {
  // Esse comando baixa o arquivo, verifica a integridade e reinicia o ESP32
  // Substitua pela URL do seu arquivo .bin
  t_httpUpdate_return ret = httpUpdate.update(client, servidorOTA);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("Erro no OTA: (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      DEBUG_PRINTLN("Nenhuma atualização disponível.");
      break;
    case HTTP_UPDATE_OK:
      DEBUG_PRINTLN("Atualização concluída com sucesso!");
      break;
  }
} */

void handleRoot() {
  // Exibindo a página inicial
  display.drawString(0, 40, "server 192.168.4.1");
  display.display();
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{font-family:sans-serif; padding:20px;} input{margin-bottom:10px; width:100%; padding:8px;}</style></head><body>";
  html += "<h1>Configuracao Wi-Fi</h1>";
  html += "<form method='post' action='/config'>";

  html += "SSID:<br><input type='text' name='ssid' required><br>";
  html += "Senha:<br><input type='password' name='password'><br><br>";

  html += "<b>IP Estatico (Opcional):</b><br>";
  html += "IP:<br><input type='text' name='ip' placeholder='192.168.1.100' pattern='^((\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])$'><br>";
  html += "Gateway:<br><input type='text' name='gateway' placeholder='192.168.1.1' pattern='^((\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])$'><br>";
  html += "Subnet:<br><input type='text' name='subnet' placeholder='255.255.255.0' pattern='^((\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d{1,2}|1\\d\\d|2[0-4]\\d|25[0-5])$'><br><br>";

  html += "<input type='submit' value='Salvar e Reiniciar'>";
  html += "</form></body></html>";

  server.send(200, "text/html", html);
  /*
  String html = "<html><body>";
  html += "<h1>Configuracao do Wi-Fi</h1>";
  html += "<p>Por favor, configure as informacoes de Wi-Fi.</p>";
  html += "<form method='post' action='/config'>";
  html += "SSID: <input type='text' name='ssid'><br>";
  html += "Senha: <input type='password' name='password'><br><br>";
  html += "<input type='submit' value='Salvar'>";
  html += "</form></body></html>";

  server.send(200, "text/html", html);*/
}  //end handleRoot

void handleConfig() {
  // Recebendo as informações de SSID e senha do usuário
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String ipStr = server.arg("ip");
  String gwStr = server.arg("gateway");
  String subStr = server.arg("subnet");

  // Validação: Se o IP foi preenchido, os outros campos de rede também devem ser válidos
  if (ipStr.length() > 0) {
    IPAddress t_ip, t_gw, t_sub;
    if (!t_ip.fromString(ipStr) || !t_gw.fromString(gwStr) || !t_sub.fromString(subStr)) {
      server.send(400, "text/html", "<html><body><h1>Erro!</h1><p>Enderecos IP invalidos.</p><a href='/'>Voltar</a></body></html>");
      return;
    }
  }

  p.begin("wifi-config", false);
  p.putString("ssid", ssid);
  p.putString("password", password);
  p.putString("ip", ipStr);
  p.putString("gateway", gwStr);
  p.putString("subnet", subStr);
  p.end();

  server.send(200, "text/html", "<html><body><h1>Sucesso!</h1><p>Reiniciando o ESP32...</p></body></html>");

  delay(200);

  // Reiniciando o dispositivo para se conectar à rede Wi-Fi
  ESP.restart();
}  //end HandleConfig

void accessPoint() {
  //DEBUG_PRINTLN("Starting Access Point...");
  display.drawString(0, 50, "Modo AP...");
  display.display();

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP8266AP", "123456789");
  //DEBUG_PRINTLN("Modo Access Point ativado.");
  server.on("/", HTTP_GET, handleRoot);           // Definindo a página inicial
  server.on("/config", HTTP_POST, handleConfig);  // Definindo a página de configuração
  server.begin();                                 // Iniciando o servidor
}  //end acessPoint

void conectaWiFi() {

  char htn[30];
  sprintf(htn, "DatLog %S", DispositivoName);

  p.begin("wifi-config", true);
  //delay(50);
  String ssid = p.getString("ssid", "");
  String password = p.getString("password", "");
  String s_ip = p.getString("ip", "");
  String s_gw = p.getString("gateway", "");
  String s_sub = p.getString("subnet", "");
  p.end();
  /*
  DEBUG_PRINT("Dados WiFi: ");
  DEBUG_PRINT(ssid);
  DEBUG_PRINT(" : ");
  DEBUG_PRINTLN(password);
  */

  if (ssid != "" && password != "") {
    int reinicio = 0;
      int tentativas = 0;
    while (reinicio < 5) {

      WiFi.mode(WIFI_STA);
      // Se houver configuração de IP fixo salva, aplica antes do begin
      if (s_ip.length() < 5) {
        IPAddress ip, gw, sub;
        ip.fromString(s_ip);
        gw.fromString(s_gw);
        sub.fromString(s_sub);
        WiFi.config(ip, gw, sub);
        DEBUG_PRINTLN("IP Fixo");
        DEBUG_PRINTLN(s_ip);
      }

      WiFi.begin(ssid.c_str(), password.c_str());
      tentativas = 0;
      // Lógica de timeout para voltar ao modo AP se falhar
      while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
        DEBUG_PRINT(".");
        delay(400);
        tentativas++;
      }
      //WiFi.disconnect();
      DEBUG_PRINTLN("reinicio");
      reinicio++;
      if(WiFi.status() == WL_CONNECTED){
        reinicio = 6;
        break;
      }
    }
    if (WiFi.status() != WL_CONNECTED) {
      accessPoint();
      server.handleClient();

    }
    WiFi.hostname(htn);
    /*
    // connect to WiFi using saved credentials
    //display.drawString(0, 50, "Conectando...");
    //display.display();
    int tempo = 0;
    //WiFi.mode(WIFI_STA);
    if (!WiFi.config(local_IP, gateway, subnet)) {
      WiFi.disconnect();
      accessPoint();
    }
    WiFi.begin(ssid, password);
    //DEBUG_PRINT("Connecting to ");
    //DEBUG_PRINT(ssid);
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(led, LOW);
      delay(400);
      digitalWrite(led, HIGH);
      int progress = (tempo) % 6;
      //display.drawProgressBar(0, 32, 12, 10, progress);
      //display.display();
      DEBUG_PRINT(".");
      tempo++;
      if (tempo > 100) {
        WiFi.disconnect();
        accessPoint();
        break;
      }
    }

    WiFi.hostname(htn);
    //DEBUG_PRINTLN("");
    DEBUG_PRINTLN("WiFi connected");
    DEBUG_PRINTLN("IP address: ");
    DEBUG_PRINTLN(WiFi.localIP());
    */
  } else {
    // start Access Point mode
    accessPoint();
    server.handleClient();
  }
}

void leSensor() {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[9];

  //p.begin("DS18B20", "false");  //inicializa preferences

  p.begin("wifi-config", false);  //inicializa em modo leitura e escrita
  p.getBytes("addr", addr, 8);
  //teste

  if (addr[0] != 0x28) {

    if (!ds.search(addr)) {
      ds.reset_search();
      DEBUG_PRINTLN("Fim daleitura");
      delay(100);
      //return;
    }
    DEBUG_PRINTLN("Grava endereco");
    p.putBytes("addr", addr, 8);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
    DEBUG_PRINTLN("CRC is not valid!");
    return;
  }

  enderecoFormatado = addrParaString(addr);

  //display.clear();
  //display.drawString(0, 10, enderecoFormatado);
  //display.display();
  DEBUG_PRINTLN(enderecoFormatado);

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);  // start conversion, with parasite power on at the end

  delay(1000);  // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);  // Read Scratchpad

  DEBUG_PRINT("  Data = ");
  DEBUG_PRINT2(present, HEX);
  DEBUG_PRINT(" ");

  for (i = 0; i < 9; i++) {  // we need 9 bytes
    data[i] = ds.read();
    DEBUG_PRINT2(data[i], HEX);
    DEBUG_PRINT(" ");
  }

  DEBUG_PRINT(" CRC=");
  DEBUG_PRINT2(OneWire::crc8(data, 8), HEX);
  DEBUG_PRINTLN();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3;  // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;       // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3;  // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1;  // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }

  temp = (float)raw / 16.0;
  DEBUG_PRINT("  Temperature = ");
  DEBUG_PRINT(temp);
  DEBUG_PRINTLN(" Celsius, ");
  display.setFont(ArialMT_Plain_16);

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  char bfTemp[20];
  sprintf(bfTemp, "%.2f °C", temp);
  display.drawString(64, 30, bfTemp);
  display.display();
  //delay(1000);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}  //end le sensor

String addrParaString(byte* sensorAddress) {
  char enderecoHex[17]; // 16 chars + null terminator
  snprintf(enderecoHex, sizeof(enderecoHex),
    "%02X%02X%02X%02X%02X%02X%02X%02X",
    sensorAddress[0], sensorAddress[1],
    sensorAddress[2], sensorAddress[3],
    sensorAddress[4], sensorAddress[5],
    sensorAddress[6], sensorAddress[7]
  );
  return enderecoHex;
}  //end addrParaString

float lerVoltagemBateria() {
  // 1. Ativa o Vext (Na V3, LOW ativa o regulador de periféricos/divisor)
  pinMode(VEXT_CONTROL, OUTPUT);
  digitalWrite(VEXT_CONTROL, HIGH);
  delay(10);  // Pequena pausa para estabilizar a tensão

  // 2. Configura a atenuação para ler até ~3.9V ou mais
  // O ESP32-S3 tem um ADC que precisa de calibração para precisão real
  analogReadResolution(12);  // 0-4095
  analogSetAttenuation(ADC_11db);

  uint16_t raw = analogReadMilliVolts(BAT_ADC_PIN);

  // 3. Converte para Voltagem
  // O divisor da Heltec V3 é de 390k / 100k (fator de ~4.9)
  // Multiplicamos pelo fator de escala do divisor e pela referência do ADC (3.3V)
  float voltagem = (float)raw * 490 / 100000 * fatorVoltagem;  // Ajuste o 4.02 conforme seu multímetro
  DEBUG_PRINTLN(raw);

  // 4. Desliga o Vext para economizar energia no Deep Sleep
  digitalWrite(VEXT_CONTROL, LOW);

  return voltagem;
}

void dormir() {
  display.drawString(0, 0, String(millis()));
  display.display();
  delay(600);

  DEBUG_PRINTLN(millis());
  //digitalWrite(Vext, HIGH);   //Desliga o pino VEX
  Serial.flush();
  uint32_t tempSleep = (millis() / 1000) < TIME_TO_SLEEP ? TIME_TO_SLEEP - (millis() / 1000) : TIME_TO_SLEEP;
  esp_sleep_enable_timer_wakeup(tempSleep * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}
/* ========================================================================= */
/* --------------------------------- Setup --------------------------------- */
void setup() {
  //pino para ligar o display e o Radio e periféricos
  pinMode(Vext, OUTPUT);
  pinMode(led, OUTPUT);

  digitalWrite(Vext, LOW);
  delay(10);
  // 1. Inicializa o rádio LoRa apenas para colocá-lo em Deep Sleep
  // Isso é essencial na Heltec V3 para cortar o consumo do SX1262
  //Radio.Init();
  Radio.Sleep();

  // Open serial communications and wait for port to open:
  DEBUG_BEGIN(115200);
  display.init();
  display.clear();
  //display.display();
  display.setContrast(122);

  DEBUG_PRINTLN(esp_sleep_get_wakeup_cause());

  // 2. Leitura do sensor
  digitalWrite(led, LOW);
  leSensor();
  digitalWrite(led, HIGH);

  // 3. Le bateria
  bateria = lerVoltagemBateria();
  char sBat[30];
  sprintf(sBat, "Voltagem: %.2f V", bateria);
  display.drawString(0, 10, sBat);
  display.display();
  DEBUG_PRINTLN(bateria);

  //  4. Conecta WiFi
  conectaWiFi();
  if (enviaDados())
    dormir();
  //digitalWrite(led, HIGH);

  // 5. DeepSleep - movido para ser chamado via enviaDados

}  //end setup

void loop() {
  server.handleClient();
  delay(10);
  /*
  if (WiFi.status() == WL_CONNECTED) {
    //verificar para eliminar ou inibir
    display.clear();
    display.drawString(0, 10, "  Conectado");
    display.display();

  } else {
    // Se não estiver conectado à rede Wi-Fi, lidando com as requisições do cliente
    display.drawString(0, 40, "Desconectado");
    display.display();
    //accessPoint();
    server.handleClient();
  }
  //leSensor();
  delay(10);
  */
}

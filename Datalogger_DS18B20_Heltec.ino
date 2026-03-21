/* ==================================================================================

    Linguagem Arduino,
    Datalogger temperatura JA versão sem fio


    Autor: Sergio R Marchetti
    Data: 14/03/2026
    revisão 0
    

    Placa Heltec Wifi Lora 32 V3.2
    processador ESP32

    Etapas:
    1. testar sensor DS18B20
    2. configurar Display
    3. Fazer confirgurações WIFI - OTA
    4. Configurar publicação banco
    5. Avaliar publicar POST / MQTT
    6. testar Bateria
    7. Testar deep sleep

    8. verificar Lora

  =========================================================================== */

/*============================================================================
 *      Definições específicas de cada dispositivo
 *============================================================================*/
const int dispositivo = 600;     //acrescenta ao sensor do dispositivo
#define DispositivoName "Teste"  //HostName e OTA

/* ========================================================================== */

/* ========================================================================== */
/* ---------------------------------- Bibliotecas --------------------------- */

#ifndef GPIO_IS_VALID_GPIO
#define GPIO_IS_VALID_GPIO(pin) ((pin) >= 0 && (pin) <= 48)
#endif
//#include <ArduinoOTA.h>
#include <OneWire.h>  //sensor DS18B20
//#include <Arduino.h>
#include <WiFi.h>
#include <HTTPUpdate.h>  //lib troca de OTA para atualizar o firmware via HTTP
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <HT_SSD1306Wire.h>
#include <Preferences.h>  //utilizada no lugar da EEPROM


/* ========================================================================== */

/* ========================================================================== */
/* --------------------------------Mapeamento Hardware----------------------- */

#define led LED_BUILTIN  //GPIO ?
#define oneWire_pin 2
//#define ds18b20 GPIO 45  //pino 6 lado esquerdo
// Configurações de Tempo (15 minutos = 15 * 60 * 10^6 microssegundos)
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 900  //900 = 15min

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

//#define EEPROM_SIZE 64
//#define EEPROM_SSID_OFFSET 0
//#define EEPROM_PASSWORD_OFFSET 32
const String currentVersion = "1.0.4";
const char* servidorOTA = "http://10.0.0.11/firmware/v2.bin";

//para conexão com banco de dados
const char http_site[] = "http://10.0.0.11/php/gravabanco.php";
const int http_port = 8080;

/* ========================================================================== */

/* ========================================================================== */
/* ------------------------- Variáveis Globais ------------------------------ */

String ssid = "";
String password = "";
float temp;
float offSetDHT = 0.0;  //offSet do sensor DHT21

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
    String dados_a_enviar = "{\"sensor\":\"" + String(dispositivo) + "\", \"valor\":" + String(temp) + "\", \"version\":\"" + currentVersion + "\"}";
    //"sensor=" + String(dispositivo) + "&valor=" + String(temp);  //+ "&date=" + date;
    http.begin(client, http_site);
    http.addHeader("Content-Type", "application/json");  // Definido para Json..

    int httpCode = http.POST(dados_a_enviar);

    if (httpCode == 200) {
      String response = http.getString();
      JsonDocument doc;
      deserializeJson(doc, response);

      if (doc["update"] == true) {
        String downloadUrl = doc["url"];
        Serial.println("Nova versão detectada. Atualizando...");

        // O httpUpdate cuida do download e reinicialização automática
        httpUpdate.update(client, downloadUrl);
      }
    }
  }
  http.end();  // libero recursos
  return ret;
}  //enviaDados

void verificarAtualizacao(WiFiClient& client) {
  // Esse comando baixa o arquivo, verifica a integridade e reinicia o ESP32
  // Substitua pela URL do seu arquivo .bin
  t_httpUpdate_return ret = httpUpdate.update(client, servidorOTA);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("Erro no OTA: (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("Nenhuma atualização disponível.");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("Atualização concluída com sucesso!");
      break;
  }
}

void handleRoot() {
  // Exibindo a página inicial
  display.drawString(0, 40, "server");
  display.display();
  String html = "<html><body>";
  html += "<h1>Configuracao do Wi-Fi</h1>";
  html += "<p>Por favor, configure as informacoes de Wi-Fi.</p>";
  html += "<form method='post' action='/config'>";
  html += "SSID: <input type='text' name='ssid'><br>";
  html += "Senha: <input type='password' name='password'><br><br>";
  html += "<input type='submit' value='Salvar'>";
  html += "</form></body></html>";

  server.send(200, "text/html", html);
}  //end handleRoot

void handleConfig() {
  // Recebendo as informações de SSID e senha do usuário
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  p.begin("wifi-config", false);
  p.putString("ssid", ssid);
  p.putString("password", password);

  p.end();
  // Salvar as informações na memória EEPROM ou SPIFFS
  /*strncpy(::ssid, ssid.c_str(), sizeof(::ssid) - 1);
  strncpy(::password, password.c_str(), sizeof(::password) - 1);


  EEPROM.put(EEPROM_SSID_OFFSET, ::ssid);
  EEPROM.put(EEPROM_PASSWORD_OFFSET, ::password);

  EEPROM.commit();
  */
  delay(200);

  // Reiniciando o dispositivo para se conectar à rede Wi-Fi
  ESP.restart();
}  //end HandleConfig

void accessPoint() {
  //Serial.println("Starting Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP8266AP", "123456789");
  //Serial.println("Modo Access Point ativado.");
  server.on("/", HTTP_GET, handleRoot);           // Definindo a página inicial
  server.on("/config", HTTP_POST, handleConfig);  // Definindo a página de configuração
  server.begin();                                 // Iniciando o servidor
}  //end acessPoint



void leSensor() {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[9];
  byte addr[8];
  //p.begin("DS18B20", "false");  //inicializa preferences


  if (!ds.search(addr)) {
    ds.reset_search();
    Serial.println("Fim daleitura");
    return;
  }

  for (i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
    //display.write(' ');
  }
  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);  // start conversion, with parasite power on at the end

  delay(1000);  // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);  // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for (i = 0; i < 9; i++) {  // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

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
  Serial.print("  Temperature = ");
  Serial.print(temp);
  Serial.print(" Celsius, ");
}

/* ========================================================================= */
/* --------------------------------- Setup --------------------------------- */
void setup() {

  char htn[30];
  sprintf(htn, "DatLog %S %d", DispositivoName, dispositivo);

  //pino para ligar o display
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  //delay(100);

  // 1. Desliga o rádio LoRa explicitamente para economizar bateria
  // A V3 usa pinos internos para o rádio, vamos garantir o Sleep
  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);  // Desativa o seletor do rádio SPI

  // 2. Desliga a alimentação de periféricos (Display/Sensores)
  //pinMode(Vext, OUTPUT);
  //digitalWrite(Vext, HIGH);  // HIGH desliga na V3

  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  display.init();
  display.clear();
  display.display();
  display.setContrast(255);


  //pinMode(led, OUTPUT);
  //
  //settimeofday_cb(ntpSync_cb);  //Definição de callback ntp

  // Função para inicializar o cliente NTP
  //fuso horário , horário de verão , ntp server
  //configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  //EEPROM.begin(EEPROM_SIZE);
  p.begin("wifi-config", true);

  delay(10);

  ssid = p.getString("ssid", "");
  password = p.getString("password", "");

  p.end();
  Serial.println(ssid);
  Serial.println(password);


  if (ssid != "" && password != "") {
    // connect to WiFi using saved credentials
    display.drawString(0, 10, "Conectando...");
    display.display();
    int tempo = 0;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    //Serial.print("Connecting to ");
    //Serial.print(ssid);
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(led, LOW);
      delay(500);
      int progress = (tempo) % 6;
      display.drawProgressBar(0, 32, 12, 10, progress);
      //display.display();
      Serial.print(".");
      tempo++;
      if (tempo > 60) {
        WiFi.disconnect();
        accessPoint();
        break;
      }
    }

    WiFi.hostname(htn);
    //Serial.println("");
    //Serial.println("WiFi connected");
    //Serial.println("IP address: ");
    //Serial.println(WiFi.localIP());
  } else {
    // start Access Point mode
    display.drawString(0, 10, "Modo AP...");
    display.display();
    accessPoint();
    server.handleClient();
  }
  //digitalWrite(led, HIGH);

}  //end setup

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    //verificar para eliminar ou inibir
    display.clear();
    display.drawString(0, 10, "  Conectado");
    display.display();
    delay(1000);  //apagar isso

  } else {
    // Se não estiver conectado à rede Wi-Fi, lidando com as requisições do cliente
    display.drawString(0, 40, "Desconectado");
    display.display();
    //accessPoint();
    server.handleClient();
  }
  leSensor();
  delay(10);
}

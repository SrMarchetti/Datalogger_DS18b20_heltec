/* ==================================================================================

    Linguagem Arduino,
    Datalogger temperatura JA versão sem fio


    Autor: Sergio R Marchetti
    Data: 14/03/2026
    revisão 0
    

    Placa Heltec Wifi Lora 32 V3.2
    processador ESP32 Lib. V. 3.3.7

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
#include "OneWire.h"            //v. 2.3.8 - sensor DS18B20
#include <DallasTemperature.h>  //Sensor DS18B20
#include <Arduino.h>
#include "Adafruit_SHT4x.h"     //sensor SHT41 
#include <WiFi.h>         
#include <HTTPUpdate.h>         //lib troca de OTA para atualizar o firmware via HTTP
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>        //v. 7.4.3
#include <Wire.h>
#include <HT_SSD1306Wire.h>     // Heltec ESP32 2.1.5 Heltec_ESP_Lora_V3 0.9.2
#include <Preferences.h>        //utilizada no lugar da EEPROM
#include <LoRaWan_APP.h>        //V 1.2.0
#include <esp_bt.h>
//#include "heltec.h"


/* ========================================================================== */

/* ========================================================================== */
/* --------------------------------Mapeamento Hardware----------------------- */

#define led 35  //GPIO ?
#define oneWire_pin 2    // antes era 2

#define BAT_ADC_PIN 1    // Pino de leitura (ADC)
#define VEXT_CONTROL 37  // Pino que ativa o divisor resistivo e periféricos
//#define ds18b20 GPIO 45  //pino 6 lado esquerdo
// Configurações de Tempo (15 minutos = 15 * 60 * 10^6 microssegundos)
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 900  //900 = 15min
//TwoWire I2C_0 = TwoWire(0);   // Define barramento Display I2C(0)
TwoWire I2C_1 = TwoWire(1);   // Define barramento sensor  I2C(1)

/* ========================================================================== */

/* ========================================================================== */
/* ------------------------- Instanciando objetos --------------------------- */

//WiFiClient client;                        //para conecção com banco de dados
WebServer server(80);                       // Cria o objeto 'server' na porta 80
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
                                            // addr , freq , i2c group , resolution , rst
Preferences preferences;
OneWire ds(oneWire_pin);                    // GPIO2 pino 13 da placa
DallasTemperature sensorsDS(&ds);           // usando biblioteca Dallas
//DeviceAddress insideThermometer;            // arrays to hold device address
Adafruit_SHT4x sht4 = Adafruit_SHT4x();     // GPIO41 SDA - GPIO42 SCL

/* ========================================================================== */

/* ========================================================================== */
/* -------------------------- Macros e Constantes --------------------------- */

#define DEBUG true  //Comentar para não imprimir serial

#ifdef DEBUG
  #define DEBUG_PRINT(x)      Serial.print(x)
  #define DEBUG_PRINT2(x, y)  Serial.print(x, y)
  #define DEBUG_PRINTLN(x)    Serial.println(x)
  #define DEBUG_BEGIN(x)      Serial.begin(x)
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
const char http_site[] = "http://10.0.0.11/v2/gravasensor";  //"http://10.0.0.11/v2/gravasensor";  //"http://10.0.0.11/php/gravabanco.php";
const int http_port = 3001;

const float fatorVoltagem = 1.003367;
/* ========================================================================== */

/* ========================================================================== */
/* ------------------------- Variáveis Globais ------------------------------ */

//String ssid = "";
//String password = "";
//String enderecoFormatado;
float temp, tempSHT, humi; 
float offSetDHT = 0.0;  //offSet do sensor DHT21
float bateria = 0.0;
uint8_t ds18Present;
//byte addr[8];  //endereço do sensor DS18B20
// RTC memory — rápido, volátil
RTC_DATA_ATTR bool rtcWiFiValido = false;
RTC_DATA_ATTR bool rtcIPValido = false;
RTC_DATA_ATTR bool rtcWiFiCached = false;
RTC_DATA_ATTR bool rtcSensorValido = false;
RTC_DATA_ATTR char rtcSsid[32] = "";
RTC_DATA_ATTR char rtcPassword[64] = "";
RTC_DATA_ATTR uint32_t rtcIp;
RTC_DATA_ATTR uint32_t rtcGateway;
RTC_DATA_ATTR uint32_t rtcSubnet;
//RTC_DATA_ATTR uint32_t rtcDns;
RTC_DATA_ATTR uint8_t rtcBssid[6];
RTC_DATA_ATTR int32_t rtcChannel;
RTC_DATA_ATTR uint8_t rtcaddr[8];      // endereço DS18B20 HEX
RTC_DATA_ATTR char rtcSensorAddr[17];  // endereço DS18B20 char
RTC_DATA_ATTR char rtcSHT41Addr[11];  // endereço DS18B20 char
RTC_DATA_ATTR uint8_t rtcQtFalha = 0;

/* ========================================================================== */

/* ========================================================================== */
/* ---------------------------- Protótipo das funções ----------------------- */

bool enviaDados();  //Grava banco de dados

void accessPoint();   // Habilita como AccessPoint
void handleConfig();  // Recebe SSID e Senha
void handleRoot();    // Exibe página inicial

/* ========================================================================== */


bool enviaDados() {
  //String date = dateTimeStr(time(NULL), -3);
  int ret = false;
  HTTPClient http;
  WiFiClient client;

  if (WiFi.status() == WL_CONNECTED) {
    //String dados_a_enviar = "{\"sensor\":\"286FC096F0013C67\", \"valor\":27.06, \"bateria\":3.72, \"version\":\"1.0.4\"}";
    //envia versão atual junto com os dados, para controle de versão do firmware
    String ssensor = "{\"sensor\": \"";
    String svalor  = "\", \"valor\": ";
    String mac = WiFi.macAddress();
    //DEBUG_PRINTLN(mac);
    mac.replace(":","");
    //if (rtcSensorAddr != "")
    String sSensor18 = ds18Present ? 
        ", " + ssensor + String(rtcSensorAddr) + svalor + String(temp) + "} ]" :
        " ]";
      //else
    String dados_a_enviar = "{\"sensores\": ["+ 
                              ssensor + "01" + String(rtcSHT41Addr) +
                              svalor  + String(tempSHT) + "}, " +
                              ssensor + "02" + String(rtcSHT41Addr) + 
                              svalor  + String(humi)    + "} " +
                              sSensor18 +
                              ", \"mac\":\"" + mac + "\""
                              ", \"bateria\":" + String(bateria) + 
                              ", \"version\":\"" + currentVersion + "\"}";                              
    //"sensor=" + String(dispositivo) + "&valor=" + String(temp);  //+ "&date=" + date;
    DEBUG_PRINTLN(dados_a_enviar);
    delay(40);  //tentativa de estabilizar baixa energia
    http.begin(client, http_site);
    //http.setTimeout(10000);                              // timeout 10s
    http.addHeader("Content-Type", "application/json");  // Definido para Json..

    delay(40);  //tentativa de estabilizar WiFi
    int httpCode = http.POST(dados_a_enviar);

    DEBUG_PRINTLN(httpCode);

    if (httpCode == 200) {
      String response = http.getString();
      JsonDocument doc;
      ret = true;
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
      ret = false;
    }
    display.display();
  }
  //Liberando recursos
  // 1. Finaliza o HTTP
  http.end();

  // 2. Desliga o WiFi
  WiFi.disconnect(true);  // true = apaga credenciais da RAM
  WiFi.mode(WIFI_OFF);
  
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

  preferences.begin("config", false);  //modo leitura e escrita
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.putString("ip", ipStr);
  preferences.putString("gateway", gwStr);
  preferences.putString("subnet", subStr);
  preferences.end();

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
  DEBUG_PRINTLN("Modo Access Point ativado.");
  server.on("/", HTTP_GET, handleRoot);           // Definindo a página inicial
  server.on("/config", HTTP_POST, handleConfig);  // Definindo a página de configuração
  server.begin();                                 // Iniciando o servidor
}  //end acessPoint

bool conectaWiFi() {

  //char htn[30];
  //sprintf(htn, "DatLog %S", DispositivoName);

  DEBUG_PRINT("rtcWiFiValido: ");
  DEBUG_PRINTLN(rtcWiFiValido);
  // verifica se já tem SSID cadastrado
  if (!rtcWiFiValido) {
    carregaPreferences();
    DEBUG_PRINT("rtcWiFiValido2: ");
    DEBUG_PRINTLN(rtcWiFiValido);
    if (!rtcWiFiValido) {
      accessPoint();
      while (1) server.handleClient();
    }
  }

  WiFi.mode(WIFI_STA);

  if (!rtcIPValido) {
    // RTC apagou (troca de bateria) — busca do Preferences
    DEBUG_PRINTLN("Sem IP");
    carregaPreferences();
    WiFi.begin(rtcSsid, rtcPassword);
  } else {
    // Conexão rápida com dados da RTC
    DEBUG_PRINTLN("IP Válido");
    WiFi.config(
      IPAddress(rtcIp),
      IPAddress(rtcGateway),
      IPAddress(rtcSubnet),
      IPAddress(rtcGateway));
    if (rtcWiFiCached) {
      DEBUG_PRINTLN("WiFi Cahed");
      WiFi.begin(rtcSsid, rtcPassword, rtcChannel, rtcBssid, true);
      //WiFi.begin(rtcSsid, rtcPassword); //apagar isso
    } else {
      DEBUG_PRINTLN("WiFi primeira conexão");
      WiFi.begin(rtcSsid, rtcPassword);
    }
  }

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    tentativas++;
    //DEBUG_PRINT(".");
    if (tentativas > 100) {
      rtcWiFiCached = false;  // força novo carregamento no próximo ciclo
      return false;
    }
  }
  DEBUG_PRINT("Tentasivas WiFI: ");
  DEBUG_PRINTLN(tentativas);
  rtcChannel = WiFi.channel();
  memcpy(rtcBssid, WiFi.BSSID(), 6);
  rtcWiFiCached = true;
  DEBUG_PRINT("Channel: ");
  DEBUG_PRINTLN(rtcChannel);


  //WiFi.hostname(htn);
  return true;
}

void leSensor() {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[9];
  sensorsDS.begin();
  
  ds18Present = sensorsDS.getDeviceCount();
  DEBUG_PRINTLN(ds18Present);
  if(!sensorsDS.getAddress(rtcaddr, 0)){
        DEBUG_PRINTLN("não encontrou sensor!");
        strlcpy(rtcSensorAddr, "", sizeof(rtcSensorAddr));
        return;
  }
  

  if(rtcSensorAddr == ""){  
    preferences.begin("config", false);  //inicializa em modo leitura e escrita
    preferences.putBytes("addr", rtcaddr, 8);
    strlcpy(rtcSensorAddr, addrParaString(rtcaddr).c_str(), sizeof(rtcSensorAddr));
    preferences.end();
  }

  sensorsDS.requestTemperatures();            // Send the command to get temperatures
  delay(1000);
  temp = sensorsDS.getTempC(rtcaddr);

  /*
  if (rtcaddr[0] != 0x28) {

    if (!ds.search(rtcaddr)) {
      ds.reset_search();
      DEBUG_PRINTLN("Fim daleitura");
      delay(100);
      //return;
    }
    DEBUG_PRINTLN("Grava endereco");
  }
  

  if (OneWire::crc8(rtcaddr, 7) != rtcaddr[7]) {
    DEBUG_PRINTLN("CRC is not valid!");
    return;
  }

  */
  //rtcSensorAddr = addrParaString(rtcaddr);
  //strlcpy(rtcSensorAddr, addrParaString(rtcaddr).c_str(), sizeof(rtcSensorAddr));
  //display.clear();
  //display.drawString(0, 10, enderecoFormatado);
  //display.display();
  //DEBUG_PRINT("rtcSensorAddr: ");
  //DEBUG_PRINTLN(rtcSensorAddr);
  /*
  ds.reset();
  ds.select(rtcaddr);
  ds.write(0x44, 1);  // start conversion, with parasite power on at the end

  delay(1000);  // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(rtcaddr);
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
  */
  //temp = (float)raw / 16.0;
  DEBUG_PRINT("  Temperature = ");
  DEBUG_PRINT(temp);
  DEBUG_PRINTLN(" Celsius, ");
  display.setFont(ArialMT_Plain_16);

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  char bfTemp[20];
  sprintf(bfTemp, "%.2f °C", temp);
  display.drawString(64, 25, bfTemp);
  //display.display(); não mostra agora
  //delay(1000);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}  //end le sensor

void leSHT4x(){
  sensors_event_t tempSHTevent, humidity;
  I2C_1.begin(41, 42);
  if (! sht4.begin(&I2C_1)) {
    DEBUG_PRINTLN("Erro ao iniciar SHT4x");
  }
  //Serial.println(sht4.readSerial(), HEX);
  uint32_t addrSHT = sht4.readSerial();
  snprintf(rtcSHT41Addr, sizeof(rtcSHT41Addr), "%08lX", addrSHT);
  DEBUG_PRINTLN(rtcSHT41Addr);

  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);
  sht4.getEvent(&humidity, &tempSHTevent);      // popula temperatura e humidade

  tempSHT = tempSHTevent.temperature;
  humi = humidity.relative_humidity;
  Serial.print(tempSHT);
  DEBUG_PRINT("  Temperature SHT = ");
  DEBUG_PRINT(tempSHT);
  DEBUG_PRINT(" Celsius, | ");
  DEBUG_PRINT(" Humidade SHT = ");
  DEBUG_PRINT(humi);
  DEBUG_PRINTLN(" %, ");



  display.setFont(ArialMT_Plain_10);

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  char bfTemp[20];
  sprintf(bfTemp, "%.2f °C %.2f %", tempSHT, humi);
  display.drawString(64, 42, bfTemp);
  //display.display(); não mostra agora
  //delay(1000);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

} //end leSTH4x

String addrParaString(byte* sensorAddress) {
  char enderecoHex[17];  // 16 chars + null terminator
  snprintf(enderecoHex, sizeof(enderecoHex),
           "%02X%02X%02X%02X%02X%02X%02X%02X",
           sensorAddress[0], sensorAddress[1],
           sensorAddress[2], sensorAddress[3],
           sensorAddress[4], sensorAddress[5],
           sensorAddress[6], sensorAddress[7]);
  //DEBUG_PRINTLN(enderecoHex);
  return enderecoHex;
}  //end addrParaString

float lerVoltagemBateria() {
  // 1. Ativa o Vext (Na V3, LOW ativa o regulador de periféricos/divisor)
  pinMode(VEXT_CONTROL, OUTPUT);
  digitalWrite(VEXT_CONTROL, HIGH);
  delay(100);  // Pequena pausa para estabilizar a tensão

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

void carregaPreferences() {
  //Salva o conteúde de Preferences em RTC_DATA
  DEBUG_PRINTLN("Carrega Preferences");

  preferences.begin("config", true);  // true = somente leitura
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  String ip = preferences.getString("ip", "");
  String gw = preferences.getString("gateway", "");
  String sn = preferences.getString("subnet", "");
  //String dns        = preferences.getString("dns", "");
  //String sensorAddr = preferences.getString("sensorAddr", "");
  preferences.getBytes("addr", rtcaddr, 8);
  preferences.end();

  
  // DEBUG_PRINT("SSID: ");
  // for (int i = 0; i < ssid.length(); i++) {
  //   Serial.print(ssid[i], HEX);
  //   Serial.print(" ");
  // }
  // DEBUG_PRINTLN(" .");

  // Copia para RTC
  DEBUG_PRINTLN("dados carregados internet: ");
  DEBUG_PRINTLN(ip);
  DEBUG_PRINTLN(gw);
  DEBUG_PRINTLN(sn);


  if (ssid != "" || password != "") {
    strlcpy(rtcSsid, ssid.c_str(), sizeof(rtcSsid));
    strlcpy(rtcPassword, password.c_str(), sizeof(rtcPassword));
    rtcWiFiValido = true;
  }
  if (ip != "" || gw != "") {
    IPAddress ipObj, gwObj, snObj, dnsObj;

    ipObj.fromString(ip);
    gwObj.fromString(gw);
    snObj.fromString(sn);
    //dnsObj.fromString(dns);

    rtcIp      = (uint32_t)ipObj;
    rtcGateway = (uint32_t)gwObj;
    rtcSubnet  = (uint32_t)snObj;
    //rtcDns     = (uint32_t)dnsObj;
    //rtcDns          = (uint32_t)IPAddress().fromString(dns);
    rtcIPValido = true;
  }

  if (rtcaddr[0] == 0x28) {
    strlcpy(rtcSensorAddr, addrParaString(rtcaddr).c_str(), sizeof(rtcSensorAddr));
    DEBUG_PRINT("rtcSensorAdr: ");
    DEBUG_PRINTLN(rtcSensorAddr);

    rtcSensorValido = true;
  }
}  //End CarregaPreferences

void dormir(bool falha) {
  uint32_t tempo = millis()+800;
  
  display.drawString(0, 0, String(tempo));
  DEBUG_PRINTLN(tempo);

  if (falha) {
    rtcQtFalha++;
    if (rtcQtFalha > 3){
      rtcQtFalha = 0;
      display.drawString(40, 0, "---RESETANDO---");
      display.display();
      delay(800);
      DEBUG_PRINTLN("falhou - Resetando");
      
      desligaRecursos();  
      ESP.restart();
    }
    display.drawString(20, 0, "---FALHOU---");
    display.display();
    delay(800);
    
    desligaRecursos();
    esp_sleep_enable_timer_wakeup(10 * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  } 
  
  display.display();
  delay(800);
  rtcQtFalha = 0;
  
  desligaRecursos();
  uint32_t tempSleep = (tempo / 1000) < TIME_TO_SLEEP ? TIME_TO_SLEEP - (tempo / 1000) : TIME_TO_SLEEP;
  esp_sleep_enable_timer_wakeup(tempSleep * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void desligaRecursos(){
  //desliga OLED
  display.displayOff();
  //desliga sensores e outros VEX
  digitalWrite(Vext, HIGH);
  //termina serial
  #ifdef DEBUG
    DEBUG_PRINTLN("desligando serial");
    Serial.flush();
  #endif

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

  //desliga o bluetooth
  btStop();
  esp_bt_controller_disable();

  // Open serial communications and wait for port to open:
  DEBUG_BEGIN(115200);
  display.init();
  display.clear();
  //display.display();
  display.setContrast(122);

  //se o motivo do boot for 0 le os dados de preferences
  if (!esp_sleep_get_wakeup_cause())
    carregaPreferences();

  DEBUG_PRINT("Wake up cause: ");
  DEBUG_PRINTLN(esp_sleep_get_wakeup_cause());
  DEBUG_PRINTLN(rtcSensorAddr);

  // DEBUG_PRINT("SSID: ");
  // DEBUG_PRINTLN(rtcSsid);
  // 2. Leitura do sensor
  digitalWrite(led, HIGH);
  leSensor();               //le sensor DS18B20
  leSHT4x();                //le Sensor SHT41
  digitalWrite(led, LOW);

  // 3. Le bateria
  bateria = lerVoltagemBateria();
  char sBat[30];
  sprintf(sBat, "Voltagem: %.2f V", bateria);
  display.drawString(0, 10, sBat);
  display.display();
  DEBUG_PRINTLN(bateria);

  //  4. Conecta WiFi
  //se falhar a conexão restarta

  // bool concta = conectaWiFi();
  // enviaDados();
  // dormir(false);
  
  if (!conectaWiFi()) 
    dormir(true);
  // se falhar ao enviar dados restarta
  bool envio = enviaDados();
  DEBUG_PRINT("Envio Return: ");
  DEBUG_PRINTLN(envio);
  dormir(!envio);
  
  //digitalWrite(led, HIGH);

  // 5. DeepSleep - movido para ser chamado via enviaDados

}  //end setup

void loop() {
  server.handleClient();
  delay(10);
}
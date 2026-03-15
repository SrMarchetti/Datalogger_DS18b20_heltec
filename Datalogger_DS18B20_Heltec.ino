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

#include <ArduinoOTA.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <Preferences.h>  //utilizada no lugar da EEPROM


/* ========================================================================== */

/* ========================================================================== */
/* --------------------------------Mapeamento Hardware----------------------- */

#define led LED_BUILTIN  //GPIO ?
#define ds18b20 GPIO 45  //pino 6 lado esquerdo
// Configurações de Tempo (15 minutos = 15 * 60 * 10^6 microssegundos)
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 900  //900 = 15min
Preferences p;

/* ========================================================================== */

/* ========================================================================== */
/* ------------------------- Instanciando objetos --------------------------- */

//WiFiClient client;  //para conecção com banco de dados
WebServer server(80);                                                                     // Cria o objeto 'server' na porta 80
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);  // addr , freq , i2c group , resolution , rst

/* ========================================================================== */

/* ========================================================================== */
/* -------------------------- Macros e Constantes --------------------------- */

//#define EEPROM_SIZE 64
//#define EEPROM_SSID_OFFSET 0
//#define EEPROM_PASSWORD_OFFSET 32
String ssid = "";
String password = "";

//para conexão com banco de dados
const char http_site[] = "http://10.0.0.11/php/gravabanco.php";
const int http_port = 8080;

/* ========================================================================== */

/* ========================================================================== */
/* ------------------------- Variáveis Globais ------------------------------ */

float temp;

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

    //for (int i = 0; i < MaxSensors; i++) {
    //  if (sensor[i].id) {
    String dados_a_enviar = "sensor=" + String(dispositivo) + "&valor=" + String(temp);  //+ "&date=" + date;
    http.begin(client, http_site);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // defino texto plano..
    int codigo_respuesta = http.POST(dados_a_enviar);
    if (codigo_respuesta > 0) {
      //Serial.println("Código HTTP: " + String(codigo_respuesta));

      if (codigo_respuesta == 200) {
        String cuerpo_respuesta = http.getString();
        //Serial.println("El servidor respondió: ");
        //Serial.println(cuerpo_respuesta);
        ret += 0;
      }
    } else {
      //Serial.print("Error enviado POST, código: ");
      //Serial.println(codigo_respuesta);
      ret += 1;
    }
  }
  //}
  //}
  http.end();  // libero recursos
  return ret;

}  //end getPage

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


/* ========================================================================= */
/* --------------------------------- Setup --------------------------------- */
void setup() {

  char htn[30];
  sprintf(htn, "DatLog %S %d", DispositivoName, dispositivo);
  // Open serial communications and wait for port to open:

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);

  Serial.begin(115200);
  display.init();
  display.clear();
  display.display();


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


  //atualização via OTA
  ArduinoOTA.setHostname(htn);

  //No authentication by default
  ArduinoOTA.setPassword("PASSWORD");

  //Password can be set with it's md5 value as well
  //MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  //ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    display.clear();
    display.setFont(ArialMT_Plain_10);          //Set font size
    display.setTextAlignment(TEXT_ALIGN_LEFT);  //Set font alignment
    display.drawString(0, 0, "Start Updating....");



    Serial.printf("Start Updating....Type:%s\n", (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem");
  });

  ArduinoOTA.onEnd([]() {
    display.clear();
    display.drawString(0, 0, "Update Complete!");
    Serial.println("Update Complete!");

    ESP.restart();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    String pro = String(progress / (total / 100)) + "%";
    int progressbar = (progress / (total / 100));
    //int progressbar = (progress / 5) % 100;
    //int pro = progress / (total / 100);

    display.clear();
#if defined(WIRELESS_STICK)
    display.drawProgressBar(0, 11, 64, 8, progressbar);  // draw the progress bar
    display.setTextAlignment(TEXT_ALIGN_CENTER);         // draw the percentage as String
    display.drawString(10, 20, pro);
#else
    display.drawProgressBar(0, 32, 120, 10, progressbar);  // draw the progress bar
    display.setTextAlignment(TEXT_ALIGN_CENTER);           // draw the percentage as String
    display.drawString(64, 15, pro);
#endif
    display.display();

    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    String info = "Error Info:";
    switch (error) {
      case OTA_AUTH_ERROR:
        info += "Auth Failed";
        Serial.println("Auth Failed");
        break;

      case OTA_BEGIN_ERROR:
        info += "Begin Failed";
        Serial.println("Begin Failed");
        break;

      case OTA_CONNECT_ERROR:
        info += "Connect Failed";
        Serial.println("Connect Failed");
        break;

      case OTA_RECEIVE_ERROR:
        info += "Receive Failed";
        Serial.println("Receive Failed");
        break;

      case OTA_END_ERROR:
        info += "End Failed";
        Serial.println("End Failed");
        break;
    }

    display.clear();
    display.drawString(0, 0, info);
    ESP.restart();
  });

  ArduinoOTA.begin();

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
    display.drawString(0,40,"Desconectado");
    display.display();
    //accessPoint();
    server.handleClient();
  }
  delay(10);
}

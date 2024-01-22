#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>

#include <esp_camera.h>

#include <FS.h>
#include <SD_MMC.h>

#include <EEPROM.h> 

//---------------------------------------------------
#define LED_PIN_R 13 //pino LED Red
#define LED_PIN_G 15 //pino LED Gren
#define LED_PIN_B 14 //pino LED Blue
//---------------------------------------------------

//---------------------------------------------------
camera_config_t config;

int quality = 8;
int FPS_desejado = 12;

int TL_ms = 1000;
int TL_photos = 2;
//---------------------------------------------------

//---------------------------------------------------
#define EEPROM_SIZE 2 //bytes

int pictureNumber = 0;
bool start_SD = false;
//---------------------------------------------------

//---------------------------------------------------
//Wi-Fi

// Defina as credenciais do ponto de acesso Wi-Fi
const char* ssid = "Attlas";
const char* password = "";

WebServer server(80);  // Cria um servidor na porta 80
//---------------------------------------------------






//---------------------------------------------------
// FUNCTIONS
//---------------------------------------------------

//---------------------------------------------------
void initLEDS (){
  pinMode(LED_PIN_R, OUTPUT);
  pinMode(LED_PIN_G, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
}

void statusLEDS (bool R, bool G, bool B){
  digitalWrite(LED_PIN_R, R);
  digitalWrite(LED_PIN_G, G);
  digitalWrite(LED_PIN_B, B);
}
//---------------------------------------------------


//---------------------------------------------------
// Inicializar a câmera
void initCamera(int quality_) {
  esp_camera_deinit();

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  
  config.pin_reset = -1;  // Sem reset externo
  config.xclk_freq_hz = 20000000; //2
  config.pixel_format = PIXFORMAT_JPEG;
  
  //init with high specs to pre-allocate larger buffers
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = quality_;
  config.fb_count = 4;

  /*
    FRAMESIZE_96X96: Quadro de 96x96 pixels.
    FRAMESIZE_QQVGA: Quadro de 160x120 pixels.
    FRAMESIZE_QCIF: Quadro de 176x144 pixels.
    FRAMESIZE_HQVGA: Quadro de 240x176 pixels.
    FRAMESIZE_240X240: Quadro de 240x240 pixels.
    FRAMESIZE_CIF: Quadro de 352x288 pixels.
    FRAMESIZE_VGA: Quadro de 640x480 pixels.
    FRAMESIZE_SVGA: Quadro de 800x600 pixels.
    FRAMESIZE_XGA: Quadro de 1024x768 pixels.
    FRAMESIZE_SXGA: Quadro de 1280x1024 pixels.
    FRAMESIZE_UXGA: Quadro de 1600x1200 pixels.
  */

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Erro ao inicializar a câmera! (0x%x)\n", err);
    statusLEDS(true, false, false);
  }
  else {
    Serial.printf("Câmera ok\n", err);
    statusLEDS(false, false, true);
  } 
}

void cameraLive() {
  sensor_t * s = esp_camera_sensor_get();

  WiFiClient client = server.client();

  // Send HTTP headers for text response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println("");

  int frameCount = 0;
  unsigned long startTime = millis();

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    int tentativas = 10; 
    while (!fb) { 
      Serial.println("Camera capture failed");
      esp_camera_fb_return(fb);
      delay(100);
      camera_fb_t * fb = esp_camera_fb_get();

      tentativas = tentativas - 1;
      if (tentativas == 0){
        statusLEDS(true, false, false);
        break;
      }
    }

    client.println("--frame");
    client.println("Content-Type: image/jpeg");
    client.println("Content-Length: " + String(fb->len));
    client.println("");
    client.write(fb->buf, fb->len);
    client.println("");

    esp_camera_fb_return(fb);

    frameCount++; // Increment frame count
    unsigned long currentTime = millis();
    if (currentTime - startTime >= 1000) { // Print FPS every second
      Serial.print("FPS: ");
      Serial.print(frameCount);
      Serial.print(", JPEG quality: ");
      Serial.print(quality);

      Serial.print(", Wifi RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      
      if ((quality > 5) && (quality < 55)){
        if(frameCount < FPS_desejado){
          if(quality < 50){ //máximo da má qualidade
            quality++;
            s->set_quality(s, quality);
          }
        }
        else if(frameCount > FPS_desejado){
          if(quality > 10){ //máximo da boa qualidade
            quality--;
            s->set_quality(s, quality);
          }
        }
      }

      frameCount = 0;
      startTime = currentTime;
    }
  }

  statusLEDS(false, true, false);
  esp_camera_deinit();
  Serial.println("Cliente desconectado");
}


//---------------------------------------------------


//---------------------------------------------------
void initWIFI (){
  //se existir a rede
  Serial.printf("Tentar se conectar a %s\n", ssid);
  WiFi.begin(ssid, password);

  int tentativas = 10;
  while (WiFi.status() != WL_CONNECTED) {
    delay(900);

    tentativas = tentativas - 1;
    Serial.printf("Tentativa: %d\n", tentativas);
    if (tentativas == 0){
      break;
    }
  }


  if (tentativas != 0){ //conseguiu conectar
    Serial.print("Conexão Wi-Fi estabelecida!\nEndereço IP: ");
    Serial.println(WiFi.localIP());
    statusLEDS(false, true, false); //RGB
  }
  else 
  {
    Serial.println("Sem rede Wi-Fi");
    delay(500);

    //se não existir, criar AP
    if (WiFi.softAP(ssid, password)) {
      Serial.println("Ponto de acesso (AP) criado com sucesso!");

      IPAddress IP = WiFi.softAPIP();
      delay(1000);
      statusLEDS(false, true, false); //RGB

      Serial.print("Endereço IP do servidor: ");
      Serial.println(IP);
      Serial.println("Pode levar um tempo para que a rede apareça");
    } else {
      Serial.println("Falha ao criar ponto de acesso (AP).");
      statusLEDS(true, false, false); //RGB
    }
  }
}
//---------------------------------------------------

//---------------------------------------------------
void initSDcard (){
  if (!start_SD){
    if (!SD_MMC.begin()) {
      Serial.println("Falha ao montar o sistema de arquivos do cartão SD");
      return;
    }
    Serial.println("Cartão SD montado");

    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
      Serial.println("No SD Card attached");
      return;
    }

    // initialize EEPROM with predefined size
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(0, 0);
    EEPROM.write(1, 0);
    EEPROM.commit();
    start_SD = true;
  }
}

void captureImage() {
  camera_fb_t *fb = esp_camera_fb_get();
  int tentativas = 1; 
  while (!fb) { 
    Serial.println("Camera capture failed");
    esp_camera_fb_return(fb);
    delay(100);
    camera_fb_t * fb = esp_camera_fb_get();

    tentativas = tentativas - 1;
    if (tentativas == 0){
      //statusLEDS(true, false, false);
      break;
    }
  }

  pictureNumber = EEPROM.read(0) + 1;

  // Path where new picture will be saved in SD Card
  String path = "/picture-" + String(EEPROM.read(1)) + "-" + String(pictureNumber) +".jpg";

  fs::FS &fs = SD_MMC; 
  Serial.printf("Picture file name: %s\n", path.c_str());
  
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file in writing mode");
  } 
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.printf("Saved file to path: %s\n", path.c_str());

    //gravar o restante
    if (pictureNumber < 256){ 
      EEPROM.write(0, pictureNumber);
    }
    else{
      EEPROM.write(1, EEPROM.read(1) + 1);
      EEPROM.write(0 , 0);

      if (EEPROM.read(1) + 1 > 254){ //resetar caso acabe a memória
        EEPROM.write(1 , 0);
      }
    }

    EEPROM.commit();
  }
  file.close();
  esp_camera_fb_return(fb);

  // r      Open text file for reading.  The stream is positioned at the
  //        beginning of the file.

  // r+     Open for reading and writing.  The stream is positioned at the
  //        beginning of the file.

  // w      Truncate file to zero length or create text file for writing.
  //        The stream is positioned at the beginning of the file.

  // w+     Open for reading and writing.  The file is created if it does
  //        not exist, otherwise it is truncated.  The stream is
  //        positioned at the beginning of the file.

  // a      Open for appending (writing at end of file).  The file is
  //        created if it does not exist.  The stream is positioned at the
  //        end of the file.

  // a+     Open for reading and appending (writing at end of file).  The
  //        file is created if it does not exist.  The initial file
  //        position for reading is at the beginning of the file, but
  //        output is always appended to the end of the file.
}

void captureImages(int interval, int numImages) {
  for (int i = 0; i < numImages; i++) {
    captureImage();
    delay(interval);
  }
}

//---------------------------------------------------

//---------------------------------------------------
void handleButton1 (){
  Serial.println("Botão 1 pressionado");

  statusLEDS(false, false, true);
  initCamera(quality);
  cameraLive();
  statusLEDS(false, true, false);
}

void handleButton2 (){
  Serial.println("Botão 2 pressionado!");
  server.send(200, "text/plain", "Time-lapse mode has started.");

  statusLEDS(false, false, true); //RGB

  initCamera(quality);
  initSDcard();
  captureImages(TL_ms, TL_photos); //ms, qts

  initLEDS();
  statusLEDS(false, true, false); //RGB

  Serial.println("The time-lapse has been finished.");
  server.send(200, "text/plain", "The time-lapse has been finished.");
}

void handleButton3 (){
  Serial.println("Botão 3 pressionado! Video live");
  server.send(200, "text/plain", "Video live config! \n nothing to set");
}

void handleButton4(){
  Serial.println("Botão 4 pressionado! Time-lapse config");
  
  String html = "<html><head><meta charset='UTF-8'><style>";
  html += "body { text-align: center; margin-top: 100px; background-color: #efefef;}";
  html += "button { padding: 10px 20px; font-size: 16px; width: 250px; margin-bottom: 10px; }";
  html += "input { margin-bottom: 10px; }";
  html += "</style></head><body>";
  html += "<h1>Time-lapse Config</h1>";
  html += "<form action='/update-timelapse-config' method='post'>";
  html += "Intervalo entre fotos (ms): <input type='text' name='TL_ms' value='" + String(TL_ms) + "'><br>";
  html += "Número de fotos: <input type='text' name='TL_photos' value='" + String(TL_photos) + "'><br>";
  html += "<input type='submit' value='Atualizar Configurações'>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleRoot() {
  String html = "<html><head><style>";
  html += "body { text-align: center; margin-top: 100px; background-color: #efefef;}";
  html += "button { padding: 10px 20px; font-size: 16px; width: 250px; margin-bottom: 10px; }";
  html += "</style></head><body>";
  html += "<h1>ESP32 Cam - Web Server</h1>";
  html += "<p>Page control:</p>";
  html += "<a href='/button1'><button>Video live</button></a><br>";
  html += "<a href='/button2'><button>Time-lapse mode</button></a><br>";

  html += "<a href='/button3'><button>Video live config</button></a><br>";
  html += "<a href='/button4'><button>Time-lapse config</button></a><br>";
  html += "</body></html>";

  server.send(200, "text/html", html);
  Serial.println("Web page acessada");
  statusLEDS(false, true, false);
}

void initServidor (){
  // Rotas da Web
  server.on("/", HTTP_GET, handleRoot);   // Página inicial   
  server.on("/button1", HTTP_GET, handleButton1); // Rota para o botão 1
  server.on("/button2", HTTP_GET, handleButton2); // Rota para o botão 2
  server.on("/button3", HTTP_GET, handleButton3); // Rota para o botão 3
  server.on("/button4", HTTP_GET, handleButton4); // Rota para o botão 4

  server.on("/update-timelapse-config", HTTP_POST, [](){
    TL_ms = server.arg("TL_ms").toInt();
    TL_photos = server.arg("TL_photos").toInt();
    server.send(200, "text/plain", "Configurações atualizadas com sucesso!");

    Serial.print("TL_ms ");
    Serial.println(TL_ms);
    Serial.print("TL_photos ");
    Serial.println(TL_photos);
  });


  server.begin();
  Serial.println("Servidor iniciado com sucesso!");
  statusLEDS(false, true, false); //RGB
}
//---------------------------------------------------
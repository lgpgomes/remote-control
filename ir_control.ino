#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <ESP8266WebServer.h>
#include <FirebaseESP8266.h>
#include <ESP8266WiFi.h>

#define LED_BOARD 2
#define LED_IR 14 //D5
#define RECV_PIN 12 //D6

#define FIREBASE_HOST "ircontroltv-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "zVcoOqwEBGC9ZfZbMc64ZuK9c2Tn9kEI24Hh8pda"

#define SSID_WIFI "INTELBRAS"
#define PASS_WIFI "samsung+aquario"

IRsend irsend(LED_IR);
IRrecv irrecv(RECV_PIN);
decode_results results;
ESP8266WebServer server(80);

unsigned long irStartTime = 0;
bool handlingIr = false;
char buttonId[64] = "";

FirebaseData firebaseData;



void connectToWifi() {
  Serial.print("\nConnecting to Wifi ");
  WiFi.begin(SSID_WIFI, PASS_WIFI);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  digitalWrite(LED_BOARD, HIGH);
  Serial.print("\nConnected to ");
  Serial.println(SSID_WIFI);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void dump(decode_results *results) {
  char capturedIrCode[500];
  int capturedIrCodeLength = 0;
  for (int i = 1; i < results->rawlen; i++) {
    capturedIrCodeLength += snprintf(capturedIrCode + capturedIrCodeLength, sizeof(capturedIrCode) - capturedIrCodeLength, "%ld,", results->rawbuf[i] * USECPERTICK);
  }
  if (capturedIrCodeLength > 0) {
    capturedIrCode[capturedIrCodeLength - 1] = '\0';
    if (buttonId != NULL && buttonId != "") {
      if (Firebase.setString(firebaseData, "/codes/" + String(buttonId), capturedIrCode)) {
        Serial.println("NEW CODE SAVED.");
      }
    }
  }
}

void handleIRControl() {
  char buttonId[64] = "";
  server.arg("buttonId").toCharArray(buttonId, 64);
  char* codeIR = getDataFromFirebase(buttonId);
  sendIr( codeIR );
  server.send(200, "text/plain", "IR enviado.");
}



void sendIr(char *codeIR) {
  std::vector<unsigned int> irCodeVector;
  char *token = strtok(codeIR, ",");
  while (token != NULL) {
    irCodeVector.push_back(strtoul(token, NULL, 10));
    token = strtok(NULL, ",");
  }
  irsend.sendRaw(irCodeVector.data(), irCodeVector.size(), 32);
}




void changeCode() {
  server.arg("buttonId").toCharArray(buttonId, 64);
  handlingIr = true;
  server.send(200, "text/plain", "Solicitando alteração de ");
}



void getCode() {
  char buttonId[64] = "";
  server.arg("buttonId").toCharArray(buttonId, 64);
  char* codeIR = getDataFromFirebase(buttonId);
  server.send(200, "text/plain", codeIR);
}



char* getDataFromFirebase(const char* buttonId) {
  static char codeIR[350];
  char firebasePath[6];
  sprintf(firebasePath, "codes/%s", buttonId);

  if (Firebase.getString(firebaseData, firebasePath)) {
    const String& jsonString = firebaseData.stringData();
    jsonString.toCharArray(codeIR, sizeof(codeIR));
  } else {
    Serial.println("Erro ao buscar dados do Firebase");
    codeIR[0] = '\0';
  }
  return codeIR;
}


String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) {
    path += "index.html";
  }
  String contentType = getContentType(path);
  File file = SPIFFS.open(path, "r");
  if (file) {
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  irsend.begin();
  irrecv.enableIRIn();
  SPIFFS.begin();
  pinMode(LED_BOARD, OUTPUT);
  digitalWrite(LED_BOARD, LOW);

  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "404: Not Found");
    }
  });
  server.on("/sendIrCode", handleIRControl);
  server.on("/changeIrCode", changeCode);
  server.on("/getIrCode", getCode);

  server.serveStatic("/", SPIFFS, "/", "max-age=86400");
  connectToWifi();
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  server.begin();
}



void loop() {
  server.handleClient();
  if (handlingIr) {
    if (irStartTime == 0) {
      irrecv.resume();
      irStartTime = millis();
    } else {
      if (irrecv.decode(&results)) {
        dump(&results);
        handlingIr = false;
        irStartTime = 0;
      } else if (millis() - irStartTime >= 10000 ) {
        handlingIr = false;
        irStartTime = 0;
        Serial.println("Timeout: Nenhum código IR recebido após 10 segundos.");
      }
    }
  }
}

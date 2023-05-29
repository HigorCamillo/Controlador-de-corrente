#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "EmonLib.h"
#include <TimeLib.h>

EnergyMonitor SCT013;
int pinSCT = 32;   // Pino analógico conectado ao SCT-013
int tensao = 127;
int potencia;

// Configura\xe7\xf5es de rede
const char* ssid = "SeuWifi";
const char* password = "SuaSenha";

// Cria uma inst\xe2ncia do servidor web ass\xedncrono
AsyncWebServer server(80);

int Pin2 = 2;
int Pin19 = 19;
int Pin22 = 22;

unsigned long previousMillis = 0;
const long interval = 100;  // Intervalo de atualiza\xe7\xe3o em milissegundos

const int numReadings = 5;  // N\xfamero de leituras para cada if
double readings[numReadings];  // Array para armazenar as leituras
int readingCount = 0;  // Contador de leituras
int relayCount1 = 0;  // Contador de acionamentos do rel\xe9 no primeiro est\xe1gio
int relayCount2 = 0;  // Contador de acionamentos do rel\xe9 no segundo est\xe1gio

// Vari\xe1veis para o contador do m\xeas atual
int currentMonth = 0;
int relayCount1Month = 0;
int relayCount2Month = 0;

void setup() {
  Serial.begin(9600); // Inicializa a comunica\xe7\xe3o serial com uma taxa de 9600 bps
  pinMode(pinSCT, INPUT);
  pinMode(Pin19, OUTPUT);
  pinMode(Pin2, OUTPUT);
  pinMode(Pin22, OUTPUT);

  // Conecta-se \xe0 rede WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao WiFi...");
  }

  // Mostra o endere\xe7o IP atribu\xeddo ao ESP32
  Serial.println("Conectado ao WiFi!");
  Serial.print("Endere\xe7o IP: ");
  Serial.println(WiFi.localIP());

  // Configura o sensor de corrente
  SCT013.current(pinSCT, 0.19);

  // Configura as rotas do servidor web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><body>";
    html += "<h1>Monitor de Energia</h1>";
    html += "<h2>Valores Atuais:</h2>";
    html += "<div id='currentValues'></div>";
    html += "<canvas id='chart'></canvas>";
    html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
    html += "<script>";
    html += "var chartData = {";
    html += "labels: ['Rel\xe9 1', 'Rel\xe9 2'],";
    html += "datasets: [{";
    html += "label: 'Acionamentos do Rel\xe9',";
    html += "data: [0, 0],"; // Inicializa os dados do gr\xe1fico com zero
    html += "backgroundColor: ['rgba(75, 192, 192, 0.2)', 'rgba(153, 102, 255, 0.2)'],";
    html += "borderColor: ['rgba(75, 192, 192, 1)', 'rgba(153, 102, 255, 1)'],";
    html += "borderWidth: 1";
    html += "}]";
    html += "};";
    html += "var chartOptions = {";
    html += "scales: {";
    html += "y: {";
    html += "beginAtZero: true";
    html += "}";
    html += "}";
    html += "};";
    html += "var ctx = document.getElementById('chart').getContext('2d');";
    html += "var chart = new Chart(ctx, {";
    html += "type: 'bar',";
    html += "data: chartData,";
    html += "options: chartOptions";
    html += "});";
    html += "setInterval(updateValues, 2000);";
    html += "function updateValues() {";
    html += "var xhttp = new XMLHttpRequest();";
    html += "xhttp.onreadystatechange = function() {";
    html += "if (this.readyState == 4 && this.status == 200) {";
    html += "var response = JSON.parse(this.responseText);";
    html += "chartData.datasets[0].data = [response.relay1, response.relay2];";
    html += "chart.update();";
    html += "document.getElementById('currentValues').innerHTML = 'Corrente: ' + response.current + ' A<br>' + 'Pot\xeancia: ' + response.power + ' W';";
    html += "}";
    html += "};";
    html += "xhttp.open('GET', '/values', true);";
    html += "xhttp.send();";
    html += "}";
    html += "</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/values", HTTP_GET, [](AsyncWebServerRequest *request){
    double averageIrms = calculateAverageIrms();  // Calcula a m\xe9dia das leituras
    potencia = averageIrms * tensao;              // Calcula o valor da Pot\xeancia Instant\xe2nea

    String values = "{";
    values += "\"current\": " + String(averageIrms) + ",";
    values += "\"power\": " + String(potencia) + ",";
    values += "\"relay1\": " + String(relayCount1Month) + ",";
    values += "\"relay2\": " + String(relayCount2Month);
    values += "}";
    request->send(200, "application/json", values);
  });

  // Inicia o servidor web
  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // Atualiza a leitura da corrente a cada intervalo
    previousMillis = currentMillis;
    double Irms = SCT013.calcIrms(4090); // Calcula o valor da corrente

    readings[readingCount] = Irms;  // Armazena a leitura no array
    readingCount = (readingCount + 1) % numReadings;  // Atualiza o contador de leituras

    if (readingCount == 0) {
      // Se o contador de leituras atingir o n\xfamero desejado, realiza o if
      double averageIrms = calculateAverageIrms();  // Calcula a m\xe9dia das leituras

      // Use a m\xe9dia das leituras nos ifs
      if (averageIrms < 0.5) {
        digitalWrite(Pin2, LOW);
        digitalWrite(Pin19, LOW);
        digitalWrite(Pin22, LOW);
      } else if (averageIrms >= 0.5 && averageIrms < 1) {
        digitalWrite(Pin2, HIGH);
        digitalWrite(Pin19, LOW);
        digitalWrite(Pin22, LOW);
        relayCount1++;  // Incrementa o contador de acionamentos do rel\xe9 no primeiro est\xe1gio
        relayCount1Month++;  // Incrementa o contador de acionamentos do rel\xe9 no primeiro est\xe1gio do m\xeas atual
      } else if (averageIrms >= 1) {
        digitalWrite(Pin2, HIGH);
        digitalWrite(Pin19, HIGH);
        digitalWrite(Pin22, LOW);
        relayCount2++;  // Incrementa o contador de acionamentos do rel\xe9 no segundo est\xe1gio
        relayCount2Month++;  // Incrementa o contador de acionamentos do rel\xe9 no segundo est\xe1gio do m\xeas atual
      }

  if (relayCount1 >= 3) {
    delay(5000);
    relayCount1 = 0;
  }
  
  if (relayCount2 >= 3) {
    delay(5000);
    relayCount2 = 0;
  }

      // Verifica se o m\xeas atual mudou
      int currentMonthTemp = month();
      if (currentMonthTemp != currentMonth) {
        currentMonth = currentMonthTemp;
        relayCount1Month = 0;  // Reinicia o contador de acionamentos do rel\xe9 no primeiro est\xe1gio do m\xeas atual
        relayCount2Month = 0;  // Reinicia o contador de acionamentos do rel\xe9 no segundo est\xe1gio do m\xeas atual
      }
    }
  }
}

double calculateAverageIrms() {
  double sum = 0;
  for (int i = 0; i < numReadings; i++) {
    sum += readings[i];
  }
  return sum / numReadings;
}

#include <SoftwareSerial.h>
#include <Wire.h>
#include <SPI.h>
#include <Ethernet.h> // biblioteca para efetuar conexão com a internet
#include <dht.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>




#define pinSensorGround A0
#define DHT11_PIN 8
#define pinSensorsmoke A1
dht DHT;

short DHT_OK = -1;
short CMD_RESET = -1;
SoftwareSerial sim(5, 6); // pinos rx/tx
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(10,31,5,4); // ip para servidor local

// Inicializa a biblioteca Ethernet Server
// com o IP e porta que você quiser usar
// (porta 80 é padrão para HTTP):
EthernetServer server(80);

char Received_SMS;
byte MaxTemperature = 29;
byte MaxHumidity = 70;
byte MaxGround_Humidity = 50;
byte MaxSmoke = 40;
char numbers[2][15] = {"+xxxxxxxxxxxxx","+xxxxxxxxxxxxx"};
String Data_SMS;
String mensagem;
unsigned long delay1 = 3600000;
unsigned long delay2 = 0;
unsigned long delay3 = 0;
int analogGroundDry = 1022; //VALOR MEDIDO COM O SOLO SECO (VOCÊ PODE FAZER TESTES E AJUSTAR ESTE VALOR - muito seco é 300)
int analogGroundWet = 387; //VALOR MEDIDO COM O SOLO MOLHADO (VOCÊ PODE FAZER TESTES E AJUSTAR ESTE VALOR - muito molhado é 170)
int percGroundMin = 0; //MENOR PERCENTUAL DO SOLO SECO (0% - NÃO ALTERAR)
int percGroundMax = 100; //MAIOR PERCENTUAL DO SOLO MOLHADO (100% - NÃO ALTERAR)
int analogSmokeHigh = 362;
int analogSmokeLow = 50;
int percSmokeMax = 100;
int percSmokeMin = 0;

struct Sensors {
  byte   chk;
  float temperature;
  float humidity;
  float ground_humidity;
  float smoke;
};
void (*funcReset)() = 0;
void setup() {
  //Inicializando Serial
  Serial.begin(9600);
  while (!Serial) {
    ; // espera a porta serial ser conectada. Necessário apenas para nativos com porta USB
  }
  Serial.println("Ethernet WebServer Example");

  // Inicia a conexão com o sevidor
  Ethernet.begin(mac, ip);

  // Inicia o servidor
  server.begin();
  Serial.print(F("O Servidor está em "));
  Serial.println(Ethernet.localIP());
  Serial.println(F("Iniciando sistema..."));
  sim.begin(9600);
  ReceiveMode();
}

Sensors *getSensors() {   // * significa que esta função retornará um ponteiro
  Sensors *sensors;       // define um ponteiro para a variável -> ponteiro nada mais é que um endereço de memória reservado para uma variável
  //(por exemplo: quando você recebe o valor de uma variável, você recebe a cópia do valor, quando você usa um ponteiro,
  //você pega a variável original propriamente dita), quando algum valor é alterado nela, é alterado diretamente no endereço
  //de memória reservado para a variável.

  sensors = (Sensors*)malloc(sizeof(Sensors)); // malloc serve para alocar espaço para as variáveis dos sensores

  sensors->chk = DHT.read11(DHT11_PIN); //CHK da struct sensor recebe o valor dos sensores DHT11
  sensors->temperature = DHT.temperature; //temperature da struct sensors recebe o valor da temperatura
  sensors->humidity = DHT.humidity; //humidity da struct sensors recebe o valor da umidade do ar
  sensors->ground_humidity = map(analogRead(pinSensorGround), analogGroundDry, analogGroundWet, percGroundMin, percGroundMax); //ground_humidity da struct sensors recebe o valor da umidade do solo
  sensors->smoke = map(analogRead(pinSensorsmoke), analogSmokeLow, analogSmokeHigh, percSmokeMin, percSmokeMax);

  return sensors; // retorna os valores de todos os sensores
}

void loop() {
  Sensors *sensors;
  sensors = getSensors(); // recebe o valor dos sensores
  String RSMS;

  if (millis() - delay2 >= 86400000) { // a cada 24 horas reinicia o arduino
    funcReset();
  } else if (millis() - delay3 >= 300000) { // a cada 5 minutos restabelece conexão com o SIM800L
    noInterrupts();
    interrupts();
    delay(1000);
    delay3 = millis();
    RestartArduino();
  }





  if (sensors->ground_humidity > MaxGround_Humidity || sensors->temperature > MaxTemperature || sensors->humidity > MaxHumidity || sensors->smoke > MaxSmoke) { // verifica se há alguma anormalidade em qualquer 1 dos 3 sensores
    if ((millis() - delay1) > 10800000) { // após notificar uma vez, ele demora 3 horas para notificar novamente.
      Serial.println("ANORMALIDADE NOS SENSORES!!");
      sensors = getSensors();
      //Data_SMS = "ANORMALIDADE NOS SENSORES!"; // Define a mensagem do alerta a ser enviado
      //for (int i = 0; i < 2; i++) {
        //Send_Data(numbers[i]);
      //  delay(1000);
      //}
      // Send_Data(number); //envia a mensagem designada à variável Data_SMS
      //delay(3000);
      Data_SMS = "ANORMALIDADE NOS SENSORES!\nUmidade Solo (Max: 50%): " + String(sensors->ground_humidity, 1) +
                 "%" + "\nTemp (Max:29C): " + String(sensors->temperature, 1) + "C" + "\nUmidade Ar(Max:70%): " + String(sensors->humidity, 1) +
                 "%" + "\nFumaca(Max:40%): " + String(sensors->smoke, 1) + "%";
      for (int i = 0; i < 2 ; i++) {
        Send_Data(numbers[i]);
        delay(3000);
      }
      //Send_Data(number); //envia a mensagem designada à variável Data_SMS
      delay1 = millis();
      delay(6000);
      CallPhone();
    }
  }

  while (sim.available() > 0) { // verifica se há novas mensagens
    Received_SMS = sim.read(); // Designa a mensagem recebida a uma variável
    Serial.print(Received_SMS); // Printa a mensagem recebida
    RSMS.concat(Received_SMS); // Concatena a variável RSMS que está vazia com a variável Received_SMS
    DHT_OK = RSMS.indexOf("1234"); //verifica se a palavra dentro das áspas está presente no sms recebido
    CMD_RESET = RSMS.indexOf("reset");
  }

  //verifica se a variável DHT_OK é diferente de -1 (Caso a variável seja -1 quer dizer que não há mensagem recebida)
  if (DHT_OK != -1) {
    //String number2;
    //Data_SMS = RSMS;
    String CellNumtemp = RSMS.substring(RSMS.indexOf("+55"));
    String send_number = CellNumtemp.substring(0, 14);
    Serial.println(F("\nReceived sensors request"));//indica que a mensagem Relatorio foi recebida
    Serial.print("Número:" + send_number + "\r");
    Serial.print(F("\nUmidade do solo = "));
    Serial.print(sensors->ground_humidity);
    Serial.print(F("%"));
    Serial.print(F("\nTemperatura = "));
    Serial.print(sensors->temperature);
    Serial.print(F("C"));
    Serial.print(F("\nUmidade relativa do ar = "));
    Serial.print(sensors->humidity);
    Serial.print(F("%"));
    Serial.print(F("\nPorcentagem de fumaca: "));
    Serial.print(sensors->smoke);
    Serial.print(F("%"));
    Data_SMS = "Umidade Solo (Max: 50%): " + String(sensors->ground_humidity, 1) +
               "%" + "\nTemp (Max:29C): " + String(sensors->temperature, 1) + "C" + "\nUmidade Ar(Max:70%): " + String(sensors->humidity, 1) +
               "%" + "\nFumaca(Max:40%): " + String(sensors->smoke, 1) + "%";

    Send_Data(send_number);
    DHT_OK = -1;
    delay(2000);
  }

  if (CMD_RESET != -1) {
    String CellNumtemp = RSMS.substring(RSMS.indexOf("+55"));
    String send_number = CellNumtemp.substring(0, 14);
    Data_SMS = "Arduino reiniciado!";
    Send_Data(send_number);
    CMD_RESET = -1;
    funcReset();

  }

  free(sensors); // libera o espaço utilizado pelas variáveis do sensores

  EthernetClient client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n' && currentLineIsBlank) { //monta toda a pagina em HTML/CSS no Servidor Web Local
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 60");  // refresh the page automatically every 60 sec
          client.println();
          client.println(F("<!DOCTYPE html>"));
          client.println(F("<html lang='en'>"));
          client.print(F("<head>"));
          client.print(F("    <meta charset='UTF-8'>"));
          client.println(F("<style type=\"text/css\">"));
          client.println(F(".styled-table {border-collapse: collapse;margin: 25px 0;font-size: 0.9em;font-family: sans-serif;width: 600px;box-shadow: 0 0 20px rgba(0, 0, 0, 0.15);overflow-y: auto;}.styled-table thead tr {background-color: #009879;color: #ffffff;text-align: left;}.styled-table th,.styled-table td {padding: 12px 15px;}.styled-table tbody tr {border-bottom: 1px solid #dddddd;}.styled-table tbody tr:nth-of-type(even) {background-color: #f3f3f3;}.styled-table tbody tr:last-of-type {border-bottom: 2px solid #009879;}.styled-table tbody tr.active-row {font-weight: bold; color: #009879;}@media screen and (max-width:600px) {.styled-table {width: 100vw;margin-right: 10px;}}@media screen and (max-width:376px) {.styled-table {overflow: scroll;overflow: auto;}}@media screen and (max-width:281) {.styled-table {overflow: scroll;overflow: auto; }}"));
          client.println(F("</style>"));
          client.print(F("    <meta http-equiv='X-UA-Compatible' content='IE=edge'>"));
          client.print(F("    <meta name='viewport' content='width=device-width, initial-scale=1.0'>"));
          client.print(F("    <title>Document</title>"));
          client.print(F("</head>"));
          client.print(F("<body>"));
          client.print(F("<div style='width:100%;display:flex;justify-content:center;align-items: center;'>"));
          client.print(F("    <table class='styled-table'>"));
          client.print(F("        <thead>"));
          client.print(F("            <tr >"));
          client.print(F("                <th></th>"));
          client.print(F("                <th>Relatório sala fria"));
          
          client.print(F("                <th></th>"));
          client.print(F("            </tr>"));
          client.print(F("            <tr>"));
          client.print(F("                <th>Sensor</th>"));
          client.print(F("                <th>Valor</th>"));
          client.print(F("                <th>Status</th>"));
          client.print(F("            </tr>"));
          client.print(F("        </thead>"));
          client.print(F("        <tbody>"));
          client.print(F("            <tr class='active-row'>"));
          client.print(F("                <td>Temperatura</td>"));
          client.print(F("<td>"));
          client.print(sensors->temperature);
          client.print("ºC");
          client.print(F("</td>"));
          if (sensors->temperature > 35) {
            client.print(F("                <td style='color:red'>CRÍTICA</td>"));
          } else if (sensors->temperature >= 30) {
            client.print(F("                <td style='color:orange'>ANORMAL</td>"));
          } else {
            client.print(F("                <td>Normal</td>"));
          }

          client.print(F("            </tr>"));
          client.print(F("            <tr class='active-row'>"));
          client.print(F("                <td>Umidade do Ar</td>"));
          client.print(F("<td>"));
          client.print(sensors->humidity);
          client.print("%");
          client.print(F("</td>"));
          if (sensors->humidity > 75) {
            client.print(F("                <td style='color:red'>CRÍTICA</td>"));
          } else if (sensors->humidity > 70) {
            client.print(F("                <td style='color:orange'>ANORMAL</td>"));
          } else {
            client.print(F("                <td>Normal</td>"));
          }
          client.print(F("            </tr>"));
          client.print(F("            <tr class='active-row'>"));
          client.print(F("                <td>Umidade do Solo</td>"));
          client.print(F("<td>"));
          client.print(sensors->ground_humidity);
          client.print("%");
          client.print(F("</td>"));
          if (sensors->ground_humidity > 50) {
            client.print(F("                <td style='color:red'>CRÍTICA</td>"));
          } else if (sensors->ground_humidity > 40) {
            client.print(F("                <td style='color:orange'>ANORMAL</td>"));
          } else {
            client.print(F("                <td>Normal</td>"));
          }
          client.print(F("            </tr>"));
          client.print(F("            <tr class='active-row'>"));
          client.print(F("                <td>Fumaça</td>"));
          client.print(F("<td>"));
          client.print(sensors->smoke);
          client.print("%");
          client.print(F("</td>"));
          if (sensors->smoke > 50) {
            client.print(F("                <td style='color:red'>CRÍTICA</td>"));
          } else if (sensors->smoke > 30) {
            client.print(F("                <td style='color:orange'>ANORMAL</td>"));
          } else {
            client.print(F("                <td>Normal</td>"));
          }
          client.print(F("            </tr>"));
          // and so on... ~~~~~~~~~~~~~~~~~~~~~~
          client.print(F("        </tbody>"));
          client.print(F("    </table>"));
          client.print(F("</div>"));
          client.print(F("</body>"));
          client.println(F("</html>"));
          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    // delay para o web server receber as informações
    delay(1);
    // close the connection:
    client.stop();
  }







}// ~~~~~~~~~~~~Final do Loop~~~~~~~~~~~~


void pollSms() { // Prepara o GSM
  delay(500);

  while (Serial.available())
  {
    sim.write(Serial.read());
  }

  while (sim.available())
  {
    Serial.write(sim.read());
  }
}
void CallPhone() {
  Serial.println("Realizando chamada...");
    sim.print(F("ATD"));
    sim.print(numbers[0]);
    sim.print(F(";\r\n"));
  pollSms();
}
void ReceiveMode() { //prepara o arduino para receber SMS
  sim.println(F("AT"));
  pollSms();

  sim.println(F("AT+CMGF=1"));
  pollSms();

  sim.println(F("AT+CNMI=2,2,0,0,0"));
  pollSms();
}

void Send_Data(String n) {
  Serial.println("\nEnviando dados..."); // Displays on the serial monitor
  sim.print(F("AT+CMGF=1\r")); //Prepara o GSM para o modo de SMS
  delay(1000);
  sim.print("AT+CMGS=\"" + n + "\"\r"); //phone number
  delay(1000);
  sim.print(Data_SMS); //Essa string é enviada como SMS
  delay(1000);
  sim.print((char)26); //Finaliza o comando AT
  delay(1000);
  sim.println();
  Serial.println(F("Dados enviados."));
  delay(500);
}

void RestartArduino() {
  Serial.println("\nReiniciando sistema...");
  ReceiveMode();// Chama a função ReceiveMode
}

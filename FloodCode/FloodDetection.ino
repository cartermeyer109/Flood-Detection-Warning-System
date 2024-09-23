// Arduino Uno R4 Wi-Fi Water Level Sensor with Email Alarm. Attribution at bottom. 

#include "Arduino_LED_Matrix.h"
#include "sendanim.h"
#include <WiFiS3.h> //Access wifi card
#include <ESP_Mail_Client.h> //send email and texts

ArduinoLEDMatrix matrix;//Creates a matrix for animations

WiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP

//Motor Variables
const int motor1Power = 12;
const int motor2Power = 13;

//Water sensor variables
const int WATER_DATA_PIN = A0;
const int WATER_POWER_PIN = 7;
int waterValue = 0;
int waterLevelMet = false;

//Timer Variables
int currentTime = 0;
int timer = 0;

#define WIFI_SSID ""
#define WIFI_PASS "" // not needed with MAC address filtering


#define SMTP_HOST ""
#define SMTP_PORT esp_mail_smtp_port_587

#define AUTHOR_EMAIL "" // Gmail sign in
#define AUTHOR_PASSWORD ""  // app password

#define RECIPIENT_EMAIL "" // @vtext.com turns an email into a Verizon text message. Other carriers have different addresses.

SMTPSession smtp;
void smtpCallback(SMTP_Status status); // Callback function to get the Email sending status
Session_Config config; // Declare the Session_Config for user defined session credentials
SMTP_Message message; // Declare the message class

void connectwifi(){
  matrix.loadSequence(LEDMATRIX_ANIMATION_WIFI_SEARCH);
  matrix.play(true);
  Serial.begin(9600);
  WiFi.begin(WIFI_SSID, WIFI_PASS);  // WiFi.begin(WIFI_SSID) or WiFi.begin(WIFI_SSID, WIFI_PASS); depending on if password is necessary
  Serial.print("Connecting to Wi-Fi.");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  matrix.loadSequence(LEDMATRIX_ANIMATION_CHECK);
  matrix.play();
  Serial.print("\nConnected with IP: ");
  Serial.println(WiFi.localIP());
}

void setupEmail(){
  config.server.host_name = SMTP_HOST; // Set the session config
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  //config.login.user_domain = "";
  config.login.user_domain = F("127.0.0.1"); // not having this might have been causing a crash?         **** 

  message.sender.name = "Flood Detection Warning System"; // Setup the email message
  message.sender.email = AUTHOR_EMAIL;  
  message.clearRecipients();
  message.addRecipient(F("You"), RECIPIENT_EMAIL);
  message.subject = "FLOOD ALARM";
  message.text.content = "Water levels have risen above your set height. Flood prevention measures have activated"; //TODO Change this to be on topic
}

void sendEmail(){
  matrix.loadSequence(sendAnimation);
  matrix.play(true);

  smtp.setTCPTimeout(10); // not having this might have been causing a crash?                ****
  
  if (!smtp.connect(&config)){
    ESP_MAIL_PRINTF("\nConnection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  } else {
    Serial.println("\nsmtp.connect successful");
  }
  
  if (!smtp.isLoggedIn()){
    Serial.println("Not yet logged in.");
    return;
  }
  else{
    if (smtp.isAuthenticated())
      Serial.println("Successfully logged in.");
    else
      Serial.println("Connected with no Auth.");
  }

  if (MailClient.sendMail(&smtp, &message)) {
    Serial.println("Email successfully sent");
    matrix.loadSequence(LEDMATRIX_ANIMATION_CHECK);
    matrix.play();
  } 
  else {
    ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  }
}

//Reads the water-level sensor
void readSensor(){
  digitalWrite(WATER_POWER_PIN, HIGH);  // Turn the sensor ON
  delay(10);              // wait 10 milliseconds
  waterValue = analogRead(WATER_DATA_PIN);
  digitalWrite(WATER_POWER_PIN, LOW);   // Turn the sensor OFF
  Serial.println(waterValue);
}

//Disconnects from the Wi-Fi and ends email communications
void disconnectWifi(){
    WiFi.disconnect();
    MailClient.networkReconnect(true);
    smtp.debug(1);
    smtp.callback(smtpCallback); // Set the callback function to get the sending results
    Serial.println("Wifi disconnect successful");
}

void setup(){
  //Starts matrix and conncets to Wi-FI before starting demo
  matrix.begin();
  setupEmail();
  connectwifi();

  //Connects all devices and starts pumping water into primary cup
  pinMode(motor1Power, OUTPUT);
  pinMode(motor2Power, OUTPUT);
  pinMode(WATER_POWER_PIN, OUTPUT);
  digitalWrite(motor1Power, LOW);
  digitalWrite(motor2Power, HIGH);
  digitalWrite(WATER_POWER_PIN, LOW);

  //Set timer
  timer = 0;
  currentTime = millis();
}

void loop(){
  //Updates timer since first motor started
  timer += (millis() - currentTime);
  currentTime = millis();

  //Reads water-level sensor every second
  readSensor();
  
  //The first time the set water-level is reaches, send the email
  if (waterValue  >= 450 && waterLevelMet == false) {
    Serial.println("Water level limit found");
    digitalWrite(motor1Power, HIGH);
    sendEmail();
    disconnectWifi();
    waterLevelMet = true;  
  }

  //After water-level is reaches, keep pumping water out while water is high
  if (waterLevelMet == true && waterValue > 425) {
    digitalWrite(motor1Power, HIGH);
  }
  else {
    digitalWrite(motor1Power, LOW);
  }

  //5 seconds after e-mail is sent: stop flooding the primary cup
  if (timer > 5000) {
    digitalWrite(motor2Power, LOW);
  }
  
  delay(1000);
}

void smtpCallback(SMTP_Status status){
  Serial.println(status.info());
  if (status.success()){
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      SMTP_Result result = smtp.sendingResult.getItem(i);
      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------");
    smtp.sendingResult.clear(); // You need to clear sending result as the memory usage will grow up.
  }
}

/*
built based on examples provided by the following and many others:
Mobizt https://github.com/mobizt/ESP-Mail-Client/blob/master/examples/SMTP/Send_HTML/Send_HTML.ino
Rui Santos https://RandomNerdTutorials.com/esp32-send-email-smtp-server-arduino-ide/

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

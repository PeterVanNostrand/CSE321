#include <WiFi101.h> // Library for WiFi connectivity
#include <WiFiUdp.h> // Library for UDP connections
#include <Wire.h> // I2C functionality for RTC
#include "RTClib.h" // RTC library for interface
#include "user_settings.h" // User specfic configuration file

#define GREEN 10
#define RED 11
#define BLUE 12
#define ALARMINT 6
#define BUZZER 5
#define LOOP_DELAY 100

String html_text_1 =
"<!DOCTYPE html>"
"<html lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\">"
"<head>"
"    <meta charset=\"utf-8\" />"
"    <title></title>"
"</head>"
"<body style=\"background-color:#111111\">"
"    <style>"
"        .textbox {"
"            background-color: #222222;"
"            border: ridge;"
"            border-color: #333333;"
"            width: 100%;"
"            color: #aaaaaa;"
"            text-align: center;"
"            margin: 3px 0px 3px 0px;"
"            height: 30px;"
"            font-size: 1.2em;"
"        }"
"    </style>"
"    <script type=\"text/javascript\" src=\"http://code.jquery.com/jquery-3.3.1.min.js\"></script>"
"    <div id=\"container\" style=\"width:80%; max-width:600px; min-width:200px; height:300px; margin:auto; position:absolute; top:0; bottom:0; left:0; right:0; color:#aaaaaa\">"
"        <div style=\"text-align:center\">Welcome to Peter VanNostrand's CSE321 Final Project!</div>"
"        <input type=\"number\" id=\"RED\" placeholder=\"Red Value\" class=\"textbox\">"
"        <br />"
"        <input type=\"number\" id=\"GREEN\" placeholder=\"Green Value\" class=\"textbox\">"
"        <br />"
"        <input type=\"number\" id=\"BLUE\" placeholder=\"Blue Value\" class=\"textbox\">"
"        <br />";
String html_text_2 =
"        <input type=\"date\" id=\"Alarm_Date\" placeholder=\"Alarm Date\" class=\"textbox\">"
"        <br />"
"        <input type=\"time\" id=\"Alarm_Time\" placeholder=\"Alarm Time\" class=\"textbox\">"
"        <br />"
"        <input type=\"number\" id=\"sunrise_duration\" placeholder=\"Sunrise Duration\" class=\"textbox\">"
"        <br />"
"        <input type=\"button\" value=\"Update\" id=\"update\" class=\"textbox\" style=\"width: 101%; height: 35px; color:#aaaa; border-color:#777779\" />"
"    </div>"
"    <script type=\"text/javascript\">"
"        $('#update').click(function () {"
"            var redval = $('#RED').val();"
"            var grnval = $('#GREEN').val();"
"            var bluval = $('#BLUE').val();"
"            var pathname = $(location).attr(\"href\");";
String html_text_3 =
"            var alarm_date_local = new Date($('#Alarm_Date').val() + \"T\" + $('#Alarm_Time').val() + \":00.000\" + \"Z\");"
"            var alarm_date_utc = (alarm_date_local.getTime() + (new Date().getTimezoneOffset() * 60000)) / 1000;"
"            var sunrise_duration = $('#sunrise_duration').val();"
"            if (pathname.indexOf('?') != -1) pathname = pathname.substring(0, pathname.indexOf('/?'));"
"            window.location.replace(pathname + '/' + '?R=' + redval + '?G=' + grnval + '?B=' + bluval + '?A=' + alarm_date_utc + '?S=' + sunrise_duration);"
"        });"
"        //$(document).ready(function () {"
"        //    var current_datetime = new Date();"
"        //    $('#Alarm_Date').val(current_datetime.getFullYear() + '-' + (current_datetime.getMonth()+1) +'-'+current_datetime.getDate());"
"        //    $('#Alarm_Time').val(\"08:00\");"
"        //});"
"    </script>"
"</body>"
"</html>";

// WiFi Connection Variables
IPAddress localip;
RTC_DS3231 rtc;
WiFiServer server(80);
String request_string = "";
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// NTP Server Variables
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
unsigned int localPort = 2390; // Port to listen for UDP Packets
WiFiUDP Udp;

// Alarm values
volatile int sunrise_minutes = 0;
volatile bool start_sunrise_sequence = false;
volatile long sunrise_loop_counter = 0;
volatile double light_step = 0;
bool is_first_interrupt = true;
DateTime alarm_time(0);
bool start_alarm_sequence = false;
int alarm_loop_counter = 0;

void setup() {
  WiFi.setPins(8,7,4,2); // Tell WiFi101 library which pins to use
  Serial.begin(115200);
  while(!Serial);
  if(!rtc.begin()){
    Serial.println("RTC NOT FOUND!");
    while(1);  
  }
  pinMode(ALARMINT, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(ALARMINT), alarm_interrupt, FALLING);
  digitalWrite(BUZZER, LOW);
  analogWrite(RED, 0);
  analogWrite(GREEN, 0);
  analogWrite(BLUE, 0);
  connect_to_network();
  if(rtc.lostPower()){
    Serial.println("Real Time Clock Lost Power!");
    Udp.begin(localPort);
    unsigned long local_epoch = get_current_epoch();
    rtc.adjust(DateTime(local_epoch)); // set clock to current date time
    Serial.println("Clock Adjusted!");
  }
  Serial.print("Current Time: ");
  print_datetime(rtc.now());
  server.begin();
  Serial.println("Server up!");
  Serial.println("READY");
//  alarm_time = DateTime(rtc.now().unixtime()+20);
//  set_alarm(alarm_time);
//  Serial.print("Alarm Set For: ");
//  print_datetime(alarm_time);
}

void loop() {
  WiFiClient client = server.available();   // listen for incoming clients
  if (client) {                             // if you get a client,
    request_string = "";                    // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (request_string.length() == 0) {
            client.println("HTTP/1.1 200 OK"); // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            client.println("Content-type:text/html"); // and a content-type so the client knows what's coming, then a blank line
            client.println(); // the content of the HTTP response follows the header:
            client.println(html_text_1);
            client.println(html_text_2);
            client.println(html_text_3);
            client.println();// The HTTP response ends with another blank line:
            break; // break out of the while loop:
          }
          else {      // if you got a newline, then clear request_string:
            if(request_string.indexOf("GET")!=-1) parse_query_strings(request_string);
            request_string = "";
          }
        }
        else if (c != '\r') {    // if you got anything else but a carriage return character,
          request_string += c;      // add it to the end of the request_string
        }
      }
    }
    client.stop(); // close the connection:
  }
  if(start_sunrise_sequence){
     int new_val = (int)(light_step*sunrise_loop_counter);
     if(new_val > 255){
      new_val = 255;
      sunrise_minutes = 0;
      start_sunrise_sequence = false;
      sunrise_loop_counter = 0;
      light_step = 0;      
      Serial.println("Sunrise Complete!");
     }
     analogWrite(GREEN, new_val);
     analogWrite(BLUE, new_val);
     analogWrite(RED, new_val);
     sunrise_loop_counter++;
  }
  if(start_alarm_sequence){ // turn on the alarm with a beep-beep------beep-beep pattern five time
    digitalWrite(BUZZER, HIGH);
    delay(150);
    digitalWrite(BUZZER, LOW);
    delay(150);
    digitalWrite(BUZZER, HIGH);
    delay(150);
    digitalWrite(BUZZER, LOW);
    delay(250);
    alarm_loop_counter++;
      if(alarm_loop_counter==5){
      start_alarm_sequence = false;
      alarm_loop_counter = 0;
      Serial.println("Alarm Complete!");
    }
  }
  delay(LOOP_DELAY);
}

void parse_query_strings(String request_string){
  while(request_string.indexOf('?')!=-1){
    int query_start = request_string.indexOf('?'); // get the start of the query string
    int query_divider = request_string.indexOf('='); // get the = sign that splits query into key and value

    // find the end of the key value pair
    int next_space = request_string.indexOf(' ',query_start+1);
    int next_question = request_string.indexOf('?', query_start+1);
    if(next_space<0) next_space = 0x7FFF;
    if(next_question<0) next_question = 0x7FFF;
    int query_end = min(next_space, next_question);

    // get the key and value
    String query_key = request_string.substring(query_start+1,query_divider);
    String query_value = request_string.substring(query_divider+1,query_end);
    
    // perform the action indicated by the key value pair
    long query_int = query_value.toInt();
    if(query_key=="R" && query_int>-1 && query_int<256)
      analogWrite(RED, query_int);
    else if(query_key=="G" && query_int>-1 && query_int<256)
      analogWrite(GREEN, query_int);
    else if(query_key=="B" && query_int>-1 && query_int<256)
      analogWrite(BLUE, query_int);
    else if(query_key=="A" && query_int>0 && alarm_time.unixtime()==0){ // if an alarm isn't currently scheduled, and a time has been passed
      alarm_time = DateTime(query_int);
      Serial.print("RECEIVED ALARM TIME AS: ");
      print_datetime(alarm_time);
    }
    else if(query_key=="S" && query_int>-2){
      if(query_int==-1){
        cancel_alarm();
        Serial.println("Cancelled Alarm!");
      }
      else if(alarm_time.unixtime() != 0){
        sunrise_minutes = query_int;
        Serial.print("SUNRISE MINUTES: ");
        Serial.println(sunrise_minutes);
        DateTime sunrise_start = DateTime(alarm_time.unixtime()-sunrise_minutes*60);
        Serial.print("Sunrise will start at: ");
        print_datetime(sunrise_start);
        set_alarm(sunrise_start);
      }
    }

    // cut off key value pair we just used
    request_string = request_string.substring(query_end);
  }
}

void set_alarm(DateTime dt){
  // compute new values for alarm datetime registers
  uint8_t seconds = (dt.second()/10 << 4) | (dt.second()%10);
  uint8_t minutes = (dt.minute()/10 << 4) | (dt.minute()%10);
  uint8_t hours = (dt.hour()/10 << 4) | (dt.hour()%10);
  uint8_t date = (dt.day()/10 << 4) | (dt.day()%10);

  // Write alarm datetime to the corresponding registers
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write((byte)0x07); // start at byte 7
  Wire.write((byte)seconds);
  Wire.write((byte)minutes);
  Wire.write((byte)hours);
  Wire.write((byte)date);
  Wire.endTransmission();

  // Get the current control register
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write((byte)0x0E); // start at byte 0Eh
  Wire.endTransmission();
  Wire.requestFrom(DS3231_ADDRESS, 1);
  uint8_t control = Wire.read();

  // Update the control register
  control |= B00000101;
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write((byte)0x0E); // start at byte 0Eh
  Wire.write((byte)control);
  Wire.endTransmission();
}

void cancel_alarm(){
  // Get the current control register
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write((byte)0x0E); // start at byte 0Eh
  Wire.endTransmission();
  Wire.requestFrom(DS3231_ADDRESS, 1);
  uint8_t control = Wire.read();

  // Update the control register to turn of alarm 1
  control &= B11111110;
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write((byte)0x0E); // start at byte 0Eh
  Wire.write((byte)control);
  Wire.endTransmission();

  // reset all alarm field
  start_alarm_sequence = false;
  alarm_loop_counter = 0;
  sunrise_minutes = 0;
  start_sunrise_sequence = false;
  sunrise_loop_counter = 0;
  light_step = 0;
  is_first_interrupt = true;
  alarm_time = DateTime(0);
}

void alarm_interrupt(){
  Serial.println("INTERUPTED!");
  // Get the current control status register
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write((byte)0x0F); // start at byte 0Eh
  Wire.endTransmission();
  Wire.requestFrom(DS3231_ADDRESS, 1);
  uint8_t control_status = Wire.read();

  // Update the control status register to clear interrupt flag
  control_status &= B11111110;
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write((byte)0x0F); // start at byte 0Eh
  Wire.write((byte)control_status);
  Wire.endTransmission();
  
  // ensure that for sunrise all LEDs start off
  if(is_first_interrupt && sunrise_minutes>0){
    Serial.println("Sunrise Started!");
    start_sunrise_sequence = true;
    is_first_interrupt = false;
    analogWrite(GREEN, 0);
    analogWrite(RED, 0);
    analogWrite(BLUE, 0);

    double num_loop_till_alarm = (double)sunrise_minutes*60 / LOOP_DELAY * 1000;
    Serial.print("# loops till alarm: ");
    Serial.println(num_loop_till_alarm);
    light_step = 255.0/num_loop_till_alarm;
    Serial.print("Light Step: ");
    Serial.println(light_step);

    set_alarm(alarm_time);
    Serial.print("Set alarm for: ");
    print_datetime(alarm_time);
  }
  else{
    Serial.println("Alarm started!");
    start_alarm_sequence = true;
    int alarm_loop_counter = 0;
    is_first_interrupt = true;
    alarm_time = DateTime(0);
    Serial.println("Alarm reset for next alarm!");
  }
}

void connect_to_network(){
  char ssid[] = USER_SSID; // Network SSID
  char pass[] = USER_PASS; // Network Password
  int connection_status = WL_IDLE_STATUS; // stores current wifi state
  while(connection_status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.print(ssid);
    Serial.println("...");
    connection_status = WiFi.begin(ssid, pass); // attempt connection
    delay(10000); // wait 10 seconds for connection
  }
  Serial.print("Connected to ");
  Serial.print(ssid);
  Serial.println("!");
  localip = WiFi.localIP();
  Serial.print("LOCAL IP = ");
  Serial.println(localip);
}

unsigned long int get_current_epoch(){
  while(1){
    int receive_attempts = 0;
    Serial.println("Getting Current Time...");
    sendNTPpacket(timeServer, Udp, packetBuffer, NTP_PACKET_SIZE); // send a request packet to the time server
    while(receive_attempts<50){
      if(Udp.parsePacket()){ // Server returned a packet
        Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
        //the timestamp starts at byte 40 of the received packet and is four bytes, or two words, long
        unsigned long epoch = (word(packetBuffer[40], packetBuffer[41]) << 16 | word(packetBuffer[42], packetBuffer[43])) - 2208988800UL;
        //Serial.println("Time Fetched!");
        return epoch;
      }
      delay(100);
      receive_attempts++;
    }
  }  
}

unsigned long sendNTPpacket(IPAddress& address, WiFiUDP& Udp, byte packetBuffer[], const int& NTP_PACKET_SIZE) // send an NTP request to the time server at the given address
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);   // set all bytes in the buffer to 0
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, send packet requesting a timestamp
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void print_datetime(DateTime dt){
  DateTime local_time = DateTime(dt.unixtime()+USER_TIMEZONE*3600);
  Serial.print(local_time.year(), DEC);
  Serial.print('/');
  Serial.print(local_time.month(), DEC);
  Serial.print('/');
  Serial.print(local_time.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[local_time.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(local_time.hour(), DEC);
  Serial.print(':');
  Serial.print(local_time.minute(), DEC);
  Serial.print(':');
  Serial.print(local_time.second(), DEC);
  Serial.println();
}

#include "WS_ETH.h"
#include <WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// External declarations for web server
extern WebServer server;
extern void handleRoot();
extern void handleGetData();
extern void handleGetInputs();
extern void handleSwitch1();
extern void handleSwitch2();
extern void handleSwitch3();
extern void handleSwitch4();
extern void handleSwitch5();
extern void handleSwitch6();
extern void handleSwitch7();
extern void handleSwitch8();
extern void handleSwitch9();
extern void handleSwitch0();
extern void handleRTCPage();
extern void handleNewEvent();
extern void handleUpTimeAndEvent();
extern void handleDeleteEvent();

static bool eth_connected = false;
static bool eth_connected_Old = false;
IPAddress ETH_ip;
// NTP setup
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", timezone*3600, 60000);  // NTP server, time offset in seconds, update interval

void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      printf("ETH Started\r\n");
      //set eth hostname here
      ETH.setHostname("esp32-eth0");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED: printf("ETH Connected\r\n"); break;
    case ARDUINO_EVENT_ETH_GOT_IP:    printf("ETH Got IP: '%s'\n", esp_netif_get_desc(info.got_ip.esp_netif)); //printf("%s\r\n",ETH);
      ETH_ip = ETH.localIP(); 
      printf("ETH Got IP: %d.%d.%d.%d\n", ETH_ip[0], ETH_ip[1], ETH_ip[2], ETH_ip[3]);
#if USE_TWO_ETH_PORTS
      // printf("%d\r\n",ETH1);
#endif
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      printf("ETH Lost IP\r\n");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      printf("ETH Disconnected\r\n");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      printf("ETH Stopped\r\n");
      eth_connected = false;
      break;
    default: break;
  }
}

void testClient(const char *host, uint16_t port) {
  printf("\nconnecting to \r\n");;
  printf("%s\r\n",host);

  NetworkClient client;
  if (!client.connect(host, port)) {
    printf("connection failed\r\n");
    return;
  }
  client.printf("GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
  while (client.connected() && !client.available());
  while (client.available()) {
    printf("%c",(char)client.read());
  }

  printf("closing connection\n");
  client.stop();
}

void ETH_Init(void) {
  printf("Ethernet Start\r\n");
  Network.onEvent(onEvent);

  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI);

  // Configure static IP for isolated network (no router/DHCP)
  IPAddress local_IP(192, 168, 1, 100);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  ETH.config(local_IP, gateway, subnet);
#if USE_TWO_ETH_PORTS
  ETH1.begin(ETH1_PHY_TYPE, ETH1_PHY_ADDR, ETH1_PHY_CS, ETH1_PHY_IRQ, ETH1_PHY_RST, SPI);
#endif
  xTaskCreatePinnedToCore(
    EthernetTask,    
    "EthernetTask",   
    4096,                
    NULL,                 
    2,                   
    NULL,                 
    0                   
  );
}
void EthernetTask(void *parameter) {
  extern WebServer server;
  while(1){
    if (eth_connected && !eth_connected_Old) {
      eth_connected_Old = eth_connected;
      RGB_Open_Time(0, 60, 0,1000, 0); 
      printf("Network port connected!\r\n");
      
      // Start web server for Ethernet mode
      server.on("/", handleRoot);
      server.on("/getData", handleGetData);
      server.on("/getInputs", handleGetInputs);
      server.on("/Switch1", handleSwitch1);
      server.on("/Switch2", handleSwitch2);
      server.on("/Switch3", handleSwitch3);
      server.on("/Switch4", handleSwitch4);
      server.on("/Switch5", handleSwitch5);
      server.on("/Switch6", handleSwitch6);
      server.on("/Switch7", handleSwitch7);
      server.on("/Switch8", handleSwitch8);
      server.on("/AllOn", handleSwitch9);
      server.on("/AllOff", handleSwitch0);
      server.on("/RTC_Event", handleRTCPage);
      server.on("/NewEvent", handleNewEvent);
      server.on("/getTimeAndEvent", handleUpTimeAndEvent);
      server.on("/DeleteEvent", handleDeleteEvent);
      server.begin();
      printf("ETH - Web server started\r\n");
      // Acquisition_time() skipped - no internet on isolated network (NTP would block forever)
    }
    else if(!eth_connected && eth_connected_Old){
      eth_connected_Old = eth_connected;
      printf("Network port disconnected!\r\n");
    }

    // Handle web server requests
    if (eth_connected) {
      server.handleClient();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
  vTaskDelete(NULL);
}
void Acquisition_time(void) {                                   // Get the network time and set to DS3231 to be called after the WIFI connection is successful
  timeClient.begin();
  timeClient.update();

  time_t currentTime = timeClient.getEpochTime();
  while(currentTime < 1609459200)                           // Using the current timestamp to compare with a known larger value,1609459200 is a known larger timestamp value that corresponds to January 1, 2021
  {
    timeClient.update();  
    currentTime = timeClient.getEpochTime();
    printf("ETH - Online clock error!!!\r\n");
  }
  struct tm *localTime = localtime(&currentTime);
  static datetime_t PCF85063_Time = {0};
  PCF85063_Time.year = localTime->tm_year + 1900;
  PCF85063_Time.month = localTime->tm_mon + 1;
  PCF85063_Time.day = localTime->tm_mday;
  PCF85063_Time.dotw = localTime->tm_wday;
  PCF85063_Time.hour = localTime->tm_hour;
  PCF85063_Time.minute = localTime->tm_min;
  PCF85063_Time.second = localTime->tm_sec;
  PCF85063_Set_All(PCF85063_Time);
}

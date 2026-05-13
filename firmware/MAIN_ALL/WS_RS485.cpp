#include "WS_RS485.h"
#include <algorithm>

HardwareSerial lidarSerial(1);  // Using serial port 1
uint8_t data[][8] = {                                       // ESP32-S3-POE-ETH-8DI-8RO Control Command (RS485 receiving data)
  { 0x06, 0x05, 0x00, 0x01, 0x55, 0x00, 0xA2, 0xED },       // [0]  CH1 Toggle
  { 0x06, 0x05, 0x00, 0x02, 0x55, 0x00, 0x52, 0xED },       // [1]  CH2 Toggle
  { 0x06, 0x05, 0x00, 0x03, 0x55, 0x00, 0x03, 0x2D },       // [2]  CH3 Toggle
  { 0x06, 0x05, 0x00, 0x04, 0x55, 0x00, 0xB2, 0xEC },       // [3]  CH4 Toggle
  { 0x06, 0x05, 0x00, 0x05, 0x55, 0x00, 0xE3, 0x2C },       // [4]  CH5 Toggle
  { 0x06, 0x05, 0x00, 0x06, 0x55, 0x00, 0x13, 0x2C },       // [5]  CH6 Toggle
  { 0x06, 0x05, 0x00, 0x07, 0x55, 0x00, 0x42, 0xEC },       // [6]  CH7 Toggle
  { 0x06, 0x05, 0x00, 0x08, 0x55, 0x00, 0x72, 0xEF },       // [7]  CH8 Toggle
  { 0x06, 0x05, 0x00, 0xFF, 0xFF, 0x00, 0xBD, 0xBD },       // [8]  ALL ON
  { 0x06, 0x05, 0x00, 0xFF, 0x00, 0x00, 0xFC, 0x4D },       // [9]  ALL OFF
  { 0x06, 0x05, 0x00, 0x01, 0xFF, 0x00, 0xDC, 0x4D },       // [10] CH1 ON
  { 0x06, 0x05, 0x00, 0x02, 0xFF, 0x00, 0x2C, 0x4D },       // [11] CH2 ON
  { 0x06, 0x05, 0x00, 0x03, 0xFF, 0x00, 0x7D, 0x8D },       // [12] CH3 ON
  { 0x06, 0x05, 0x00, 0x04, 0xFF, 0x00, 0xCC, 0x4C },       // [13] CH4 ON
  { 0x06, 0x05, 0x00, 0x05, 0xFF, 0x00, 0x9D, 0x8C },       // [14] CH5 ON
  { 0x06, 0x05, 0x00, 0x06, 0xFF, 0x00, 0x6D, 0x8C },       // [15] CH6 ON
  { 0x06, 0x05, 0x00, 0x07, 0xFF, 0x00, 0x3C, 0x4C },       // [16] CH7 ON
  { 0x06, 0x05, 0x00, 0x08, 0xFF, 0x00, 0x0C, 0x4F },       // [17] CH8 ON
  { 0x06, 0x05, 0x00, 0x01, 0x00, 0x00, 0x9D, 0xBD },       // [18] CH1 OFF
  { 0x06, 0x05, 0x00, 0x02, 0x00, 0x00, 0x6D, 0xBD },       // [19] CH2 OFF
  { 0x06, 0x05, 0x00, 0x03, 0x00, 0x00, 0x3C, 0x7D },       // [20] CH3 OFF
  { 0x06, 0x05, 0x00, 0x04, 0x00, 0x00, 0x8D, 0xBC },       // [21] CH4 OFF
  { 0x06, 0x05, 0x00, 0x05, 0x00, 0x00, 0xDC, 0x7C },       // [22] CH5 OFF
  { 0x06, 0x05, 0x00, 0x06, 0x00, 0x00, 0x2C, 0x7C },       // [23] CH6 OFF
  { 0x06, 0x05, 0x00, 0x07, 0x00, 0x00, 0x7D, 0xBC },       // [24] CH7 OFF
  { 0x06, 0x05, 0x00, 0x08, 0x00, 0x00, 0x4D, 0xBF },       // [25] CH8 OFF
};
uint8_t Send_Data[][8] = {                                  // Modbus RTU Relay Control Command (RS485 send data)
  { 0x01, 0x05, 0x00, 0x00, 0x55, 0x00, 0xF2, 0x9A },       // Modbus RTU Relay CH1 Toggle
  { 0x01, 0x05, 0x00, 0x01, 0x55, 0x00, 0xA3, 0x5A },       // Modbus RTU Relay CH2 Toggle
  { 0x01, 0x05, 0x00, 0x02, 0x55, 0x00, 0x53, 0x5A },       // Modbus RTU Relay CH3 Toggle
  { 0x01, 0x05, 0x00, 0x03, 0x55, 0x00, 0x02, 0x9A },       // Modbus RTU Relay CH4 Toggle
  { 0x01, 0x05, 0x00, 0x04, 0x55, 0x00, 0xB3, 0x5B },       // Modbus RTU Relay CH5 Toggle
  { 0x01, 0x05, 0x00, 0x05, 0x55, 0x00, 0xE2, 0x9B },       // Modbus RTU Relay CH6 Toggle
  { 0x01, 0x05, 0x00, 0x06, 0x55, 0x00, 0x12, 0x9B },       // Modbus RTU Relay CH7 Toggle
  { 0x01, 0x05, 0x00, 0x07, 0x55, 0x00, 0x43, 0x5B },       // Modbus RTU Relay CH8 Toggle
  { 0x01, 0x05, 0x00, 0xFF, 0xFF, 0xFF, 0xFC, 0x4A },       // Modbus RTU Relay ALL ON
  { 0x01, 0x05, 0x00, 0xFF, 0x00, 0x00, 0xFD, 0xFA },       // Modbus RTU Relay ALL OFF
};
uint8_t buf[20] = {0};          // Data storage area
int numRows = sizeof(data) / sizeof(data[0]);

void SetData(uint8_t* data, size_t length) {
  lidarSerial.write(data, length);                          // Send data from the RS485
}
void ReadData(uint8_t* buf, uint8_t length) {
  uint8_t Receive_Flag = 0;       
  Receive_Flag = lidarSerial.available();
  if (Receive_Flag >= length) {
    lidarSerial.readBytes(buf, length); 
    char printBuf[length * 3 + 1];
    sprintf(printBuf, "Received data: ");
    for (int i = 0; i < length; i++) {
      sprintf(printBuf + strlen(printBuf), "%02X ", buf[i]); 
    }
    printf(printBuf); 
    /*************************
    Add a receiving data handler
    *************************/
    Receive_Flag = 0;
    memset(buf, 0, sizeof(buf));   
  }
}
void RS485_Analysis(uint8_t *buf)
{
  switch(buf[1])
  {
    case Extension_CH1:
      SetData(Send_Data[0],sizeof(Send_Data[0])); 
      printf("|***  Toggle expansion channel 1 ***|\r\n");
      break;
    case Extension_CH2:
      SetData(Send_Data[1],sizeof(Send_Data[1])); 
      printf("|***  Toggle expansion channel 2 ***|\r\n");
      break;
    case Extension_CH3:
      SetData(Send_Data[2],sizeof(Send_Data[2])); 
      printf("|***  Toggle expansion channel 3 ***|\r\n");
      break;
    case Extension_CH4:
      SetData(Send_Data[3],sizeof(Send_Data[3])); 
      printf("|***  Toggle expansion channel 4 ***|\r\n");
      break;
    case Extension_CH5:
      SetData(Send_Data[4],sizeof(Send_Data[4])); 
      printf("|***  Toggle expansion channel 5 ***|\r\n");
      break;
    case Extension_CH6:
      SetData(Send_Data[5],sizeof(Send_Data[5])); 
      printf("|***  Toggle expansion channel 6 ***|\r\n");
      break;
    case Extension_CH7:
      SetData(Send_Data[6],sizeof(Send_Data[6])); 
      printf("|***  Toggle expansion channel 7 ***|\r\n");
      break;
    case Extension_CH8:
      SetData(Send_Data[7],sizeof(Send_Data[7])); 
      printf("|***  Toggle expansion channel 8 ***|\r\n");
      break;
    case Extension_ALL_ON:
      SetData(Send_Data[8],sizeof(Send_Data[8])); 
      printf("|***  Enable all extension channels ***|\r\n");
      break;
    case Extension_ALL_OFF:
      SetData(Send_Data[9],sizeof(Send_Data[9])); 
      printf("|***  Close all expansion channels ***|\r\n");
      break;
    default:
      printf("Note : Non-control external device instructions !\r\n");
  }
}
uint32_t Baudrate = 0;
double  transmission_time = 0;
double RS485_cmd_Time = 0;
void RS485_Init()                                             // Initializing serial port
{    
  Baudrate = 9600;                                            // Set the baud rate of the serial port                                              
  lidarSerial.begin(Baudrate, SERIAL_8N1, RXD1, TXD1);        // Initializing serial port
  transmission_time = 10.0 / Baudrate * 1000 ;
  RS485_cmd_Time = transmission_time*8;                       // 8:data length
  xTaskCreatePinnedToCore(
    RS485Task,    
    "RS485Task",   
    4096,                
    NULL,                 
    3,                   
    NULL,                 
    0                   
  );
}

void RS485Task(void *parameter) {
  while(1){
    RS485_Loop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  vTaskDelete(NULL);
}

void RS485_Loop()
{
  uint8_t Receive_Flag = 0;       // Receiving mark
  Receive_Flag = lidarSerial.available();

  if (Receive_Flag > 0) {
    if(RS485_cmd_Time > 1)
      delay((uint16_t)RS485_cmd_Time);
    else
      delay(1);
    Receive_Flag = lidarSerial.available();
    lidarSerial.readBytes(buf, Receive_Flag);
    if(Receive_Flag == 9){
      // Byte 0: device address — discard if not ours
      if(buf[0] != DEVICE_ADDRESS){
        Receive_Flag = 0;
        memset(buf, 0, sizeof(buf));
        return;
      }
      uint8_t *cmd = buf + 1;  // 8-byte payload after address byte

      // Query digital inputs: ADDR 06 01 00 00 00 00 00 00
      uint8_t query_di[8] = {0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      if(std::equal(cmd, cmd + 8, std::begin(query_di))){
        extern bool DIN_Flag[8];
        uint8_t di_byte = 0;
        for(int j = 0; j < 8; j++){
          if(DIN_Flag[j]) di_byte |= (1 << j);
        }
        uint8_t response[9] = {DEVICE_ADDRESS, 0x06, 0x01, di_byte, 0x00, 0x00, 0x00, 0x00, 0x00};
        delay(2);  // RS485 half-duplex turnaround
        lidarSerial.write(response, 9);
        printf("RS485 DI query → 0x%02X\r\n", di_byte);
        Receive_Flag = 0;
        memset(buf, 0, sizeof(buf));
        return;
      }

      // Relay commands: match 8-byte payload against command table
      uint8_t i = 0;
      for(i = 0; i < numRows; i++){
        if(std::equal(cmd, cmd + 8, std::begin(data[i]))){
          if(i < 8){
            // Per-channel toggle (indices 0-7 → CH1-CH8)
            cmd[0] = i + 1 + 48;
            Relay_Analysis(cmd, RS485_Mode);
          } else if(i == 8){
            // ALL ON
            cmd[0] = '9';
            Relay_Analysis(cmd, RS485_Mode);
          } else if(i == 9){
            // ALL OFF
            cmd[0] = '0';
            Relay_Analysis(cmd, RS485_Mode);
          } else if(i >= 10 && i < 18){
            // Per-channel explicit ON (indices 10-17 → CH1-CH8)
            uint8_t ch = i - 10;
            if(!Relay_Flag[ch]){
              cmd[0] = ch + 1 + 48;
              Relay_Analysis(cmd, RS485_Mode);
            }
          } else {
            // Per-channel explicit OFF (indices 18-25 → CH1-CH8)
            uint8_t ch = i - 18;
            if(Relay_Flag[ch]){
              cmd[0] = ch + 1 + 48;
              Relay_Analysis(cmd, RS485_Mode);
            }
          }
          break;
        }
      }
      if(i >= numRows)
        printf("Note : Non-instruction data was received - RS485 !\r\n");
    }
    else{
      printf("Note : Non-instruction data was received. Bytes: %d - RS485 !\r\n", Receive_Flag);
    }
    Receive_Flag = 0;
    memset(buf, 0, sizeof(buf));
  }
}
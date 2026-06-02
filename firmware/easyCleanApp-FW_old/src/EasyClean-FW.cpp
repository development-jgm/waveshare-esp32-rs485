// NO RF version:

/*
 * Project EasyCleanFW
 * Author: Javier G.
 * Date:
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

// Compile and Flash -> get into the src foder of this project and run the next command to flash to a boron device called NewShopBoron: particle flash NewShopBoron .
// Just compile ->  get into the src foder of this project and run the next command to compile it  for a boron: particle compile boron .
/*

For compiling for other platforms, replace boron with the type of device you have:
p2 (also Photon 2)
boron
argon
bsom (B Series SoM, 4xx)
b5som (B Series SoM, 5xx)
tracker
photon
p1
electron (also E Series)
*/

// Include Particle Device OS APIs
#include "Particle.h"
// ATTENTION: Uncomment next credential lines if on Wifi module (Argon or Photon2)
// #include "wifi_credentials/setup-hotspots.h"

// ATTENTION: Uncomment next line if on Wifi module (Argon or Photon2). This is to be able to set ssid name and password programatically
// ATTENTION:  I have seen that reading the ssid name and password programatically is not working for hotels with captive portal. 
// Therefore, read wifi credentials from setup-hotspots.h ONLY at hotels where there is no captive portal. 
// If there is captive portal, flash the photon2 using hotspot of my mobile phone changed to the name of the ssid first. Then include the mac
// of the photon2 in the whitelist of the hotel's network and give the device to Guillermo so that when the technician powers it up, it conects to the ssid of the hotel.
// SYSTEM_MODE(SEMI_AUTOMATIC);

// ATTENTION: Uncomment the following two lines to use the device on ethernet and set flag isEthernetFeatherWing for this shop as true
// Then simply program photon 2 with this sketch over wifi, and after that connect ethernet cable to the feather wing, insert the photon 2 on the feather wing and power it over usb
// SYSTEM_MODE(SEMI_AUTOMATIC);
// SYSTEM_THREAD(ENABLED); // This line allows your code to run before it gets a successful connection to the Particle Coud following the guide: https://docs.particle.io/reference/device-os/api/ethernet/ethernet/
// Tested on a photon 2 with Particle Ethernet Feather Wing https://store.particle.io/products/particle-ethernet-featherwing
// Photon will connect to the cloud and start sending the message "I am connected over ethernet"
// When using the FeatherWing with Gen 3 devices (Argon, Boron, Xenon), pins D3, D4, and D5 are reserved for Ethernet control pins (reset, interrupt, and chip select). You can not use these pins in your sketch.
// When using Ethernet with the Boron SoM, pins A7, D22, and D8 are reserved for the Ethernet control pins (reset, interrupt, and chip select). You can not use these pins in your sketch.

// Show system, cloud connectivity, and application logs over USB
// View logs with CLI using 'particle serial monitor --follow'
// SerialLogHandler logHandler(LOG_LEVEL_INFO);

// -------Copy and paste with the right config for each shop-----------
// ATTENTION: Test RF module connected to: buzzerPin A2, rfPulsePin S4, rfModulePowerPin = A5 
int shopId = 50;
int machineId[] = {99, 100, 101, 102, 0, 0};
int tariffId[] = {53, 54, 53, 53, 0, 0};
int usagePrice[] = {5, 6, 5, 5, 0, 0};  // ToDo: Get this from the cloud
bool isNonBrandedMachine[] = {false, false, false, false, false, false};
bool isArgon = false;
bool isBoron = false;
bool isPhoton2 = true;
bool isEthernetFeatherWing = false;
bool isEvalBoard = false;
bool isMounBoard = false;
bool isMuonEthernet = false;
int numberOfPulsesPerActivation = 45; // Attention! This value is normally 45, but there are a few exceptions. At Sunset Harbour it was changed to 55 after changing tariff of dryer to 5 euro. In Palm beach, this value is 40, becasue the dryers can not handle exceeding pulses and change to 90 min if exceed 40 pulses. In shops where price is 2euro, this value is 25
// --------------Here ends the configuration------------------------

int startMachinePin[6];      // this is populated on setup depending on device: argon, boron, photon2, ethernetFeatherWing or evalBoard 
int availableMachinePin[6];  // this is populated on setup depending on device: argon, boron, photon2, ethernetFeatherWing or evalBoard 
int isMachinePoweredPin[6];  // this is populated on setup depending on device: argon, boron, photon2, ethernetFeatherWing or evalBoard 
bool activateMachineFromCloudFlag[] = {false, false, false, false, false, false};
bool machineWasActivatedFromCloud[] = {false, false, false, false, false, false};
bool checkAvailableMachinePinLow[] = {false, false, false, false, false, false};
bool checkAvailableMachinePinHigh[] = {false, false, false, false, false, false};
bool aNewPaymentIsPossible[] = {true, true, true, true, true, true};
bool usbConnectedChanged = false;
String azureToken = "";   // DELETE THIS LINE when azure api replaced by supabase
int msTimePulseWidth = 50;
int msTimeBetweenPulses = 200;
int maximumNumberOfMachinesForThisBoard = 0;
 // ATTENTION: Not using watchdog refresh, but still necesarry for shops with watchdog HW to avoid causing resets, this pin will be configured as an
 // output and set to LOW in setup() and then never used again, so the that the reset is nevere triggered by the watchdog
int watchdogRefreshPin;

int publishNetworkInfo(String command) {
    String interfaceActive = "None";
    String ip = "0.0.0.0";

    String mac = "";

    if (WiFi.ready()) {
        interfaceActive = "Wi-Fi";
        ip = WiFi.localIP();
        uint8_t m[6];
        WiFi.macAddress(m);
        mac = String::format("%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    }

    // JSON simplificado
    String payload = "{";
    payload += "\"interface\":\"" + interfaceActive + "\",";
    payload += "\"ip\":\"" + ip + "\"";
    if (mac.length() > 0) {
        payload += ",\"mac\":\"" + mac + "\"";
    }
    payload += "}";

    // Publicamos al Cloud con nombre fijo
    Particle.publish("NetworkInfo", payload, PRIVATE);

    // También lo mostramos por Serial
    Serial.println("==== Network Info ====");
    Serial.println(payload);
    Serial.println("=====================");

    return 1; // necesario para Particle.function
}

void sendCashPaymentToSupabaseEdgeFunction(int machineId, int tariffId)
{
  String data = String::format("{\"machineId\":%d, \"tariffId\":%d}", machineId, tariffId);
  Particle.publish("SupabaseCashPayment/", data, PRIVATE);
}

int getMachineIndexFromMachineId(int machineIdToTranslate)
{
  int machineIndex = -1;
  for (int i = 0; i < 6; i++)
  {
    if (machineId[i] == machineIdToTranslate)
    {
      machineIndex = i;
      break;
    }
  }
  return machineIndex;
}

// Si la máquina está enchufada, retorna un 1
int testMachineIsPowered(String machineId)
{
  int machineIndex = getMachineIndexFromMachineId(machineId.toInt());
  if (digitalRead(isMachinePoweredPin[machineIndex]) == HIGH)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

int activateMachine(String machineId)
{
  int machineIndex = getMachineIndexFromMachineId(machineId.toInt());
  activateMachineFromCloudFlag[machineIndex] = true;
  return 1;
}

void isWorkingMachine0Change()
{
  if (digitalRead(availableMachinePin[0]) == LOW)
  {
    checkAvailableMachinePinLow[0] = true;
  }
  if (digitalRead(availableMachinePin[0]) == HIGH)
  {
    checkAvailableMachinePinHigh[0] = true;
  }
}

void isWorkingMachine1Change()
{
  if (digitalRead(availableMachinePin[1]) == LOW)
  {
    checkAvailableMachinePinLow[1] = true;
  }
  if (digitalRead(availableMachinePin[1]) == HIGH)
  {
    checkAvailableMachinePinHigh[1] = true;
  }
}

void isWorkingMachine2Change()
{
  if (digitalRead(availableMachinePin[2]) == LOW)
  {
    checkAvailableMachinePinLow[2] = true;
  }
  if (digitalRead(availableMachinePin[2]) == HIGH)
  {
    checkAvailableMachinePinHigh[2] = true;
  }
}

void isWorkingMachine3Change()
{
  if (digitalRead(availableMachinePin[3]) == LOW)
  {
    checkAvailableMachinePinLow[3] = true;
  }
  if (digitalRead(availableMachinePin[3]) == HIGH)
  {
    checkAvailableMachinePinHigh[3] = true;
  }
}

void isWorkingMachine4Change()
{
  if (digitalRead(availableMachinePin[4]) == LOW)
  {
    checkAvailableMachinePinLow[4] = true;
  }
  if (digitalRead(availableMachinePin[4]) == HIGH)
  {
    checkAvailableMachinePinHigh[4] = true;
  }
}

void isWorkingMachine5Change()
{
  if (digitalRead(availableMachinePin[5]) == LOW)
  {
    checkAvailableMachinePinLow[5] = true;
  }
  if (digitalRead(availableMachinePin[5]) == HIGH)
  {
    checkAvailableMachinePinHigh[5] = true;
  }
}

void checkAvailableMachinePinLowFunction(int machineNumber)
{
  checkAvailableMachinePinLow[machineNumber] = false;
  for (int i = 0; i < 10; i++)
  {
    delay(10);
    if (digitalRead(availableMachinePin[machineNumber]) == HIGH)
    {
      return;
    }
  }
  machineWasActivatedFromCloud[machineNumber] = false;
  aNewPaymentIsPossible[machineNumber] = true;
}

void checkAvailableMachinePinHighFunction(int machineNumber)
{
  checkAvailableMachinePinHigh[machineNumber] = false;
  for (int i = 0; i < 10; i++)
  {
    delay(10);
    if (digitalRead(availableMachinePin[machineNumber]) == LOW)
    {
      return;
    }
  }
  if (machineWasActivatedFromCloud[machineNumber])
  {
    return;
  }
  if (!aNewPaymentIsPossible[machineNumber])
  {
    return;
  }
  if (digitalRead(isMachinePoweredPin[machineNumber]) == HIGH)
  { // Si la máquina está apagada, no ha habido un pago, sino que aparece como no disponible porque precisamente está apagada
    return;
  }
  sendCashPaymentToSupabaseEdgeFunction(machineId[machineNumber], tariffId[machineNumber]);
  aNewPaymentIsPossible[machineNumber] = false;
}

// DELETE THE FOLLOWING HANDLER when azure api replaced by supabase
// Handle the integration response
void myHandlerForLogClientIn(const char *event, const char *data)
{
  azureToken = data;
}

// Function called from the backend to test if boron is online
int testConnectionToShop(String data)
{
  return data.toInt();
}

// Si la máquina está siendo usada actualmente, retorna un 1
int testMachineIsUnderUsage(String machineId)
{
  int machineIndex = getMachineIndexFromMachineId(machineId.toInt());
  if (digitalRead(availableMachinePin[machineIndex]) == HIGH)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

void configureDeviceForShop(){
  // ATTENTION: Comment next if-block if not on Boron or Argon
  // if (isBoron || isArgon)
  // {
  //   maximumNumberOfMachinesForThisBoard = 6;
  //   startMachinePin[0] = A4;
  //   startMachinePin[1] = D12;
  //   startMachinePin[2] = D9;
  //   startMachinePin[3] = D2;
  //   startMachinePin[4] = D5;
  //   startMachinePin[5] = D8;
  //   availableMachinePin[0] = A3;
  //   availableMachinePin[1] = D13;
  //   availableMachinePin[2] = D10;
  //   availableMachinePin[3] = D1;
  //   availableMachinePin[4] = D4;
  //   availableMachinePin[5] = D7;
  //   isMachinePoweredPin[0] = A2;
  //   isMachinePoweredPin[1] = A5;
  //   isMachinePoweredPin[2] = D11;
  //   isMachinePoweredPin[3] = D0;
  //   isMachinePoweredPin[4] = D3;
  //   isMachinePoweredPin[5] = D6;
  //   watchdogRefreshPin = A0;
  //   return;
  // }
  // ATTENTION: Comment next if-block if not on Photon2 
  if (isPhoton2)
  {
    maximumNumberOfMachinesForThisBoard = 6;
    startMachinePin[0] = S4;
    startMachinePin[1] = S0;
    startMachinePin[2] = D8;
    startMachinePin[3] = D2;
    startMachinePin[4] = D5;
    startMachinePin[5] = D10;
    availableMachinePin[0] = A5;
    availableMachinePin[1] = S2;
    availableMachinePin[2] = D9;
    availableMachinePin[3] = D1;
    availableMachinePin[4] = D4;
    availableMachinePin[5] = D7;
    isMachinePoweredPin[0] = A2;
    isMachinePoweredPin[1] = S3;
    isMachinePoweredPin[2] = S1;
    isMachinePoweredPin[3] = D0;
    isMachinePoweredPin[4] = D3;
    isMachinePoweredPin[5] = D6;
    watchdogRefreshPin = A0;
    return;
  }
  // ATTENTION: Comment next if-block if not on EthernetFeatherWing
  // if (isEthernetFeatherWing)
  // {
  //   maximumNumberOfMachinesForThisBoard = 6;
  //   System.enableFeature(FEATURE_ETHERNET_DETECTION);
  //   Particle.connect();
  //   startMachinePin[0] = S1;
  //   startMachinePin[1] = S0;
  //   startMachinePin[2] = A2;
  //   startMachinePin[3] = D2;
  //   startMachinePin[4] = D5;
  //   startMachinePin[5] = D10;
  //   availableMachinePin[0] = D9;
  //   availableMachinePin[1] = S3;
  //   availableMachinePin[2] = A5;
  //   availableMachinePin[3] = D1;
  //   availableMachinePin[4] = D4;
  //   availableMachinePin[5] = D7;
  //   isMachinePoweredPin[0] = D8;
  //   isMachinePoweredPin[1] = S2;
  //   isMachinePoweredPin[2] = S4;
  //   isMachinePoweredPin[3] = D0;
  //   isMachinePoweredPin[4] = D3;
  //   isMachinePoweredPin[5] = D6;
  //   watchdogRefreshPin = A0;
  //   return;
  // }
  // ATTENTION: Comment next if-block if not on Evaluation Board
  // if (isEvalBoard)
  // {
  //   maximumNumberOfMachinesForThisBoard = 6;
  //   startMachinePin[0] = D7;
  //   startMachinePin[1] = D4;
  //   startMachinePin[2] = D13;
  //   startMachinePin[3] = D8;
  //   startMachinePin[4] = A2;
  //   startMachinePin[5] = A4;
  //   availableMachinePin[0] = D6;
  //   availableMachinePin[1] = D23;
  //   availableMachinePin[2] = D12;
  //   availableMachinePin[3] = D3;
  //   availableMachinePin[4] = A1;
  //   availableMachinePin[5] = A5;
  //   isMachinePoweredPin[0] = D5;
  //   isMachinePoweredPin[1] = D22;
  //   isMachinePoweredPin[2] = D11;
  //   isMachinePoweredPin[3] = D2;
  //   isMachinePoweredPin[4] = A0;
  //   isMachinePoweredPin[5] = A3;
  //   watchdogRefreshPin = D9;
  //   return;
  // }
  // ATTENTION: Comment next if-block if not on Muon board
  // if (isMounBoard)
  // {
  //   maximumNumberOfMachinesForThisBoard = 5;
  //   startMachinePin[0] = A1;
  //   startMachinePin[1] = A5;
  //   startMachinePin[2] = D24;
  //   startMachinePin[3] = D25;
  //   startMachinePin[4] = A2;
  //   // startMachinePin[5] = (NO HAY SEXTA MAQUINA EN EL MUON)
  //   availableMachinePin[0] = D4;
  //   availableMachinePin[1] = D27;
  //   availableMachinePin[2] = D6;
  //   availableMachinePin[3] = D22;
  //   availableMachinePin[4] = D5;
  //   // availableMachinePin[5] = (NO HAY SEXTA MAQUINA EN EL MUON)
  //   isMachinePoweredPin[0] = D26;
  //   isMachinePoweredPin[1] = A0;
  //   isMachinePoweredPin[2] = D2;
  //   isMachinePoweredPin[3] = A6;
  //   isMachinePoweredPin[4] = D3;
  //   // isMachinePoweredPin[5] = (NO HAY SEXTA MAQUINA EN EL MUON)
  //   watchdogRefreshPin = D20;
  //   return;
  // }
}

// ATTENTION: Comment next function, if not on Wifi module (Argon or Photon2)
// void setupWifi() {
//   WiFi.on();
//   WiFi.disconnect();
//   WiFi.clearCredentials();
//   int numWifiCreds = sizeof(wifiCreds) / sizeof(*wifiCreds);
//   for (int i = 0; i < numWifiCreds; i++) {
//     credentials creds = wifiCreds[i];
//     WiFi.setCredentials(creds.ssid, creds.password, creds.authType, creds.cipher);
//   }
//   WiFi.connect();
//   waitUntil(WiFi.ready);
//   Particle.connect();
// }

void setup()
{
  // Log.trace("This is trace message");
  // Log.info("This is info message");
  // Log.warn("This is warn message");
  // Log.error("This is error message");

  // ATTENTION: Comment next if-block, if not on Wifi module (Argon or Photon2)
  // if (isPhoton2 || isArgon)
  // {
  //   setupWifi();
  // }

  configureDeviceForShop();

   // ATTENTION: Not using watchdog refresh, but still necesarry for shops with watchdog HW to avoid causing resets, this pin will be configured as an
 // output and set to LOW in setup() and then never used again, so the that the reset is nevere triggered by the watchdog
  pinMode(watchdogRefreshPin, OUTPUT);
  digitalWrite(watchdogRefreshPin, LOW);

  // === DELETE THIS BLOCK when azure api replaced by supabase=========================
  // Subscribe to the integration response event
  Particle.subscribe("hook-response/AzureLogClientIn/", myHandlerForLogClientIn, MY_DEVICES);
  // === DELETE THIS BLOCK when azure api replaced by supabase=========================

  for (int i = 0; i < 6; i++)
  {
    if (isEthernetFeatherWing && i == 4)
    { // if ethernet featherwing, D3 D4 and D5 are busy with ethernet module, so machine 5 (i = 4) is not working and no pin should be asigned for it as output or input
      continue;
    }
    pinMode(startMachinePin[i], OUTPUT);
    digitalWrite(startMachinePin[i], LOW);
    pinMode(availableMachinePin[i], INPUT_PULLUP);
    pinMode(isMachinePoweredPin[i], INPUT_PULLUP);
  }
  attachInterrupt(availableMachinePin[0], isWorkingMachine0Change, CHANGE);
  attachInterrupt(availableMachinePin[1], isWorkingMachine1Change, CHANGE);
  attachInterrupt(availableMachinePin[2], isWorkingMachine2Change, CHANGE);
  attachInterrupt(availableMachinePin[3], isWorkingMachine3Change, CHANGE);
  if (!isEthernetFeatherWing)
  {
    attachInterrupt(availableMachinePin[4], isWorkingMachine4Change, CHANGE);
  }
  attachInterrupt(availableMachinePin[5], isWorkingMachine5Change, CHANGE);

  Particle.function("activateMachine", activateMachine);
  Particle.function("testConnectionToShop", testConnectionToShop);
  Particle.function("testMachineIsPowered", testMachineIsPowered);
  Particle.function("testMachineIsUnderUsage", testMachineIsUnderUsage);
  Particle.function("publishNetworkInfo", publishNetworkInfo);
}

void loop()
{
  delay(500);

  for (int i = 0; i < 6; i++)
  {
    if (activateMachineFromCloudFlag[i])
    {
      for (int k = 1; k <= 10; k++)
      { // Repeat activation 10 times till it works

        if (isNonBrandedMachine[i])
        {
          // If machine is non branded, activate it through coing reader
          for (int j = 0; j < numberOfPulsesPerActivation; j++)
          {
            digitalWrite(startMachinePin[i], HIGH);
            delay(msTimePulseWidth);
            digitalWrite(startMachinePin[i], LOW);
            delay(msTimeBetweenPulses);
          }
        }
        else
        {
          // If machine is original, activate it through H7 pin as usual
          for (int j = 0; j < usagePrice[i]; j++)
          {
            digitalWrite(startMachinePin[i], HIGH);
            delay(50);
            digitalWrite(startMachinePin[i], LOW);
            delay(50);
          }
        }

        delay(200);

        if (!(isEthernetFeatherWing && i == 4))
        {
          if (digitalRead(availableMachinePin[i]) == HIGH)
          {
            break; // Machine activated, so break the loop
          }
        }
      }
      machineWasActivatedFromCloud[i] = true;
      activateMachineFromCloudFlag[i] = false;
    }
    if (checkAvailableMachinePinLow[i])
    {
      checkAvailableMachinePinLowFunction(i);
    }
    if (checkAvailableMachinePinHigh[i])
    {
      checkAvailableMachinePinHighFunction(i);
    }
    delay(50);
  }
}


// RF version:
// /*
//  * Project EasyCleanFW
//  * Author: Javier G.
//  * Date:
//  * For comprehensive documentation and examples, please visit:
//  * https://docs.particle.io/firmware/best-practices/firmware-template/
//  */

// // Compile and Flash -> get into the src foder of this project and run the next command to flash to a boron device called NewShopBoron: particle flash NewShopBoron .
// // Just compile ->  get into the src foder of this project and run the next command to compile it  for a boron: particle compile boron .
// /*

// For compiling for other platforms, replace boron with the type of device you have:
// p2 (also Photon 2)
// boron
// argon
// bsom (B Series SoM, 4xx)
// b5som (B Series SoM, 5xx)
// tracker
// photon
// p1
// electron (also E Series)
// */

// // Include Particle Device OS APIs
// #include "Particle.h"
// // ATTENTION: Uncomment next credential lines if on Wifi module (Argon or Photon2)
// // #include "wifi_credentials/setup-hotspots.h"

// // ATTENTION: Uncomment next line if on Wifi module (Argon or Photon2). This is to be able to set ssid name and password programatically
// // ATTENTION:  I have seen that reading the ssid name and password programatically is not working for hotels with captive portal.
// // Therefore, read wifi credentials from setup-hotspots.h ONLY at hotels where there is no captive portal.
// // If there is captive portal, flash the photon2 using hotspot of my mobile phone changed to the name of the ssid first. Then include the mac
// // of the photon2 in the whitelist of the hotel's network and give the device to Guillermo so that when the technician powers it up, it conects to the ssid of the hotel.
// // SYSTEM_MODE(SEMI_AUTOMATIC);

// // ATTENTION: Uncomment the following two lines to use the device on ethernet and set flag isEthernetFeatherWing for this shop as true
// // Then simply program photon 2 with this sketch over wifi, and after that connect ethernet cable to the feather wing, insert the photon 2 on the feather wing and power it over usb
// // SYSTEM_MODE(SEMI_AUTOMATIC);
// // SYSTEM_THREAD(ENABLED); // This line allows your code to run before it gets a successful connection to the Particle Coud following the guide: https://docs.particle.io/reference/device-os/api/ethernet/ethernet/
// // Tested on a photon 2 with Particle Ethernet Feather Wing https://store.particle.io/products/particle-ethernet-featherwing
// // Photon will connect to the cloud and start sending the message "I am connected over ethernet"
// // When using the FeatherWing with Gen 3 devices (Argon, Boron, Xenon), pins D3, D4, and D5 are reserved for Ethernet control pins (reset, interrupt, and chip select). You can not use these pins in your sketch.
// // When using Ethernet with the Boron SoM, pins A7, D22, and D8 are reserved for the Ethernet control pins (reset, interrupt, and chip select). You can not use these pins in your sketch.

// // Show system, cloud connectivity, and application logs over USB
// // View logs with CLI using 'particle serial monitor --follow'
// // SerialLogHandler logHandler(LOG_LEVEL_INFO);

// // -------Copy and paste with the right config for each shop-----------
// // ATTENTION: Test RF module connected to: buzzerPin A2, rfPulsePin S4, rfModulePowerPin = A5
// int shopId = 72;
// int machineId[] = {0, 147, 148, 0, 0, 0}; // ATTENTION: Boquilla 1 se dejó para RF module
// int tariffId[] = {0, 85, 86, 0, 0, 0};
// int usagePrice[] = {0, 5, 4, 0, 0, 0}; // ToDo: Get this from the cloud
// bool isNonBrandedMachine[] = {false, false, false, false, false, false};
// bool isArgon = false;
// bool isBoron = false;
// bool isPhoton2 = true;
// bool isEthernetFeatherWing = false;
// bool isEvalBoard = false;
// bool isMounBoard = false;
// // --------------Here ends the configuration------------------------

// int startMachinePin[6];     // this is populated on setup depending on device: argon, boron, photon2, ethernetFeatherWing or evalBoard
// int availableMachinePin[6]; // this is populated on setup depending on device: argon, boron, photon2, ethernetFeatherWing or evalBoard
// int isMachinePoweredPin[6]; // this is populated on setup depending on device: argon, boron, photon2, ethernetFeatherWing or evalBoard
// bool activateMachineFromCloudFlag[] = {false, false, false, false, false, false};
// bool machineWasActivatedFromCloud[] = {false, false, false, false, false, false};
// bool checkAvailableMachinePinLow[] = {false, false, false, false, false, false};
// bool checkAvailableMachinePinHigh[] = {false, false, false, false, false, false};
// bool aNewPaymentIsPossible[] = {true, true, true, true, true, true};
// bool usbConnectedChanged = false;
// String azureToken = ""; // DELETE THIS LINE when azure api replaced by supabase
// int msTimePulseWidth = 50;
// int msTimeBetweenPulses = 200;
// int numberOfPulsesPerActivation = 55; // Attention! It was originally 45 but then set to 55 after sunset harbour changed tariff of dryer to 5 euro
// int maximumNumberOfMachinesForThisBoard = 0;
// // ATTENTION: Not using watchdog refresh, but still necesarry for shops with watchdog HW to avoid causing resets, this pin will be configured as an
// // output and set to LOW in setup() and then never used again, so the that the reset is nevere triggered by the watchdog
// int watchdogRefreshPin;
// int buzzerPin;                                   // buzzer pasivo
// int rfPulsePin;                                  // entrada de pulsos (optoacoplador)
// int rfModulePowerPin;                            // pin para controlar alimentación del módulo RF
// const unsigned long rfPulseTimeout = 2000;       // tiempo sin pulsos para terminar la ráfaga
// const unsigned long rfLongPulseThreshold = 2000; // tiempo para considerar pulso largo
// const unsigned long rfShortPulseMax = 1000;      // máximo para considerar pulso corto
// bool isMaintenanceModeActive = false;
// #define NOTE_C7 2093
// #define NOTE_D7 2349
// #define NOTE_E7 2637
// #define NOTE_G7 3136
// int melody_beep[] = {NOTE_C7};
// int noteDurations_beep[] = {4};
// int melody_mario[] = {NOTE_E7, NOTE_E7, 0, NOTE_E7, 0, NOTE_C7, NOTE_E7, 0, NOTE_G7};
// int noteDurations_mario[] = {8, 8, 8, 8, 8, 8, 8, 8, 4};
// int melody_ack[] = {NOTE_C7, NOTE_C7, NOTE_C7};
// int noteDurations_ack[] = {12, 12, 12};

// void playMelody(int melody[], int durations[], int length)
// {
//   for (int i = 0; i < length; i++)
//   {
//     int noteDuration = 1000 / durations[i];
//     if (melody[i] > 0)
//       tone(buzzerPin, melody[i], noteDuration);
//     delay(noteDuration * 1.3);
//     noTone(buzzerPin);
//   }
// }

// void rfCheck()
// {
//   static unsigned long pulseLowStart = 0;
//   static bool pulseActive = false;
//   static bool marioPlayed = false;
//   static unsigned int burstCount = 0;
//   static unsigned long lastPulseTime = 0;

//   unsigned long now = millis();
//   int pinState = digitalRead(rfPulsePin);

//   // --- Detectar inicio del pulso ---
//   if (pinState == LOW && !pulseActive)
//   {
//     pulseActive = true;
//     pulseLowStart = now;
//     marioPlayed = false; // reset para nuevo pulso
//   }

//   // --- Detectar pulso largo en tiempo real ---
//   if (pulseActive && !marioPlayed)
//   {
//     unsigned long heldTime = now - pulseLowStart;
//     if (heldTime >= rfLongPulseThreshold)
//     {
//       if (!isMaintenanceModeActive)
//       {
//         // Primer pulso largo → reproducir Mario y activar modo mantenimiento
//         playMelody(melody_mario, noteDurations_mario, sizeof(melody_mario) / sizeof(int));
//         isMaintenanceModeActive = true;
//         Particle.publish("MaintenanceMode", "Activated", PRIVATE);
//       }
//       else
//       {
//         // Segundo pulso largo → reproducir acknowledgement y desactivar modo mantenimiento
//         playMelody(melody_ack, noteDurations_ack, sizeof(melody_ack) / sizeof(int));
//         isMaintenanceModeActive = false;
//         Particle.publish("MaintenanceMode", "Deactivated", PRIVATE);
//       }
//       marioPlayed = true; // evitar reproducir de nuevo mientras el pin sigue LOW
//     }
//   }

//   // --- Detectar fin del pulso ---
//   if (pinState == HIGH && pulseActive)
//   {
//     pulseActive = false;
//     unsigned long pulseDuration = now - pulseLowStart;

//     // Pulsos cortos → ráfaga solo si está activo el modo mantenimiento
//     if (isMaintenanceModeActive && pulseDuration > 0 && pulseDuration <= rfShortPulseMax)
//     {
//       burstCount++;
//       playMelody(melody_beep, noteDurations_beep, sizeof(melody_beep) / sizeof(int));
//       lastPulseTime = now;
//     }
//   }

//   // --- Activar máquina e insertar registro en supabase si el número de pulsos está entre 1 y 5 ---
//   if (isMaintenanceModeActive && burstCount > 0 && (now - lastPulseTime > rfPulseTimeout))
//   {
//     if (burstCount >= 1 || burstCount <= 5)
//     {
//       activateMachineFromCloudFlag[burstCount] = true;
//       String data = String::format("{\"selected_machine_id\":%d}", machineId[burstCount]);
//       Particle.publish("SupabaseRfActivation/", data, PRIVATE);
//     }
//     burstCount = 0;
//   }
// }

// void sendCashPaymentToSupabaseEdgeFunction(int machineId, int tariffId)
// {
//   String data = String::format("{\"machineId\":%d, \"tariffId\":%d}", machineId, tariffId);
//   Particle.publish("SupabaseCashPayment/", data, PRIVATE);
// }

// int getMachineIndexFromMachineId(int machineIdToTranslate)
// {
//   int machineIndex = -1;
//   for (int i = 0; i < 6; i++)
//   {
//     if (machineId[i] == machineIdToTranslate)
//     {
//       machineIndex = i;
//       break;
//     }
//   }
//   return machineIndex;
// }

// // Si la máquina está enchufada, retorna un 1
// int testMachineIsPowered(String machineId)
// {
//   int machineIndex = getMachineIndexFromMachineId(machineId.toInt());
//   if (digitalRead(isMachinePoweredPin[machineIndex]) == HIGH)
//   {
//     return 0;
//   }
//   else
//   {
//     return 1;
//   }
// }

// int activateMachine(String machineId)
// {
//   int machineIndex = getMachineIndexFromMachineId(machineId.toInt());
//   activateMachineFromCloudFlag[machineIndex] = true;
//   return 1;
// }

// void isWorkingMachine0Change()
// {
//   if (digitalRead(availableMachinePin[0]) == LOW)
//   {
//     checkAvailableMachinePinLow[0] = true;
//   }
//   if (digitalRead(availableMachinePin[0]) == HIGH)
//   {
//     checkAvailableMachinePinHigh[0] = true;
//   }
// }

// void isWorkingMachine1Change()
// {
//   if (digitalRead(availableMachinePin[1]) == LOW)
//   {
//     checkAvailableMachinePinLow[1] = true;
//   }
//   if (digitalRead(availableMachinePin[1]) == HIGH)
//   {
//     checkAvailableMachinePinHigh[1] = true;
//   }
// }

// void isWorkingMachine2Change()
// {
//   if (digitalRead(availableMachinePin[2]) == LOW)
//   {
//     checkAvailableMachinePinLow[2] = true;
//   }
//   if (digitalRead(availableMachinePin[2]) == HIGH)
//   {
//     checkAvailableMachinePinHigh[2] = true;
//   }
// }

// void isWorkingMachine3Change()
// {
//   if (digitalRead(availableMachinePin[3]) == LOW)
//   {
//     checkAvailableMachinePinLow[3] = true;
//   }
//   if (digitalRead(availableMachinePin[3]) == HIGH)
//   {
//     checkAvailableMachinePinHigh[3] = true;
//   }
// }

// void isWorkingMachine4Change()
// {
//   if (digitalRead(availableMachinePin[4]) == LOW)
//   {
//     checkAvailableMachinePinLow[4] = true;
//   }
//   if (digitalRead(availableMachinePin[4]) == HIGH)
//   {
//     checkAvailableMachinePinHigh[4] = true;
//   }
// }

// void isWorkingMachine5Change()
// {
//   if (digitalRead(availableMachinePin[5]) == LOW)
//   {
//     checkAvailableMachinePinLow[5] = true;
//   }
//   if (digitalRead(availableMachinePin[5]) == HIGH)
//   {
//     checkAvailableMachinePinHigh[5] = true;
//   }
// }

// void checkAvailableMachinePinLowFunction(int machineNumber)
// {
//   checkAvailableMachinePinLow[machineNumber] = false;
//   for (int i = 0; i < 10; i++)
//   {
//     delay(10);
//     if (digitalRead(availableMachinePin[machineNumber]) == HIGH)
//     {
//       return;
//     }
//   }
//   machineWasActivatedFromCloud[machineNumber] = false;
//   aNewPaymentIsPossible[machineNumber] = true;
// }

// void checkAvailableMachinePinHighFunction(int machineNumber)
// {
//   checkAvailableMachinePinHigh[machineNumber] = false;
//   for (int i = 0; i < 10; i++)
//   {
//     delay(10);
//     if (digitalRead(availableMachinePin[machineNumber]) == LOW)
//     {
//       return;
//     }
//   }
//   if (machineWasActivatedFromCloud[machineNumber])
//   {
//     return;
//   }
//   if (!aNewPaymentIsPossible[machineNumber])
//   {
//     return;
//   }
//   if (digitalRead(isMachinePoweredPin[machineNumber]) == HIGH)
//   { // Si la máquina está apagada, no ha habido un pago, sino que aparece como no disponible porque precisamente está apagada
//     return;
//   }
//   if (!isMaintenanceModeActive)
//   {
//     sendCashPaymentToSupabaseEdgeFunction(machineId[machineNumber], tariffId[machineNumber]);
//   }
//   aNewPaymentIsPossible[machineNumber] = false;
// }

// // DELETE THE FOLLOWING HANDLER when azure api replaced by supabase
// // Handle the integration response
// void myHandlerForLogClientIn(const char *event, const char *data)
// {
//   azureToken = data;
// }

// // Function called from the backend to test if boron is online
// int testConnectionToShop(String data)
// {
//   return data.toInt();
// }

// // Si la máquina está siendo usada actualmente, retorna un 1
// int testMachineIsUnderUsage(String machineId)
// {
//   int machineIndex = getMachineIndexFromMachineId(machineId.toInt());
//   if (digitalRead(availableMachinePin[machineIndex]) == HIGH)
//   {
//     return 1;
//   }
//   else
//   {
//     return 0;
//   }
// }

// void configureDeviceForShop()
// {
//   // ATTENTION: Comment next if-block if not on Boron or Argon
//   // if (isBoron || isArgon)
//   // {
//   //   maximumNumberOfMachinesForThisBoard = 6;
//   //   startMachinePin[0] = A4;
//   //   startMachinePin[1] = D12;
//   //   startMachinePin[2] = D9;
//   //   startMachinePin[3] = D2;
//   //   startMachinePin[4] = D5;
//   //   startMachinePin[5] = D8;
//   //   availableMachinePin[0] = A3;
//   //   availableMachinePin[1] = D13;
//   //   availableMachinePin[2] = D10;
//   //   availableMachinePin[3] = D1;
//   //   availableMachinePin[4] = D4;
//   //   availableMachinePin[5] = D7;
//   //   isMachinePoweredPin[0] = A2;
//   //   isMachinePoweredPin[1] = A5;
//   //   isMachinePoweredPin[2] = D11;
//   //   isMachinePoweredPin[3] = D0;
//   //   isMachinePoweredPin[4] = D3;
//   //   isMachinePoweredPin[5] = D6;
//   //   watchdogRefreshPin = A0;
//   //   return;
//   // }
//   // ATTENTION: Comment next if-block if not on Photon2
//   if (isPhoton2)
//   {
//     maximumNumberOfMachinesForThisBoard = 6;
//     startMachinePin[1] = S0;
//     startMachinePin[2] = D8;
//     startMachinePin[3] = D2;
//     startMachinePin[4] = D5;
//     startMachinePin[5] = D10;
//     availableMachinePin[1] = S2;
//     availableMachinePin[2] = D9;
//     availableMachinePin[3] = D1;
//     availableMachinePin[4] = D4;
//     availableMachinePin[5] = D7;
//     isMachinePoweredPin[1] = S3;
//     isMachinePoweredPin[2] = S1;
//     isMachinePoweredPin[3] = D0;
//     isMachinePoweredPin[4] = D3;
//     isMachinePoweredPin[5] = D6;
//     watchdogRefreshPin = A0;
//     buzzerPin = A2;        // buzzer pasivo
//     rfPulsePin = S4;       // entrada de pulsos (optoacoplador)
//     rfModulePowerPin = A5; // pin para controlar alimentación del módulo RF
//     return;
//   }
//   // ATTENTION: Comment next if-block if not on EthernetFeatherWing
//   // if (isEthernetFeatherWing)
//   // {
//   //   maximumNumberOfMachinesForThisBoard = 6;
//   //   System.enableFeature(FEATURE_ETHERNET_DETECTION);
//   //   Particle.connect();
//   //   startMachinePin[0] = S1;
//   //   startMachinePin[1] = S0;
//   //   startMachinePin[2] = A2;
//   //   startMachinePin[3] = D2;
//   //   startMachinePin[4] = D5;
//   //   startMachinePin[5] = D10;
//   //   availableMachinePin[0] = D9;
//   //   availableMachinePin[1] = S3;
//   //   availableMachinePin[2] = A5;
//   //   availableMachinePin[3] = D1;
//   //   availableMachinePin[4] = D4;
//   //   availableMachinePin[5] = D7;
//   //   isMachinePoweredPin[0] = D8;
//   //   isMachinePoweredPin[1] = S2;
//   //   isMachinePoweredPin[2] = S4;
//   //   isMachinePoweredPin[3] = D0;
//   //   isMachinePoweredPin[4] = D3;
//   //   isMachinePoweredPin[5] = D6;
//   //   watchdogRefreshPin = A0;
//   //   return;
//   // }
//   // ATTENTION: Comment next if-block if not on Evaluation Board
//   // if (isEvalBoard)
//   // {
//   //   maximumNumberOfMachinesForThisBoard = 6;
//   //   startMachinePin[0] = D7;
//   //   startMachinePin[1] = D4;
//   //   startMachinePin[2] = D13;
//   //   startMachinePin[3] = D8;
//   //   startMachinePin[4] = A2;
//   //   startMachinePin[5] = A4;
//   //   availableMachinePin[0] = D6;
//   //   availableMachinePin[1] = D23;
//   //   availableMachinePin[2] = D12;
//   //   availableMachinePin[3] = D3;
//   //   availableMachinePin[4] = A1;
//   //   availableMachinePin[5] = A5;
//   //   isMachinePoweredPin[0] = D5;
//   //   isMachinePoweredPin[1] = D22;
//   //   isMachinePoweredPin[2] = D11;
//   //   isMachinePoweredPin[3] = D2;
//   //   isMachinePoweredPin[4] = A0;
//   //   isMachinePoweredPin[5] = A3;
//   //   watchdogRefreshPin = D9;
//   //   return;
//   // }
//   // ATTENTION: Comment next if-block if not on Muon board
//   // if (isMounBoard)
//   // {
//   //   maximumNumberOfMachinesForThisBoard = 5;
//   //   startMachinePin[0] = A1;
//   //   startMachinePin[1] = A5;
//   //   startMachinePin[2] = D24;
//   //   startMachinePin[3] = D25;
//   //   startMachinePin[4] = A2;
//   //   startMachinePin[5] = (NO HAY SEXTA MAQUINA EN EL MUON)
//   //   availableMachinePin[0] = D4;
//   //   availableMachinePin[1] = D27;
//   //   availableMachinePin[2] = D6;
//   //   availableMachinePin[3] = D22;
//   //   availableMachinePin[4] = D5;
//   //   availableMachinePin[5] = (NO HAY SEXTA MAQUINA EN EL MUON)
//   //   isMachinePoweredPin[0] = D26;
//   //   isMachinePoweredPin[1] = A0;
//   //   isMachinePoweredPin[2] = D2;
//   //   isMachinePoweredPin[3] = A6;
//   //   isMachinePoweredPin[4] = D3;
//   //   isMachinePoweredPin[5] = (NO HAY SEXTA MAQUINA EN EL MUON)
//   //   watchdogRefreshPin = D20;
//   //   return;
//   // }
// }

// // ATTENTION: Comment next function, if not on Wifi module (Argon or Photon2)
// // void setupWifi() {
// //   WiFi.on();
// //   WiFi.disconnect();
// //   WiFi.clearCredentials();
// //   int numWifiCreds = sizeof(wifiCreds) / sizeof(*wifiCreds);
// //   for (int i = 0; i < numWifiCreds; i++) {
// //     credentials creds = wifiCreds[i];
// //     WiFi.setCredentials(creds.ssid, creds.password, creds.authType, creds.cipher);
// //   }
// //   WiFi.connect();
// //   waitUntil(WiFi.ready);
// //   Particle.connect();
// // }

// void setup()
// {
//   // Log.trace("This is trace message");
//   // Log.info("This is info message");
//   // Log.warn("This is warn message");
//   // Log.error("This is error message");

//   // ATTENTION: Comment next if-block, if not on Wifi module (Argon or Photon2)
//   // if (isPhoton2 || isArgon)
//   // {
//   //   setupWifi();
//   // }

//   configureDeviceForShop();

//   pinMode(rfPulsePin, INPUT_PULLUP);
//   pinMode(buzzerPin, OUTPUT);
//   pinMode(rfModulePowerPin, OUTPUT);
//   digitalWrite(rfModulePowerPin, HIGH); // activar alimentación del módulo RF

//   // ATTENTION: Not using watchdog refresh, but still necesarry for shops with watchdog HW to avoid causing resets, this pin will be configured as an
//   // output and set to LOW in setup() and then never used again, so the that the reset is nevere triggered by the watchdog
//   pinMode(watchdogRefreshPin, OUTPUT);
//   digitalWrite(watchdogRefreshPin, LOW);

//   // === DELETE THIS BLOCK when azure api replaced by supabase=========================
//   // Subscribe to the integration response event
//   Particle.subscribe("hook-response/AzureLogClientIn/", myHandlerForLogClientIn, MY_DEVICES);
//   // === DELETE THIS BLOCK when azure api replaced by supabase=========================

//   for (int i = 0; i < 6; i++)
//   {
//     if (isEthernetFeatherWing && i == 4)
//     { // if ethernet featherwing, D3 D4 and D5 are busy with ethernet module, so machine 5 (i = 4) is not working and no pin should be asigned for it as output or input
//       continue;
//     }
//     pinMode(startMachinePin[i], OUTPUT);
//     digitalWrite(startMachinePin[i], LOW);
//     pinMode(availableMachinePin[i], INPUT_PULLUP);
//     pinMode(isMachinePoweredPin[i], INPUT_PULLUP);
//   }
//   attachInterrupt(availableMachinePin[0], isWorkingMachine0Change, CHANGE);
//   attachInterrupt(availableMachinePin[1], isWorkingMachine1Change, CHANGE);
//   attachInterrupt(availableMachinePin[2], isWorkingMachine2Change, CHANGE);
//   attachInterrupt(availableMachinePin[3], isWorkingMachine3Change, CHANGE);
//   if (!isEthernetFeatherWing)
//   {
//     attachInterrupt(availableMachinePin[4], isWorkingMachine4Change, CHANGE);
//   }
//   attachInterrupt(availableMachinePin[5], isWorkingMachine5Change, CHANGE);

//   Particle.function("activateMachine", activateMachine);
//   Particle.function("testConnectionToShop", testConnectionToShop);
//   Particle.function("testMachineIsPowered", testMachineIsPowered);
//   Particle.function("testMachineIsUnderUsage", testMachineIsUnderUsage);
// }

// void loop()
// {
//   delay(20);

//   rfCheck();

//   for (int i = 0; i < 6; i++)
//   {
//     if (activateMachineFromCloudFlag[i])
//     {
//       for (int k = 1; k <= 10; k++)
//       { // Repeat activation 10 times till it works

//         if (isNonBrandedMachine[i])
//         {
//           // If machine is non branded, activate it through coing reader
//           for (int j = 0; j < numberOfPulsesPerActivation; j++)
//           {
//             digitalWrite(startMachinePin[i], HIGH);
//             delay(msTimePulseWidth);
//             digitalWrite(startMachinePin[i], LOW);
//             delay(msTimeBetweenPulses);
//           }
//         }
//         else
//         {
//           // If machine is original, activate it through H7 pin as usual
//           for (int j = 0; j < usagePrice[i]; j++)
//           {
//             digitalWrite(startMachinePin[i], HIGH);
//             delay(50);
//             digitalWrite(startMachinePin[i], LOW);
//             delay(50);
//           }
//         }

//         delay(200);

//         if (!(isEthernetFeatherWing && i == 4))
//         {
//           if (digitalRead(availableMachinePin[i]) == HIGH)
//           {
//             break; // Machine activated, so break the loop
//           }
//         }
//       }
//       machineWasActivatedFromCloud[i] = true;
//       activateMachineFromCloudFlag[i] = false;
//     }
//     if (checkAvailableMachinePinLow[i])
//     {
//       checkAvailableMachinePinLowFunction(i);
//     }
//     if (checkAvailableMachinePinHigh[i])
//     {
//       checkAvailableMachinePinHighFunction(i);
//     }
//     // delay(50); // Must be deleted so that rfCheck() works properly
//   }
// }

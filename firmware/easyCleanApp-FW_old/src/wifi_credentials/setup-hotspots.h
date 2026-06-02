/*
 * src/wifi_credentials/setup-hotspots.h
 */

// See https://docs.particle.io/reference/firmware/photon/#setcredentials- for details
struct credentials { char *ssid; char *password; int authType; int cipher; };

const credentials wifiCreds[] = {
  // Set wifi creds here (last entry will be tried first when connecting)
  {.ssid="DIGIFIBRA-PLUS-25CF", .password="y4cH3+mP4!", .authType=WPA2, .cipher=WLAN_CIPHER_AES},
  {.ssid="iPhone de Javier", .password="JavierGlez-2", .authType=WPA2, .cipher=WLAN_CIPHER_AES},
  {.ssid="GUILLE", .password="guillermo1", .authType=WPA2, .cipher=WLAN_CIPHER_AES},
  {.ssid="ServicioTecnico", .password="McClean_1", .authType=WPA2, .cipher=WLAN_CIPHER_AES},
  // // Tienda:
  // {.ssid="McClean", .password="McClean2019", .authType=WPA2, .cipher=WLAN_CIPHER_AES}, // mcclean la cuesta lavadoras
  // {.ssid="Lavadores", .password="", .authType=WPA2, .cipher=WLAN_CIPHER_AES}, // Sunset Bay Atamanes
  // {.ssid="ResortWIFI", .password="", .authType=WPA2, .cipher=WLAN_CIPHER_AES} // Sunset Harbour
  // {.ssid="Vodafone7585", .password="Malibu2023", .authType=WPA2, .cipher=WLAN_CIPHER_AES} // Malibu Park Hotel
  // {.ssid="Hollywood Mirage 1", .password="onahotels", .authType=WPA2, .cipher=WLAN_CIPHER_AES} // Hollywood Mirage 1
};

// Si no conozco la wifi de los Muon, les he puesto ServicioTecnico (pass: McClean_1) para que se puedan conectar desde el hotspot de un movil de servicio tecnico
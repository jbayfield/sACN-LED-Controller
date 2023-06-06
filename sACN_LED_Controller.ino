/*
 * E.131/sACN Ethernet receiver node
 * Josh Bayfield - June 2023
 */

#include <ESPAsyncE131.h>
#include <ETH.h>
#include <Adafruit_NeoPixel.h>

// sACN parameters
#define UNIVERSE 1                      // First DMX Universe to listen for
#define UNIVERSE_COUNT 2                // Total number of Universes to listen for, starting at UNIVERSE
#define CONTROL_UNIVERSE 2              // The universe with the data we actually care about

// LED control parameters
#define START_ADDRESS 1                 // Start address to take data from (e.g. DMX 1)
#define STRIP_LENGTH 10                 // Number of pixels to control
#define LED_DATA_PIN 15                 // Pin of the LED strip data line

// Ethernet constants, no touchy
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_POWER_PIN -1          // Pin# of the enable signal for the external crystal oscillator (-1 to disable for internal APLL source)
#define ETH_TYPE ETH_PHY_LAN8720  // Type of the Ethernet PHY (LAN8720 or TLK110)
#define ETH_ADDR 0                // I²C-address of Ethernet PHY (0 or 1 for LAN8720, 31 for TLK110)
#define ETH_MDC_PIN 23            // Pin# of the I²C clock signal for the Ethernet PHY
#define ETH_MDIO_PIN 18           // Pin# of the I²C IO signal for the Ethernet PHY
#define NRST 5

static bool eth_connected = false;
Adafruit_NeoPixel led_strip = Adafruit_NeoPixel(STRIP_LENGTH, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

// ESPAsyncE131 instance with UNIVERSE_COUNT buffer slots
ESPAsyncE131 e131(UNIVERSE_COUNT);

void WiFiEvent(WiFiEvent_t event) // Callback that is triggered when an Ethernet event occurs
{
  switch (event)
  {
  case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      // set eth hostname here
      ETH.setHostname("esp32-sacn-node");
      break;
  case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
  case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex())
      {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
  case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
  case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
  default:
      break;
  }
}

void setup() {
    Serial.begin(115200);
    delay(10);
    WiFi.onEvent(WiFiEvent); // Defines the callback (above) to be called when an Ethernet event occurs

    // Initialise the LED driver...
    led_strip.begin();

    // Initialise the Ethernet interface...
    pinMode(NRST, OUTPUT);
    digitalWrite(NRST, 0);
    delay(200);
    digitalWrite(NRST, 1);
    delay(200);
    digitalWrite(NRST, 0);
    delay(200);
    digitalWrite(NRST, 1);
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
    
    if (e131.begin(E131_MULTICAST, UNIVERSE, UNIVERSE_COUNT))   // Listen via Multicast
        Serial.println(F("Listening for data..."));
    else 
        Serial.println(F("*** e131.begin failed ***"));
}

void loop() {
  if (eth_connected)
  {
    if (!e131.isEmpty()) {
        e131_packet_t packet;
        e131.pull(&packet);
        if(htons(packet.universe) == CONTROL_UNIVERSE) {
          for (int pixel_index = 0; pixel_index < STRIP_LENGTH; pixel_index++) {
            int red_channel = START_ADDRESS + (pixel_index * 3);
            int green_channel = START_ADDRESS + (pixel_index * 3) + 1;
            int blue_channel = START_ADDRESS + (pixel_index * 3) + 2;
            
            int red_value = packet.property_values[red_channel];
            int green_value = packet.property_values[green_channel];
            int blue_value = packet.property_values[blue_channel];
            
            led_strip.setPixelColor(pixel_index, led_strip.Color(red_value, green_value, blue_value));

            Serial.printf("Setting pixel %u | R: %u / G: %u / B: %u \n",
                pixel_index + 1,
                packet.property_values[red_channel],
                packet.property_values[green_channel],
                packet.property_values[blue_channel]
            );
          }
          led_strip.show();
        }
    }
  }
}

//Include Libraries
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Define pins
#define CE_PIN 9
#define CSN_PIN 8

// Create RF24 radio object
RF24 radio(CE_PIN, CSN_PIN);

// Address through which two modules communicate.
const byte address[6] = "JBKPJ";

struct MessageWithTimestamp {
  int packetNumber;
  unsigned long timestamp;
  char message[20];
};

MessageWithTimestamp data;
int cnt = 0;

void setup() {
  while (!Serial);
  Serial.begin(9600);

  // Power stabilization delay
  Serial.println("Power stabilization...\n");
  delay(10000);

  // Initialize the radio object
  radio.begin();

  // Ways to Improve the Range
  radio.setChannel(108);                    // Change Channel Frequency (100~125)
  radio.setDataRate(RF24_250KBPS);          // Use a Lower Data Rate (-94dBm)
  radio.setPALevel(RF24_PA_MAX);            // Use of Higher Output Power (0dBm)

  // Set the address
  radio.openWritingPipe(address);

  // Set module as transmitter
  radio.stopListening();

  Serial.println("[Transmitter Start]");
  Serial.println("Packet No.\tTime(ms)\tMessage");
  Serial.println("===========================================");
}

void loop() {
  data.packetNumber = cnt;
  data.timestamp = millis();
  const char text[] = "Hello World";
  strcpy(data.message, text);

  // Send data to Receiver
  radio.write(&data, sizeof(data));     // Up to 32 bytes (maximum packet)

  // Debug message
  Serial.print(data.packetNumber);
  Serial.print("\t\t");
  Serial.print(data.timestamp);
  Serial.print("\t\t");
  Serial.println(data.message);

  cnt++;

  delay(1000);
}

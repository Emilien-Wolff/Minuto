#include <Wire.h>
#include <SparkFun_TMP117.h>
#include <EEPROM.h>

// Création de l'objet TMP117
TMP117 sensor;

const int thermistorPin = A0;  // Pin analogique pour le point milieu du diviseur de tension
const float R_fixed = 100.0; // Résistance fixe du diviseur (en ohms)
const float V_ref = 4.6;       // Tension de référence de l'Arduino R4
const int adcMax = 1023;       // Résolution du convertisseur analogique-numérique (10 bits)

const int maxSamples = 50;     // Nombre maximum d'échantillons à stocker
float temperatures[maxSamples];  // Tableau pour les températures
float voltages[maxSamples];      // Tableau pour les tensions
int sampleIndex = 0;             // Index d'échantillon actuel

// Adresses EEPROM pour stocker les données
int eepromAddress = 0;

// Fonction pour écrire un float dans l'EEPROM
void EEPROM_writeFloat(int address, float value) {
  byte *data = (byte*)&value;
  for (int i = 0; i < sizeof(float); i++) {
    EEPROM.write(address + i, data[i]);
  }
}

// Fonction pour lire un float depuis l'EEPROM
float EEPROM_readFloat(int address) {
  float value;
  byte *data = (byte*)&value;
  for (int i = 0; i < sizeof(float); i++) {
    data[i] = EEPROM.read(address + i);
  }
  return value;
}

void setup() {
  Wire.begin();
  Serial.begin(115200);

  // Initialisation du TMP117 avec la bibliothèque SparkFun
  if (sensor.begin() == false) {
    Serial.println("Impossible de détecter le TMP117. Veuillez vérifier le câblage !");
    while (1);
  }

  Serial.println("TMP117 détecté. Démarrage du programme...");
}

void loop() {
  // Lecture de la température avec le TMP117
  float temperatureC = sensor.readTempC();

  // Lecture de la tension au point milieu du diviseur de tension
  int adcValue = analogRead(thermistorPin);
  float V_mid = (adcValue / (float)adcMax) * V_ref;

/*
  // Stockage des données dans les tableaux
  if (sampleIndex < maxSamples) {
    temperatures[sampleIndex] = temperatureC;
    voltages[sampleIndex] = V_mid;
    sampleIndex++;
  } else {
    // Lorsque le nombre maximum d'échantillons est atteint, écrire dans l'EEPROM
    Serial.println("Ecriture dans l'EEPROM...");
    for (int i = 0; i < maxSamples; i++) {
      EEPROM_writeFloat(eepromAddress, temperatures[i]);
      eepromAddress += sizeof(float);  // Incrémenter l'adresse pour la température
      EEPROM_writeFloat(eepromAddress, voltages[i]);
      eepromAddress += sizeof(float);  // Incrémenter l'adresse pour la tension
    }

    // Réinitialiser pour recommencer à stocker les nouveaux échantillons
    sampleIndex = 0;
    eepromAddress = 0;
  }
*/
  // Affichage des résultats
  Serial.print("Température: ");
  Serial.print(temperatureC);
  Serial.print(" | Tension: ");
  Serial.println(V_mid);

  delay(1000);  // Attendre 1 seconde entre chaque mesure
}

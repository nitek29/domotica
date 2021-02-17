#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Wire.h>

#define loop_delay 100

#define LED 7

#define motor_tps_on 5000

#define MOTOR_UP 2        //Pin 2 pour le moteur (fermer)
#define MOTOR_DOWN 4      //Pin 4 pour le moteur (ouvrir)

SoftwareSerial bluetooth(10, 11);
LiquidCrystal_I2C lcd(0x27,16,2);

int cpt_motor = 0;       // compteur pour le moteur
int motor_up = 0;        // 0=False, 1=True
int motor_down = 0;      // 0=False, 1=True
int shutter_is_open = 1; // par default c'est ouvert
int mode_auto = 1;  // Mode automatique on par défaut

String etat = "off"; // Etat du chauffage
int seuil = 15;

String c;
String reception;

void setup()
{
    analogReference(INTERNAL);
  
    // Initialisation de l'écran LCD
    lcd.init();
    lcd.backlight();

    // Message Initialisation
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("DOMOTICA");
    lcd.setCursor(0, 1);
    lcd.print("Initialisation");

    // Message initialisation moteur
    pinMode(MOTOR_UP, OUTPUT);
    pinMode(MOTOR_DOWN, OUTPUT);
    
    // Initialisation de la LED
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

    // Initialisation de la communication série
    Serial.begin(9600);    
    while (!Serial) { }
    Serial.println("Demarrage connexion serie : Ok");

    // Initialisation de la communication Bluetooth
    bluetooth.begin(9600);  // 9600 en mode normal / 38400 en mode commande
    while (!bluetooth) {
        Serial.println("Attente reponse bluetooth");
    }
    Serial.println("Demarrage connexion bluetooth serie : Ok");

    // Message Bienvenue
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("DOMOTICA");
    lcd.setCursor(3, 1);
    lcd.print("Bienvenue!");
}

void loop()
{
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DOMOTICA");

    reception = ""; // Message bluetooth
    c = ""; // Message vers Serial

    check_lumiere();
    check_volet();
    check_temp();

    if (bluetooth.available()) {
      reception = bluetooth.readString();
      Serial.print("\nReception de : ");
      Serial.print(reception);
    }
  
    if(Serial.available()) {
      c = Serial.readString();
      Serial.print("\nEnvoie vers Bluetooth : ");
      Serial.print(c);
      bluetooth.print(c);
    }
    Serial.flush();

    reception_commande();
    
    delay(loop_delay);
}


void check_lumiere() {
    int currentLight = analogRead(A0);
    
    lcd.setCursor(12, 0);
    // lcd.print(currentLight);

    if (currentLight < 300) {
        lcd.print("NUIT");
        if(shutter_is_open and !motor_down and mode_auto) fermer_volet();
    } else {
        lcd.print("JOUR");
        if(!shutter_is_open and !motor_up and mode_auto) ouvrir_volet();
    }
}

int get_temperature() {
    return (int)((analogRead(A2) * 500) / 1023);
}

void check_temp() {
    int temperature = (int)((analogRead(A2)*500)/1023);

    lcd.setCursor(0, 1);
    lcd.print("Chauffage : ");

    lcd.setCursor(12, 1);

    if(mode_auto && temperature < seuil) {
        etat = "on";
    } 
    if(mode_auto && temperature >= seuil){
      etat = "off";
    }

    lcd.print(etat);
}

void check_volet() {
    if (( motor_up or motor_down) and cpt_motor*loop_delay <= motor_tps_on) {
        cpt_motor++;

        // turn on or off the led
        if (cpt_motor%2 == 0) {
            digitalWrite(LED, LOW);
        } else {
            digitalWrite(LED, HIGH);
        }

        // set motor to on
        if (motor_up) {
            digitalWrite(MOTOR_UP, HIGH);
        } else {
            digitalWrite(MOTOR_DOWN, HIGH);
        }
    } else {
        // turn off motor and led
        digitalWrite(LED, LOW);
        digitalWrite(MOTOR_UP, LOW);
        digitalWrite(MOTOR_DOWN, LOW);

        if (motor_up) shutter_is_open = 1;
        else if (motor_down) shutter_is_open = 0;

        // reset var
        motor_up = 0;
        motor_down = 0;
        cpt_motor = 0;
    }
}

void ouvrir_volet() {
    if (!shutter_is_open) {
        digitalWrite(MOTOR_DOWN, LOW);

        motor_down = 0;
        motor_up = 1;
        cpt_motor = 0;
    
        Serial.println("[DEBUG] Ouverture des volets !");

        // envoi vers l'autre carte
        bluetooth.listen();
        bluetooth.println("[auto] volet en ouverture");
    }
}

void fermer_volet() {
    if (shutter_is_open) {
        digitalWrite(MOTOR_UP, LOW);

        motor_up = 0;
        motor_down = 1;
        cpt_motor = 0;
        
        Serial.println("[DEBUG] Fermeture des volets !");
        
        // envoi vers l'autre carte
        bluetooth.listen();
        bluetooth.println("[auto] volet en fermeture");
    }
}

void reception_commande() {
    if(reception != "") {
        if(reception == "temperature\r\n") {
            bluetooth.println(get_temperature());
        }
        else if(reception == "volet_ouvert\r\n") {
            if(!shutter_is_open and !motor_up) {
                mode_auto = 0;
                ouvrir_volet();
                bluetooth.println("volet en ouverture...");
            } else {
                bluetooth.println("Volet deja ouvert");
            }
        }
        else if(reception == "volet_ferme\r\n") {
            if(shutter_is_open and !motor_down) {
                mode_auto = 0;
                fermer_volet();
                bluetooth.println("volet en fermeture...");
            } else {
                bluetooth.println("Volet deja ferme");
            }
        }
        else if(reception == "volet_etat\r\n") {
          if(shutter_is_open) {
            bluetooth.println("Les volets sont ouverts");
          } else {
            bluetooth.println("Les volets sont fermes");
          }
        }
        else if(reception == "mode_auto\r\n") {
            if(!mode_auto) {
              bluetooth.println("Mode automatique active");
              mode_auto = 1;
            }
            else {
              bluetooth.println("Mode automatique deja active");
            }
        }
        else if(reception == "chauffage_allume\r\n") {
            if(etat=="on"){
                bluetooth.println("Chauffage deja allume");
            } else {
                mode_auto = 0;
                etat = "on";
                bluetooth.println("Le chauffage s'allume");
            }
        }
        else if(reception == "chauffage_eteint\r\n") {
           if(etat=="off"){
                bluetooth.println("Chauffage deja eteint");
            } else {
                mode_auto = 0;
                etat = "off";
                bluetooth.println("Le chauffage s'eteint");
            }
        }
        else {
          bluetooth.println("Commande inconnue");
        }
    }
}

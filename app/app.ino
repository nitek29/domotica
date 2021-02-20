#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Wire.h>

// Delai utilisé pour la fonction loop
#define loop_delay 100

#define LED 7   //Pin 7 pour la led

#define motor_tps_on 5000  //Duree en ms de l'activation du moteur
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
int seuil = 15; // Seuil de températion pour que le chauffage s'allume automatiquement 

// Variables utilisées pour les échanges bluetooth
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
    // Affichage de DOMOTICA en continu sur l'écran
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DOMOTICA");

    reception = ""; // Message bluetooth
    c = ""; // Message vers Serial

    // Permet de vérifier s'il fait jour ou nuit 
    check_lumiere();

    // Permet de vérifier si les volets doivent être ouvert ou non
    check_volet();

    // Permet de vérifier si les chauffages doivent être allumés
    check_temp();

    // Réception des messages via bluetooth
    if (bluetooth.available()) {
      reception = bluetooth.readString();
      Serial.print("\nReception de : ");
      Serial.print(reception);
    }
  
    // Envoi de message vers le bluetooth via le terminal serie
    if(Serial.available()) {
      c = Serial.readString();
      Serial.print("\nEnvoie vers Bluetooth : ");
      Serial.print(c);
      bluetooth.print(c);
    }
    Serial.flush();

    // Suivant ce qu'on a reçu, traitement de la commande
    reception_commande();
    
    delay(loop_delay);
}


// Fonction permettant d'afficher à l'écran s'il fait jour ou NUIT
// S'il fait nuit, les volets doivent se fermer
// S'il fait jour, les volets doivent s'ouvrir
void check_lumiere() {
    int currentLight = analogRead(A0);  // Lecture de la valeur sur le port A0
    
    lcd.setCursor(12, 0);
    // lcd.print(currentLight);

    // On affiche le jour / nuit suivant la valeur reçue
    if (currentLight < 300) {
        lcd.print("NUIT");
        if(shutter_is_open and !motor_down and mode_auto) fermer_volet(); // Pour être fermé, les volets doivent être en position haute (et le mode auto activé)
    } else {
        lcd.print("JOUR");
        if(!shutter_is_open and !motor_up and mode_auto) ouvrir_volet(); // Pour être ouvert, les volets doivent être en position basse (et le mode auto activé)
    }
}

// Permet de récupérer la température actuelle
int get_temperature() {
    // Récupération de la valeur sur le port A2
    // Valeur converti en température Celsius
    return (int)((analogRead(A2) * 500) / 1023);
}

// Permet de vérifier si les chauffages doivent être allumés ou éteints
void check_temp() {
    int temperature = (int)((analogRead(A2)*500)/1023);

    lcd.setCursor(0, 1);
    lcd.print("Chauffage : ");

    lcd.setCursor(12, 1);

    // Suivant la valeur du seuil (par défaut 15°), on allume ou non la température. (Le mode auto doit être activé)
    if(mode_auto && temperature < seuil) {
        etat = "on";
    } 
    if(mode_auto && temperature >= seuil){
      etat = "off";
    }

    // On affiche à l'écran l'état du chauffage
    lcd.print(etat);
}

// Permet de déterminer si le moteur des volets doit toujours être activé
void check_volet() {
    if (( motor_up or motor_down) and cpt_motor*loop_delay <= motor_tps_on) {
        cpt_motor++;

        // On fait clignoter la led lors de l'activation du moteur
        if (cpt_motor%2 == 0) {
            digitalWrite(LED, LOW); // LED éteint
        } else {
            digitalWrite(LED, HIGH); // LED alumée
        }

        // Allumage du moteur
        if (motor_up) {
            digitalWrite(MOTOR_UP, HIGH); // Moteur allumé dans le sens ouverture
        } else {
            digitalWrite(MOTOR_DOWN, HIGH); // Moteur allumé dans le sens fermeture
        }
    } else {
        // Extinction du moteur et de la LED
        digitalWrite(LED, LOW);
        digitalWrite(MOTOR_UP, LOW);
        digitalWrite(MOTOR_DOWN, LOW);

        // On enregistre l'état du volet (ouvert ou fermé)
        if (motor_up) shutter_is_open = 1;
        else if (motor_down) shutter_is_open = 0;

        // Remise à zéro du contexte du moteur
        motor_up = 0;
        motor_down = 0;
        cpt_motor = 0;
    }
}

// Permet d'ouvrir les volets
void ouvrir_volet() {
    // Si les volets ne sont pas déjà ouverts
    if (!shutter_is_open) {
        digitalWrite(MOTOR_DOWN, LOW);  // Extinction dans le sens fermeture

        motor_down = 0;
        motor_up = 1; // Moteur en position haute
        cpt_motor = 0;
    
        Serial.println("[DEBUG] Ouverture des volets !");

        // envoi vers l'autre carte via la connexion bluetooth
        bluetooth.listen();
        bluetooth.println("[auto] volet en ouverture");
    }
}

// Permet de fermer les volets
void fermer_volet() {
    // Si les volets ne sont pas déjà fermés
    if (shutter_is_open) {
        digitalWrite(MOTOR_UP, LOW);  // Extinction dans le sens ouverture

        motor_up = 0;
        motor_down = 1; // Moteur en position fermé
        cpt_motor = 0;
        
        Serial.println("[DEBUG] Fermeture des volets !");
        
        // envoi vers l'autre carte via la connexion bluetooth
        bluetooth.listen();
        bluetooth.println("[auto] volet en fermeture");
    }
}

// Permet de traiter les messages reçu via la connexion bluetooth
void reception_commande() {
    // Si on a reçu un message
    if(reception != "") {
        // On envoi la température via bluetooth
        if(reception == "temperature\r\n") {
            bluetooth.println(get_temperature());
        }
        // On ouvre les volets (s'ils ne le sont pas déjà (sinon on envoi un message))
        else if(reception == "volet_ouvert\r\n") {
            if(!shutter_is_open and !motor_up) {
                mode_auto = 0;
                ouvrir_volet();
                bluetooth.println("volet en ouverture...");
            } else {
                bluetooth.println("Volet deja ouvert");
            }
        }
        // On ferme les volets (s'ils ne le sont pas déjà (sinon on envoi un message))
        else if(reception == "volet_ferme\r\n") {
            if(shutter_is_open and !motor_down) {
                mode_auto = 0;
                fermer_volet();
                bluetooth.println("volet en fermeture...");
            } else {
                bluetooth.println("Volet deja ferme");
            }
        }
        // On envoi la position des volets via bluetooth
        else if(reception == "volet_etat\r\n") {
          if(shutter_is_open) {
            bluetooth.println("Les volets sont ouverts");
          } else {
            bluetooth.println("Les volets sont fermes");
          }
        }
        // On réactive le mode auto (qui est désactivé dès que l'utilisateur force une commande (changement position volet et état chauffage))
        else if(reception == "mode_auto\r\n") {
            if(!mode_auto) {
              bluetooth.println("Mode automatique active");
              mode_auto = 1;
            }
            else {
              bluetooth.println("Mode automatique deja active");
            }
        }
        // On allume le chauffage (s'il ne l'est pas déjà)
        else if(reception == "chauffage_allume\r\n") {
            if(etat=="on"){
                bluetooth.println("Chauffage deja allume");
            } else {
                mode_auto = 0;
                etat = "on";
                bluetooth.println("Le chauffage s'allume");
            }
        }
        // On éteint le chauffage (s'il ne l'est pas déjà)
        else if(reception == "chauffage_eteint\r\n") {
           if(etat=="off"){
                bluetooth.println("Chauffage deja eteint");
            } else {
                mode_auto = 0;
                etat = "off";
                bluetooth.println("Le chauffage s'eteint");
            }
        }
        // Si le message ne correspond pas à une des commandes, on renvoie un message d'erreur
        else {
          bluetooth.println("Commande inconnue");
        }
    }
}

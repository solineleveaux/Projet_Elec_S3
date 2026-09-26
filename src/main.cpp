#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define adresse_ecran_A 0x3C
#define adresse_ecran_B 0x3D
#define SCREEN_WIDTH 128                         // Largeur de l'écran OLED, en pixels 
#define SCREEN_HEIGHT 64                         // Hauteur de l'écran OLED, en pixels
U8G2_SSD1306_128X64_NONAME_1_HW_I2C ecranA(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_1_HW_I2C ecranB(U8G2_R0, U8X8_PIN_NONE);

#define pinArduinoRaccordementSignalSW  2        // La pin D2 de l'Arduino recevra la ligne SW du module KY-040
#define pinArduinoRaccordementSignalCLK 3        // La pin D3 de l'Arduino recevra la ligne CLK du module KY-040
#define pinArduinoRaccordementSignalDT  4        // La pin D4 de l'Arduino recevra la ligne DT du module KY-040

// Variables
int etatPrecedentLigneSW;                       // Variable qui nous permettra de stocker le dernier état de la ligne SW, afin de le comparer à l'actuel
int etatPrecedentLigneCLK;                      // Variable qui nous permettra de stocker le dernier état de la ligne CLK, afin de le comparer à l'actuel
int etatPrecedentLigneDT;                       // Variable qui nous permettra de stocker le dernier état de la ligne DT, afin de le comparer à l'actuel
int compteur = 0;                               // Variable qui nous permettra de compter combien de crans ont été parcourus, sur l'encodeur   
bool Boutton_Appuye = false;                    // Variable qui memorise les appui boutons de l'encodeur  

// Historique pour la courbe : 1 octet par colonne = 128 octets de RAM
const uint8_t NB_POINTS = 128;
uint8_t historique[NB_POINTS];
uint8_t indexCourant = 0;   // prochaine case à écrire = point le plus ancien

// Zone de la courbe sur l'écran B
const uint8_t COURBE_Y = 12;   // haut de la zone
const uint8_t COURBE_H = 52;   // hauteur (12 + 52 = 64)

// Variable heure pour récupere l'heure
String heure = "16:00";

// Variable bpm pour récupérer le BPM
int bpm = 80;

void changementSurLigneCLK() 
{

  // Lecture des lignes CLK et DT, issue du KY-040, arrivant sur l'arduino
  int etatActuelDeLaLigneCLK = digitalRead(pinArduinoRaccordementSignalCLK);
  int etatActuelDeLaLigneDT = digitalRead(pinArduinoRaccordementSignalDT);

  // Si CLK = 1 et DT = 0, et que l'ancienCLK = 0 et ancienDT = 1, alors le bouton a été tourné d'un cran vers la droite (sens horaire, donc incrémentation)
  if((etatActuelDeLaLigneCLK == HIGH) && (etatActuelDeLaLigneDT == LOW) && (etatPrecedentLigneCLK == LOW) && (etatPrecedentLigneDT == HIGH)) {
    compteur++;   // Alors on incrémente le compteur
  }

  // Si CLK = 1 et DT = 1, et que l'ancienCLK = 0 et ancienDT = 0, alors le bouton a été tourné d'un cran vers la gauche (sens antihoraire, donc décrémentation)
  if((etatActuelDeLaLigneCLK == HIGH) && (etatActuelDeLaLigneDT == HIGH) && (etatPrecedentLigneCLK == LOW) && (etatPrecedentLigneDT == LOW)) {
    compteur--;   // Alors on décrémente le compteur
  }

  // Et on mémorise ces états actuels comme étant "les nouveaux anciens", pour le "tour suivant" !
  etatPrecedentLigneCLK = etatActuelDeLaLigneCLK;
  etatPrecedentLigneDT = etatActuelDeLaLigneDT;
  
}

void changementSurLigneSW() 
{
    // On lit le nouvel état de la ligne SW
    int etatActuelDeLaLigneSW = digitalRead(pinArduinoRaccordementSignalSW);
    if(etatActuelDeLaLigneSW == LOW)
    {
      Boutton_Appuye = true;
    }
    etatPrecedentLigneSW = etatActuelDeLaLigneSW;
}


// Écran A : titre, grande valeur numérique et barre de niveau
void dessinerEcranA() {

  ecranA.firstPage();
  do {
    // Heure en haut centré
    ecranA.setFont(u8g2_font_logisoso24_tn); // Police avec de grands chiffres

    int longeur_texte_pixel = strlen(heure.c_str())*14;
    int position_heure_x = (SCREEN_WIDTH - longeur_texte_pixel)/2;
    ecranA.drawStr(position_heure_x, 26, heure.c_str()); // 26 en y car 24 de police +2

    // On dessine une ligne au milieu de l'écran
    ecranA.drawHLine(10, 30, SCREEN_WIDTH - 2*10);   // ligne de séparation (a partir de 10 pixel a gauche, et de longeur 128 - 2*10)
    // 30 car 24 de police, +2 px au dessus, +4 px en dessous

    // Dessin du Cœur en bas à gauche
    // On peut utiliser un caractère symbole de la police u8g2_font_open_iconic_human_2x_t (code 65 = cœur)
    // Ou le dessiner directement avec des formes géo/lignes pour être indépendant de la police :
    ecranA.drawDisc(24, 46, 4);  // Lobe gauche du cœur
    ecranA.drawDisc(31, 46, 4);  // Lobe droit du cœur
    ecranA.drawTriangle(20, 47, 35, 47, 27, 58); // Pointe du cœur

    // Fréquence cardiaque (bpm)
    ecranA.setFont(u8g2_font_logisoso24_tn); // Grands chiffres pour la valeur
    String bpmStr = String(bpm);

    // Position Y du bas du texte (bas de l'écran moins une marge de 5)
    int yBpm = SCREEN_HEIGHT - 5; 
    
    // Position X du chiffre BPM
    int xBpm = (SCREEN_WIDTH / 2) - 10; // A partir de la moitié de l'écran en x -10
    ecranA.drawStr(xBpm, yBpm, bpmStr.c_str()); // On ecrit

    // Calcul dynamique de la position du texte "bpm" grâce à la largeur réelle du chiffre affiché
    int largeurChiffresBpm = ecranA.getStrWidth(bpmStr.c_str()); // On regarde la longueur de la chaine en pixel du BPM
    int xTexteBpm = xBpm + largeurChiffresBpm + 5; // Marge de 5 pixels après le chiffre

    // "bpm" en petite police
    ecranA.setFont(u8g2_font_6x10_tr);
    ecranA.drawStr(xTexteBpm, yBpm, "bpm");
    
  } while (ecranA.nextPage());
}

// Écran B : titre et courbe défilante des 128 dernières mesures
void dessinerEcranB() {
  ecranB.firstPage();
  do {
    ecranB.setFont(u8g2_font_6x10_tr);
    ecranB.drawStr(5, 12, "PPG"); // Titre "PPG" en haut à gauche

    // Axe X 
    ecranB.drawHLine(5, 52, 100); 
    // Flèche sur l'axe X
    ecranB.drawLine(102, 50, 105, 52);
    ecranB.drawLine(102, 54, 105, 52);
    // Légende axe X
    ecranB.drawStr(85, 62, "t (s)");

    // Axe Y
    ecranB.drawVLine(105, 12, 40);
    // Graduations axe Y
    ecranB.drawStr(110, 15, "1.0");
    ecranB.drawStr(110, 34, "0.5");
    ecranB.drawStr(110, 53, "0.0");

    // 3. Signal PPG factice ("en dur")
    // Tracer une sinusoïde amortie/périodique pour simuler le PPG
    for (int x = 5; x < 104; x++) 
    {
      // Équation factice centrée en Y=32 avec amplitude ~15px
      int y = 32 - (15 * sin((x - 5) * 0.25)); 
      
      if (x > 5) 
      {
        int y_prev = 32 - (15 * sin((x - 6) * 0.25));
        ecranB.drawLine(x - 1, y_prev, x, y);
      }
    }

  } while (ecranB.nextPage());
}


void setup() {
  // Configuration des pins
  pinMode(pinArduinoRaccordementSignalSW, INPUT_PULLUP);
  pinMode(pinArduinoRaccordementSignalCLK, INPUT);
  pinMode(pinArduinoRaccordementSignalDT, INPUT);

  // Initialisation de l'ecran
  Wire.begin();
  ecranA.setI2CAddress(adresse_ecran_A * 2); // U8g2 attend l'adresse sur 8 bits
  ecranB.setI2CAddress(adresse_ecran_B * 2); // U8g2 attend l'adresse sur 8 bits
  ecranA.setBusClock(400000);       // I2C rapide : affichage plus fluide
  ecranB.setBusClock(400000);
  ecranA.begin();
  ecranB.begin();

  etatPrecedentLigneSW = digitalRead(pinArduinoRaccordementSignalSW);
  etatPrecedentLigneCLK = digitalRead(pinArduinoRaccordementSignalCLK);
  etatPrecedentLigneDT = digitalRead(pinArduinoRaccordementSignalDT);

  attachInterrupt(digitalPinToInterrupt(pinArduinoRaccordementSignalCLK), changementSurLigneCLK, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pinArduinoRaccordementSignalSW), changementSurLigneSW, FALLING); // Que sur la descente pour limiter les rebonds

}

void loop() 
{
  dessinerEcranA();
  dessinerEcranB();
  delay(50);
}
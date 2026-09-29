#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>

#define adresse_ecran_A 0x3C
#define adresse_ecran_B 0x3D
#define SCREEN_WIDTH 128                         // Largeur de l'écran OLED, en pixels 
#define SCREEN_HEIGHT 64                         // Hauteur de l'écran OLED, en pixels
U8G2_SSD1306_128X64_NONAME_1_HW_I2C ecranA(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_1_HW_I2C ecranB(U8G2_R0, U8X8_PIN_NONE);

#define pinArduinoRaccordementSignalSW  2        // La pin D2 de l'Arduino recevra la ligne SW du module KY-040
#define pinArduinoRaccordementSignalCLK 3        // La pin D3 de l'Arduino recevra la ligne CLK du module KY-040
#define pinArduinoRaccordementSignalDT  4        // La pin D4 de l'Arduino recevra la ligne DT du module KY-040
#define pinBoutonChoixEcran 9

// Variables
int etatPrecedentLigneSW;                       // Variable qui nous permettra de stocker le dernier état de la ligne SW, afin de le comparer à l'actuel
int etatPrecedentLigneCLK;                      // Variable qui nous permettra de stocker le dernier état de la ligne CLK, afin de le comparer à l'actuel
int etatPrecedentLigneDT;                       // Variable qui nous permettra de stocker le dernier état de la ligne DT, afin de le comparer à l'actuel
int compteur = 0;                               // Variable qui nous permettra de compter combien de crans ont été parcourus, sur l'encodeur   
int ancien_compteur=0;                          // Variable pour l'incrémentation de l'encodeur
bool Boutton_Appuye = false;                    // Variable qui memorise les appui boutons de l'encodeur  
char ecran_selectionne = 'A';                   // Variable qui sait sur quel écran on est, A pour ecran A et B pour écran B
int Choix_reglages = 0;                         // Variable qui sait quel est le chox de l'utilisateur dans les reglages
int Choix_langues = 0;                          // Variable pour le choix de la langue du menu
int Choix_donnes = 0;                          // Variable pour le choix de la langue du menu
char Langue = 'F';                              // Variable qui stocke la langue choisie (F -> Francais, A -> Anglais)
int position_liste = 0;                         // Variable qui sait où on en est dans l'affichage de la liste de données

// Historique pour la courbe : 1 octet par colonne = 128 octets de RAM
const uint8_t NB_POINTS = 128;
uint8_t historique[NB_POINTS];
uint8_t indexCourant = 0;   // prochaine case à écrire = point le plus ancien
int zoomPPG = 3; // Zoom sur la courbe du PPG
int dernierCompteur = 0; // Dernier compteur de l'encodeur

// Zone de la courbe sur l'écran B
const uint8_t COURBE_Y = 12;   // haut de la zone
const uint8_t COURBE_H = 52;   // hauteur (12 + 52 = 64)

// Variable heure pour récupere l'heure
String heure = "16:00";

// Variable bpm pour récupérer le BPM
int bpm = 80;

// Structure de données pour les enregistrement
struct donnes
{
  int mesure;
  DateTime date_heure;
};


// Strucutre pour stocker les différents enregistrement 
struct Table_donnes
{
  int nombre_donnes = 0;
  donnes liste_donnes[20];  // On autorise 20 enregistrements max
};

Table_donnes tables_patient;


// Enumeration des etats de la machine d'etat app
enum{Demarrage, Reglages, Donnees, Langues} etat_app= Demarrage;  // enumeration des etat de app


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

void gererEncodeurSelonEcran() 
{
  int delta = compteur - dernierCompteur; // Calcul de la variation depuis le dernier tour
  if (delta == 0) return;

  if (ecran_selectionne == 'B') 
  {
    zoomPPG += delta; // Sur l'écran B : on utilise le mouvement pour modifier le zoom
    
    // Bornage du zoom (ex: entre 1 et 5)
    if (zoomPPG < 1) zoomPPG = 1;
    if (zoomPPG > 5) zoomPPG = 5;
  } 
  else if (ecran_selectionne == 'A') 
  {
    // Sur l'écran A : tu pourras utiliser 'delta' pour naviguer dans un menu ou changer d'état
    // ex: indexMenu += delta;
  }

  dernierCompteur = compteur; // Remise à niveau du tracker
}


void liste_essai()
{
  tables_patient.nombre_donnes = 5;
  tables_patient.liste_donnes[0].mesure=60;
  tables_patient.liste_donnes[0].date_heure=DateTime(2026, 9, 29, 22, 00, 00);
  tables_patient.liste_donnes[1].mesure=70;
  tables_patient.liste_donnes[1].date_heure=DateTime(2026, 9, 29, 22, 01, 00);
  tables_patient.liste_donnes[2].mesure=80;
  tables_patient.liste_donnes[2].date_heure=DateTime(2026, 9, 29, 23, 00, 00);
  tables_patient.liste_donnes[3].mesure=90;
  tables_patient.liste_donnes[3].date_heure=DateTime(2026, 9, 29, 23, 15, 00);
  tables_patient.liste_donnes[4].mesure=100;
  tables_patient.liste_donnes[4].date_heure=DateTime(2026, 9, 29, 23, 59, 00);

}

void app ()
{
  switch (etat_app)
  {
    case Demarrage : 
      ecranA.firstPage();
      do 
      {
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
      
      ecranB.firstPage();
      do 
      {
        ecranB.setFont(u8g2_font_6x10_tr);
        ecranB.setCursor(5, 12);
        ecranB.print(F("PPG"));
      
        // Axes et repères
        ecranB.drawHLine(5, 52, 100); 
        ecranB.drawLine(102, 50, 105, 52);
        ecranB.drawLine(102, 54, 105, 52);
        ecranB.setCursor(85, 62);
        ecranB.print(F("t (s)"));

        ecranB.drawVLine(105, 12, 40);
        ecranB.setCursor(110, 15);
        ecranB.print(F("1.0"));
        ecranB.setCursor(110, 34);
        ecranB.print(F("0.5"));
        ecranB.setCursor(110, 53);
        ecranB.print(F("0.0"));

        //ecranB.drawStr(110, 15, "1.0");
        //ecranB.drawStr(110, 34, "0.5");
        //ecranB.drawStr(110, 53, "0.0");

        // Facteur d'échelle basé sur la variable zoomPPG dédiée
        float pasTemps = 0.08 * zoomPPG;

        for (int x = 5; x < 104; x++) 
        {
          int y = 32 - (15 * sin((x - 5) * pasTemps)); 
          
          if (x > 5) 
          {
            int y_prev = 32 - (15 * sin((x - 6) * pasTemps));
            ecranB.drawLine(x - 1, y_prev, x, y);
          }
        } 
      } while (ecranB.nextPage());

      if (ecran_selectionne == 'A') 
      {
        if (Boutton_Appuye)
        {
          Boutton_Appuye = false;
          etat_app = Reglages ;
        }
      }

      break;
    
    case Reglages : 
      ecranA.firstPage();
      do 
      {
        ecranA.setFont(u8g2_font_6x10_tr); // Police avec de grands chiffres
        switch (Choix_reglages)
        {
          case 0:
            ecranA.setCursor(0, 10);
            ecranA.print(F("> Voir les données"));
            ecranA.setCursor(0, 25);
            ecranA.print(F("  Changer la langue"));
            ecranA.setCursor(0, 40);
            ecranA.print(F("  Retour"));

            //ecranA.drawStr(0, 10, "> Voir les données"); // On commence en haut à gauche
            //ecranA.drawStr(0, 25, "  Changer la langue"); // On ecrit après la ligne sauté
            //ecranA.drawStr(0, 40, "  Retour"); // On peut retourner à l'écran précédent
            break;
          case 1:
            ecranA.setCursor(0, 10);
            ecranA.print(F("  Voir les données"));
            ecranA.setCursor(0, 25);
            ecranA.print(F("> Changer la langue"));
            ecranA.setCursor(0, 40);
            ecranA.print(F("  Retour"));

            //ecranA.drawStr(0, 10, "  Voir les données"); // On commence en haut à gauche
            //ecranA.drawStr(0, 25, "> Changer la langue"); // On ecrit après la ligne sauté
            //ecranA.drawStr(0, 40, "  Retour"); // On peut retourner à l'écran précédent
            break;
          case 2:
            ecranA.setCursor(0, 10);
            ecranA.print(F("  Voir les données"));
            ecranA.setCursor(0, 25);
            ecranA.print(F("  Changer la langue"));
            ecranA.setCursor(0, 40);
            ecranA.print(F("> Retour"));

            //ecranA.drawStr(0, 10, "  Voir les données"); // On commence en haut à gauche
            //ecranA.drawStr(0, 25, "  Changer la langue"); // On ecrit après la ligne sauté
            //ecranA.drawStr(0, 40, "> Retour"); // On peut retourner à l'écran précédent
            break;
          
        } 
      } while(ecranA.nextPage());

      if (compteur != ancien_compteur) 
      {
        Choix_reglages = Choix_reglages + compteur - ancien_compteur;
        ancien_compteur=compteur;
        if (Choix_reglages>2) Choix_reglages=0;
        if (Choix_reglages<0) Choix_reglages=2;
      }

      if (Boutton_Appuye)
      {
        if (Choix_reglages == 0)
        {
          Boutton_Appuye = false;
          etat_app = Donnees;
        }
        if (Choix_reglages == 1)
        {
          Boutton_Appuye = false;
          etat_app = Langues;
        }
        if (Choix_reglages == 2)
        {
          Boutton_Appuye = false;
          etat_app = Demarrage;
        }
      }

      break ;
    
    case Donnees : 
      char ligne[50];
      ecranA.firstPage();
      do 
      {
        ecranA.setFont(u8g2_font_6x10_tr); // Police avec de grands chiffres
        if (position_liste<tables_patient.nombre_donnes)
        {
          sprintf(ligne, "> %d BPM", tables_patient.liste_donnes[position_liste].mesure);
          ecranA.drawStr(0, 10, ligne); // On commence en haut à gauche
        }
        else if (position_liste==tables_patient.nombre_donnes)
        {
          ecranA.setCursor(0,10);
          ecranA.print(F("> Retour"));          
        }
        else
        {
          ecranA.setCursor(0,10);
          ecranA.print(F("       "));     
        }

        if (position_liste+1<tables_patient.nombre_donnes)
        {
          sprintf(ligne, "  %d BPM", tables_patient.liste_donnes[position_liste+1].mesure);
          ecranA.drawStr(0, 25, ligne); // On commence en haut à gauche
        }
        else if (position_liste+1==tables_patient.nombre_donnes)
        {
          ecranA.setCursor(0,25);
          ecranA.print(F("  Retour"));          
        }
        else
        {
          ecranA.setCursor(0,25);
          ecranA.print(F("       "));     
        }

        if (position_liste+2<tables_patient.nombre_donnes)
        {
          sprintf(ligne, "  %d BPM", tables_patient.liste_donnes[position_liste+2].mesure);
          ecranA.drawStr(0, 40, ligne); // On commence en haut à gauche
        }
        else if (position_liste+2==tables_patient.nombre_donnes)
        {
          ecranA.setCursor(0,40);
          ecranA.print(F("  Retour"));          
        }
        else
        {
          ecranA.setCursor(0,40);
          ecranA.print(F("       "));     
        }
      } while(ecranA.nextPage());

      if (compteur != ancien_compteur) 
      {
        if (compteur>ancien_compteur)
        {
          position_liste++;
        }
        else
        {
          position_liste--;
        }
        ancien_compteur=compteur;
        
        if (position_liste>tables_patient.nombre_donnes) position_liste=tables_patient.nombre_donnes;
        if (position_liste<=0) position_liste=0;
      }

      if (Boutton_Appuye)
      {
        if (position_liste == tables_patient.nombre_donnes)
        {
          Boutton_Appuye = false;
          etat_app = Reglages;
        }
        if (Choix_donnes == 1)
        {
          Boutton_Appuye = false;
          //etat_app = Langues;
        }
      }
      break ;
    
    case Langues : 
      ecranA.firstPage();
      do 
      {
        ecranA.setFont(u8g2_font_6x10_tr); // Police avec de grands chiffres
        switch (Choix_langues)
        {
          //ecranA.drawStr(0, 10, "Selectionnez la langue : "); // On commence en haut à gauche
          ecranA.setCursor(0, 10);// On commence en haut à gauche
          ecranA.print(F("Selectionnez la langue : "));

          case 0:
            ecranA.setCursor(0, 10);// On commence en haut à gauche
            ecranA.print(F("> Francais"));
            ecranA.setCursor(0, 25);// On commence en haut à gauche
            ecranA.print(F("  Anglais"));
            ecranA.setCursor(0, 40);// On commence en haut à gauche
            ecranA.print(F("  Retour"));

            //ecranA.drawStr(0, 10, "> Francais"); // On commence en haut à gauche
            //ecranA.drawStr(0, 25, "  Anglais"); // On ecrit après la ligne sauté
            //ecranA.drawStr(0, 40, "  Retour"); // On peut retourner à l'écran précédent
            break;
          case 1:
            ecranA.setCursor(0, 10);// On commence en haut à gauche
            ecranA.print(F("  Francais"));
            ecranA.setCursor(0, 25);// On commence en haut à gauche
            ecranA.print(F("> Anglais"));
            ecranA.setCursor(0, 40);// On commence en haut à gauche
            ecranA.print(F("  Retour"));
            break;
          case 2:
            ecranA.setCursor(0, 10);// On commence en haut à gauche
            ecranA.print(F("  Francais"));
            ecranA.setCursor(0, 25);// On commence en haut à gauche
            ecranA.print(F("  Anglais"));
            ecranA.setCursor(0, 40);// On commence en haut à gauche
            ecranA.print(F("> Retour"));
            break;
          
        } 
      } while(ecranA.nextPage());

      if (compteur != ancien_compteur) 
      {
        Choix_langues = Choix_langues + compteur - ancien_compteur;
        ancien_compteur=compteur;
        if (Choix_langues>2) Choix_langues=0;
        if (Choix_langues<0) Choix_langues=2;
      }

      if (Boutton_Appuye)
      {
        if (Choix_langues == 0)
        {
          Boutton_Appuye = false;
          Langue = 'F';
          etat_app = Reglages;
        }
        if (Choix_langues == 1)
        {
          Boutton_Appuye = false;
          Langue = 'A';
          etat_app = Reglages;
        }
        if (Choix_langues == 2)
        {
          Boutton_Appuye = false;
          etat_app = Reglages;
        }
      }

      break ;
      
  }
}

void setup() 
{
  // Configuration des pins
  pinMode(pinArduinoRaccordementSignalSW, INPUT_PULLUP);
  pinMode(pinArduinoRaccordementSignalCLK, INPUT);
  pinMode(pinArduinoRaccordementSignalDT, INPUT);
  pinMode(pinBoutonChoixEcran, INPUT_PULLUP);

  // Initialisation de l'ecran
  Wire.begin();
  ecranA.setI2CAddress(adresse_ecran_A * 2); // U8g2 attend l'adresse sur 8 bits
  ecranB.setI2CAddress(adresse_ecran_B * 2); // U8g2 attend l'adresse sur 8 bits
  ecranA.setBusClock(400000);       // I2C rapide : affichage plus fluide
  ecranB.setBusClock(400000);
  ecranA.begin();
  ecranB.begin();
  Serial.begin(9600);

  etatPrecedentLigneSW = digitalRead(pinArduinoRaccordementSignalSW);
  etatPrecedentLigneCLK = digitalRead(pinArduinoRaccordementSignalCLK);
  etatPrecedentLigneDT = digitalRead(pinArduinoRaccordementSignalDT);

  attachInterrupt(digitalPinToInterrupt(pinArduinoRaccordementSignalCLK), changementSurLigneCLK, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pinArduinoRaccordementSignalSW), changementSurLigneSW, FALLING); // Que sur la descente pour limiter les rebonds

  liste_essai();
}

void loop() 
{
  gererEncodeurSelonEcran(); // Traite le delta de l'encodeur selon l'écran actif
  app();
}
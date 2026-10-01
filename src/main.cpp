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
int Choix_voir_donnes = 0;                      // Variable pour choisir selon la données choisie si on veut la supprimer ou retourner
char Langue = 'F';                              // Variable qui stocke la langue choisie (F -> Francais, A -> Anglais)
int position_liste = 0;                         // Variable qui sait où on en est dans l'affichage de la liste de données
bool suppression_reussie = false;               // Variable pour verifier la bonne suppression des donnees
bool bouton_choix_ecran = false;                        // Variable qui regarde si le bouton de choix de l'ecran a ete appuye
unsigned long dernierTempsBouton = 0;           // Variable pour eviter de compter plusieurs fois un appui sur le bouton
bool dernierEtatBouton = HIGH;                  // Variable pour bien vérifier que l'etat du bouton a change


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
donnes donne_a_afficher;                        // Variable pour afficher la données dont on veut les informations

// Enumeration des etats de la machine d'etat app
enum{Demarrage, Reglages, Donnees, Langues, voirDonnes, ValidationSuppression} etat_app= Demarrage;  // enumeration des etat de app


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

bool supprimer_donnee_courante() 
{
  // 1. Vérification de sécurité : s'assurer qu'il y a des données à supprimer
  // et que l'index position_liste est valide
  if (tables_patient.nombre_donnes <= 0 || position_liste >= tables_patient.nombre_donnes) 
  {
    return false;
  }

  // 2. Boucle pour décaler tous les éléments situés après 'position_liste' d'un cran vers la gauche
  for (int i = position_liste; i < tables_patient.nombre_donnes - 1; i++) 
  {
    tables_patient.liste_donnes[i] = tables_patient.liste_donnes[i + 1];
  }

  // 3. Réduction du nombre total de données stockées
  tables_patient.nombre_donnes--;

  // 4. Ajustement de l'index de sélection du menu
  // Si on vient de supprimer le dernier élément du tableau, on recule la position
  if (position_liste >= tables_patient.nombre_donnes && position_liste > 0) 
  {
    position_liste = tables_patient.nombre_donnes - 1;
  }

  return true;
}

void detection_appui_bouton() 
{
  bool etatActuel = digitalRead(pinBoutonChoixEcran);

  // Anti-rebond simple de 50 ms
  if (etatActuel == LOW && dernierEtatBouton == HIGH ) 
  {
    dernierTempsBouton = millis();
    bouton_choix_ecran = true;
    
    Serial.print("Bouton appuye ");
    dernierEtatBouton = etatActuel;
  }
  else if (etatActuel==HIGH)
  {
    dernierEtatBouton=HIGH;
  }
}


void choix_deecran() 
{
    if (ecran_selectionne == 'A')
    {
      ecran_selectionne = 'B';
    } 
    else 
    {
      ecran_selectionne = 'A';
    }

    Serial.print("Bouton appuye ! Ecran choisi : ");
    Serial.println(ecran_selectionne);
    //dernierEtatBouton = etatActuel;
}

void app ()
{

  switch (etat_app)
  {
    case Demarrage : 
      if (bouton_choix_ecran)
      {
        bouton_choix_ecran=false;
        choix_deecran();
      }

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
      if (bouton_choix_ecran)
      {
        bouton_choix_ecran=false;
        etat_app=Demarrage;
      }

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
          position_liste = 0;
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
      
      if (bouton_choix_ecran)
      {
        bouton_choix_ecran=false;
        etat_app=Demarrage;
      }

      ecranA.firstPage();
      do 
      {
        ecranA.setFont(u8g2_font_6x10_tr); // Police avec de grands chiffres
        if (position_liste<tables_patient.nombre_donnes)
        {
          sprintf(ligne, "> %d BPM - %d-%d-%d", 
            tables_patient.liste_donnes[position_liste].mesure, tables_patient.liste_donnes[position_liste].date_heure.year(), 
            tables_patient.liste_donnes[position_liste].date_heure.month(), tables_patient.liste_donnes[position_liste].date_heure.day()
          );
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
          sprintf(ligne, "  %d BPM - %d-%d-%d", 
            tables_patient.liste_donnes[position_liste+1].mesure, tables_patient.liste_donnes[position_liste+1].date_heure.year(), 
            tables_patient.liste_donnes[position_liste+1].date_heure.month(), tables_patient.liste_donnes[position_liste+1].date_heure.day()
          );
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
          sprintf(ligne, "  %d BPM - %d-%d-%d", 
            tables_patient.liste_donnes[position_liste+2].mesure, tables_patient.liste_donnes[position_liste+2].date_heure.year(), 
            tables_patient.liste_donnes[position_liste+2].date_heure.month(), tables_patient.liste_donnes[position_liste+2].date_heure.day()
          );
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
        if (position_liste != tables_patient.nombre_donnes)
        {
          Boutton_Appuye = false;
          donne_a_afficher = tables_patient.liste_donnes[position_liste];
          etat_app = voirDonnes;
        }
      }
      
      break ;
    
    case voirDonnes : 
      
      if (bouton_choix_ecran)
      {
        bouton_choix_ecran=false;
        etat_app=Demarrage;
      }

      ecranA.firstPage();
      do 
      {
        ecranA.setFont(u8g2_font_6x10_tr); // Police avec de grands chiffres
        ecranA.setCursor(0, 10);// On commence en haut à gauche
        ecranA.print(F("Données de la mesure : "));

        char chaineBPM[30];
        sprintf(chaineBPM, "MESURE : %d BPM", donne_a_afficher.mesure);
        ecranA.drawStr(0, 20, chaineBPM);

        char chaineDate[30];
        sprintf(chaineDate, "DATE : %d-%d-%d", donne_a_afficher.date_heure.year(), donne_a_afficher.date_heure.month(), donne_a_afficher.date_heure.day());
        ecranA.drawStr(0, 30, chaineDate); 

        char chaineHeure[30];
        sprintf(chaineHeure, "HEURE : %d:%d:%d", donne_a_afficher.date_heure.hour(), donne_a_afficher.date_heure.minute(), donne_a_afficher.date_heure.second());
        ecranA.drawStr(0, 40, chaineHeure); 

        switch (Choix_voir_donnes)
        {      

          case 0:
            ecranA.setCursor(0, 50);// On commence en haut à gauche
            ecranA.print(F("> Supprimer"));
            ecranA.setCursor(0, 60);// On commence en haut à gauche
            ecranA.print(F("  Retour"));
            break;
          case 1:
            ecranA.setCursor(0, 50);// On commence en haut à gauche
            ecranA.print(F("  Supprimer"));
            ecranA.setCursor(0, 60);// On commence en haut à gauche
            ecranA.print(F("> Retour"));
            break;          
        } 
      } while(ecranA.nextPage());

      if (compteur != ancien_compteur) 
      {
        Choix_voir_donnes = Choix_voir_donnes + compteur - ancien_compteur;
        ancien_compteur=compteur;
        if (Choix_voir_donnes>1) Choix_voir_donnes=0;
        if (Choix_voir_donnes<0) Choix_voir_donnes=1;
      }

      if (Boutton_Appuye)
      {
        if (Choix_voir_donnes == 0)
        {
          Boutton_Appuye = false;
          suppression_reussie = supprimer_donnee_courante();
          etat_app = ValidationSuppression;
        }
        if (Choix_voir_donnes == 1)
        {
          Boutton_Appuye = false;
          position_liste = 0;
          etat_app = Donnees;
        }
      }

      break;
    
    case ValidationSuppression : 

      if (bouton_choix_ecran)
      {
        bouton_choix_ecran=false;
        etat_app=Demarrage;
      }

      ecranA.firstPage();
      do 
      {
        ecranA.setFont(u8g2_font_6x10_tr); // Police avec de grands chiffres
        if (suppression_reussie) 
        { 
          ecranA.setCursor(0, 10);
          ecranA.print(F("La donnee a bien ete"));
          ecranA.setCursor(0, 20);
          ecranA.print(F("supprimee"));
        }
        else
        { 
          ecranA.setCursor(0, 10);
          ecranA.print(F("Erreur lors de la"));
          ecranA.setCursor(0, 20);
          ecranA.print(F("suppression"));
        }

        ecranA.setCursor(0, 40);
        ecranA.print(F("Cliquer sur l'encodeur"));
        ecranA.setCursor(0, 50);
        ecranA.print(F("pour retourner"));
      } while(ecranA.nextPage());

      if (Boutton_Appuye)
      {
        Boutton_Appuye = false;
        etat_app = Donnees;
      }
      
      break ;

    case Langues : 
      
      if (bouton_choix_ecran)
      {
        bouton_choix_ecran=false;
        etat_app=Demarrage;
      }
      
      ecranA.firstPage();
      do 
      {
        ecranA.setFont(u8g2_font_6x10_tr); // Police avec de grands chiffres
        switch (Choix_langues)
        {
          ecranA.setCursor(0, 10);// On commence en haut à gauche
          ecranA.print(F("Selectionnez la langue : "));

          case 0:
            ecranA.setCursor(0, 25);
            ecranA.print(F("> Francais"));
            ecranA.setCursor(0, 40);
            ecranA.print(F("  Anglais"));
            ecranA.setCursor(0, 55);
            ecranA.print(F("  Retour"));
            break;

          case 1:
            ecranA.setCursor(0, 25);// On commence en haut à gauche
            ecranA.print(F("  Francais"));
            ecranA.setCursor(0, 40);// On commence en haut à gauche
            ecranA.print(F("> Anglais"));
            ecranA.setCursor(0, 55);// On commence en haut à gauche
            ecranA.print(F("  Retour"));
            break;
          case 2:
            ecranA.setCursor(0, 25);// On commence en haut à gauche
            ecranA.print(F("  Francais"));
            ecranA.setCursor(0, 40);// On commence en haut à gauche
            ecranA.print(F("  Anglais"));
            ecranA.setCursor(0, 55);// On commence en haut à gauche
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

  Serial.begin(9600);
  Serial.println("Démarrage du programme...");

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

  liste_essai();
}

void loop() 
{
  gererEncodeurSelonEcran(); // Traite le delta de l'encodeur selon l'écran actif
  //choix_ecran();
  detection_appui_bouton();
  app();
}
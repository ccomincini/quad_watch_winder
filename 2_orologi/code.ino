//Bibliothek "stepper.h" laden
 #include <Stepper.h>
 #define STEPS 100
  
//Schrittmotor definieren
//Die Motoreingänge (IN1, IN2, IN3, IN4) werden mit den Anschlüssen 8, 9, 10 ,11 verbunden
  Stepper stepper1(STEPS, 8, 10, 9, 11);     // Drehung im Uhrzeigersinn
  Stepper stepper2(STEPS, 4, 6, 5, 7);       // Drehung gegen Uhrzeigersinn
  
  int  Steps2Take = 0;                            //Variable für Schrittzahl definieren
  long Zeit =0;                                   //Variable für Zeit definieren
  //************************************************************
  // Für den Motor-Typ : http://tiptopboards.com/151-moteur-pas-%C3%A0-pas-r%C3%A9duct%C3%A9-de-5v-4-fils-driver-.html
  // 64 Paare pro Umdrehungr, 4 Spulen, Drehwinkel von 5.625°
  // mechanische Übersetzung 1:64
  //   360° / 5.625° * 64 = 4096 Winkel mit Multiplikation 
  //   360° / 5.625° * 64  * 4 Spulen / 2 Bipole = 2048 Schritte / Umdrehung
  
int Steuerung;
void setup() 
 {                
  Serial.begin(9600);     // 9600 bps
  Serial.println("Motor OK"); 
  pinMode(2, OUTPUT); // Definiere Pin 5 als LED Pin
  }

void loop() 
  {  
    
  stepper1.setSpeed(150);      //Geschwindigkeit von 300 ist max /  Reduzieren der Zahl für langsamere Bewegung
  stepper2.setSpeed(150);      //100 ermöglicht ein hohes Drehmoment / bei >300 vibriert der Motor wenn er nicht dreht
 

if (Steuerung<=16)
{
// 1. Viertel Stunde  
// 1 komplette Umdrehung benötigt 2048 Schritte (1 Umdrehung dauert ca. 9s) ==> 12 Umdrehungen

  digitalWrite(2, HIGH); 
// Uhr 1
  Steps2Take  = 24576;  
  stepper1.step(Steps2Take);       //Anzahl der Schritte an Programm übergeben
 
// Uhr 2
  Steps2Take  = 24576;  
  stepper2.step(Steps2Take);       //Anzahl der Schritte an Programm übergeben
  digitalWrite(2, LOW);
  
// Pause
  delay(684000);                   //15 Minuten Pause

// 2. Viertel Stunde  
// 1 komplette Umdrehung benötigt 2048 Schritte (1 Umdrehung dauert ca. 4,5s) ==> 13 Umdrehungen

  digitalWrite(2, HIGH);
// Uhr 1
  Steps2Take  = 26624;  
  stepper1.step(Steps2Take);       //Anzahl der Schritte an Programm übergeben

// Uhr 2
  Steps2Take  = 26624;  
  stepper2.step(Steps2Take);       //Anzahl der Schritte an Programm übergeben
  digitalWrite(2, LOW);
  
// Pause
  delay(666000);                   //15 Minuten Pause

// 3. Viertel Stunde  
// 1 komplette Umdrehung benötigt 2048 Schritte (1 Umdrehung dauert ca. 4,5s) ==> 12 Umdrehungen

  digitalWrite(2, HIGH);
// Uhr 1
  Steps2Take  = 24576;  
  stepper1.step(Steps2Take);       //Anzahl der Schritte an Programm übergeben
 
// Uhr 2
  Steps2Take  = 24576;  
  stepper2.step(Steps2Take);       //Anzahl der Schritte an Programm übergeben
  digitalWrite(2, LOW);
  
// Pause
  delay(684000);                   //15 Minuten Pause

// 4. Viertel Stunde  
// 1 komplette Umdrehung benötigt 2048 Schritte (1 Umdrehung dauert ca. 4,5s) ==> 13 Umdrehungen

  digitalWrite(2, HIGH);
// Uhr 1
  Steps2Take  = 26624;  
  stepper1.step(Steps2Take);       //Anzahl der Schritte an Programm übergeben

// Uhr 2
  Steps2Take  = 26624;  
  stepper2.step(Steps2Take);       //Anzahl der Schritte an Programm übergeben
  digitalWrite(2, LOW);

// Pause
  delay(666000);                   //15 Minuten Pause
  
Steuerung++;                     //Variable "Steuerung" um 1 erhöhen  
 }
else{

//Pause 
  delay(28800000);                 //8 Stunden Pause   
Steuerung=0;                       //Variable "Steuerung" zurücksetzen
 }
 }

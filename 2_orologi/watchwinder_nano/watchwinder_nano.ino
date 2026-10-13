/*
 * Caricaorologi a 2 posti - versione per Arduino Nano
 *
 * Derivato da "Arduino Double Watch Winder" di JayS
 * https://www.printables.com/model/1227833-arduino-double-watch-winder
 * Licenza CC BY-NC-SA 4.0: https://creativecommons.org/licenses/by-nc-sa/4.0/
 *
 * Modifiche di Carlo Comincini, Invisiblefarm s.r.l., distribuite con la
 * stessa licenza:
 * - bobine dei motori spente dopo ogni rotazione (l'originale le lascia
 *   alimentate anche a motore fermo);
 * - commenti tradotti in italiano e corretti dove non corrispondevano al codice.
 * Tempi, giri e verso di rotazione sono identici all'originale.
 *
 * Arduino Nano e Arduino Uno usano lo stesso ATmega328P con gli stessi pin:
 * i collegamenti dello schema originale (wiring.pdf) restano validi.
 */

#include <Stepper.h>

// Valore usato dalla libreria solo per calcolare la velocita'. Il 28BYJ-48
// compie in realta' 2048 passi per giro dell'albero di uscita.
#define STEPS 100

// Pin collegati agli ingressi IN1..IN4 dei driver ULN2003. La libreria vuole
// i pin nell'ordine IN1, IN3, IN2, IN4.
const int M1_IN1 = 8, M1_IN2 = 9, M1_IN3 = 10, M1_IN4 = 11;
const int M2_IN1 = 4, M2_IN2 = 5, M2_IN3 = 6, M2_IN4 = 7;

// LED del pulsante: acceso mentre i motori girano
const int PIN_LED = 2;

Stepper stepper1(STEPS, M1_IN1, M1_IN3, M1_IN2, M1_IN4);  // orologio 1, senso orario
Stepper stepper2(STEPS, M2_IN1, M2_IN3, M2_IN2, M2_IN4);  // orologio 2, senso antiorario

// Passi per giro dell'albero di uscita: a velocita' 150 un giro dura circa 8 s
const int PASSI_GIRO = 2048;

// Ore di funzionamento contate dall'accensione; dopo 17 ore pausa di 8 ore
int ore = 0;

// Toglie corrente alle quattro bobine di un motore
void spegniMotore(int in1, int in2, int in3, int in4) {
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
}

// Un quarto d'ora: gira prima l'orologio 1 e poi l'orologio 2, quindi attende.
// La pausa e' calcolata dall'autore in modo che rotazione e attesa durino
// insieme circa 15 minuti.
void quartoDora(int giri, unsigned long pausaMs) {
  int passi = giri * PASSI_GIRO;

  digitalWrite(PIN_LED, HIGH);

  stepper1.step(passi);
  spegniMotore(M1_IN1, M1_IN2, M1_IN3, M1_IN4);

  stepper2.step(passi);
  spegniMotore(M2_IN1, M2_IN2, M2_IN3, M2_IN4);

  digitalWrite(PIN_LED, LOW);

  delay(pausaMs);
}

void setup() {
  Serial.begin(9600);
  Serial.println("Motor OK");

  pinMode(PIN_LED, OUTPUT);

  // Velocita' 150: la massima e' circa 300; valori bassi danno piu' coppia.
  // Oltre 300 il motore vibra senza girare.
  stepper1.setSpeed(150);
  stepper2.setSpeed(150);

  // La libreria imposta i pin come uscite: si parte con le bobine spente
  spegniMotore(M1_IN1, M1_IN2, M1_IN3, M1_IN4);
  spegniMotore(M2_IN1, M2_IN2, M2_IN3, M2_IN4);
}

void loop() {
  if (ore <= 16) {
    // Un'ora: quarti alterni da 12 e 13 giri, cioe' 50 giri l'ora per orologio
    quartoDora(12, 684000UL);   // 98 s di rotazione per orologio + 11,4 min di attesa
    quartoDora(13, 666000UL);   // 106 s di rotazione per orologio + 11,1 min di attesa
    quartoDora(12, 684000UL);
    quartoDora(13, 666000UL);
    ore++;
  } else {
    delay(28800000UL);          // 8 ore di pausa
    ore = 0;
  }
}

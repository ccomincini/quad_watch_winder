/*
 * ============================================================================
 *  WATCH WINDER 2 POSTI - Raspberry Pi Pico 2 W
 * ============================================================================
 *
 *  Autore: Carlo Comincini
 *
 *  Stesso firmware del caricatore a 4 posti (watchwinder/watchwinder.ino),
 *  con i valori di fabbrica adatti alla scatola a 2 posti: posti 3 e 4
 *  spenti, 1600 giri al giorno in entrambi i versi, 1900 us fra i passi,
 *  fascia 08:00-20:00.
 *  Il formato della configurazione in EEPROM e' identico, quindi una
 *  configurazione salvata con l'uno resta valida con l'altro.
 *
 *  Hardware:
 *    - Raspberry Pi Pico 2 W
 *    - 2 x motore passo-passo 28BYJ-48 (5 V) con driver ULN2003
 *    - Modulo RTC DS3231SN con EEPROM AT24C32 integrata
 *    - Alimentatore esterno 5 V 2,5 A, pulsante 16 mm come interruttore
 *
 *  Meccanica: "Arduino Double Watch Winder" di JayS (Printables 1227833),
 *  porta-orologio sull'albero del motore, rapporto 1:1.
 *  Schema: 2_orologi/schema-elettrico-2-posti.svg
 *
 *  Collegamenti RTC (bus I2C1):
 *    3V3(OUT) -> VCC     GND -> GND     GP26 -> SDA     GP27 -> SCL
 *
 *  Collegamenti motori (tutti in ordine crescente IN1..IN4):
 *    Motore 1: GP6  GP7  GP8  GP9
 *    Motore 2: GP10 GP11 GP12 GP13
 *    Motore 3: GP2  GP3  GP4  GP5    (non usato, spento di fabbrica)
 *    Motore 4: GP18 GP19 GP20 GP21   (non usato, spento di fabbrica)
 *
 *  ATTENZIONE ALIMENTAZIONE: i 5 V delle schede ULN2003 devono arrivare
 *  direttamente dall'alimentatore esterno, NON dai pin del Pico.
 *  La massa (GND) deve essere in comune tra Pico e schede ULN2003.
 *
 *  Scheda da selezionare in Arduino IDE:
 *    Raspberry Pi Pico 2 W  (core "Raspberry Pi Pico/RP2040" di Earle Philhower)
 * ============================================================================
 */

#include <Wire.h>

// ============================================================================
//  CONFIGURAZIONE - valori di default, modificabili anche da monitor seriale
// ============================================================================

// --- Fascia oraria di funzionamento -----------------------------------------
// Il winder ruota da ORA_INIZIO (inclusa) a ORA_FINE (esclusa).
#define DEF_ORA_INIZIO      8      // 08:00 -> inizio attivita'
#define DEF_ORA_FINE       20      // 20:00 -> stop notturno

// --- Rotazione ---------------------------------------------------------------
// TPD = Turns Per Day, giri completi al giorno richiesti dall'orologio.
// Il Breitling Chronomat 81950 (Valjoux 7750) carica solo in un verso e
// chiede circa 800 TPD; il Seiko 5 carica in entrambi i versi. Nel winder
// giroscopico di JayS il verso rispetto al quadrante non e' riconoscibile,
// quindi si gira in entrambi i versi con i giri raddoppiati: almeno meta'
// (circa 800) cadono nel verso utile al Valjoux 7750.
#define DEF_TPD          1600

// Durata di un ciclo completo: ogni CICLO_MINUTI il winder esegue la sua
// quota di giri, poi resta fermo fino al ciclo successivo.
#define DEF_CICLO_MINUTI   15

// Intervallo fra due mezzi passi, in microsecondi.
// 1800 us -> circa 7,4 s per giro (8 giri/minuto). Piu' alto = piu' lento e
// silenzioso; sotto i 1200 us il 28BYJ-48 comincia a perdere passi.
// Qui 1900 us: un po' piu' lento e silenzioso, come nel caricatore a 4 posti.
#define DEF_PASSO_US     1900

// Pausa fra la fase oraria e quella antioraria, in millisecondi.
#define DEF_PAUSA_META_MS 1500

// Distanza fra la partenza di un motore e quella del successivo, in secondi.
// Con un valore piccolo (5 s) i quattro motori lavorano praticamente in
// parallelo: lo sfasamento serve solo a non far coincidere gli spunti, che
// sono il transitorio di corrente peggiore. Il ciclo diventa cosi' molto piu'
// silenzioso, perche' il rumore si concentra in una finestra breve.
// Con 0 si torna alla ripartizione automatica in quarti di ciclo, cioe' un
// motore per volta.
#define DEF_SFALSAMENTO_S   5

// --- Caratteristiche meccaniche ----------------------------------------------
// Il 28BYJ-48 compie 4096 mezzi passi per ogni giro dell'albero di uscita.
#define PASSI_PER_GIRO_MOTORE 4096L
// Giri di motore necessari per un giro del porta-orologio (1.0 = presa diretta).
#define DEF_RAPPORTO       1.0f

// --- Verso di rotazione per ogni posto ---------------------------------------
//   0 = bidirezionale (meta' giri orari, meta' antiorari)  <-- in uso
//   1 = solo orario
//   2 = solo antiorario
#define DEF_MODO_VERSO      0

// Se un motore risulta girare al contrario rispetto agli altri, mettere true
// nella sua posizione senza rifare i cablaggi.
#define DEF_INVERTI_1   false
#define DEF_INVERTI_2   false
#define DEF_INVERTI_3   false
#define DEF_INVERTI_4   false

// Posti attivi: nella scatola a 2 posti ci sono solo i motori 1 e 2.
#define DEF_ATTIVO_1     true
#define DEF_ATTIVO_2     true
#define DEF_ATTIVO_3     false
#define DEF_ATTIVO_4     false

// ============================================================================
//  PIN
// ============================================================================

#define PIN_SDA   26
#define PIN_SCL   27

const uint8_t PIN_MOTORE[4][4] = {
  {  6,  7,  8,  9 },   // Motore 1 - ULN2003 #1
  { 10, 11, 12, 13 },   // Motore 2 - ULN2003 #2
  {  2,  3,  4,  5 },   // Motore 3 - ULN2003 #3
  { 18, 19, 20, 21 }    // Motore 4 - ULN2003 #4
};

// ============================================================================
//  INDIRIZZI I2C
// ============================================================================

#define ADDR_DS3231   0x68
#define ADDR_EEPROM   0x57   // AT24C32 sul modulo RTC

// ============================================================================
//  SEQUENZA DI ECCITAZIONE (mezzo passo, 8 combinazioni)
// ============================================================================

const uint8_t SEQUENZA[8][4] = {
  { 1, 0, 0, 0 },
  { 1, 1, 0, 0 },
  { 0, 1, 0, 0 },
  { 0, 1, 1, 0 },
  { 0, 0, 1, 0 },
  { 0, 0, 1, 1 },
  { 0, 0, 0, 1 },
  { 1, 0, 0, 1 }
};

// ============================================================================
//  CONFIGURAZIONE PERSISTENTE (salvata nella EEPROM AT24C32)
// ============================================================================

#define CONFIG_MAGIC   0x57573202UL   // "WW2" + versione
#define CONFIG_ADDR    0x0000

struct Config {
  uint32_t magic;
  uint8_t  oraInizio;
  uint8_t  oraFine;
  uint16_t tpd;
  uint8_t  cicloMinuti;
  uint16_t passoUs;
  uint16_t pausaMetaMs;
  uint16_t sfalsamentoS;
  float    rapporto;
  uint8_t  modoVerso;
  uint8_t  attivo[4];
  uint8_t  inverti[4];
  uint8_t  checksum;
};

Config cfg;

// ============================================================================
//  STATO DEI MOTORI
// ============================================================================

enum StatoWinder {
  WS_FERMO,       // bobine diseccitate
  WS_ORARIO,      // rotazione oraria in corso
  WS_PAUSA_META,  // breve sosta fra i due versi
  WS_ANTIORARIO   // rotazione antioraria in corso
};

struct Winder {
  StatoWinder    stato;
  uint8_t        indicePasso;      // posizione nella SEQUENZA (0..7)
  long           passiRimasti;     // passi ancora da eseguire nella fase
  long           passiFaseCw;      // passi previsti per la fase oraria
  long           passiFaseCcw;     // passi previsti per la fase antioraria
  unsigned long  ultimoPassoUs;    // micros() dell'ultimo passo eseguito
  unsigned long  finePausaMs;      // millis() di fine pausa intermedia
  unsigned long  partenzaMs;       // millis() a cui deve partire nel ciclo
  bool           partenzaArmata;   // true finche' non e' partito in questo ciclo
  unsigned long  passiEseguiti;    // contatore diagnostico dei passi totali
  bool           test;             // true se sta girando per un comando di test
};

Winder winder[4];

// ============================================================================
//  VARIABILI GLOBALI
// ============================================================================

struct DataOra {
  uint16_t anno;
  uint8_t  mese, giorno, ora, minuto, secondo;
};

unsigned long cicloInizioMs = 0;     // millis() di inizio del ciclo corrente
unsigned long cicloDurataMs = 0;     // durata del ciclo in ms
long          passiPerCiclo  = 0;    // passi totali per motore in un ciclo

bool rtcPresente   = false;
bool eepromPresente = false;
bool forzaAttivo   = false;          // comando 'f': ignora la fascia oraria
bool sospeso       = false;          // comando 'x': tutto fermo

uint8_t ultimaOraLog = 255;
unsigned long ultimoLogMs = 0;

// Cache della lettura RTC: interrogare il DS3231 a ogni giro di loop()
// occuperebbe il bus I2C per circa 1 ms, disturbando il timing dei passi.
DataOra       oraCache;
bool          oraCacheValida = false;
unsigned long ultimaLetturaRtcMs = 0;

// Il LED integrato del Pico W/2W e' pilotato dal chip CYW43 via SPI:
// va aggiornato solo quando cambia stato, mai a ogni giro di loop().
int8_t statoLed = -1;

String bufferSeriale = "";

// ============================================================================
//  DS3231 - accesso diretto ai registri (nessuna libreria esterna)
// ============================================================================

static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

bool rtcLeggi(DataOra &t) {
  Wire1.beginTransmission(ADDR_DS3231);
  Wire1.write(0x00);
  if (Wire1.endTransmission() != 0) return false;
  if (Wire1.requestFrom(ADDR_DS3231, 7) != 7) return false;

  t.secondo = bcd2dec(Wire1.read() & 0x7F);
  t.minuto  = bcd2dec(Wire1.read() & 0x7F);
  uint8_t regOra = Wire1.read();
  if (regOra & 0x40) {                       // formato 12 ore
    uint8_t h = bcd2dec(regOra & 0x1F);
    if (regOra & 0x20) { if (h != 12) h += 12; }
    else               { if (h == 12) h = 0;  }
    t.ora = h;
  } else {
    t.ora = bcd2dec(regOra & 0x3F);
  }
  Wire1.read();                              // giorno della settimana, non usato
  t.giorno = bcd2dec(Wire1.read() & 0x3F);
  uint8_t regMese = Wire1.read();
  t.mese = bcd2dec(regMese & 0x1F);
  t.anno = 2000 + bcd2dec(Wire1.read());
  if (regMese & 0x80) t.anno += 100;
  return true;
}

bool rtcScrivi(const DataOra &t) {
  Wire1.beginTransmission(ADDR_DS3231);
  Wire1.write(0x00);
  Wire1.write(dec2bcd(t.secondo));
  Wire1.write(dec2bcd(t.minuto));
  Wire1.write(dec2bcd(t.ora));               // bit 6 a 0 -> formato 24 ore
  Wire1.write(1);                            // giorno settimana fittizio
  Wire1.write(dec2bcd(t.giorno));
  Wire1.write(dec2bcd(t.mese));              // secolo a 0
  Wire1.write(dec2bcd((uint8_t)(t.anno % 100)));
  if (Wire1.endTransmission() != 0) return false;

  // Azzera il flag "oscillator stopped" nel registro di stato 0x0F
  Wire1.beginTransmission(ADDR_DS3231);
  Wire1.write(0x0F);
  Wire1.write(0x00);
  Wire1.endTransmission();
  return true;
}

bool rtcOrologioFermo() {
  Wire1.beginTransmission(ADDR_DS3231);
  Wire1.write(0x0F);
  if (Wire1.endTransmission() != 0) return true;
  if (Wire1.requestFrom(ADDR_DS3231, 1) != 1) return true;
  return (Wire1.read() & 0x80) != 0;         // bit OSF
}

float rtcTemperatura() {
  Wire1.beginTransmission(ADDR_DS3231);
  Wire1.write(0x11);
  if (Wire1.endTransmission() != 0) return -127.0f;
  if (Wire1.requestFrom(ADDR_DS3231, 2) != 2) return -127.0f;
  int8_t  parteIntera = (int8_t)Wire1.read();
  uint8_t parteDec    = Wire1.read() >> 6;
  return parteIntera + parteDec * 0.25f;
}

// ============================================================================
//  EEPROM AT24C32
// ============================================================================

bool eepromScriviBlocco(uint16_t indirizzo, const uint8_t *dati, uint8_t n) {
  Wire1.beginTransmission(ADDR_EEPROM);
  Wire1.write((uint8_t)(indirizzo >> 8));
  Wire1.write((uint8_t)(indirizzo & 0xFF));
  for (uint8_t i = 0; i < n; i++) Wire1.write(dati[i]);
  if (Wire1.endTransmission() != 0) return false;
  delay(6);                                  // tempo di scrittura interno
  return true;
}

bool eepromLeggiBlocco(uint16_t indirizzo, uint8_t *dati, uint8_t n) {
  Wire1.beginTransmission(ADDR_EEPROM);
  Wire1.write((uint8_t)(indirizzo >> 8));
  Wire1.write((uint8_t)(indirizzo & 0xFF));
  if (Wire1.endTransmission() != 0) return false;
  if (Wire1.requestFrom(ADDR_EEPROM, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) dati[i] = Wire1.read();
  return true;
}

uint8_t calcolaChecksum(const Config &c) {
  const uint8_t *p = (const uint8_t *)&c;
  uint8_t somma = 0;
  // Solo i byte che precedono il campo checksum: restano esclusi il checksum
  // stesso e il riempimento finale della struttura.
  for (size_t i = 0; i < offsetof(Config, checksum); i++) somma ^= p[i];
  return somma;
}

void configDefault() {
  cfg.magic       = CONFIG_MAGIC;
  cfg.oraInizio   = DEF_ORA_INIZIO;
  cfg.oraFine     = DEF_ORA_FINE;
  cfg.tpd         = DEF_TPD;
  cfg.cicloMinuti = DEF_CICLO_MINUTI;
  cfg.passoUs     = DEF_PASSO_US;
  cfg.pausaMetaMs = DEF_PAUSA_META_MS;
  cfg.sfalsamentoS = DEF_SFALSAMENTO_S;
  cfg.rapporto    = DEF_RAPPORTO;
  cfg.modoVerso   = DEF_MODO_VERSO;
  cfg.attivo[0]   = DEF_ATTIVO_1;  cfg.attivo[1]  = DEF_ATTIVO_2;
  cfg.attivo[2]   = DEF_ATTIVO_3;  cfg.attivo[3]  = DEF_ATTIVO_4;
  cfg.inverti[0]  = DEF_INVERTI_1; cfg.inverti[1] = DEF_INVERTI_2;
  cfg.inverti[2]  = DEF_INVERTI_3; cfg.inverti[3] = DEF_INVERTI_4;
  cfg.checksum    = calcolaChecksum(cfg);
}

bool configCarica() {
  Config tmp;
  uint8_t *p = (uint8_t *)&tmp;
  uint16_t n = sizeof(Config);
  for (uint16_t off = 0; off < n; off += 24) {
    uint8_t blocco = (n - off > 24) ? 24 : (uint8_t)(n - off);
    if (!eepromLeggiBlocco(CONFIG_ADDR + off, p + off, blocco)) return false;
  }
  if (tmp.magic != CONFIG_MAGIC) return false;
  if (tmp.checksum != calcolaChecksum(tmp)) return false;
  cfg = tmp;
  return true;
}

bool configSalva() {
  cfg.checksum = calcolaChecksum(cfg);
  const uint8_t *p = (const uint8_t *)&cfg;
  uint16_t n = sizeof(Config);
  for (uint16_t off = 0; off < n; off += 16) {   // AT24C32: pagine da 32 byte
    uint8_t blocco = (n - off > 16) ? 16 : (uint8_t)(n - off);
    if (!eepromScriviBlocco(CONFIG_ADDR + off, p + off, blocco)) return false;
  }
  return true;
}

// ============================================================================
//  CONTROLLO MOTORI
// ============================================================================

void bobineSpente(uint8_t m) {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(PIN_MOTORE[m][i], LOW);
}

void applicaPasso(uint8_t m, uint8_t indice) {
  for (uint8_t i = 0; i < 4; i++)
    digitalWrite(PIN_MOTORE[m][i], SEQUENZA[indice][i] ? HIGH : LOW);
}

// Avanza di un mezzo passo. orario = true -> senso orario (visto da sopra,
// salvo inversioni di cablaggio corrette da cfg.inverti).
void eseguiPasso(uint8_t m, bool orario) {
  bool avanti = cfg.inverti[m] ? !orario : orario;
  if (avanti) winder[m].indicePasso = (winder[m].indicePasso + 1) & 0x07;
  else        winder[m].indicePasso = (winder[m].indicePasso + 7) & 0x07;
  applicaPasso(m, winder[m].indicePasso);
}

long passiPerGiroOrologio() {
  return (long)(PASSI_PER_GIRO_MOTORE * cfg.rapporto);
}

// Calcola quanti passi deve fare ogni motore in un ciclo e come dividerli
// fra le due direzioni.
void ricalcolaPiano() {
  cicloDurataMs = (unsigned long)cfg.cicloMinuti * 60UL * 1000UL;

  int oreAttive = (int)cfg.oraFine - (int)cfg.oraInizio;
  if (oreAttive <= 0) oreAttive += 24;

  float cicliAlGiorno = (oreAttive * 60.0f) / (float)cfg.cicloMinuti;
  float giriPerCiclo  = (float)cfg.tpd / cicliAlGiorno;

  passiPerCiclo = (long)(giriPerCiclo * passiPerGiroOrologio() + 0.5f);

  for (uint8_t m = 0; m < 4; m++) {
    if (cfg.modoVerso == 1) {                 // solo orario
      winder[m].passiFaseCw  = passiPerCiclo;
      winder[m].passiFaseCcw = 0;
    } else if (cfg.modoVerso == 2) {          // solo antiorario
      winder[m].passiFaseCw  = 0;
      winder[m].passiFaseCcw = passiPerCiclo;
    } else {                                  // bidirezionale
      winder[m].passiFaseCw  = passiPerCiclo / 2;
      winder[m].passiFaseCcw = passiPerCiclo - winder[m].passiFaseCw;
    }
    // Partenze sfalsate. Con sfalsamentoS a 0 si mantiene la ripartizione
    // storica in quarti di ciclo (un motore per volta); con un valore in
    // secondi le partenze sono ravvicinate e i motori lavorano insieme,
    // distanziati quel tanto che basta a separare gli spunti.
    if (cfg.sfalsamentoS > 0)
      winder[m].partenzaMs = (unsigned long)m * (unsigned long)cfg.sfalsamentoS * 1000UL;
    else
      winder[m].partenzaMs = (unsigned long)m * (cicloDurataMs / 4UL);
  }
}

// Durata stimata di una sessione completa di un motore, in millisecondi.
unsigned long durataSessioneMs() {
  unsigned long t = (unsigned long)((float)passiPerCiclo * cfg.passoUs / 1000.0f);
  if (cfg.modoVerso == 0) t += cfg.pausaMetaMs;
  return t;
}

void avviaSessione(uint8_t m, bool comeTest) {
  winder[m].test = comeTest;
  if (winder[m].passiFaseCw > 0) {
    winder[m].stato        = WS_ORARIO;
    winder[m].passiRimasti = winder[m].passiFaseCw;
  } else if (winder[m].passiFaseCcw > 0) {
    winder[m].stato        = WS_ANTIORARIO;
    winder[m].passiRimasti = winder[m].passiFaseCcw;
  } else {
    winder[m].stato = WS_FERMO;
    return;
  }
  winder[m].ultimoPassoUs = micros();
}

void fermaTutti() {
  for (uint8_t m = 0; m < 4; m++) {
    winder[m].stato = WS_FERMO;
    winder[m].test  = false;
    bobineSpente(m);
  }
}

// Aggiornamento non bloccante di un singolo motore.
void aggiornaWinder(uint8_t m) {
  Winder &w = winder[m];

  switch (w.stato) {

    case WS_FERMO:
      return;

    case WS_PAUSA_META:
      if ((long)(millis() - w.finePausaMs) >= 0) {
        w.stato        = WS_ANTIORARIO;
        w.passiRimasti = w.passiFaseCcw;
        w.ultimoPassoUs = micros();
      }
      return;

    case WS_ORARIO:
    case WS_ANTIORARIO: {
      unsigned long ora = micros();
      if ((unsigned long)(ora - w.ultimoPassoUs) < cfg.passoUs) return;
      w.ultimoPassoUs += cfg.passoUs;
      // se siamo molto in ritardo (es. dopo una stampa seriale lunga)
      // riallinea il riferimento per non accumulare una raffica di passi
      if ((unsigned long)(ora - w.ultimoPassoUs) > (unsigned long)cfg.passoUs * 4UL)
        w.ultimoPassoUs = ora;

      eseguiPasso(m, w.stato == WS_ORARIO);
      w.passiRimasti--;
      w.passiEseguiti++;

      if (w.passiRimasti <= 0) {
        if (w.stato == WS_ORARIO && w.passiFaseCcw > 0) {
          bobineSpente(m);
          w.stato      = WS_PAUSA_META;
          w.finePausaMs = millis() + cfg.pausaMetaMs;
        } else {
          bobineSpente(m);
          w.stato = WS_FERMO;
          if (w.test) {
            w.test = false;
            Serial.print(F("[TEST] Motore "));
            Serial.print(m + 1);
            Serial.println(F(" - sessione conclusa."));
            ricalcolaPiano();   // ripristina il piano normale dopo il test
          }
        }
      }
      return;
    }
  }
}

// ============================================================================
//  LOGICA ORARIA
// ============================================================================

bool fasciaAttiva(const DataOra &t) {
  if (cfg.oraInizio == cfg.oraFine) return true;      // sempre attivo
  if (cfg.oraInizio < cfg.oraFine)
    return (t.ora >= cfg.oraInizio && t.ora < cfg.oraFine);
  // fascia che scavalca la mezzanotte
  return (t.ora >= cfg.oraInizio || t.ora < cfg.oraFine);
}

// ============================================================================
//  STAMPE
// ============================================================================

void stampaDueCifre(uint8_t v) {
  if (v < 10) Serial.print('0');
  Serial.print(v);
}

void stampaOra(const DataOra &t) {
  stampaDueCifre(t.giorno); Serial.print('/');
  stampaDueCifre(t.mese);   Serial.print('/');
  Serial.print(t.anno);     Serial.print(' ');
  stampaDueCifre(t.ora);    Serial.print(':');
  stampaDueCifre(t.minuto); Serial.print(':');
  stampaDueCifre(t.secondo);
}

const char *nomeStato(StatoWinder s) {
  switch (s) {
    case WS_FERMO:      return "fermo";
    case WS_ORARIO:     return "orario";
    case WS_PAUSA_META: return "pausa";
    case WS_ANTIORARIO: return "antiorario";
  }
  return "?";
}

void stampaAiuto() {
  Serial.println();
  Serial.println(F("=========== COMANDI DISPONIBILI ==========="));
  Serial.println(F("  ?              questo elenco"));
  Serial.println(F("  s              stato completo del sistema"));
  Serial.println(F("  t              legge data/ora e temperatura dall'RTC"));
  Serial.println(F("  T aaaa-mm-gg hh:mm:ss   imposta l'orologio RTC"));
  Serial.println(F("  Tc             imposta l'RTC all'ora di compilazione"));
  Serial.println();
  Serial.println(F("  m1..m4         test: un giro orario + un giro antiorario"));
  Serial.println(F("  d1..d4         test verso: 3 giri solo orari"));
  Serial.println(F("  x              ferma tutto (sospensione)"));
  Serial.println(F("  a              torna al funzionamento automatico"));
  Serial.println(F("  f              forza attivita' ignorando la fascia oraria"));
  Serial.println();
  Serial.println(F("  CONFIGURAZIONE (seguita da 'w' per salvare in EEPROM)"));
  Serial.println(F("  h <ini> <fin>  fascia oraria, es. 'h 9 20'"));
  Serial.println(F("  p <tpd>        giri al giorno, es. 'p 650'"));
  Serial.println(F("  c <minuti>     durata del ciclo, es. 'c 15'"));
  Serial.println(F("  o <secondi>    sfalsamento fra le partenze, es. 'o 5' (0=uno per volta)"));
  Serial.println(F("  v <us>         microsecondi fra i passi, es. 'v 1800'"));
  Serial.println(F("  r <verso>      0=bidirezionale 1=orario 2=antiorario"));
  Serial.println(F("  g <rapporto>   giri motore per giro orologio, es. 'g 1.0'"));
  Serial.println(F("  e<n> <0|1>     abilita/disabilita il posto n, es. 'e3 0'"));
  Serial.println(F("  i<n> <0|1>     inverte il verso del motore n, es. 'i1 1'"));
  Serial.println(F("  w              salva la configurazione in EEPROM"));
  Serial.println(F("  z              ripristina i valori di fabbrica"));
  Serial.println(F("==========================================="));
  Serial.println();
}

void stampaStato() {
  DataOra t;
  bool ok = rtcPresente && rtcLeggi(t);

  Serial.println();
  Serial.println(F("------------- STATO WATCH WINDER -------------"));
  Serial.print(F("Ora RTC ......... "));
  if (ok) { stampaOra(t); Serial.print(F("   ")); Serial.print(rtcTemperatura(), 2); Serial.println(F(" C")); }
  else      Serial.println(F("NON DISPONIBILE"));

  Serial.print(F("Fascia oraria ... "));
  stampaDueCifre(cfg.oraInizio); Serial.print(F(":00 - "));
  stampaDueCifre(cfg.oraFine);   Serial.println(F(":00"));

  Serial.print(F("Modalita' ....... "));
  if (sospeso)          Serial.println(F("SOSPESA (comando x)"));
  else if (forzaAttivo) Serial.println(F("FORZATA ATTIVA (comando f)"));
  else if (ok)          Serial.println(fasciaAttiva(t) ? F("ATTIVA (giorno)") : F("RIPOSO (notte)"));
  else                  Serial.println(F("ATTIVA (RTC assente, ripiego)"));

  Serial.print(F("TPD richiesti ... ")); Serial.println(cfg.tpd);
  Serial.print(F("Ciclo ........... ")); Serial.print(cfg.cicloMinuti); Serial.println(F(" minuti"));

  float giriCiclo = (float)passiPerCiclo / (float)passiPerGiroOrologio();
  Serial.print(F("Giri per ciclo .. ")); Serial.println(giriCiclo, 2);
  Serial.print(F("Passi per ciclo . ")); Serial.println(passiPerCiclo);
  Serial.print(F("Velocita' ....... ")); Serial.print(cfg.passoUs);
  Serial.print(F(" us/passo  ("));
  Serial.print(60000000.0f / ((float)cfg.passoUs * PASSI_PER_GIRO_MOTORE), 2);
  Serial.println(F(" giri/min)"));

  unsigned long durata = durataSessioneMs();
  Serial.print(F("Durata sessione . ")); Serial.print(durata / 1000.0f, 1); Serial.println(F(" s per motore"));
  if (cfg.sfalsamentoS > 0) {
    unsigned long passo = (unsigned long)cfg.sfalsamentoS * 1000UL;
    Serial.print(F("Sfalsamento ..... ")); Serial.print(cfg.sfalsamentoS);
    Serial.println(F(" s fra una partenza e l'altra"));
    unsigned long fineUltimo = passo * 3UL + durata;
    Serial.print(F("Fine ultimo ..... ")); Serial.print(fineUltimo / 1000.0f, 1);
    Serial.print(F(" s su ")); Serial.print(cicloDurataMs / 1000.0f, 1);
    Serial.println(F(" s di ciclo"));
    if (fineUltimo > cicloDurataMs)
      Serial.println(F("  ATTENZIONE: l'ultimo motore non chiude la sessione dentro il ciclo."));
    if (durata > passo)
      Serial.println(F("  Nota: i motori lavorano insieme, picco fino a circa 1,2 A."));
  } else {
    unsigned long slot = cicloDurataMs / 4UL;
    Serial.print(F("Slot per motore . ")); Serial.print(slot / 1000.0f, 1); Serial.println(F(" s"));
    if (durata > slot)
      Serial.println(F("  ATTENZIONE: le sessioni si sovrappongono, il picco di corrente aumenta."));
  }

  Serial.print(F("Verso ........... "));
  if (cfg.modoVerso == 0)      Serial.println(F("bidirezionale"));
  else if (cfg.modoVerso == 1) Serial.println(F("solo orario"));
  else                         Serial.println(F("solo antiorario"));

  Serial.print(F("Rapporto ........ ")); Serial.println(cfg.rapporto, 3);
  Serial.print(F("EEPROM .......... ")); Serial.println(eepromPresente ? F("presente") : F("assente"));

  Serial.println(F("---------------------------------------------"));
  for (uint8_t m = 0; m < 4; m++) {
    Serial.print(F("  Posto ")); Serial.print(m + 1);
    Serial.print(cfg.attivo[m] ? F("  [attivo]  ") : F("  [spento]  "));
    Serial.print(F("stato: ")); Serial.print(nomeStato(winder[m].stato));
    if (winder[m].stato == WS_ORARIO || winder[m].stato == WS_ANTIORARIO) {
      Serial.print(F("  mancano ")); Serial.print(winder[m].passiRimasti); Serial.print(F(" passi"));
    }
    Serial.print(F("   giri totali: "));
    Serial.print((float)winder[m].passiEseguiti / (float)passiPerGiroOrologio(), 1);
    if (cfg.inverti[m]) Serial.print(F("   (verso invertito)"));
    Serial.println();
  }
  Serial.println(F("---------------------------------------------"));
  Serial.println();
}

// ============================================================================
//  COMANDI DA MONITOR SERIALE
// ============================================================================

void avviaTest(uint8_t m, uint8_t giri, bool soloOrario) {
  if (m > 3) return;
  fermaTutti();
  long passi = giri * passiPerGiroOrologio();
  if (soloOrario) { winder[m].passiFaseCw = passi; winder[m].passiFaseCcw = 0; }
  else            { winder[m].passiFaseCw = passi; winder[m].passiFaseCcw = passi; }
  Serial.print(F("[TEST] Motore ")); Serial.print(m + 1);
  Serial.print(F(": ")); Serial.print(giri);
  Serial.println(soloOrario ? F(" giri in senso orario.") : F(" giro orario + 1 giro antiorario."));
  avviaSessione(m, true);
}

void eseguiComando(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  char c = cmd.charAt(0);
  String resto = cmd.substring(1);
  resto.trim();

  switch (c) {

    case '?': case 'H':
      stampaAiuto();
      break;

    case 's':
      stampaStato();
      break;

    case 't': {
      DataOra t;
      if (rtcPresente && rtcLeggi(t)) {
        Serial.print(F("RTC: ")); stampaOra(t);
        Serial.print(F("   temperatura ")); Serial.print(rtcTemperatura(), 2);
        Serial.println(F(" C"));
        if (rtcOrologioFermo())
          Serial.println(F("  ATTENZIONE: l'oscillatore si e' fermato, l'ora non e' attendibile. Reimpostarla."));
      } else {
        Serial.println(F("RTC non raggiungibile sul bus I2C1 (GP26/GP27)."));
      }
      break;
    }

    case 'T': {
      if (resto.length() > 0 && resto.charAt(0) == 'c') {
        DataOra t;
        // Ricava data e ora dai macro del compilatore
        const char *md = __DATE__;   // "Sep  2 2026"
        const char *mt = __TIME__;   // "14:35:07"
        const char *mesi = "JanFebMarAprMayJunJulAugSepOctNovDec";
        char nomeMese[4] = { md[0], md[1], md[2], 0 };
        const char *pos = strstr(mesi, nomeMese);
        t.mese   = pos ? ((pos - mesi) / 3 + 1) : 1;
        t.giorno = atoi(md + 4);
        t.anno   = atoi(md + 7);
        t.ora    = atoi(mt);
        t.minuto = atoi(mt + 3);
        t.secondo= atoi(mt + 6);
        if (rtcScrivi(t)) { Serial.print(F("RTC impostato a: ")); stampaOra(t); Serial.println(); }
        else                Serial.println(F("Scrittura RTC fallita."));
        break;
      }
      // formato atteso: T aaaa-mm-gg hh:mm:ss
      DataOra t;
      int n = sscanf(resto.c_str(), "%hu-%hhu-%hhu %hhu:%hhu:%hhu",
                     &t.anno, &t.mese, &t.giorno, &t.ora, &t.minuto, &t.secondo);
      if (n != 6) {
        Serial.println(F("Formato non valido. Esempio: T 2026-09-02 14:35:00"));
        break;
      }
      if (t.mese < 1 || t.mese > 12 || t.giorno < 1 || t.giorno > 31 ||
          t.ora > 23 || t.minuto > 59 || t.secondo > 59) {
        Serial.println(F("Valori di data/ora fuori intervallo."));
        break;
      }
      if (rtcScrivi(t)) { Serial.print(F("RTC impostato a: ")); stampaOra(t); Serial.println(); }
      else                Serial.println(F("Scrittura RTC fallita."));
      break;
    }

    case 'm': {
      int n = resto.toInt();
      if (n >= 1 && n <= 4) avviaTest(n - 1, 1, false);
      else Serial.println(F("Uso: m1 | m2 | m3 | m4"));
      break;
    }

    case 'd': {
      int n = resto.toInt();
      if (n >= 1 && n <= 4) avviaTest(n - 1, 3, true);
      else Serial.println(F("Uso: d1 | d2 | d3 | d4"));
      break;
    }

    case 'x':
      sospeso = true; forzaAttivo = false;
      fermaTutti();
      Serial.println(F("Sistema sospeso. Usare 'a' per riprendere."));
      break;

    case 'a':
      sospeso = false; forzaAttivo = false;
      ricalcolaPiano();
      cicloInizioMs = millis();
      for (uint8_t m = 0; m < 4; m++) winder[m].partenzaArmata = true;
      Serial.println(F("Funzionamento automatico ripristinato."));
      break;

    case 'f':
      sospeso = false; forzaAttivo = true;
      ricalcolaPiano();
      cicloInizioMs = millis();
      for (uint8_t m = 0; m < 4; m++) winder[m].partenzaArmata = true;
      Serial.println(F("Attivita' forzata: la fascia oraria viene ignorata."));
      break;

    case 'h': {
      int ini, fin;
      if (sscanf(resto.c_str(), "%d %d", &ini, &fin) == 2 &&
          ini >= 0 && ini <= 23 && fin >= 0 && fin <= 23) {
        cfg.oraInizio = ini; cfg.oraFine = fin;
        ricalcolaPiano();
        Serial.print(F("Fascia oraria: ")); stampaDueCifre(cfg.oraInizio);
        Serial.print(F(":00 - ")); stampaDueCifre(cfg.oraFine); Serial.println(F(":00"));
      } else Serial.println(F("Uso: h <ora_inizio> <ora_fine>   esempio: h 9 20"));
      break;
    }

    case 'p': {
      int v = resto.toInt();
      if (v >= 100 && v <= 2000) {
        cfg.tpd = v; ricalcolaPiano();
        Serial.print(F("TPD impostati a ")); Serial.println(cfg.tpd);
      } else Serial.println(F("Valore ammesso: 100..2000"));
      break;
    }

    case 'c': {
      int v = resto.toInt();
      if (v >= 1 && v <= 120) {
        cfg.cicloMinuti = v; ricalcolaPiano();
        Serial.print(F("Ciclo impostato a ")); Serial.print(cfg.cicloMinuti); Serial.println(F(" minuti"));
      } else Serial.println(F("Valore ammesso: 1..120 minuti"));
      break;
    }

    case 'o': {
      if (resto.length() == 0) {
        Serial.println(F("Uso: o <secondi>   esempio: o 5   (0 = un motore per volta)"));
        break;
      }
      int v = resto.toInt();
      if (v >= 0 && v <= 600) {
        cfg.sfalsamentoS = (uint16_t)v; ricalcolaPiano();
        if (v == 0) Serial.println(F("Partenze in quarti di ciclo: un motore per volta"));
        else { Serial.print(F("Sfalsamento fra le partenze: ")); Serial.print(v); Serial.println(F(" s")); }
      } else Serial.println(F("Valore ammesso: 0..600 secondi"));
      break;
    }

    case 'v': {
      int v = resto.toInt();
      if (v >= 1000 && v <= 10000) {
        cfg.passoUs = v;
        Serial.print(F("Intervallo fra i passi: ")); Serial.print(cfg.passoUs); Serial.println(F(" us"));
      } else Serial.println(F("Valore ammesso: 1000..10000 us"));
      break;
    }

    case 'r': {
      int v = resto.toInt();
      if (v >= 0 && v <= 2) {
        cfg.modoVerso = v; ricalcolaPiano();
        Serial.print(F("Verso: "));
        Serial.println(v == 0 ? F("bidirezionale") : (v == 1 ? F("solo orario") : F("solo antiorario")));
      } else Serial.println(F("Uso: r 0 | r 1 | r 2"));
      break;
    }

    case 'g': {
      float v = resto.toFloat();
      if (v >= 0.1f && v <= 20.0f) {
        cfg.rapporto = v; ricalcolaPiano();
        Serial.print(F("Rapporto impostato a ")); Serial.println(cfg.rapporto, 3);
      } else Serial.println(F("Valore ammesso: 0.1..20.0"));
      break;
    }

    case 'e': case 'i': {
      if (resto.length() < 1) { Serial.println(F("Uso: e<n> <0|1>  oppure  i<n> <0|1>")); break; }
      int n = resto.charAt(0) - '0';
      int v = resto.substring(1).toInt();
      if (n >= 1 && n <= 4 && (v == 0 || v == 1)) {
        if (c == 'e') {
          cfg.attivo[n - 1] = v;
          if (!v) { winder[n - 1].stato = WS_FERMO; bobineSpente(n - 1); }
          Serial.print(F("Posto ")); Serial.print(n);
          Serial.println(v ? F(" abilitato") : F(" disabilitato"));
        } else {
          cfg.inverti[n - 1] = v;
          Serial.print(F("Motore ")); Serial.print(n);
          Serial.println(v ? F(": verso invertito") : F(": verso normale"));
        }
      } else Serial.println(F("Uso: e<n> <0|1>  esempio: e3 0"));
      break;
    }

    case 'w':
      if (!eepromPresente) { Serial.println(F("EEPROM non rilevata: impossibile salvare.")); break; }
      Serial.println(configSalva() ? F("Configurazione salvata in EEPROM.")
                                   : F("Salvataggio in EEPROM fallito."));
      break;

    case 'z':
      configDefault();
      ricalcolaPiano();
      Serial.println(F("Valori di fabbrica ripristinati (usare 'w' per renderli permanenti)."));
      break;

    default:
      Serial.print(F("Comando sconosciuto: '")); Serial.print(cmd);
      Serial.println(F("'   Digitare ? per l'elenco."));
      break;
  }
}

void leggiSeriale() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (bufferSeriale.length() > 0) {
        eseguiComando(bufferSeriale);
        bufferSeriale = "";
      }
    } else if (bufferSeriale.length() < 60) {
      bufferSeriale += ch;
    }
  }
}

// ============================================================================
//  SETUP
// ============================================================================

void setup() {
  // Tutti i pin dei motori in uscita e a livello basso: le bobine devono
  // restare diseccitate finche' non serve muovere.
  for (uint8_t m = 0; m < 4; m++) {
    for (uint8_t i = 0; i < 4; i++) {
      pinMode(PIN_MOTORE[m][i], OUTPUT);
      digitalWrite(PIN_MOTORE[m][i], LOW);
    }
    winder[m].stato          = WS_FERMO;
    winder[m].indicePasso    = 0;
    winder[m].passiEseguiti  = 0;
    winder[m].partenzaArmata = true;
    winder[m].test           = false;
  }

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  unsigned long attesa = millis();
  while (!Serial && (millis() - attesa) < 4000) { delay(50); }
  delay(300);

  Serial.println();
  Serial.println(F("============================================"));
  Serial.println(F("  WATCH WINDER 2 POSTI - Raspberry Pi Pico 2 W"));
  Serial.println(F("============================================"));

  // Bus I2C1 sui pin GP26 (SDA) e GP27 (SCL)
  Wire1.setSDA(PIN_SDA);
  Wire1.setSCL(PIN_SCL);
  Wire1.begin();
  Wire1.setClock(100000);

  // Rilevamento dei dispositivi I2C
  Wire1.beginTransmission(ADDR_DS3231);
  rtcPresente = (Wire1.endTransmission() == 0);
  Wire1.beginTransmission(ADDR_EEPROM);
  eepromPresente = (Wire1.endTransmission() == 0);

  Serial.print(F("RTC DS3231 (0x68) .... "));
  Serial.println(rtcPresente ? F("trovato") : F("NON TROVATO - controllare i collegamenti"));
  Serial.print(F("EEPROM AT24C32 (0x57)  "));
  Serial.println(eepromPresente ? F("trovata") : F("non trovata"));

  // Configurazione: dalla EEPROM se valida, altrimenti valori di fabbrica
  if (eepromPresente && configCarica())
    Serial.println(F("Configurazione caricata dalla EEPROM."));
  else {
    configDefault();
    Serial.println(F("Configurazione di fabbrica in uso."));
  }

  if (rtcPresente && rtcOrologioFermo()) {
    Serial.println();
    Serial.println(F("  *** L'RTC segnala che l'oscillatore si e' fermato. ***"));
    Serial.println(F("  *** L'ora non e' attendibile: impostarla con il comando T. ***"));
    Serial.println();
  }

  // Prima lettura dell'ora gia' qui: senza, nel primo secondo del loop l'ora
  // risulterebbe non valida e i motori partirebbero per ripiego anche di notte.
  if (rtcPresente) {
    oraCacheValida = rtcLeggi(oraCache);
    ultimaLetturaRtcMs = millis();
  }

  ricalcolaPiano();
  cicloInizioMs = millis();

  stampaStato();
  stampaAiuto();
}

// ============================================================================
//  LOOP
// ============================================================================

void loop() {
  leggiSeriale();

  // Se un motore sta girando per un test, lo lasciamo lavorare senza
  // interferenze dalla logica dei cicli.
  bool inTest = false;
  for (uint8_t m = 0; m < 4; m++) if (winder[m].test) inTest = true;

  if (inTest) {
    for (uint8_t m = 0; m < 4; m++) aggiornaWinder(m);
    return;
  }

  if (sospeso) {
    if (statoLed != 0) { statoLed = 0; digitalWrite(LED_BUILTIN, LOW); }
    return;
  }

  // Stato della fascia oraria: l'RTC viene interrogato una volta al secondo
  // e il risultato tenuto in cache, per non rubare tempo al passo dei motori.
  if (rtcPresente && (millis() - ultimaLetturaRtcMs) >= 1000UL) {
    ultimaLetturaRtcMs = millis();
    oraCacheValida = rtcLeggi(oraCache);
  }
  bool oraValida = oraCacheValida;
  // Se l'RTC non risponde, per prudenza si continua a caricare gli orologi.
  bool attivo = forzaAttivo || !oraValida || fasciaAttiva(oraCache);

  int8_t ledVoluto = attivo ? 1 : 0;
  if (ledVoluto != statoLed) {
    statoLed = ledVoluto;
    digitalWrite(LED_BUILTIN, statoLed ? HIGH : LOW);
  }

  // Log di cortesia a ogni cambio d'ora
  if (oraValida && oraCache.ora != ultimaOraLog && millis() - ultimoLogMs > 2000) {
    ultimaOraLog = oraCache.ora;
    ultimoLogMs  = millis();
    Serial.print(F("[")); stampaOra(oraCache); Serial.print(F("] "));
    Serial.println(attivo ? F("fascia attiva - i motori lavorano")
                          : F("fascia notturna - motori fermi"));
  }

  if (!attivo) {
    for (uint8_t m = 0; m < 4; m++) {
      if (winder[m].stato != WS_FERMO) { winder[m].stato = WS_FERMO; bobineSpente(m); }
      winder[m].partenzaArmata = true;
    }
    return;
  }

  // Gestione del ciclo
  unsigned long trascorso = millis() - cicloInizioMs;
  if (trascorso >= cicloDurataMs) {
    cicloInizioMs += cicloDurataMs;
    trascorso = millis() - cicloInizioMs;
    for (uint8_t m = 0; m < 4; m++) winder[m].partenzaArmata = true;
  }

  // Avvio sfalsato dei motori
  for (uint8_t m = 0; m < 4; m++) {
    if (!cfg.attivo[m]) continue;
    if (winder[m].partenzaArmata && winder[m].stato == WS_FERMO &&
        trascorso >= winder[m].partenzaMs) {
      winder[m].partenzaArmata = false;
      avviaSessione(m, false);
    }
  }

  for (uint8_t m = 0; m < 4; m++) aggiornaWinder(m);
}

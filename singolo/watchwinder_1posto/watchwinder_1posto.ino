/*
 * ============================================================================
 *  WATCH WINDER 1 POSTO - NodeMCU V3 (ESP8266)
 * ============================================================================
 *
 *  Autore: Carlo Comincini
 *
 *  Deriva dal firmware del caricatore a 2 posti
 *  (2_orologi/watchwinder_2posti/watchwinder_2posti.ino), ridotto a un solo
 *  motore e portato sull'ESP8266. Il comportamento e' lo stesso:
 *   - fasce orarie distinte per i giorni feriali e per il fine settimana, di
 *     fabbrica 08:00-21:00 dal lunedi' al venerdi' e 10:00-21:00 il sabato e
 *     la domenica; i giri al giorno restano uguali in tutti e due i casi;
 *   - ciclo di 15 minuti: a ogni ciclo il motore compie la sua quota di giri,
 *     divisa fra i due versi, poi si ferma con le bobine spente;
 *   - il DS3231 tiene l'ora UTC e il firmware la converte in ora italiana,
 *     con il passaggio automatico fra ora solare e ora legale, che funziona
 *     anche senza rete;
 *   - ora da internet (NTP) via wifi alla prima occasione dopo l'accensione e
 *     poi ogni 12 ore, sempre a motore fermo; fra una sincronizzazione e
 *     l'altra la radio resta spenta;
 *   - nome e password della rete si impostano dal monitor seriale (comando
 *     'n') e restano solo nella EEPROM del modulo RTC, mai nel codice;
 *   - configurazione salvata nella EEPROM del modulo RTC (comando 'w'), con
 *     un formato diverso da quello del 2 posti (CONFIG_MAGIC distinto);
 *   - configurazione anche dal telefono, senza cavo: premendo RST due volte
 *     a circa un secondo di distanza (o con il comando 'k') la scheda ferma
 *     il motore, crea la rete wifi aperta "Watchwinder" e mostra una pagina
 *     web per rete, giri, fasce orarie, velocita' e ora. La rete si spegne
 *     dopo il salvataggio o dopo 10 minuti senza attivita'.
 *
 *  Hardware:
 *    - NodeMCU V3 (LoLin, modulo ESP-12, chip USB CH340G)
 *    - 1 motore passo-passo 28BYJ-48 (5 V) con driver ULN2003
 *    - Modulo RTC DS3231 (ZS-042) con EEPROM AT24C32 integrata
 *    - Alimentazione solo dal micro-USB, caricatore 5 V da almeno 1 A
 *
 *  Meccanica: "Single Watch Winder" di Robbie Kieffer (Printables 1290039),
 *  remix del doppio di JayS; porta-orologio sull'albero del motore, 1:1.
 *  Schema: singolo/schema-elettrico-1-posto.svg
 *
 *  Collegamenti:
 *    RTC:      SCL -> D1 (GPIO5)    SDA -> D2 (GPIO4)    VCC -> 3V   GND -> G
 *    ULN2003:  IN1 -> D5 (GPIO14)   IN2 -> D6 (GPIO12)
 *              IN3 -> D7 (GPIO13)   IN4 -> D8 (GPIO15)
 *              +   -> VU (5 V del connettore USB)        -   -> G
 *    LED di stato: quello integrato su D4 (GPIO2), acceso = fascia attiva,
 *    lampeggiante = configurazione da telefono.
 *    Pulsante RST (gia' sulla scheda): due pressioni entro 5 secondi aprono
 *    la configurazione da telefono. Il segnale fra i due riavvii sta nella
 *    memoria interna dell'ESP8266, che resiste al RST ma non allo
 *    spegnimento.
 *    Pulsante FLASH (GPIO0): tenuto 3 s apre o chiude la configurazione, se
 *    fa contatto. Sulla scheda di Carlo NON funziona (provato il 6/11/2026:
 *    tenuto premuto durante il RST non porta la scheda in programmazione),
 *    per questo c'e' il doppio RST. Su una scheda con il FLASH funzionante
 *    va premuto a scheda accesa: premuto mentre si collega il cavo, avvia la
 *    scheda in modalita' di programmazione.
 *
 *  ATTENZIONE: il VCC del modulo RTC va sul 3V, mai sul 5 V. Mai collegare un
 *  secondo alimentatore su VU con il cavo USB inserito.
 *  D8 (GPIO15) deve essere basso all'accensione: lo tengono basso la
 *  resistenza della scheda e l'ingresso dell'ULN2003.
 *
 *  Scheda da selezionare in Arduino IDE:
 *    NodeMCU 1.0 (ESP-12E Module), core "esp8266" 3.1.2
 *    (FQBN esp8266:esp8266:nodemcuv2)
 * ============================================================================
 */

#include <stddef.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

// ============================================================================
//  CONFIGURAZIONE - valori di default, modificabili anche da monitor seriale
// ============================================================================

// --- Fascia oraria di funzionamento -----------------------------------------
// Il winder ruota da ORA_INIZIO (inclusa) a ORA_FINE (esclusa). Il sabato e
// la domenica valgono le ore _WE. Inizio uguale alla fine = sempre attivo.
#define DEF_ORA_INIZIO      8      // lunedi'-venerdi': 08:00 -> inizio attivita'
#define DEF_ORA_FINE       21      // lunedi'-venerdi': 21:00 -> stop notturno
#define DEF_ORA_INIZIO_WE  10      // sabato e domenica: 10:00
#define DEF_ORA_FINE_WE    21      // sabato e domenica: 21:00

// --- Rotazione ---------------------------------------------------------------
// TPD = Turns Per Day, giri completi al giorno richiesti dall'orologio.
// Il caricatore e' per un Longines automatico da donna degli anni '80, di
// cui non si conoscono modello e calibro. I movimenti automatici da donna
// piu' diffusi (per esempio l'ETA 2671, base del Longines L561) chiedono
// circa 650 giri al giorno e si caricano in entrambi i versi; per i suoi
// automatici Longines indica 650-800. Si usano 800 giri in entrambi i
// versi, con un po' di margine. Se l'orologio si fermasse pur stando sul
// caricatore, probabilmente si carica in un verso solo: salire a 1300-1600
// con il comando 'p'.
#define DEF_TPD           800

// Durata di un ciclo completo: ogni CICLO_MINUTI il winder esegue la sua
// quota di giri, poi resta fermo fino al ciclo successivo.
#define DEF_CICLO_MINUTI   15

// Intervallo fra due mezzi passi, in microsecondi.
// 1200 us -> circa 4,9 s per giro. Sotto i 1200 us il 28BYJ-48 comincia a
// perdere passi. Il rumore dipende dalle risonanze della plastica, non solo
// dalla velocita': nelle prove del 6/11/2026, con l'orologio montato, 1200
// e' stato il valore piu' silenzioso fra 1200, 1500, 1800, 1900, 2500, 3000
// e 4000. Per provarne altri si usa il comando 'v'.
#define DEF_PASSO_US     1200

// Pausa fra la fase oraria e quella antioraria, in millisecondi.
#define DEF_PAUSA_META_MS 1500

// --- Caratteristiche meccaniche ----------------------------------------------
// Il 28BYJ-48 compie 4096 mezzi passi per ogni giro dell'albero di uscita.
#define PASSI_PER_GIRO_MOTORE 4096L
// Giri di motore necessari per un giro del porta-orologio (1.0 = presa diretta).
#define DEF_RAPPORTO       1.0f

// --- Verso di rotazione -----------------------------------------------------
//   0 = bidirezionale (meta' giri orari, meta' antiorari)  <-- di fabbrica
//   1 = solo orario
//   2 = solo antiorario
#define DEF_MODO_VERSO      0

// Se il motore gira al contrario del previsto, mettere true (o usare 'i 1')
// invece di rifare i collegamenti.
#define DEF_INVERTI     false

// --- Ora da internet ---------------------------------------------------------
#define NTP_SERVER        "pool.ntp.org"
#define NTP_PORTA_LOCALE   2390
#define NTP_INTERVALLO_H     12    // ore fra due sincronizzazioni riuscite
#define NTP_RIPROVA_MIN      15    // minuti prima di riprovare dopo un errore
#define WIFI_ATTESA_MS    15000    // tempo massimo per collegarsi alla rete
#define NOME_RETE   "watchwinder"  // nome con cui la scheda si presenta in rete

// --- Configurazione da telefono ----------------------------------------------
#define RETE_CONFIG       "Watchwinder"  // rete wifi aperta creata dalla scheda
#define PRESSIONE_CONFIG_MS   3000       // FLASH tenuto premuto per aprirla/chiuderla
#define CONFIG_DURATA_MIN       10       // si chiude dopo 10 minuti senza richieste
#define MAX_RETI_VICINE         12       // reti proposte nella pagina
#define DOPPIO_RST_MS         5000       // finestra per il secondo RST
#define DOPPIO_RST_BLOCCO       32       // blocco da 4 byte nella memoria RTC dell'ESP8266
#define DOPPIO_RST_MAGIC  0x44525354UL   // "DRST": primo RST gia' avvenuto

// ============================================================================
//  PIN
// ============================================================================

#define PIN_SDA   D2     // GPIO4
#define PIN_SCL   D1     // GPIO5

// IN1..IN4 del driver ULN2003
const uint8_t PIN_MOTORE[4] = { D5, D6, D7, D8 };   // GPIO14, 12, 13, 15

// Pulsante FLASH della NodeMCU: chiude a massa, resistenza di pull-up sulla
// scheda. Dopo l'avvio e' un ingresso libero.
#define PIN_FLASH  0     // GPIO0 (D3)

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

#define CONFIG_MAGIC   0x57573101UL   // "WW1" + versione
#define CONFIG_ADDR    0x0000

struct Config {
  uint32_t magic;
  uint8_t  oraInizio;      // lunedi'-venerdi'
  uint8_t  oraFine;
  uint8_t  oraInizioWe;    // sabato e domenica
  uint8_t  oraFineWe;
  uint16_t tpd;
  uint16_t passoUs;
  uint16_t pausaMetaMs;
  uint8_t  cicloMinuti;
  uint8_t  modoVerso;
  float    rapporto;
  uint8_t  inverti;
  uint8_t  checksum;
};

Config cfg;

// Rete wifi, in un blocco separato della EEPROM: si imposta con il comando
// 'n' e non compare mai nel codice, che e' pubblico. Stesso formato del
// 2 posti.
#define WIFI_MAGIC     0x57574901UL
#define WIFI_ADDR      0x0100

struct WifiCfg {
  uint32_t magic;
  char     ssid[33];       // al massimo 32 caratteri
  char     password[64];   // al massimo 63 caratteri (WPA2)
  uint8_t  checksum;
};

WifiCfg wifiCfg;
bool    wifiCfgValida = false;

// ============================================================================
//  STATO DEL MOTORE
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
  bool           partenzaArmata;   // true finche' non e' partito in questo ciclo
  unsigned long  passiEseguiti;    // contatore diagnostico dei passi totali
  bool           test;             // true se sta girando per un comando di test
};

Winder winder;

// ============================================================================
//  VARIABILI GLOBALI
// ============================================================================

struct DataOra {
  uint16_t anno;
  uint8_t  mese, giorno, ora, minuto, secondo;
};

unsigned long cicloInizioMs = 0;     // millis() di inizio del ciclo corrente
unsigned long cicloDurataMs = 0;     // durata del ciclo in ms
long          passiPerCiclo  = 0;    // passi totali in un ciclo

bool rtcPresente   = false;
bool eepromPresente = false;
unsigned long ultimaRicercaRtcMs = 0; // ultimo tentativo di trovare l'RTC assente
bool pianoFineSettimana = false;     // il piano in uso e' quello di sabato e domenica

unsigned long prossimaSyncMs = 0;    // millis() del prossimo tentativo NTP
unsigned long ultimoTentativoMs = 0; // millis() dell'ultimo tentativo NTP
bool          avvisoRtcAzzerato = false;
bool          syncFatta    = false;  // almeno una sincronizzazione riuscita
bool          syncRiuscita = false;  // esito dell'ultimo tentativo
DataOra       oraUltimaSync;         // ora italiana dell'ultima riuscita
bool forzaAttivo   = false;          // comando 'f': ignora la fascia oraria
bool sospeso       = false;          // comando 'x': tutto fermo

uint8_t ultimaOraLog = 255;
unsigned long ultimoLogMs = 0;

// Cache della lettura RTC: interrogare il DS3231 a ogni giro di loop()
// occuperebbe il bus I2C per circa 1 ms, disturbando il timing dei passi.
DataOra       oraCache;
bool          oraCacheValida = false;
unsigned long ultimaLetturaRtcMs = 0;

int8_t statoLed = -1;

String bufferSeriale = "";

// Configurazione da telefono
const IPAddress IP_CONFIG(192, 168, 4, 1);
ESP8266WebServer server(80);
DNSServer        dns;
bool          modoConfig = false;
unsigned long configAttivitaMs = 0;     // millis() dell'ultima richiesta ricevuta
bool          uscitaProgrammata = false;
unsigned long uscitaMs = 0;             // millis() a cui chiudere la configurazione
bool          syncDopoConfig = false;   // rete cambiata: sincronizzare all'uscita
unsigned long ultimoLampeggioMs = 0;
String        retiVicine[MAX_RETI_VICINE];
uint8_t       numRetiVicine = 0;
unsigned long flashInizioMs = 0;        // 0 = pulsante non premuto
bool          flashAttesaRilascio = false;
bool          doppioRstInAttesa = false; // segnale scritto, da cancellare a fine finestra

// Il LED integrato della NodeMCU (GPIO2) si accende a livello basso.
void impostaLed(bool acceso) {
  digitalWrite(LED_BUILTIN, acceso ? LOW : HIGH);
}

// ============================================================================
//  DS3231 - accesso diretto ai registri (nessuna libreria esterna)
// ============================================================================

static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

bool rtcLeggi(DataOra &t) {
  Wire.beginTransmission(ADDR_DS3231);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(ADDR_DS3231, 7) != 7) return false;

  t.secondo = bcd2dec(Wire.read() & 0x7F);
  t.minuto  = bcd2dec(Wire.read() & 0x7F);
  uint8_t regOra = Wire.read();
  if (regOra & 0x40) {                       // formato 12 ore
    uint8_t h = bcd2dec(regOra & 0x1F);
    if (regOra & 0x20) { if (h != 12) h += 12; }
    else               { if (h == 12) h = 0;  }
    t.ora = h;
  } else {
    t.ora = bcd2dec(regOra & 0x3F);
  }
  Wire.read();                               // giorno della settimana, non usato
  t.giorno = bcd2dec(Wire.read() & 0x3F);
  uint8_t regMese = Wire.read();
  t.mese = bcd2dec(regMese & 0x1F);
  t.anno = 2000 + bcd2dec(Wire.read());
  if (regMese & 0x80) t.anno += 100;
  return true;
}

bool rtcScrivi(const DataOra &t) {
  Wire.beginTransmission(ADDR_DS3231);
  Wire.write(0x00);
  Wire.write(dec2bcd(t.secondo));
  Wire.write(dec2bcd(t.minuto));
  Wire.write(dec2bcd(t.ora));                // bit 6 a 0 -> formato 24 ore
  Wire.write(1);                             // fittizio: si ricava dalla data
  Wire.write(dec2bcd(t.giorno));
  Wire.write(dec2bcd(t.mese));               // secolo a 0
  Wire.write(dec2bcd((uint8_t)(t.anno % 100)));
  if (Wire.endTransmission() != 0) return false;

  // Azzera il flag "oscillator stopped" nel registro di stato 0x0F
  Wire.beginTransmission(ADDR_DS3231);
  Wire.write(0x0F);
  Wire.write(0x00);
  Wire.endTransmission();
  return true;
}

bool rtcOrologioFermo() {
  Wire.beginTransmission(ADDR_DS3231);
  Wire.write(0x0F);
  if (Wire.endTransmission() != 0) return true;
  if (Wire.requestFrom(ADDR_DS3231, 1) != 1) return true;
  return (Wire.read() & 0x80) != 0;          // bit OSF
}

float rtcTemperatura() {
  Wire.beginTransmission(ADDR_DS3231);
  Wire.write(0x11);
  if (Wire.endTransmission() != 0) return -127.0f;
  if (Wire.requestFrom(ADDR_DS3231, 2) != 2) return -127.0f;
  int8_t  parteIntera = (int8_t)Wire.read();
  uint8_t parteDec    = Wire.read() >> 6;
  return parteIntera + parteDec * 0.25f;
}

// Giorno della settimana ricavato dalla data (0 = domenica ... 6 = sabato),
// metodo di Sakamoto. Il registro del DS3231 non si usa perche' rtcScrivi()
// non lo imposta.
uint8_t giornoSettimana(const DataOra &t) {
  static const uint8_t k[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  if (t.mese < 1 || t.mese > 12) return 1;   // data non valida: lunedi'
  uint16_t a = t.anno;
  if (t.mese < 3) a--;
  return (a + a / 4 - a / 100 + a / 400 + k[t.mese - 1] + t.giorno) % 7;
}

bool fineSettimana(const DataOra &t) {
  uint8_t g = giornoSettimana(t);
  return g == 0 || g == 6;
}

// Secondi dal 1/1/1970 per una data e viceversa (algoritmi di H. Hinnant).
uint32_t secondiDaData(const DataOra &t) {
  int32_t  a   = (int32_t)t.anno - (t.mese <= 2 ? 1 : 0);
  int32_t  era = a / 400;
  uint32_t aoe = (uint32_t)(a - era * 400);                     // anno dell'era
  uint32_t gda = (153U * (t.mese > 2 ? t.mese - 3U : t.mese + 9U) + 2U) / 5U
                 + t.giorno - 1U;                               // giorno da marzo
  uint32_t gde = aoe * 365U + aoe / 4U - aoe / 100U + gda;       // giorno dell'era
  uint32_t giorni = (uint32_t)(era * 146097 + (int32_t)gde - 719468);
  return giorni * 86400UL + t.ora * 3600UL + t.minuto * 60UL + t.secondo;
}

void dataDaSecondi(uint32_t s, DataOra &t) {
  uint32_t resto = s % 86400UL;
  t.ora     = resto / 3600UL;
  t.minuto  = (resto / 60UL) % 60UL;
  t.secondo = resto % 60UL;
  uint32_t z   = s / 86400UL + 719468UL;
  uint32_t era = z / 146097UL;
  uint32_t gde = z - era * 146097UL;
  uint32_t aoe = (gde - gde / 1460UL + gde / 36524UL - gde / 146096UL) / 365UL;
  uint32_t gda = gde - (365UL * aoe + aoe / 4UL - aoe / 100UL);
  uint32_t mp  = (5UL * gda + 2UL) / 153UL;
  t.giorno = gda - (153UL * mp + 2UL) / 5UL + 1UL;
  t.mese   = mp < 10UL ? mp + 3UL : mp - 9UL;
  t.anno   = aoe + era * 400UL + (t.mese <= 2 ? 1 : 0);
}

// Ora italiana: UTC+1, UTC+2 con l'ora legale europea, che va dall'ultima
// domenica di marzo all'ultima domenica di ottobre con il cambio alle 01:00 UTC.
bool oraLegale(const DataOra &utc) {
  if (utc.mese < 3 || utc.mese > 10) return false;
  if (utc.mese > 3 && utc.mese < 10) return true;
  DataOra fineMese = utc;
  fineMese.giorno = 31;                      // marzo e ottobre hanno 31 giorni
  uint8_t domenica = 31 - giornoSettimana(fineMese);
  if (utc.mese == 3)
    return utc.giorno > domenica || (utc.giorno == domenica && utc.ora >= 1);
  return utc.giorno < domenica || (utc.giorno == domenica && utc.ora < 1);
}

void utcInLocale(const DataOra &utc, DataOra &locale) {
  dataDaSecondi(secondiDaData(utc) + (oraLegale(utc) ? 7200UL : 3600UL), locale);
}

// Nell'ora che a fine ottobre si ripete vale l'ora solare.
void localeInUtc(const DataOra &locale, DataOra &utc) {
  uint32_t s = secondiDaData(locale) - 3600UL;
  dataDaSecondi(s, utc);
  if (oraLegale(utc)) dataDaSecondi(s - 3600UL, utc);
}

// Il DS3231 tiene l'ora UTC; il resto del firmware ragiona in ora italiana.
bool leggiOra(DataOra &locale) {
  DataOra utc;
  if (!rtcLeggi(utc)) return false;
  utcInLocale(utc, locale);
  return true;
}

bool scriviOra(const DataOra &locale) {
  DataOra utc;
  localeInUtc(locale, utc);
  return rtcScrivi(utc);
}

// Un RTC azzerato (batteria scarica o tolta, contatto incerto) riparte dal
// 1/1/2000: un'ora precedente al 2026 non e' attendibile.
bool oraAttendibile() {
  return oraCacheValida && oraCache.anno >= 2026;
}

// ============================================================================
//  EEPROM AT24C32
// ============================================================================

bool eepromScriviBlocco(uint16_t indirizzo, const uint8_t *dati, uint8_t n) {
  Wire.beginTransmission(ADDR_EEPROM);
  Wire.write((uint8_t)(indirizzo >> 8));
  Wire.write((uint8_t)(indirizzo & 0xFF));
  for (uint8_t i = 0; i < n; i++) Wire.write(dati[i]);
  if (Wire.endTransmission() != 0) return false;
  delay(6);                                  // tempo di scrittura interno
  return true;
}

bool eepromLeggiBlocco(uint16_t indirizzo, uint8_t *dati, uint8_t n) {
  Wire.beginTransmission(ADDR_EEPROM);
  Wire.write((uint8_t)(indirizzo >> 8));
  Wire.write((uint8_t)(indirizzo & 0xFF));
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(ADDR_EEPROM, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) dati[i] = Wire.read();
  return true;
}

bool eepromLeggi(uint16_t indirizzo, void *dati, uint16_t n) {
  uint8_t *p = (uint8_t *)dati;
  for (uint16_t off = 0; off < n; off += 24) {
    uint8_t blocco = (n - off > 24) ? 24 : (uint8_t)(n - off);
    if (!eepromLeggiBlocco(indirizzo + off, p + off, blocco)) return false;
  }
  return true;
}

// Blocchi da 16 byte: partendo da un indirizzo multiplo di 16 non scavalcano
// mai le pagine da 32 byte della AT24C32.
bool eepromScrivi(uint16_t indirizzo, const void *dati, uint16_t n) {
  const uint8_t *p = (const uint8_t *)dati;
  for (uint16_t off = 0; off < n; off += 16) {
    uint8_t blocco = (n - off > 16) ? 16 : (uint8_t)(n - off);
    if (!eepromScriviBlocco(indirizzo + off, p + off, blocco)) return false;
  }
  return true;
}

uint8_t sommaXor(const void *dati, size_t n) {
  const uint8_t *p = (const uint8_t *)dati;
  uint8_t somma = 0;
  for (size_t i = 0; i < n; i++) somma ^= p[i];
  return somma;
}

// Solo i byte che precedono il campo checksum: restano esclusi il checksum
// stesso e il riempimento finale della struttura.
uint8_t calcolaChecksum(const Config &c) {
  return sommaXor(&c, offsetof(Config, checksum));
}

void configDefault() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic       = CONFIG_MAGIC;
  cfg.oraInizio   = DEF_ORA_INIZIO;
  cfg.oraFine     = DEF_ORA_FINE;
  cfg.oraInizioWe = DEF_ORA_INIZIO_WE;
  cfg.oraFineWe   = DEF_ORA_FINE_WE;
  cfg.tpd         = DEF_TPD;
  cfg.passoUs     = DEF_PASSO_US;
  cfg.pausaMetaMs = DEF_PAUSA_META_MS;
  cfg.cicloMinuti = DEF_CICLO_MINUTI;
  cfg.modoVerso   = DEF_MODO_VERSO;
  cfg.rapporto    = DEF_RAPPORTO;
  cfg.inverti     = DEF_INVERTI;
  cfg.checksum    = calcolaChecksum(cfg);
}

bool configCarica() {
  Config tmp;
  if (!eepromLeggi(CONFIG_ADDR, &tmp, sizeof(Config))) return false;
  if (tmp.magic != CONFIG_MAGIC) return false;
  if (tmp.checksum != calcolaChecksum(tmp)) return false;
  cfg = tmp;
  return true;
}

bool configSalva() {
  cfg.checksum = calcolaChecksum(cfg);
  return eepromScrivi(CONFIG_ADDR, &cfg, sizeof(Config));
}

bool wifiCarica() {
  WifiCfg tmp;
  if (!eepromLeggi(WIFI_ADDR, &tmp, sizeof(WifiCfg))) return false;
  if (tmp.magic != WIFI_MAGIC) return false;
  if (tmp.checksum != sommaXor(&tmp, offsetof(WifiCfg, checksum))) return false;
  tmp.ssid[sizeof(tmp.ssid) - 1] = 0;
  tmp.password[sizeof(tmp.password) - 1] = 0;
  wifiCfg = tmp;
  return true;
}

bool wifiSalva() {
  wifiCfg.checksum = sommaXor(&wifiCfg, offsetof(WifiCfg, checksum));
  return eepromScrivi(WIFI_ADDR, &wifiCfg, sizeof(WifiCfg));
}

// ============================================================================
//  CONTROLLO DEL MOTORE
// ============================================================================

void bobineSpente() {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(PIN_MOTORE[i], LOW);
}

void applicaPasso(uint8_t indice) {
  for (uint8_t i = 0; i < 4; i++)
    digitalWrite(PIN_MOTORE[i], SEQUENZA[indice][i] ? HIGH : LOW);
}

// Avanza di un mezzo passo. orario = true -> senso orario, salvo inversione
// impostata con cfg.inverti.
void eseguiPasso(bool orario) {
  bool avanti = cfg.inverti ? !orario : orario;
  if (avanti) winder.indicePasso = (winder.indicePasso + 1) & 0x07;
  else        winder.indicePasso = (winder.indicePasso + 7) & 0x07;
  applicaPasso(winder.indicePasso);
}

long passiPerGiroOrologio() {
  return (long)(PASSI_PER_GIRO_MOTORE * cfg.rapporto);
}

// Calcola quanti passi fare in un ciclo e come dividerli fra le due
// direzioni. I giri al giorno restano gli stessi anche quando la fascia di
// sabato e domenica e' piu' corta: cambia la quota per ciclo.
// Senza un'ora valida dall'RTC si usa la fascia dei giorni feriali.
void ricalcolaPiano() {
  cicloDurataMs = (unsigned long)cfg.cicloMinuti * 60UL * 1000UL;

  pianoFineSettimana = oraAttendibile() && fineSettimana(oraCache);
  uint8_t ini = pianoFineSettimana ? cfg.oraInizioWe : cfg.oraInizio;
  uint8_t fin = pianoFineSettimana ? cfg.oraFineWe   : cfg.oraFine;

  int oreAttive = (int)fin - (int)ini;
  if (oreAttive <= 0) oreAttive += 24;

  float cicliAlGiorno = (oreAttive * 60.0f) / (float)cfg.cicloMinuti;
  float giriPerCiclo  = (float)cfg.tpd / cicliAlGiorno;

  passiPerCiclo = (long)(giriPerCiclo * passiPerGiroOrologio() + 0.5f);

  if (cfg.modoVerso == 1) {                   // solo orario
    winder.passiFaseCw  = passiPerCiclo;
    winder.passiFaseCcw = 0;
  } else if (cfg.modoVerso == 2) {            // solo antiorario
    winder.passiFaseCw  = 0;
    winder.passiFaseCcw = passiPerCiclo;
  } else {                                    // bidirezionale
    winder.passiFaseCw  = passiPerCiclo / 2;
    winder.passiFaseCcw = passiPerCiclo - winder.passiFaseCw;
  }
}

// Durata stimata di una sessione completa, in millisecondi.
unsigned long durataSessioneMs() {
  unsigned long t = (unsigned long)((float)passiPerCiclo * cfg.passoUs / 1000.0f);
  if (cfg.modoVerso == 0) t += cfg.pausaMetaMs;
  return t;
}

void avviaSessione(bool comeTest) {
  winder.test = comeTest;
  if (winder.passiFaseCw > 0) {
    winder.stato        = WS_ORARIO;
    winder.passiRimasti = winder.passiFaseCw;
  } else if (winder.passiFaseCcw > 0) {
    winder.stato        = WS_ANTIORARIO;
    winder.passiRimasti = winder.passiFaseCcw;
  } else {
    winder.stato = WS_FERMO;
    return;
  }
  winder.ultimoPassoUs = micros();
}

void fermaMotore() {
  winder.stato = WS_FERMO;
  winder.test  = false;
  bobineSpente();
}

// Aggiornamento non bloccante del motore.
void aggiornaWinder() {
  Winder &w = winder;

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

      eseguiPasso(w.stato == WS_ORARIO);
      w.passiRimasti--;
      w.passiEseguiti++;

      if (w.passiRimasti <= 0) {
        bobineSpente();
        if (w.stato == WS_ORARIO && w.passiFaseCcw > 0) {
          w.stato       = WS_PAUSA_META;
          w.finePausaMs = millis() + cfg.pausaMetaMs;
        } else {
          w.stato = WS_FERMO;
          if (w.test) {
            w.test = false;
            Serial.println(F("[TEST] Sessione conclusa."));
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
  bool we     = fineSettimana(t);
  uint8_t ini = we ? cfg.oraInizioWe : cfg.oraInizio;
  uint8_t fin = we ? cfg.oraFineWe   : cfg.oraFine;
  if (ini == fin) return true;                         // sempre attivo
  if (ini < fin)
    return (t.ora >= ini && t.ora < fin);
  // fascia che scavalca la mezzanotte: conta il giorno di calendario in corso
  return (t.ora >= ini || t.ora < fin);
}

// ============================================================================
//  STAMPE
// ============================================================================

void stampaDueCifre(uint8_t v) {
  if (v < 10) Serial.print('0');
  Serial.print(v);
}

// Data e ora in forma leggibile, es. "ven 06/11/2026 16:30:00".
String testoOra(const DataOra &t) {
  static const char *const NOMI_GIORNO[7] = { "dom", "lun", "mar", "mer", "gio", "ven", "sab" };
  char s[32];
  snprintf(s, sizeof(s), "%s %02u/%02u/%04u %02u:%02u:%02u", NOMI_GIORNO[giornoSettimana(t)],
           t.giorno, t.mese, t.anno, t.ora, t.minuto, t.secondo);
  return String(s);
}

void stampaOra(const DataOra &t) {
  Serial.print(testoOra(t));
}

// Legge "aaaa-mm-gg hh:mm:ss" (anche senza zeri iniziali) e controlla i valori.
bool dataOraDaTesto(const char *testo, DataOra &t) {
  int a, me, g, h, mi, se;
  if (sscanf(testo, "%d-%d-%d %d:%d:%d", &a, &me, &g, &h, &mi, &se) != 6) return false;
  if (a < 2000 || a > 2099 || me < 1 || me > 12 || g < 1 || g > 31 ||
      h < 0 || h > 23 || mi < 0 || mi > 59 || se < 0 || se > 59) return false;
  t.anno = a; t.mese = me; t.giorno = g; t.ora = h; t.minuto = mi; t.secondo = se;
  return true;
}

void stampaFascia(uint8_t ini, uint8_t fin) {
  stampaDueCifre(ini); Serial.print(F(":00 - "));
  stampaDueCifre(fin); Serial.print(F(":00"));
  if (ini == fin) Serial.print(F("  (sempre attivo)"));
  Serial.println();
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
  Serial.println(F("  T aaaa-mm-gg hh:mm:ss   imposta l'orologio RTC (ora italiana)"));
  Serial.println(F("  Tc             imposta l'RTC all'ora di compilazione"));
  Serial.println(F("  n              sincronizza subito l'ora da internet"));
  Serial.println(F("  n <rete> <password>   salva la rete wifi in EEPROM e sincronizza"));
  Serial.println(F("  n -            cancella la rete wifi salvata"));
  Serial.println(F("  k              apre/chiude la configurazione da telefono (come due RST ravvicinati)"));
  Serial.println();
  Serial.println(F("  m              test: un giro orario + un giro antiorario"));
  Serial.println(F("  d              test verso: 3 giri solo orari"));
  Serial.println(F("  x              ferma tutto (sospensione)"));
  Serial.println(F("  a              torna al funzionamento automatico"));
  Serial.println(F("  f              forza attivita' ignorando la fascia oraria"));
  Serial.println();
  Serial.println(F("  CONFIGURAZIONE (seguita da 'w' per salvare in EEPROM)"));
  Serial.println(F("  h <ini> <fin>  stessa fascia oraria tutti i giorni, es. 'h 9 20'"));
  Serial.println(F("  h <ini> <fin> <ini> <fin>   lun-ven e sab-dom, es. 'h 8 21 10 21'"));
  Serial.println(F("  p <tpd>        giri al giorno, es. 'p 650'"));
  Serial.println(F("  c <minuti>     durata del ciclo, es. 'c 15'"));
  Serial.println(F("  v <us>         microsecondi fra i passi, es. 'v 1200'"));
  Serial.println(F("  r <verso>      0=bidirezionale 1=orario 2=antiorario"));
  Serial.println(F("  g <rapporto>   giri motore per giro orologio, es. 'g 1.0'"));
  Serial.println(F("  i <0|1>        inverte il verso del motore, es. 'i 1'"));
  Serial.println(F("  w              salva la configurazione in EEPROM"));
  Serial.println(F("  z              ripristina i valori di fabbrica"));
  Serial.println(F("==========================================="));
  Serial.println();
}

void stampaStato() {
  DataOra t;
  bool ok = rtcPresente && leggiOra(t);

  Serial.println();
  Serial.println(F("------------- STATO WATCH WINDER -------------"));
  Serial.print(F("Ora RTC ......... "));
  if (ok) { stampaOra(t); Serial.print(F("   ")); Serial.print(rtcTemperatura(), 2); Serial.println(F(" C")); }
  else      Serial.println(F("NON DISPONIBILE"));

  Serial.print(F("Ora da internet . "));
  if (!wifiCfgValida) Serial.println(F("rete wifi non impostata (comando n)"));
  else {
    Serial.print(F("rete ")); Serial.print(wifiCfg.ssid);
    if (syncFatta) { Serial.print(F(", sincronizzata ")); stampaOra(oraUltimaSync); }
    else             Serial.print(F(", nessuna sincronizzazione riuscita"));
    if (syncFatta && !syncRiuscita) Serial.print(F("  (ultimo tentativo fallito)"));
    Serial.println();
  }

  Serial.print(F("Fascia lun-ven .. ")); stampaFascia(cfg.oraInizio, cfg.oraFine);
  Serial.print(F("Fascia sab-dom .. ")); stampaFascia(cfg.oraInizioWe, cfg.oraFineWe);

  Serial.print(F("Modalita' ....... "));
  if (modoConfig)       Serial.println(F("CONFIGURAZIONE DA TELEFONO (rete " RETE_CONFIG ")"));
  else if (sospeso)     Serial.println(F("SOSPESA (comando x)"));
  else if (forzaAttivo) Serial.println(F("FORZATA ATTIVA (comando f)"));
  else if (ok && t.anno >= 2026) Serial.println(fasciaAttiva(t) ? F("ATTIVA (giorno)") : F("RIPOSO (notte)"));
  else if (ok)          Serial.println(F("ATTIVA (RTC azzerato, ripiego)"));
  else                  Serial.println(F("ATTIVA (RTC assente, ripiego)"));

  Serial.print(F("TPD richiesti ... ")); Serial.println(cfg.tpd);
  Serial.print(F("Ciclo ........... ")); Serial.print(cfg.cicloMinuti); Serial.println(F(" minuti"));

  float giriCiclo = (float)passiPerCiclo / (float)passiPerGiroOrologio();
  Serial.print(F("Giri per ciclo .. ")); Serial.print(giriCiclo, 2);
  Serial.println(pianoFineSettimana ? F("  (fascia sab-dom)") : F("  (fascia lun-ven)"));
  Serial.print(F("Passi per ciclo . ")); Serial.println(passiPerCiclo);
  Serial.print(F("Velocita' ....... ")); Serial.print(cfg.passoUs);
  Serial.print(F(" us/passo  ("));
  Serial.print(60000000.0f / ((float)cfg.passoUs * PASSI_PER_GIRO_MOTORE), 2);
  Serial.println(F(" giri/min)"));

  unsigned long durata = durataSessioneMs();
  Serial.print(F("Durata sessione . ")); Serial.print(durata / 1000.0f, 1);
  Serial.print(F(" s su ")); Serial.print(cicloDurataMs / 1000.0f, 1);
  Serial.println(F(" s di ciclo"));
  if (durata > cicloDurataMs)
    Serial.println(F("  ATTENZIONE: la sessione non si chiude dentro il ciclo."));

  Serial.print(F("Verso ........... "));
  if (cfg.modoVerso == 0)      Serial.print(F("bidirezionale"));
  else if (cfg.modoVerso == 1) Serial.print(F("solo orario"));
  else                         Serial.print(F("solo antiorario"));
  Serial.println(cfg.inverti ? F("  (motore invertito)") : F(""));

  Serial.print(F("Rapporto ........ ")); Serial.println(cfg.rapporto, 3);
  Serial.print(F("EEPROM .......... ")); Serial.println(eepromPresente ? F("presente") : F("assente"));

  Serial.print(F("Motore .......... ")); Serial.print(nomeStato(winder.stato));
  if (winder.stato == WS_ORARIO || winder.stato == WS_ANTIORARIO) {
    Serial.print(F("  mancano ")); Serial.print(winder.passiRimasti); Serial.print(F(" passi"));
  }
  Serial.print(F("   giri dall'accensione: "));
  Serial.println((float)winder.passiEseguiti / (float)passiPerGiroOrologio(), 1);
  Serial.println(F("---------------------------------------------"));
  Serial.println();
}

// ============================================================================
//  ORA DA INTERNET (NTP via wifi)
// ============================================================================

// Chiede l'ora a un server NTP: secondi dal 1/1/1970, UTC.
bool richiestaNtp(uint32_t &epoch) {
  IPAddress ip;
  if (!WiFi.hostByName(NTP_SERVER, ip)) return false;
  WiFiUDP udp;
  if (!udp.begin(NTP_PORTA_LOCALE)) return false;
  uint8_t pacchetto[48] = { 0 };
  pacchetto[0] = 0x1B;                       // versione 3, modo client
  udp.beginPacket(ip, 123);
  udp.write(pacchetto, sizeof(pacchetto));
  udp.endPacket();

  bool ok = false;
  unsigned long inizio = millis();
  while (millis() - inizio < 3000UL) {
    if (udp.parsePacket() >= 48) {
      udp.read(pacchetto, sizeof(pacchetto));
      // Campo "transmit timestamp": secondi dal 1/1/1900 nei byte 40-43
      uint32_t dal1900 = ((uint32_t)pacchetto[40] << 24) | ((uint32_t)pacchetto[41] << 16) |
                         ((uint32_t)pacchetto[42] << 8)  |  (uint32_t)pacchetto[43];
      epoch = dal1900 - 2208988800UL;
      if (pacchetto[44] & 0x80) epoch++;     // arrotonda la frazione di secondo
      ok = epoch > 1767225600UL;             // scarta risposte vuote (prima del 2026)
      break;
    }
    delay(10);
  }
  udp.stop();
  return ok;
}

// Accende la radio e si collega alla rete salvata. Le credenziali non vengono
// scritte nella memoria flash della scheda (persistent false): restano solo
// nella EEPROM del modulo RTC.
bool collegaWifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(NOME_RETE);
  WiFi.begin(wifiCfg.ssid, wifiCfg.password[0] ? wifiCfg.password : nullptr);
  unsigned long inizio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inizio < WIFI_ATTESA_MS) delay(100);
  return WiFi.status() == WL_CONNECTED;
}

void spegniWifi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// Collega il wifi, chiede l'ora, la scrive nel DS3231 e spegne la radio.
// Blocca il programma per qualche secondo: il loop la chiama solo a motore
// fermo. Se il motore sta girando (comando 'n') le bobine si spengono finche'
// dura il collegamento, cosi' il picco della radio non si somma al motore.
bool sincronizzaOra() {
  bobineSpente();
  Serial.print(F("[NTP] Collegamento alla rete ")); Serial.print(wifiCfg.ssid); Serial.println(F("..."));
  bool ok = false;
  uint32_t epoch = 0;
  if (!collegaWifi()) {
    Serial.println(F("[NTP] Rete non raggiungibile o password errata."));
  } else if (!richiestaNtp(epoch)) {
    Serial.println(F("[NTP] Nessuna risposta dal server dell'ora."));
  } else if (!rtcPresente) {
    Serial.println(F("[NTP] Ora ricevuta, ma l'RTC non e' disponibile."));
  } else {
    DataOra prima, utc;
    bool primaOk = rtcLeggi(prima);
    dataDaSecondi(epoch, utc);
    if (!rtcScrivi(utc)) {
      Serial.println(F("[NTP] Scrittura dell'ora nell'RTC fallita."));
    } else {
      ok = true;
      utcInLocale(utc, oraUltimaSync);
      oraCache = oraUltimaSync;
      oraCacheValida = true;
      ultimaLetturaRtcMs = millis();
      Serial.print(F("[NTP] Ora sincronizzata: ")); stampaOra(oraUltimaSync);
      if (primaOk) {
        Serial.print(F("   (RTC corretto di "));
        Serial.print((long)(epoch - secondiDaData(prima)));
        Serial.print(F(" s)"));
      }
      Serial.println();
    }
  }
  spegniWifi();

  ultimoTentativoMs = millis();
  syncRiuscita = ok;
  if (ok) syncFatta = true;
  prossimaSyncMs = millis() + (ok ? NTP_INTERVALLO_H * 3600000UL : NTP_RIPROVA_MIN * 60000UL);
  // Il motore, se stava girando, riprende dal passo in cui si era fermato.
  winder.ultimoPassoUs = micros();
  if (winder.stato == WS_ORARIO || winder.stato == WS_ANTIORARIO)
    applicaPasso(winder.indicePasso);
  return ok;
}

// ============================================================================
//  CONFIGURAZIONE DA TELEFONO (rete wifi propria e pagina web)
// ============================================================================
//
// Con due RST ravvicinati, con FLASH tenuto 3 secondi o con il comando 'k' la scheda ferma il
// motore e crea la rete aperta RETE_CONFIG. Un server DNS risponde a ogni nome
// con l'indirizzo della scheda: cosi' il telefono, che cerca una sua pagina di
// prova su internet, riceve la nostra e la apre da solo, come nelle reti
// degli alberghi. La pagina non mostra mai la password salvata.

String testoHtml(const String &s) {
  String r;
  r.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if      (c == '&')  r += F("&amp;");
    else if (c == '<')  r += F("&lt;");
    else if (c == '>')  r += F("&gt;");
    else if (c == '"')  r += F("&quot;");
    else if (c == '\'') r += F("&#39;");
    else                r += c;
  }
  return r;
}

void apriPagina(String &p) {
  p.reserve(7000);
  p = F("<!DOCTYPE html><html lang=\"it\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Caricaorologi</title><style>"
        "body{font-family:-apple-system,Helvetica,Arial,sans-serif;margin:0 auto;max-width:480px;"
        "padding:16px;color:#222;background:#faf8f5}"
        "h1{font-size:1.4em;margin:.2em 0 .6em}h2{font-size:1.1em;margin:1.4em 0 .4em;color:#6b4f1d}"
        "label{display:block;margin:.6em 0 .2em}"
        "input,select{font-size:1em;padding:.45em;border:1px solid #bbb;border-radius:6px;box-sizing:border-box}"
        "input.l,select{width:100%}input.o{width:4.2em}"
        "button{font-size:1.05em;padding:.7em;border:0;border-radius:8px;background:#6b4f1d;color:#fff;"
        "width:100%;margin-top:1em}button.s{background:#8a8a8a}"
        ".nota{font-size:.85em;color:#666;margin:.3em 0}.err{color:#b00020}"
        ".stato{background:#fff;border:1px solid #ddd;border-radius:8px;padding:.6em .8em}"
        ".stato p{margin:.25em 0}a{color:#6b4f1d}"
        "</style></head><body><h1>Caricaorologi</h1>");
}

void inviaPagina(String &p) {
  p += F("</body></html>");
  server.send(200, "text/html; charset=utf-8", p);
}

void aggiungiNumero(String &p, const __FlashStringHelper *nome, int valore,
                    int minimo, int massimo, const __FlashStringHelper *classe) {
  p += F("<input class=\""); p += classe;
  p += F("\" name=\"");      p += nome;
  p += F("\" type=\"number\" inputmode=\"numeric\" min=\""); p += String(minimo);
  p += F("\" max=\"");       p += String(massimo);
  p += F("\" value=\"");     p += String(valore);
  p += F("\">");
}

void paginaPrincipale() {
  configAttivitaMs = millis();
  String p;
  apriPagina(p);

  DataOra t;
  bool oraOk = rtcPresente && leggiOra(t) && t.anno >= 2026;
  p += F("<div class=\"stato\"><p>Ora: <b>");
  if (oraOk) p += testoOra(t);
  else       p += F("non impostata");
  p += F("</b></p><p>Funzionamento: ");
  if (sospeso)     p += F("sospeso");
  else if (!oraOk) p += F("gira sempre, finché l'ora non è impostata");
  else             p += fasciaAttiva(t) ? F("nella fascia di lavoro") : F("a riposo, fuori fascia");
  p += F("</p><p>Rete wifi: ");
  if (wifiCfgValida) {
    p += testoHtml(wifiCfg.ssid);
    if (syncFatta) { p += F(", ora presa da internet il "); p += testoOra(oraUltimaSync); }
  } else {
    p += F("nessuna");
  }
  p += F("</p></div>");

  // Rete wifi
  p += F("<form method=\"post\" action=\"/salva\"><h2>Rete wifi</h2>"
         "<label>Nome della rete</label><input class=\"l\" name=\"ssid\" list=\"reti\" maxlength=\"32\" "
         "autocapitalize=\"off\" autocorrect=\"off\" spellcheck=\"false\" value=\"");
  if (wifiCfgValida) p += testoHtml(wifiCfg.ssid);
  p += F("\"><datalist id=\"reti\">");
  for (uint8_t i = 0; i < numRetiVicine; i++) {
    p += F("<option value=\""); p += testoHtml(retiVicine[i]); p += F("\">");
  }
  p += F("</datalist><label>Password</label><input class=\"l\" name=\"pw\" type=\"password\" "
         "maxlength=\"63\" autocomplete=\"off\">"
         "<p class=\"nota\">Password vuota: con lo stesso nome di rete resta quella salvata, con un "
         "nome nuovo vale come rete senza password. Nome vuoto: nessuna rete, l'ora la tiene "
         "l'orologio interno.</p>");

  // Orologio
  p += F("<h2>Orologio</h2><label>Giri al giorno</label>");
  aggiungiNumero(p, F("tpd"), cfg.tpd, 100, 2000, F("l"));
  p += F("<label>Verso di rotazione</label><select name=\"verso\">");
  static const char *const VERSI[3] = { "entrambi i versi", "solo orario", "solo antiorario" };
  for (uint8_t i = 0; i < 3; i++) {
    p += F("<option value=\""); p += String(i); p += '"';
    if (cfg.modoVerso == i) p += F(" selected");
    p += '>'; p += VERSI[i]; p += F("</option>");
  }
  p += F("</select>");

  // Orari
  p += F("<h2>Orari di lavoro</h2><p>Lunedì-venerdì dalle ");
  aggiungiNumero(p, F("i1"), cfg.oraInizio, 0, 23, F("o"));
  p += F(" alle ");
  aggiungiNumero(p, F("f1"), cfg.oraFine, 0, 23, F("o"));
  p += F("</p><p>Sabato e domenica dalle ");
  aggiungiNumero(p, F("i2"), cfg.oraInizioWe, 0, 23, F("o"));
  p += F(" alle ");
  aggiungiNumero(p, F("f2"), cfg.oraFineWe, 0, 23, F("o"));
  p += F("</p><p class=\"nota\">Ore intere. Inizio uguale alla fine: sempre attivo.</p>");

  // Motore
  p += F("<h2>Motore</h2><label>Microsecondi fra un passo e l'altro</label>");
  aggiungiNumero(p, F("v"), cfg.passoUs, 1000, 10000, F("l"));
  p += F("<p class=\"nota\">Più alto: più lento. Sotto 1200 il motore può perdere passi.</p>"
         "<button type=\"submit\">Salva e torna al funzionamento normale</button></form>");

  // Ora dal telefono
  p += F("<h2>Ora</h2><form method=\"post\" action=\"/ora\" onsubmit=\"var d=new Date();"
         "this.t.value=d.getFullYear()+'-'+(d.getMonth()+1)+'-'+d.getDate()+' '+"
         "d.getHours()+':'+d.getMinutes()+':'+d.getSeconds()\">"
         "<input type=\"hidden\" name=\"t\"><button type=\"submit\">Prendi l'ora dal telefono</button>"
         "</form><p class=\"nota\">Serve solo senza rete wifi: con la rete l'ora arriva da internet.</p>"
         "<form method=\"post\" action=\"/esci\"><button class=\"s\" type=\"submit\">Esci senza salvare"
         "</button></form><p class=\"nota\">Questa rete si spegne da sola dopo ");
  p += String(CONFIG_DURATA_MIN);
  p += F(" minuti senza attività.</p>");
  inviaPagina(p);
}

void paginaMessaggio(const String &messaggio, bool conRitorno) {
  String p;
  apriPagina(p);
  p += messaggio;
  if (conRitorno) p += F("<p><a href=\"/\">Torna alla pagina delle impostazioni</a></p>");
  inviaPagina(p);
}

int argNumero(const char *nome) {
  String s = server.arg(nome);
  s.trim();
  if (s.length() == 0) return -1;
  for (size_t i = 0; i < s.length(); i++) if (s[i] < '0' || s[i] > '9') return -1;
  return s.toInt();
}

void salvaDaTelefono() {
  configAttivitaMs = millis();
  int tpd   = argNumero("tpd");
  int verso = argNumero("verso");
  int passo = argNumero("v");
  int ore[4] = { argNumero("i1"), argNumero("f1"), argNumero("i2"), argNumero("f2") };
  String ssid = server.arg("ssid");
  ssid.trim();
  String pw = server.arg("pw");

  String errore;
  if (tpd < 100 || tpd > 2000)      errore += F("<p class=\"err\">Giri al giorno: da 100 a 2000.</p>");
  if (verso < 0 || verso > 2)       errore += F("<p class=\"err\">Verso di rotazione non valido.</p>");
  if (passo < 1000 || passo > 10000) errore += F("<p class=\"err\">Microsecondi: da 1000 a 10000.</p>");
  for (uint8_t i = 0; i < 4; i++)
    if (ore[i] < 0 || ore[i] > 23) { errore += F("<p class=\"err\">Orari: ore intere da 0 a 23.</p>"); break; }
  if (ssid.length() > 32 || pw.length() > 63)
    errore += F("<p class=\"err\">Nome della rete al massimo 32 caratteri, password al massimo 63.</p>");
  if (!eepromPresente)
    errore += F("<p class=\"err\">Memoria del modulo RTC non trovata: impossibile salvare.</p>");
  if (errore.length()) { paginaMessaggio(errore, true); return; }

  cfg.tpd         = tpd;
  cfg.modoVerso   = verso;
  cfg.passoUs     = passo;
  cfg.oraInizio   = ore[0]; cfg.oraFine   = ore[1];
  cfg.oraInizioWe = ore[2]; cfg.oraFineWe = ore[3];
  bool ok = configSalva();
  ricalcolaPiano();

  bool reteCambiata = false;
  if (ssid.length() == 0) {
    if (wifiCfgValida) {
      memset(&wifiCfg, 0, sizeof(wifiCfg));
      wifiCfgValida = false;
      ok = wifiSalva() && ok;
      Serial.println(F("[CONFIG] Rete wifi cancellata."));
    }
  } else if (!(wifiCfgValida && ssid == wifiCfg.ssid && pw.length() == 0)) {
    memset(&wifiCfg, 0, sizeof(wifiCfg));
    wifiCfg.magic = WIFI_MAGIC;
    strncpy(wifiCfg.ssid, ssid.c_str(), sizeof(wifiCfg.ssid) - 1);
    strncpy(wifiCfg.password, pw.c_str(), sizeof(wifiCfg.password) - 1);
    wifiCfgValida = wifiSalva();
    ok = wifiCfgValida && ok;
    reteCambiata = wifiCfgValida;
    Serial.print(F("[CONFIG] Rete wifi salvata: ")); Serial.println(wifiCfg.ssid);
  }
  Serial.println(ok ? F("[CONFIG] Impostazioni salvate dal telefono.")
                    : F("[CONFIG] Salvataggio dal telefono non riuscito."));

  String m;
  if (ok) {
    m = F("<p><b>Impostazioni salvate.</b></p>");
    if (reteCambiata) {
      m += F("<p>All'uscita il caricatore si collega alla rete ");
      m += testoHtml(wifiCfg.ssid);
      m += F(" per prendere l'ora da internet.</p>");
    }
    m += F("<p>Fra pochi secondi la rete " RETE_CONFIG " si spegne e il caricatore torna al "
           "funzionamento normale. Il telefono si ricollega da solo alla sua rete.</p>");
    syncDopoConfig = reteCambiata;
    uscitaProgrammata = true;
    uscitaMs = millis() + 3000UL;
  } else {
    m = F("<p class=\"err\">Salvataggio non riuscito: riprovare.</p>");
  }
  paginaMessaggio(m, !ok);
}

void oraDaTelefono() {
  configAttivitaMs = millis();
  DataOra t;
  bool ok = dataOraDaTesto(server.arg("t").c_str(), t) && rtcPresente && scriviOra(t);
  if (ok) {
    oraCacheValida = leggiOra(oraCache);
    ultimaLetturaRtcMs = millis();
    ricalcolaPiano();
    Serial.print(F("[CONFIG] Ora presa dal telefono: ")); stampaOra(t); Serial.println();
    paginaMessaggio(String(F("<p><b>Ora impostata:</b> ")) + testoOra(t) + F("</p>"), true);
  } else {
    paginaMessaggio(F("<p class=\"err\">Impostazione dell'ora non riuscita.</p>"), true);
  }
}

void esciDaTelefono() {
  paginaMessaggio(F("<p>Uscita senza salvare. Fra pochi secondi la rete " RETE_CONFIG
                    " si spegne e il caricatore torna al funzionamento normale.</p>"), false);
  uscitaProgrammata = true;
  uscitaMs = millis() + 2000UL;
}

// Ogni altro indirizzo (comprese le pagine di prova di iPhone e Android)
// rimanda alla pagina principale: e' cosi' che il telefono la apre da solo.
void reindirizza() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void avviaConfigurazione() {
  if (modoConfig) return;
  fermaMotore();
  Serial.println(F("[CONFIG] Avvio della configurazione da telefono..."));
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);

  // Elenco delle reti vicine, proposto nel campo del nome della rete.
  numRetiVicine = 0;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && numRetiVicine < MAX_RETI_VICINE; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    bool doppia = false;
    for (uint8_t j = 0; j < numRetiVicine; j++) if (retiVicine[j] == s) doppia = true;
    if (!doppia) retiVicine[numRetiVicine++] = s;
  }
  WiFi.scanDelete();

  WiFi.softAPConfig(IP_CONFIG, IP_CONFIG, IPAddress(255, 255, 255, 0));
  WiFi.softAP(RETE_CONFIG);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", IP_CONFIG);
  server.begin();

  modoConfig        = true;
  uscitaProgrammata = false;
  syncDopoConfig    = false;
  configAttivitaMs  = millis();
  Serial.print(F("[CONFIG] Rete \"" RETE_CONFIG "\" attiva, pagina http://192.168.4.1 ("));
  Serial.print(numRetiVicine);
  Serial.println(F(" reti vicine). Si chiude dopo il salvataggio, dopo 10 minuti o con 'k'."));
}

void terminaConfigurazione() {
  if (!modoConfig) return;
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  spegniWifi();
  modoConfig = false;
  uscitaProgrammata = false;
  statoLed = -1;                             // il loop riscrive il LED
  Serial.println(F("[CONFIG] Configurazione chiusa, funzionamento normale."));
  if (syncDopoConfig && wifiCfgValida) prossimaSyncMs = millis();
  syncDopoConfig = false;
  riavviaCiclo();
}

void gestisciConfigurazione() {
  dns.processNextRequest();
  server.handleClient();
  if (millis() - ultimoLampeggioMs >= 300UL) {
    ultimoLampeggioMs = millis();
    statoLed = !statoLed;
    impostaLed(statoLed);
  }
  if (uscitaProgrammata) {
    if ((long)(millis() - uscitaMs) >= 0) terminaConfigurazione();
  } else if (millis() - configAttivitaMs >= CONFIG_DURATA_MIN * 60000UL) {
    Serial.println(F("[CONFIG] Nessuna attivita' per 10 minuti."));
    terminaConfigurazione();
  }
}

// Doppio RST: al primo riavvio da pulsante si scrive un segnale nella memoria
// RTC dell'ESP8266 (resiste al RST, non allo spegnimento); se entro
// DOPPIO_RST_MS arriva un secondo RST, all'avvio il segnale c'e' ancora e si
// apre la configurazione da telefono. Chiamata all'inizio di setup().
bool rilevaDoppioRst() {
  uint32_t segnale = 0;
  ESP.rtcUserMemoryRead(DOPPIO_RST_BLOCCO, &segnale, sizeof(segnale));
  rst_info *motivo = ESP.getResetInfoPtr();
  bool daPulsante = motivo && motivo->reason == REASON_EXT_SYS_RST;
  if (daPulsante && segnale == DOPPIO_RST_MAGIC) {
    segnale = 0;
    ESP.rtcUserMemoryWrite(DOPPIO_RST_BLOCCO, &segnale, sizeof(segnale));
    return true;
  }
  segnale = DOPPIO_RST_MAGIC;
  ESP.rtcUserMemoryWrite(DOPPIO_RST_BLOCCO, &segnale, sizeof(segnale));
  doppioRstInAttesa = true;
  return false;
}

// Passata la finestra, il segnale si cancella: un RST isolato riavvia e basta.
void chiudiFinestraDoppioRst() {
  if (!doppioRstInAttesa || millis() < DOPPIO_RST_MS) return;
  uint32_t segnale = 0;
  ESP.rtcUserMemoryWrite(DOPPIO_RST_BLOCCO, &segnale, sizeof(segnale));
  doppioRstInAttesa = false;
}

// FLASH tenuto premuto PRESSIONE_CONFIG_MS apre la configurazione o, se e'
// gia' aperta, la chiude. Prima di un nuovo comando va rilasciato.
void controllaPulsante() {
  bool premuto = digitalRead(PIN_FLASH) == LOW;
  if (!premuto) { flashInizioMs = 0; flashAttesaRilascio = false; return; }
  if (flashAttesaRilascio) return;
  if (flashInizioMs == 0) {
    flashInizioMs = millis() | 1UL;
    Serial.println(F("[FLASH] Pulsante premuto: tenerlo per 3 s."));
    return;
  }
  if (millis() - flashInizioMs >= PRESSIONE_CONFIG_MS) {
    flashAttesaRilascio = true;
    if (modoConfig) terminaConfigurazione();
    else            avviaConfigurazione();
  }
}

// ============================================================================
//  COMANDI DA MONITOR SERIALE
// ============================================================================

void avviaTest(uint8_t giri, bool soloOrario) {
  if (modoConfig) { Serial.println(F("Prima chiudere la configurazione da telefono ('k').")); return; }
  fermaMotore();
  long passi = giri * passiPerGiroOrologio();
  winder.passiFaseCw  = passi;
  winder.passiFaseCcw = soloOrario ? 0 : passi;
  Serial.print(F("[TEST] ")); Serial.print(giri);
  Serial.println(soloOrario ? F(" giri in senso orario.") : F(" giro orario + 1 giro antiorario."));
  avviaSessione(true);
}

void riavviaCiclo() {
  ricalcolaPiano();
  cicloInizioMs = millis();
  winder.partenzaArmata = true;
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
      DataOra utc, t;
      if (rtcPresente && rtcLeggi(utc)) {
        utcInLocale(utc, t);
        Serial.print(F("RTC: ")); stampaOra(t);
        Serial.print(oraLegale(utc) ? F(" ora legale") : F(" ora solare"));
        Serial.print(F("   temperatura ")); Serial.print(rtcTemperatura(), 2);
        Serial.println(F(" C"));
        if (rtcOrologioFermo())
          Serial.println(F("  ATTENZIONE: l'oscillatore si e' fermato, l'ora non e' attendibile. Reimpostarla."));
      } else {
        Serial.println(F("RTC non raggiungibile sul bus I2C (D1/D2)."));
      }
      break;
    }

    case 'T': {
      DataOra t;
      if (resto.length() > 0 && resto.charAt(0) == 'c') {
        // Ricava data e ora dai macro del compilatore
        const char *md = __DATE__;   // "Nov  6 2026"
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
      } else if (!dataOraDaTesto(resto.c_str(), t)) {
        // formato atteso: T aaaa-mm-gg hh:mm:ss
        Serial.println(F("Data/ora non valida. Esempio: T 2026-11-06 14:35:00"));
        break;
      }
      if (scriviOra(t)) { Serial.print(F("RTC impostato a: ")); stampaOra(t); Serial.println(); }
      else                Serial.println(F("Scrittura RTC fallita."));
      break;
    }

    case 'm':
      avviaTest(1, false);
      break;

    case 'd':
      avviaTest(3, true);
      break;

    case 'x':
      sospeso = true; forzaAttivo = false;
      fermaMotore();
      Serial.println(F("Sistema sospeso. Usare 'a' per riprendere."));
      break;

    case 'a':
      sospeso = false; forzaAttivo = false;
      riavviaCiclo();
      Serial.println(F("Funzionamento automatico ripristinato."));
      break;

    case 'f':
      sospeso = false; forzaAttivo = true;
      riavviaCiclo();
      Serial.println(F("Attivita' forzata: la fascia oraria viene ignorata."));
      break;

    case 'h': {
      int v[4];
      int n = sscanf(resto.c_str(), "%d %d %d %d", &v[0], &v[1], &v[2], &v[3]);
      bool ok = (n == 2 || n == 4);
      for (int i = 0; ok && i < n; i++) if (v[i] < 0 || v[i] > 23) ok = false;
      if (ok) {
        // Con due soli valori la stessa fascia vale per tutti i giorni.
        if (n == 2) { v[2] = v[0]; v[3] = v[1]; }
        cfg.oraInizio   = v[0]; cfg.oraFine   = v[1];
        cfg.oraInizioWe = v[2]; cfg.oraFineWe = v[3];
        ricalcolaPiano();
        Serial.print(F("Fascia lun-ven: ")); stampaFascia(cfg.oraInizio, cfg.oraFine);
        Serial.print(F("Fascia sab-dom: ")); stampaFascia(cfg.oraInizioWe, cfg.oraFineWe);
      } else {
        Serial.println(F("Uso: h <ora_inizio> <ora_fine>   esempio: h 9 20"));
        Serial.println(F("     h <ini_lun_ven> <fin_lun_ven> <ini_sab_dom> <fin_sab_dom>   esempio: h 8 21 10 21"));
      }
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
      if (resto.length() > 0 && v >= 0 && v <= 2) {
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

    case 'i': {
      if (resto == "0" || resto == "1") {
        cfg.inverti = (resto == "1");
        Serial.println(cfg.inverti ? F("Motore: verso invertito") : F("Motore: verso normale"));
      } else Serial.println(F("Uso: i 0 | i 1"));
      break;
    }

    case 'n': {
      if (resto == "-") {
        memset(&wifiCfg, 0, sizeof(wifiCfg));
        wifiCfgValida = false;
        if (eepromPresente) wifiSalva();
        Serial.println(F("Rete wifi cancellata dalla EEPROM."));
        break;
      }
      if (resto.length() > 0) {
        // Nome della rete fino al primo spazio, poi la password, che non
        // viene mai ristampata.
        int spazio  = resto.indexOf(' ');
        String rete = (spazio < 0) ? resto : resto.substring(0, spazio);
        String pw   = (spazio < 0) ? String("") : resto.substring(spazio + 1);
        if (rete.length() > 32 || pw.length() > 63) {
          Serial.println(F("Nome della rete al massimo 32 caratteri, password al massimo 63."));
          break;
        }
        if (!eepromPresente) { Serial.println(F("EEPROM non rilevata: impossibile salvare.")); break; }
        memset(&wifiCfg, 0, sizeof(wifiCfg));
        wifiCfg.magic = WIFI_MAGIC;
        strncpy(wifiCfg.ssid, rete.c_str(), sizeof(wifiCfg.ssid) - 1);
        strncpy(wifiCfg.password, pw.c_str(), sizeof(wifiCfg.password) - 1);
        wifiCfgValida = wifiSalva();
        if (!wifiCfgValida) { Serial.println(F("Salvataggio della rete wifi fallito.")); break; }
        Serial.print(F("Rete wifi salvata in EEPROM: ")); Serial.println(wifiCfg.ssid);
      }
      if (!wifiCfgValida) { Serial.println(F("Rete wifi non impostata. Uso: n <rete> <password>")); break; }
      if (modoConfig) { Serial.println(F("Sincronizzazione all'uscita dalla configurazione da telefono.")); syncDopoConfig = true; break; }
      sincronizzaOra();
      break;
    }

    case 'k':
      if (modoConfig) terminaConfigurazione();
      else            avviaConfigurazione();
      break;

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
    } else if (bufferSeriale.length() < 120) {   // spazio per 'n <rete> <password>'
      bufferSeriale += ch;
    }
  }
}

// ============================================================================
//  SETUP
// ============================================================================

void setup() {
  // Pin del motore in uscita e a livello basso: le bobine devono restare
  // diseccitate finche' non serve muovere.
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(PIN_MOTORE[i], OUTPUT);
    digitalWrite(PIN_MOTORE[i], LOW);
  }
  winder.stato          = WS_FERMO;
  winder.indicePasso    = 0;
  winder.passiEseguiti  = 0;
  winder.partenzaArmata = true;
  winder.test           = false;

  // Subito, perche' il secondo RST puo' arrivare dopo meno di un secondo.
  bool configAllAvvio = rilevaDoppioRst();

  pinMode(LED_BUILTIN, OUTPUT);
  impostaLed(false);
  pinMode(PIN_FLASH, INPUT_PULLUP);

  // Radio spenta: si accende solo per sincronizzare l'ora o per la
  // configurazione da telefono.
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);

  // Pagine della configurazione da telefono (il server parte solo con il
  // doppio RST, con il pulsante FLASH o con 'k').
  server.on("/", HTTP_GET, paginaPrincipale);
  server.on("/salva", HTTP_POST, salvaDaTelefono);
  server.on("/ora", HTTP_POST, oraDaTelefono);
  server.on("/esci", HTTP_POST, esciDaTelefono);
  server.onNotFound(reindirizza);

  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println(F("============================================"));
  Serial.println(F("  WATCH WINDER 1 POSTO - NodeMCU V3 (ESP8266)"));
  Serial.println(F("============================================"));

  // Bus I2C su D2 (SDA) e D1 (SCL)
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);

  // Rilevamento dei dispositivi I2C
  Wire.beginTransmission(ADDR_DS3231);
  rtcPresente = (Wire.endTransmission() == 0);
  Wire.beginTransmission(ADDR_EEPROM);
  eepromPresente = (Wire.endTransmission() == 0);

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

  wifiCfgValida = eepromPresente && wifiCarica();
  Serial.print(F("Rete wifi ............ "));
  if (wifiCfgValida) Serial.println(wifiCfg.ssid);
  else               Serial.println(F("non impostata (comando n)"));

  if (rtcPresente && rtcOrologioFermo()) {
    Serial.println();
    Serial.println(F("  *** L'RTC segnala che l'oscillatore si e' fermato. ***"));
    Serial.println(F("  *** L'ora non e' attendibile: la prende da internet (n) o con T. ***"));
    Serial.println();
  }

  // Prima lettura dell'ora gia' qui: senza, nel primo secondo del loop l'ora
  // risulterebbe non valida e il motore partirebbe per ripiego anche di notte.
  if (rtcPresente) {
    oraCacheValida = leggiOra(oraCache);
    ultimaLetturaRtcMs = millis();
  }

  ricalcolaPiano();
  cicloInizioMs = millis();

  stampaStato();
  stampaAiuto();

  if (configAllAvvio) {
    Serial.println(F("[CONFIG] Doppio RST: apertura della configurazione da telefono."));
    avviaConfigurazione();
  }
}

// ============================================================================
//  LOOP
// ============================================================================

void loop() {
  leggiSeriale();
  controllaPulsante();
  chiudiFinestraDoppioRst();

  // Configurazione da telefono: motore fermo, si servono solo le pagine.
  if (modoConfig) {
    gestisciConfigurazione();
    return;
  }

  // Se il motore sta girando per un test, lo lasciamo lavorare senza
  // interferenze dalla logica dei cicli.
  if (winder.test) {
    aggiornaWinder();
    return;
  }

  // Ora da internet: alla prima occasione dopo l'accensione e poi ogni
  // NTP_INTERVALLO_H ore, solo a motore fermo perche' il collegamento al wifi
  // blocca il programma per qualche secondo.
  if (wifiCfgValida && winder.stato == WS_FERMO && (long)(millis() - prossimaSyncMs) >= 0)
    sincronizzaOra();

  if (sospeso) {
    if (statoLed != 0) { statoLed = 0; impostaLed(false); }
    return;
  }

  // Se all'accensione l'RTC non ha risposto lo si cerca di nuovo ogni
  // 5 secondi, invece di restare per sempre nel ripiego "sempre attivo".
  if (!rtcPresente && (millis() - ultimaRicercaRtcMs) >= 5000UL) {
    ultimaRicercaRtcMs = millis();
    Wire.beginTransmission(ADDR_DS3231);
    rtcPresente = (Wire.endTransmission() == 0);
    if (rtcPresente) {
      Serial.println(F("RTC DS3231 (0x68) trovato."));
      if (rtcOrologioFermo())
        Serial.println(F("  *** L'oscillatore si e' fermato: l'ora va ripresa (n o T). ***"));
      ultimaLetturaRtcMs = millis() - 1000UL;   // lettura dell'ora subito
    }
  }

  // Stato della fascia oraria: l'RTC viene interrogato una volta al secondo
  // e il risultato tenuto in cache, per non rubare tempo al passo del motore.
  if (rtcPresente && (millis() - ultimaLetturaRtcMs) >= 1000UL) {
    ultimaLetturaRtcMs = millis();
    oraCacheValida = leggiOra(oraCache);
  }
  bool oraValida = oraAttendibile();
  // RTC azzerato: si lavora per ripiego e si riprova a prendere l'ora da
  // internet ogni 2 minuti invece che ogni NTP_RIPROVA_MIN.
  if (oraCacheValida && !oraValida) {
    if (!avvisoRtcAzzerato) {
      avvisoRtcAzzerato = true;
      Serial.println(F("RTC azzerato (data del 2000): ora non attendibile, motore per ripiego."));
    }
    unsigned long entro = ultimoTentativoMs + 120000UL;
    if ((long)(prossimaSyncMs - entro) > 0) prossimaSyncMs = entro;
  } else {
    avvisoRtcAzzerato = false;
  }
  // A mezzanotte fra venerdi' e sabato e fra domenica e lunedi' cambia la
  // fascia, e con essa la quota di giri di ogni ciclo.
  if (oraValida && fineSettimana(oraCache) != pianoFineSettimana) ricalcolaPiano();
  // Se l'RTC non risponde, per prudenza si continua a caricare l'orologio.
  bool attivo = forzaAttivo || !oraValida || fasciaAttiva(oraCache);

  int8_t ledVoluto = attivo ? 1 : 0;
  if (ledVoluto != statoLed) {
    statoLed = ledVoluto;
    impostaLed(statoLed);
  }

  // Log di cortesia a ogni cambio d'ora
  if (oraValida && oraCache.ora != ultimaOraLog && millis() - ultimoLogMs > 2000) {
    ultimaOraLog = oraCache.ora;
    ultimoLogMs  = millis();
    Serial.print(F("[")); stampaOra(oraCache); Serial.print(F("] "));
    Serial.println(attivo ? F("fascia attiva - il motore lavora")
                          : F("fascia notturna - motore fermo"));
  }

  if (!attivo) {
    if (winder.stato != WS_FERMO) { winder.stato = WS_FERMO; bobineSpente(); }
    winder.partenzaArmata = true;
    return;
  }

  // Gestione del ciclo. Dopo la notte (o una sospensione) possono essere
  // passati molti cicli: si recuperano in un colpo solo, altrimenti la
  // partenza resterebbe armata e la sessione si ripeterebbe appena finita.
  unsigned long trascorso = millis() - cicloInizioMs;
  if (trascorso >= cicloDurataMs) {
    cicloInizioMs += (trascorso / cicloDurataMs) * cicloDurataMs;
    winder.partenzaArmata = true;
  }

  // Avvio della sessione all'inizio di ogni ciclo
  if (winder.partenzaArmata && winder.stato == WS_FERMO) {
    winder.partenzaArmata = false;
    avviaSessione(false);
  }

  aggiornaWinder();
}

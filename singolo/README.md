# Caricaorologi a 1 posto

Versione a un solo orologio. La meccanica è il progetto **Single Watch
Winder** di **Robbie Kieffer**, remix a un posto dell'**Arduino Double Watch
Winder** di **JayS** usato anche per il caricatore a 2 posti. L'elettronica è
diversa dagli altri due caricatori: una **NodeMCU V3** di AZ-Delivery (modulo
Wi-Fi ESP8266), un orologio in tempo reale (RTC) DS3231 e un solo motore
passo-passo 28BYJ-48 con scheda driver ULN2003.

Lavoro in corso: collegamenti e firmware sono stati provati sul banco il
6 novembre 2026; la scatola è progettata ma non ancora provata.

## Progetti originali

- **Single Watch Winder** di Robbie Kieffer, pubblicato su Printables l'8
  maggio 2025: https://www.printables.com/model/1290039-single-watch-winder.
  Robbie ha rifatto la scatola, il coperchio inferiore e l'anello esterno per
  un solo orologio; le altre parti sono quelle di JayS.
- **Arduino Double Watch Winder** di JayS (https://www.printables.com/@JayS):
  https://www.printables.com/model/1227833-arduino-double-watch-winder.

Entrambi sono sotto licenza Creative Commons Attribuzione - Non commerciale -
Condividi allo stesso modo 4.0 Internazionale (CC BY-NC-SA 4.0),
https://creativecommons.org/licenses/by-nc-sa/4.0/deed.it

## Licenze dei file di questa cartella

**CC BY-NC-SA 4.0 (Robbie Kieffer e JayS)** — tutti i file di
`single-watch-winder-model_files/`, compresa la scatola modificata e il suo
sorgente OpenSCAD. Per questi file:

- bisogna sempre citare Robbie Kieffer, JayS e i link ai progetti originali;
- non si possono vendere né il progetto né le sue modifiche;
- ogni modifica va distribuita con la stessa licenza CC BY-NC-SA 4.0.

**MIT (Carlo Comincini)** — il firmware `watchwinder_1posto/`, lo schema
elettrico `schema-elettrico-1-posto.svg` e questo README. Il firmware non
deriva dal codice di JayS ma da quello del caricatore a 2 posti di Carlo.

## Collegamenti

Schema completo: [schema-elettrico-1-posto.svg](schema-elettrico-1-posto.svg).

- **Modulo RTC DS3231** (ZS-042)
  - SCL su D1 (GPIO5), SDA su D2 (GPIO4)
  - VCC sul 3V della NodeMCU, **mai sul 5 V**; GND su G
  - 32K e SQW non collegati; batteria tampone CR2032 comune, non ricaricabile
- **Scheda driver ULN2003**
  - IN1 su D5 (GPIO14), IN2 su D6 (GPIO12), IN3 su D7 (GPIO13), IN4 su D8
    (GPIO15)
  - + su VU, cioè i 5 V che arrivano dal connettore USB; - su G
  - condensatore elettrolitico da 470-1000 µF, 16 V, fra + e - vicino al
    driver, con la banda del negativo verso G
- **Motore 28BYJ-48** nel connettore bianco del driver
- **Alimentazione** solo dal micro USB della NodeMCU, con un caricatore da
  5 V e almeno 1 A. Non collegare mai un secondo alimentatore su VU con il
  cavo USB inserito.
- **LED di stato**: quello già montato sulla NodeMCU (D4), acceso quando il
  caricatore è nella fascia oraria di lavoro, lampeggiante durante la
  configurazione dal telefono.

D8 deve essere a livello basso all'accensione, altrimenti la scheda non
parte: lo tengono basso la resistenza della NodeMCU e l'ingresso
dell'ULN2003, ed è stato verificato. Se una scheda diversa non si avviasse,
spostare IN4 su D0 (GPIO16) e cambiare il pin nel firmware. Non usare D3 e
D4, che all'accensione devono restare alti.

I piedini della NodeMCU sono saldati verso il basso: i fili si saldano
sulle loro punte, più sicure dei connettori Dupont contro le vibrazioni.

## Firmware

Sketch: `watchwinder_1posto/watchwinder_1posto.ino`, nessuna libreria
esterna. Nell'Arduino IDE serve il pacchetto per ESP8266
(`https://arduino.esp8266.com/stable/package_esp8266com_index.json`, versione
3.1.2) e la scheda **NodeMCU 1.0 (ESP-12E Module)**. Da riga di comando,
dalla cartella `singolo/`:

```
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 watchwinder_1posto
```

```
arduino-cli upload --fqbn esp8266:esp8266:nodemcuv2 -p /dev/cu.usbserial-110 watchwinder_1posto
```

La NodeMCU ha un chip USB CH340G, riconosciuto da macOS senza driver; il
nome della porta può cambiare.

Funziona come il caricatore a 2 posti, con un solo motore:

- lavora dalle 08:00 alle 21:00 dal lunedì al venerdì e dalle 10:00 alle
  21:00 il sabato e la domenica, con gli stessi giri al giorno in tutti e due
  i casi;
- ogni 15 minuti fa la sua quota di giri, metà in senso orario e metà in
  senso antiorario, poi si ferma con le bobine spente;
- di fabbrica 800 giri al giorno in entrambi i versi, pensati per
  l'orologio a cui è destinato: un Longines automatico da donna degli anni
  '80, di cui non si conoscono modello e calibro. I movimenti automatici da
  donna più diffusi (per esempio l'ETA 2671, base del Longines L561)
  chiedono circa 650 giri al giorno in entrambi i versi, e Longines indica
  650-800 per i suoi automatici. Se l'orologio si fermasse pur stando sul
  caricatore, probabilmente si carica in un verso solo: in quel caso
  portare i giri a 1300-1600 con `p`;
- il modulo RTC tiene l'ora UTC e il firmware la converte in ora italiana,
  con il passaggio automatico fra ora solare e ora legale, anche senza rete;
- prende l'ora da internet via wifi all'accensione e poi ogni 12 ore,
  sempre a motore fermo; fra una sincronizzazione e l'altra la radio è
  spenta. Se l'RTC ha perso l'ora (data del 2000) il motore lavora comunque
  e la sincronizzazione si ritenta ogni 2 minuti.

Configurazione dal monitor seriale a 115200 baud (`?` mostra tutti i
comandi; `w` salva nella memoria EEPROM del modulo RTC):

- `n <rete> <password>`: salva la rete wifi nella EEPROM del modulo RTC e
  sincronizza subito l'ora. Nome e password non stanno mai nel codice, che
  è pubblico. `n -` cancella la rete salvata;
- `h 8 21 10 21`: fasce orarie, prima dal lunedì al venerdì, poi sabato e
  domenica; con due numeri soli vale la stessa fascia per tutti i giorni;
- `p <giri>` giri al giorno, `r 0|1|2` verso (entrambi, solo orario, solo
  antiorario), `i 0|1` inverte il verso del motore;
- `v <microsecondi>` tempo fra un passo e l'altro, di fabbrica 1200 (circa
  4,9 secondi per giro). Il rumore dipende dalle vibrazioni della plastica
  più che dalla velocità: con l'orologio montato 1200 è risultato il valore
  più silenzioso fra quelli provati (da 1200 a 4000). Sotto 1200 il motore
  rischia di perdere passi;
- `m` un giro per verso, `d` tre giri in senso orario, `s` stato,
  `t` ora e temperatura dell'RTC, `k` apre o chiude la configurazione dal
  telefono.

### Configurazione dal telefono

Senza cavo e senza computer, per esempio per impostare la rete wifi di chi
riceve il caricatore:

1. con il caricatore acceso, premere **RST due volte a circa un secondo di
   distanza** (dal foro nel piano superiore, con uno stuzzicadenti). Il
   motore si ferma e il LED lampeggia;
2. sul telefono scegliere la rete wifi **Watchwinder** (aperta, senza
   password). La pagina del caricatore si apre da sola, come nelle reti
   degli alberghi; se non si apre, andare con il browser su `192.168.4.1`;
3. nella pagina si vedono ora e stato, e si possono impostare la rete wifi
   (con l'elenco delle reti vicine), i giri al giorno, il verso, le fasce
   orarie e la velocità. Il pulsante "Prendi l'ora dal telefono" serve se
   non c'è rete wifi;
4. con "Salva" o "Esci senza salvare" la rete Watchwinder si spegne e il
   caricatore torna al funzionamento normale. Si spegne da sola anche dopo
   10 minuti senza attività.

La pagina non mostra mai la password salvata. Un RST singolo riavvia e
basta. Il segnale fra i due RST sta nella memoria interna dell'ESP8266, che
resiste al riavvio da pulsante ma non allo spegnimento.

Collegata a un computer, la scheda si riavvia anche quando si apre o si
chiude il monitor seriale; due riavvii ravvicinati, per esempio subito dopo
il caricamento del firmware, aprono la configurazione come un doppio RST.
Non succede niente di male: si chiude con `k` o da sola dopo 10 minuti.
Con un caricatore USB questo non accade.

Dal telefono non si può usare il cavo USB: iOS non permette alle app di
comunicare con chip USB-seriale come il CH340 della NodeMCU.

Il firmware accetta anche il pulsante FLASH tenuto premuto per 3 secondi,
ma sulla NodeMCU usata per questo caricatore quel pulsante non fa contatto
(provato il 6 novembre 2026): per questo si usa il doppio RST.

## File

I file 3D sono tutti in `single-watch-winder-model_files/`.

| File | Origine | Contenuto |
|------|---------|-----------|
| `watchwinder_single_body_nodemcu.stl` | modifica di Robbie Kieffer | Scatola da stampare, con gli attacchi per la NodeMCU |
| `watchwinder_single_body_nodemcu.scad` | modifica di Robbie Kieffer | Sorgente OpenSCAD della scatola modificata |
| `watchwinder_single_body.stl` | Robbie Kieffer | Scatola originale, non in uso |
| `watchwinder_single_base.stl` | Robbie Kieffer | Coperchio inferiore, fissato con 6 viti |
| `watchwinder_singlering.stl` | Robbie Kieffer | Anello esterno per un solo orologio |
| `Ring_*`, `Beweger_R02.stl`, `Uhrträger_*` | JayS | Anelli e porta-orologio, con i nomi originali in tedesco |
| `Sockel_links_oben_R01.stl`, `Sockel_links_unten_R01.stl` | JayS | Supporti dell'anello, alto e basso |
| `1290039-single-watch-winder-….pdf` | Printables | Copia della pagina del modello di Robbie |

Le parti di JayS sono estratte, già orientate per la stampa, dal progetto
`../2_orologi/uhrenbeweger.3mf`; per un posto ne basta una serie.

## Scatola modificata

La scatola di Robbie è pensata per una presa di alimentazione, un interruttore
e un Arduino Pro Micro senza attacchi. Quella modificata ospita la NodeMCU e
si alimenta solo dal suo micro USB, con un caricatore da 5 V e almeno 1 A.

Modifiche rispetto all'originale:

1. chiusi il foro dell'interruttore sul retro e quello della presa sul davanti;
2. tolto il supporto interno dell'interruttore;
3. NodeMCU ruotata di 32 gradi nella parte destra, con i componenti verso il
   piano superiore: il lato del micro USB si infila in una fessura nella
   parete curva, con una finestra di 8,6 x 3,6 mm per la presa e un incavo
   esterno di 12 x 8 mm per la plastica della spina; il lato dell'antenna
   poggia su due appoggi a vite. Due fori diritti da 3 mm nel piano
   superiore permettono di premere i pulsanti RST e FLASH dall'esterno, con
   uno stuzzicadenti;
4. fori degli appoggi della scheda ULN2003 ridotti da 3,2 a 2,8 mm, per
   avvitare le M3 direttamente nella plastica;
5. aggiunto il foro del supporto alto, che mancava, e le sedi dei dadi M3 per
   i due supporti, come nel caricatore a 2 posti;
6. quattro appoggi per il modulo RTC DS3231: tre sotto i suoi fori e uno
   all'angolo che non ha il foro. Tengono il modulo 7 mm sopra il piano
   superiore, per lasciare aria e lo spazio per i connettori dei cavetti.

Montaggio della NodeMCU: si infila il lato del micro USB nella fessura
tenendo la scheda un poco inclinata, poi si abbassa il lato dell'antenna
sugli appoggi e si avvita. Il modulo RTC si monta con la batteria verso il
coperchio inferiore, così la si cambia aprendo la scatola.

Gli appoggi a vite della NodeMCU e del modulo RTC hanno uno smusso a 45
gradi alla base e un foro da 2,8 mm svasato all'imbocco. Quelli del modulo
RTC hanno un diametro di 6 mm; quelli della NodeMCU di 5 mm, perché sul
suo lato componenti non c'è spazio per una cima più larga.

Il coperchio inferiore originale si monta senza modifiche: colonne e fori
delle sue viti sono rimasti com'erano.

Viti:

- NodeMCU: due M3x8 sul lato dell'antenna, avvitate nella plastica;
- modulo RTC: due M3x8 nei fori in diagonale; il terzo foro ha l'appoggio
  forato, la vite è facoltativa. I fori del modulo misurano circa 2,8 mm:
  vanno allargati con una punta da 3,2 mm, perché la vite deve passare
  libera nel circuito e fare presa solo nell'appoggio;
- ULN2003: M3x8, avvitate nella plastica;
- supporti: M3x6 con dado M3. Una M3x8 nel supporto basso toccherebbe il
  motore.

La scatola si stampa capovolta, con il piano superiore sul piatto, come
l'originale. Per rigenerare lo STL dopo una modifica al sorgente, dalla
cartella `single-watch-winder-model_files/`:

```
openscad --backend=manifold -o watchwinder_single_body_nodemcu.stl watchwinder_single_body_nodemcu.scad
```

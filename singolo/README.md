# Caricaorologi a 1 posto

Versione a un solo orologio. La meccanica è il progetto **Single Watch
Winder** di **Robbie Kieffer**, remix a un posto dell'**Arduino Double Watch
Winder** di **JayS** usato anche per il caricatore a 2 posti. L'elettronica è
diversa dagli altri due caricatori: una **NodeMCU V3** di AZ-Delivery (modulo
Wi-Fi ESP8266), un orologio in tempo reale (RTC) DS3231 e un solo motore
passo-passo 28BYJ-48 con scheda driver ULN2003.

Lavoro in corso: la scatola è progettata ma non ancora provata, il firmware è
in preparazione.

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

**MIT (Carlo Comincini)** — questo README.

## File

Tutti in `single-watch-winder-model_files/`.

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
3. quattro appoggi a vite per la NodeMCU, ruotata di 32 gradi nella parte
   destra con i componenti verso il piano superiore, un'apertura di 13 x 9 mm
   per la spina micro USB e due fori a imbuto con canna guida per premere i
   pulsanti RST e FLASH dall'esterno, con uno stuzzicadenti;
4. fori degli appoggi della scheda ULN2003 ridotti da 3,2 a 2,5 mm, per
   avvitare direttamente nella plastica;
5. aggiunto il foro del supporto alto, che mancava, e le sedi dei dadi M3 per
   i due supporti, come nel caricatore a 2 posti.

Il modulo RTC si fissa col biadesivo. Il coperchio inferiore originale si
monta senza modifiche: colonne e fori delle sue viti sono rimasti com'erano.

Viti:

- NodeMCU e ULN2003: M3x8, avvitate nella plastica;
- supporti: M3x6 con dado M3. Una M3x8 nel supporto basso toccherebbe il
  motore.

La scatola si stampa capovolta, con il piano superiore sul piatto, come
l'originale. Per rigenerare lo STL dopo una modifica al sorgente, dalla
cartella `single-watch-winder-model_files/`:

```
openscad --backend=manifold -o watchwinder_single_body_nodemcu.stl watchwinder_single_body_nodemcu.scad
```

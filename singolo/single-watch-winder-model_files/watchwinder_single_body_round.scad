// Scatola tonda del caricaorologi a 1 posto con NodeMCU V3 (AZ-Delivery,
// ESP8266), modulo RTC DS3231 e scheda ULN2003.
//
// Deriva da watchwinder_single_body.stl di Robbie Kieffer (Printables
// 1290039, remix del doppio di JayS, Printables 1227833): ne riprende gli
// attacchi del motore (foro dell'albero e orecchie), la posizione dei
// supporti dell'anello e l'altezza. Autore delle modifiche: Carlo Comincini.
// Licenza CC BY-NC-SA 4.0, come l'originale.
//
// Coordinate dello STL di Robbie, che si stampa capovolto: z = 0 e' la faccia
// superiore della scatola (sul piatto di stampa), x verso destra guardando il
// winder dal davanti, y verso il davanti. Piano superiore 3 mm, pareti 4 mm.
//
// Rispetto alla scatola di Robbie (180 x 120 mm):
//  1. pianta tonda da 136 mm, centrata sull'albero in x e a meta' dei piedi
//     dei supporti in y;
//  2. NodeMCU sul retro, con il lato del micro USB infilato in una fessura
//     della parete, finestra per la presa e incavo esterno per la spina; due
//     appoggi a vite sul lato dell'antenna; fori diritti da 3 mm nel piano
//     superiore per i pulsanti RST e FLASH;
//  3. appoggi a vite per la ULN2003 (a sinistra) e per il modulo RTC (a
//     destra, piedini verso il retro);
//  4. supporti dell'anello: foro da 3,5 mm e sede per il dado M3;
//  5. quattro colonne per le viti M3 del coperchio
//     (watchwinder_single_base_round).

$fn = 64;
e = 0.01;

// rettangolo centrato con angoli arrotondati
module rett_arr(a, b, r) offset(r = r) offset(delta = -r) square([a, b], center = true);

// foro per le viti M3 avvitate nella plastica: stampato esce piu' stretto
d_pilota  = 2.8;

// appoggio cilindrico dal piano superiore (z 3) fino a z_cima, con uno smusso
// a 45 gradi di 1 mm alla base
module appoggio(q, d, z_cima) translate([q[0], q[1], 0]) {
    translate([0, 0, 2.9]) cylinder(d = d + 2, h = 0.1 + e);
    translate([0, 0, 3]) cylinder(d1 = d + 2, d2 = d, h = 1);
    translate([0, 0, 3]) cylinder(d = d, h = z_cima - 3);
}

// foro pilota cieco (resta 1 mm verso la faccia superiore), svasato
// all'imbocco per avviare la vite
module foro_pilota(q, z_cima) translate([q[0], q[1], 0]) {
    translate([0, 0, 1]) cylinder(d = d_pilota, h = z_cima);
    translate([0, 0, z_cima - 0.4]) cylinder(d1 = d_pilota, d2 = d_pilota + 0.8 + 2 * e, h = 0.4 + e);
}

// --- Scatola ---------------------------------------------------------------
C         = [30, 54];                // centro della pianta
R_est     = 68;                      // raggio esterno
parete    = 4;
piano     = 3;                       // spessore del piano superiore
altezza   = 28;
smusso    = 0.6;                     // sullo spigolo che poggia sul piatto

// --- Motore (attacchi presi dallo STL di Robbie) --------------------------
// Foro dell'albero in (30, 66) con incavo da 9,4 mm per il mozzo del motore,
// orecchie in (12,5; 73,5) e (47,5; 73,5).
zona_motore = [[6, 58.5], [54, 80]];

// --- NodeMCU V3 -------------------------------------------------------------
// Scheda 58 x 31,32 mm, perpendicolare alla parete del retro, lato USB verso
// la parete. Lato componenti verso il piano superiore, pin verso il basso.
// Il bordo lato USB entra nella parete: agli angoli della fessura restano
// circa 0,6 mm di parete verso l'esterno.
ang       = 0;
d_usb     = [sin(ang), -cos(ang)];   // dal centro verso la parete
u         = -d_usb;                  // asse della scheda: dal lato USB all'antenna
p         = [cos(ang), sin(ang)];    // di lato: verso il pulsante RST
r_bordo   = 64.9;                    // distanza del bordo lato USB dal centro
E         = C + d_usb * r_bordo;     // centro del bordo lato USB

lungh     = 58;
largh     = 31.32;
t_pcb     = 1.67;                    // spessore del circuito
z_comp    = 9;                       // faccia dei componenti
fori      = [[55, -12.35], [55, 12.35]];  // [lungo l'asse, di lato]: solo lato antenna
pulsanti  = [[3, -7.91], [3, 7.91]]; // FLASH, RST

gioco     = 0.3;                     // gioco della scheda nella fessura
gioco_lat = 0.75;                    // gioco ai lati della scheda, per parte
sporg_usb = 1;                       // la presa micro USB sporge dal circuito
z_usb     = z_comp - 1.35;           // asse della presa
finestra  = [3.6, 8.6];              // [alta, larga]: presa e punta della spina
incavo    = [8, 12];                 // [alto, largo]: plastica della spina

function P(a, l) = E + u * a + p * l;

// Sistema di riferimento della scheda: x lungo l'asse, y di lato.
module in_scheda() {
    multmatrix([[u[0], p[0], 0, E[0]],
                [u[1], p[1], 0, E[1]],
                [0,    0,    1, 0   ],
                [0,    0,    0, 1   ]]) children();
}

// --- Modulo RTC DS3231 (ZS-042) -------------------------------------------
// Circuito 37,2 x 20,85 mm, tre fori da circa 2,8 mm (da allargare a 3,2).
// Coordinate della scheda vista dal lato della batteria: X dal lato delle 4
// piazzole verso il lato dei 6 piedini, Y dal bordo dei due fori [3,57; 1,7]
// e [28,87; 1,7] verso il bordo opposto.
// Batteria verso il coperchio inferiore (si cambia aprendo la scatola),
// componenti verso il piano superiore, piedini verso il retro.
// Viti nei due fori in diagonale; il terzo foro e' facoltativo.
rtc_dim    = [37.2, 20.85];
rtc_c      = [68.4, 58.6];           // centro della scheda nella scatola
rtc_fori   = [[3.57, 1.7], [28.87, 1.7], [28.87, 19.15]];
rtc_angolo = [3.57, 19.15];          // quarto appoggio, senza foro
z_rtc      = 10;                     // faccia dei componenti: 7 mm sopra il piano,
                                     // per l'aria e i connettori dei cavetti

function R(X, Y) = rtc_c + [rtc_dim[1] / 2 - Y, rtc_dim[0] / 2 - X];

// --- ULN2003 ----------------------------------------------------------------
// Scheda 32 x 35 mm, fori a 26 x 30 mm; componenti verso il coperchio
// inferiore, come nel modello di JayS.
uln_c      = [-14, 54];
uln_fori   = [for (sx = [-1, 1], sy = [-1, 1]) uln_c + [sx * 13, sy * 15]];
z_uln      = 8;                      // faccia della scheda, come gli appoggi di JayS

// --- Supporti del ruotante ------------------------------------------------
// Come nella scatola di Robbie: supporto basso a 20,4 mm dal foro dell'albero
// verso il davanti, supporto alto a 57,1 mm verso il retro.
supporti   = [[30, 86.4], [30, 8.9]];

// --- Colonne per le viti del coperchio ------------------------------------
ang_colonne = [40, 140, 220, 320];   // gradi, y verso il davanti
r_colonne   = R_est - parete - 3;    // centro delle colonne
d_colonna   = 9;
function colonna(a) = C + [cos(a), sin(a)] * r_colonne;

difference() {
    union() {
        difference() {
            // guscio: piano superiore e parete, con lo smusso sul piatto
            translate([C[0], C[1], 0]) {
                cylinder(r1 = R_est - smusso, r2 = R_est, h = smusso, $fn = 360);
                translate([0, 0, smusso]) cylinder(r = R_est, h = altezza - smusso, $fn = 360);
            }
            translate([C[0], C[1], piano]) cylinder(r = R_est - parete, h = altezza, $fn = 360);
            // al posto del piano in questa zona va quello di Robbie
            translate([zona_motore[0][0], zona_motore[0][1], -1])
                cube([zona_motore[1][0] - zona_motore[0][0], zona_motore[1][1] - zona_motore[0][1], piano + 2]);
        }
        // attacchi del motore presi dallo STL di Robbie
        intersection() {
            import("watchwinder_single_body.stl", convexity = 10);
            translate([zona_motore[0][0], zona_motore[0][1], -1])
                cube([zona_motore[1][0] - zona_motore[0][0], zona_motore[1][1] - zona_motore[0][1], 7]);
        }
        // rinforzi delle sedi dei dadi dei supporti (il motore comincia a z 5,5)
        for (q = supporti) translate([q[0], q[1], 2.9]) cylinder(d = 10, h = 1.6);
        // colonne per le viti del coperchio: entrano di 1,5 mm nella parete
        for (a = ang_colonne)
            translate([colonna(a)[0], colonna(a)[1], 2.9]) cylinder(d = d_colonna, h = altezza - 2.9);
        // appoggi della NodeMCU: 5 mm, sul lato componenti non c'e' spazio per
        // una cima piu' larga
        for (f = fori) appoggio(P(f[0], f[1]), 5, z_comp);
        // appoggi del modulo RTC
        for (f = rtc_fori) appoggio(R(f[0], f[1]), 6, z_rtc);
        appoggio(R(rtc_angolo[0], rtc_angolo[1]), 4, z_rtc);
        // appoggi della ULN2003
        for (q = uln_fori) appoggio(q, 6, z_uln);
    }
    // supporti: foro passante da 3,5 e sede esagonale per il dado M3
    for (q = supporti) {
        translate([q[0], q[1], -1]) cylinder(d = 3.5, h = 7);
        translate([q[0], q[1], 1.5]) cylinder(r = 5.8 / sqrt(3), h = 3.1, $fn = 6);
    }
    // fori pilota (viti M3x8)
    for (f = fori) foro_pilota(P(f[0], f[1]), z_comp);
    for (f = rtc_fori) foro_pilota(R(f[0], f[1]), z_rtc);
    for (q = uln_fori) foro_pilota(q, z_uln);
    // colonne del coperchio: foro da 2,8 profondo 12 mm, svasato all'imbocco
    for (a = ang_colonne) translate([colonna(a)[0], colonna(a)[1], 0]) {
        translate([0, 0, altezza - 12]) cylinder(d = d_pilota, h = 12 + e);
        translate([0, 0, altezza - 0.4]) cylinder(d1 = d_pilota, d2 = d_pilota + 0.8 + 2 * e, h = 0.4 + e);
    }
    // pulsanti: foro diritto da 3 mm nel piano superiore
    for (b = pulsanti) {
        q = P(b[0], b[1]);
        translate([q[0], q[1], -e]) cylinder(d = 3, h = piano + 2 * e);
    }
    in_scheda() {
        // fessura per il circuito, su tutta la larghezza
        translate([-gioco, -(largh / 2 + gioco_lat), z_comp])
            cube([gioco + 8, largh + 2 * gioco_lat, t_pcb + gioco]);
        // spazio per presa e pulsanti sopra il circuito; ai lati la parete
        // resta e tiene la scheda dall'alto
        translate([-gioco, -10.6, z_comp - 3.1]) cube([gioco + 8, 21.2, 3.1 + e]);
        // finestra per la presa e la punta della spina
        translate([-8, 0, z_usb]) rotate([0, 90, 0])
            linear_extrude(16) rett_arr(finestra[0], finestra[1], 0.8);
        // incavo esterno per la plastica della spina, fino a 0,5 mm davanti
        // alla presa
        translate([-12, 0, z_usb]) rotate([0, 90, 0])
            linear_extrude(12 - sporg_usb - 0.5) rett_arr(incavo[0], incavo[1], 1.5);
    }
}

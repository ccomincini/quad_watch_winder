// Scatola del caricaorologi a 1 posto con NodeMCU V3 (AZ-Delivery, ESP8266).
//
// Parte da watchwinder_single_body.stl di Robbie Kieffer (Printables 1290039,
// remix del doppio di JayS, Printables 1227833) e ne deriva un file nuovo:
// l'originale non si tocca.
// Autore delle modifiche: Carlo Comincini. Licenza CC BY-NC-SA 4.0, come
// l'originale.
//
// Coordinate dello STL di Robbie, che si stampa capovolto: z = 0 e' la faccia
// superiore della scatola (sul piatto di stampa), x verso destra guardando il
// winder dal davanti, y verso il davanti. Piano superiore spesso 3 mm, pareti
// 4 mm, raggio interno delle pareti curve 56 mm.
//
// Modifiche:
//  1. chiusi il foro a D dell'interruttore (retro) e il foro della presa
//     (davanti): si alimenta solo dal micro USB della NodeMCU;
//  2. tolto il supporto interno dell'interruttore;
//  3. NodeMCU: il lato del micro USB si infila in una fessura nella parete,
//     con una finestra per la presa e un incavo esterno per la plastica della
//     spina; due appoggi a vite sul lato dell'antenna; fori diritti da 3 mm
//     nel piano superiore per i pulsanti RST e FLASH;
//  4. fori degli appoggi della ULN2003 ridotti da 3,2 a 2,8 mm, per le viti
//     M3 nella plastica;
//  5. foro del supporto alto (mancava) e sedi dei dadi M3 per i due supporti,
//     come nel due posti;
//  6. quattro appoggi per il modulo RTC DS3231, tre forati e uno d'angolo.

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

// --- NodeMCU V3 -----------------------------------------------------------
// Scheda 58 x 31,32 mm, ruotata di 32 gradi nella parte destra, lato USB
// verso la parete curva. Lato componenti verso il piano superiore, pin verso
// il basso. Il bordo lato USB entra nella parete: agli angoli della fessura
// restano circa 0,6 mm di parete verso l'esterno.
ang       = 32;
d_usb     = [sin(ang), -cos(ang)];   // dal centro della curva verso la parete
u         = -d_usb;                  // asse della scheda: dal lato USB all'antenna
p         = [cos(ang), sin(ang)];    // di lato: verso il pulsante RST
c_curva   = [60, 60];                // centro della parete curva destra
r_bordo   = 56.8;                    // distanza del bordo lato USB dal centro
E         = c_curva + d_usb * r_bordo;   // centro del bordo lato USB

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
// Misure ricavate dalle foto: circuito 37,2 x 20,85 mm, tre fori da circa
// 2,8 mm. Coordinate della scheda vista dal lato della batteria: X dal lato
// delle 4 piazzole verso il lato dei 6 piedini, Y dal bordo dei due fori
// [3,57; 1,7] e [28,87; 1,7] verso il bordo opposto.
// Batteria verso il coperchio inferiore (si cambia aprendo la scatola),
// componenti verso il piano superiore, piedini verso sinistra.
// Viti nei due fori in diagonale; il terzo foro e' facoltativo.
rtc_dim    = [37.2, 20.85];
rtc_c      = [78.1, 88.4];           // centro della scheda nella scatola
rtc_fori   = [[3.57, 1.7], [28.87, 1.7], [28.87, 19.15]];
rtc_angolo = [3.57, 19.15];          // quarto appoggio, senza foro
z_rtc      = 10;                     // faccia dei componenti: 7 mm sopra il piano,
                                     // per l'aria e i connettori dei cavetti

function R(X, Y) = rtc_c + [rtc_dim[0] / 2 - X, Y - rtc_dim[1] / 2];

// --- Supporti del ruotante ------------------------------------------------
// Distanze dal foro dell'albero (30, 66) come nel due posti: supporto basso a
// 20,4 mm verso il davanti, supporto alto a 57,1 mm verso il retro.
supporti  = [[30, 86.4], [30, 8.9]];

// --- ULN2003 (appoggi di JayS, alti 5 mm) ---------------------------------
uln       = [[-26.1, 54.7], [-44.5, 36.3], [-4.9, 33.5], [-23.3, 15.1]];

difference() {
    union() {
        difference() {
            import("watchwinder_single_body.stl", convexity = 10);
            // supporto dell'interruttore e ispessimento della parete
            translate([18.5, 4, 3]) cube([23, 14, 26]);
        }
        // foro a D dell'interruttore (retro)
        translate([22, 0, 7]) cube([16, 4, 15]);
        // foro della presa (davanti)
        translate([24, 116, 9]) cube([12, 4, 10]);
        // fori ULN2003 da 3,2: si riempiono e si rifanno da 2,5
        for (q = uln) translate([q[0], q[1], 2.9]) cylinder(d = 3.6, h = 5.1);
        // foro del supporto basso da 4,3: si riempie e si rifa' da 3,5
        translate([supporti[0][0], supporti[0][1], 0]) cylinder(d = 4.6, h = 3);
        // rinforzi delle sedi dei dadi (1,5 mm sotto il piano; il motore
        // comincia a z 5,5)
        for (q = supporti) translate([q[0], q[1], 2.9]) cylinder(d = 10, h = 1.6);
        // appoggi della NodeMCU: 5 mm, sul lato componenti non c'e' spazio per
        // una cima piu' larga
        for (f = fori) appoggio(P(f[0], f[1]), 5, z_comp);
        // appoggi del modulo RTC
        for (f = rtc_fori) appoggio(R(f[0], f[1]), 6, z_rtc);
        appoggio(R(rtc_angolo[0], rtc_angolo[1]), 4, z_rtc);
    }
    // fori pilota ULN2003, ciechi: resta 1 mm verso la faccia superiore
    for (q = uln) translate([q[0], q[1], 1]) cylinder(d = d_pilota, h = 8);
    // supporti: foro passante da 3,5 e sede esagonale per il dado M3
    for (q = supporti) {
        translate([q[0], q[1], -1]) cylinder(d = 3.5, h = 7);
        translate([q[0], q[1], 1.5]) cylinder(r = 5.8 / sqrt(3), h = 3.1, $fn = 6);
    }
    // fori pilota della NodeMCU e del modulo RTC (viti M3x8)
    for (f = fori) foro_pilota(P(f[0], f[1]), z_comp);
    for (f = rtc_fori) foro_pilota(R(f[0], f[1]), z_rtc);
    // pulsanti: foro diritto da 3 mm nel piano superiore
    for (b = pulsanti) {
        q = P(b[0], b[1]);
        translate([q[0], q[1], -e]) cylinder(d = 3, h = 3 + 2 * e);
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

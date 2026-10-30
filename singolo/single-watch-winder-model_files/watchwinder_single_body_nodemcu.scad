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
//  3. appoggi a vite per la NodeMCU, apertura per la spina micro USB, fori a
//     imbuto con canna guida per i pulsanti RST e FLASH;
//  4. fori degli appoggi della ULN2003 ridotti da 3,2 a 2,5 mm, per le viti
//     nella plastica;
//  5. foro del supporto alto (mancava) e sedi dei dadi M3 per i due supporti,
//     come nel due posti.
// Il modulo RTC si fissa col biadesivo: nessun appoggio.

$fn = 64;
e = 0.01;

// --- NodeMCU V3 -----------------------------------------------------------
// Scheda 58 x 31,32 mm, ruotata di 32 gradi nella parte destra, lato USB
// verso la parete curva. Lato componenti verso il piano superiore, pin verso
// il basso. Tutta dentro la parete: la spina entra dall'apertura.
ang       = 32;
d_usb     = [sin(ang), -cos(ang)];   // dal centro della curva verso la parete
u         = -d_usb;                  // asse della scheda: dal lato USB all'antenna
p         = [cos(ang), sin(ang)];    // di lato: verso il pulsante RST
c_curva   = [60, 60];                // centro della parete curva destra
r_bordo   = 53;                      // distanza del bordo lato USB dal centro
E         = c_curva + d_usb * r_bordo;   // centro del bordo lato USB

lungh     = 58;
largh     = 31.32;
z_comp    = 9;                       // faccia dei componenti
fori      = [[3, -12], [3, 12], [55, -12], [55, 12]];  // [lungo l'asse, di lato]
pulsanti  = [[3, -7.66], [3, 7.66]]; // FLASH, RST
h_puls    = 2.13;                    // cima del pulsante sopra il circuito

function P(a, l) = E + u * a + p * l;

// Sistema di riferimento della scheda: x lungo l'asse, y di lato.
module in_scheda() {
    multmatrix([[u[0], p[0], 0, E[0]],
                [u[1], p[1], 0, E[1]],
                [0,    0,    1, 0   ],
                [0,    0,    0, 1   ]]) children();
}

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
        // appoggi della NodeMCU
        for (f = fori) {
            q = P(f[0], f[1]);
            translate([q[0], q[1], 2.9]) cylinder(d = 7.5, h = 1.6);
            translate([q[0], q[1], 4.5]) cylinder(d = 5, h = z_comp - 4.5);
        }
        // canne guida dei pulsanti: finiscono 1 mm sopra il pulsante
        for (b = pulsanti) {
            q = P(b[0], b[1]);
            translate([q[0], q[1], 2.9]) cylinder(d = 4.2, h = z_comp - h_puls - 1 - 2.9);
        }
    }
    // fori pilota ULN2003, ciechi: resta 1 mm verso la faccia superiore
    for (q = uln) translate([q[0], q[1], 1]) cylinder(d = 2.5, h = 8);
    // supporti: foro passante da 3,5 e sede esagonale per il dado M3
    for (q = supporti) {
        translate([q[0], q[1], -1]) cylinder(d = 3.5, h = 7);
        translate([q[0], q[1], 1.5]) cylinder(r = 5.8 / sqrt(3), h = 3.1, $fn = 6);
    }
    // fori pilota della NodeMCU (viti M3x8), ciechi
    for (f = fori) {
        q = P(f[0], f[1]);
        translate([q[0], q[1], 1]) cylinder(d = 2.5, h = z_comp);
    }
    // pulsanti: imbuto da 3,2 a 2,2 mm nel piano, poi canna da 2,2
    for (b = pulsanti) {
        q = P(b[0], b[1]);
        translate([q[0], q[1], -e]) cylinder(d1 = 3.2, d2 = 2.2, h = 3 + 2 * e);
        translate([q[0], q[1], 3]) cylinder(d = 2.2, h = z_comp);
    }
    // apertura per la spina micro USB (13 x 9 mm), centrata sulla presa
    in_scheda()
        translate([-12, 0, 3.2])
            rotate([0, 90, 0]) rotate([0, 0, 90])
                linear_extrude(12)
                    offset(r = 1) offset(delta = -1)
                        translate([-6.5, 0]) square([13, 9]);
}

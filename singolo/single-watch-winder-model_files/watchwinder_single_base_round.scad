// Coperchio inferiore della scatola tonda del caricaorologi a 1 posto
// (watchwinder_single_body_round).
//
// Stesso schema del coperchio di Robbie Kieffer (watchwinder_single_base.stl,
// Printables 1290039): piastra di 2 mm, bordo largo quanto la parete e
// borchie attorno alle viti spessi 4,5 mm, fori per viti M3 con la testa
// incassata. Autore delle modifiche: Carlo Comincini. Licenza CC BY-NC-SA 4.0.
//
// z = 0 e' la faccia esterna, che poggia sul tavolo e sul piatto di stampa.
// x e y come nella scatola guardata dal davanti con il coperchio in basso:
// stesse coordinate dello STL della scatola.

$fn = 64;
e = 0.01;

C           = [30, 54];
R_est       = 68;
parete      = 4;
spessore    = 4.5;                   // bordo e borchie
piastra     = 2;                     // parte centrale
smusso      = 0.6;                   // sullo spigolo che poggia sul piatto
ang_colonne = [40, 140, 220, 320];
r_colonne   = R_est - parete - 3;
d_borchia   = 12;
function colonna(a) = C + [cos(a), sin(a)] * r_colonne;

difference() {
    union() {
        translate([C[0], C[1], 0]) {
            // piastra con lo smusso sul piatto
            cylinder(r1 = R_est - smusso, r2 = R_est, h = smusso, $fn = 360);
            translate([0, 0, smusso]) cylinder(r = R_est, h = piastra - smusso, $fn = 360);
            // bordo sotto la parete
            difference() {
                translate([0, 0, smusso]) cylinder(r = R_est, h = spessore - smusso, $fn = 360);
                translate([0, 0, -1]) cylinder(r = R_est - parete, h = spessore + 2, $fn = 360);
            }
        }
        // borchie attorno alle viti
        for (a = ang_colonne) translate([colonna(a)[0], colonna(a)[1], 0]) cylinder(d = d_borchia, h = spessore);
    }
    // viti M3: foro da 3,5 e testa incassata da 6,2, profonda 3,5 mm
    for (a = ang_colonne) translate([colonna(a)[0], colonna(a)[1], 0]) {
        translate([0, 0, -1]) cylinder(d = 3.5, h = spessore + 2);
        translate([0, 0, -e]) cylinder(d = 6.2, h = 3.5 + e);
    }
}

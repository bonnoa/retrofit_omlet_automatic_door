/*
 * ============================================================
 *  PORTE POULAILLER AUTOMATIQUE — v2
 *  ESP32 + BTS7960 + MC-38 NF + ACS712 + BH1750
 *  Home Assistant via MQTT
 *
 *  4 modes de gestion :
 *    soleil     — lever/coucher du soleil + offsets
 *    luminosite — seuils lux + durée de confirmation
 *    heure_fixe — heures définies + offsets optionnels
 *    manuel     — commandes manuelles uniquement
 *
 * ============================================================
 *  Dépendances :
 *    - PubSubClient         (Nick O'Leary)
 *    - ArduinoJson          (Benoit Blanchon)
 *    - BH1750               (Christopher Laws)
 *    - ESPAsyncWebServer    (me-no-dev)
 *    - AsyncTCP             (me-no-dev)
 * ============================================================
 */

#include <WiFi.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>
#include <time.h>
#include <math.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>

// ============================================================
//  PROTOTYPES — fonctions définies dans web_server_v2.ino
// ============================================================
extern AsyncWebSocket ws;  // déclaré dans web_server_v2.ino
void setupWebServer();
void wsBroadcastState();
void wsLog(const char* msg, const char* type);
void commandeOuvrir();
void commandeFermer();
void resetErreur();
void stopUrgence();
void sauvegarderConfig();

// ============================================================
//  CONFIGURATION — À MODIFIER
// ============================================================
#define MQTT_ENABLED  true   // true = MQTT actif, false = désactivé

const char* WIFI_SSID     = "TON_SSID";
const char* WIFI_PASSWORD = "TON_MOT_DE_PASSE";

const char* MQTT_SERVER   = "192.168.1.XX";  // IP du broker MQTT
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "mqtt_user";
const char* MQTT_PASS     = "mqtt_password";

const float LATITUDE      = 48.8566;  // Coordonnées GPS pour calcul soleil
const float LONGITUDE     = 2.3522;

const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";  // France — heure été/hiver automatique


// ============================================================
//  PINS
// ============================================================
#define PIN_RPWM      25   // BTS7960 — sens ouverture  (droite→gauche)
#define PIN_LPWM      26   // BTS7960 — sens fermeture  (gauche→droite)
#define PIN_EN        27   // BTS7960 — enable (R_EN + L_EN reliés ensemble)

#define PIN_FDC_OUV   32   // MC-38 NF — porte en position ouverte  (gauche)
#define PIN_FDC_FER   33   // MC-38 NF — porte en position fermée   (droite)

#define PIN_ACS712    34   // ACS712 — mesure courant (analogique)

#define PIN_SDA       21   // I2C SDA (BH1750)
#define PIN_SCL       22   // I2C SCL (BH1750)

// ============================================================
//  PARAMÈTRES MOTEUR
// ============================================================
// PWM_CHANNEL_R et PWM_CHANNEL_L non utilisés en API v3.x (ledcAttach par pin)
#define PWM_FREQ        1000
#define PWM_RESOLUTION  8
#define PWM_DUTY_9V     191       // ouverture — ~75% → ~9V sur alim 12V
#define PWM_DUTY_FER    140       // fermeture — ~58% → ~7V (plus doux sur la fermeture "hey poupoule, attention ça ferme")

#define TIMEOUT_OUV_MS   15000    // timeout ouverture (cycle ~7s + marge)
#define TIMEOUT_FER_MS   20000    // timeout fermeture (cycle ~10s + marge)

#define ACS712_ZERO_ADC     3050  // Calibré sur le module réel
#define ACS712_SEUIL_ADC    310   // ~2.7A → entre nominal (~2A/delta 246) et blocage (~3.3A/delta 375)

// ============================================================
//  TOPICS MQTT
// ============================================================
#define MQTT_PREFIX             "poulailler"
#define TOPIC_ETAT_PORTE        MQTT_PREFIX "/porte/etat"
#define TOPIC_COMMANDE_PORTE    MQTT_PREFIX "/porte/commande"
#define TOPIC_LUMINOSITE        MQTT_PREFIX "/capteurs/lux"
#define TOPIC_COURANT           MQTT_PREFIX "/capteurs/courant"
#define TOPIC_HEURE_SOLEIL      MQTT_PREFIX "/soleil/heures"
#define TOPIC_MODE              MQTT_PREFIX "/mode"
#define TOPIC_CONFIG            MQTT_PREFIX "/config"       // subscribe — JSON complet
#define MQTT_CLIENT_ID          "esp32_poulailler"

// ============================================================
//  TYPES
// ============================================================
enum EtatPorte {
  PORTE_FERMEE,
  PORTE_OUVERTE,
  PORTE_EN_OUVERTURE,
  PORTE_EN_FERMETURE,
  PORTE_ERREUR
};

enum ModeGestion {
  MODE_SOLEIL,       // lever/coucher + offsets
  MODE_LUMINOSITE,   // seuils lux + confirmation
  MODE_HEURE_FIXE,   // heures fixes + offsets
  MODE_MANUEL        // commandes manuelles uniquement
};

struct HeureSoleil {
  int leverH, leverM;
  int coucherH, coucherM;
};

// ============================================================
//  CONFIGURATION DES MODES
// ============================================================

// -- Mode SOLEIL --
int offsetOuv = 15;   // minutes après lever  (peut être négatif)
int offsetFer = 15;   // minutes après coucher (peut être négatif)

// -- Mode LUMINOSITE --
float luxSeuilOuverture   = 800.0;   // ouvre si lux > seuil pendant confirmationSec
float luxSeuilFermeture   = 100.0;   // ferme si lux < seuil pendant confirmationSec
int   luxConfirmationSec  = 60;      // durée de confirmation en secondes

// -- Mode HEURE FIXE --
// Heure de base + offset (offset peut être négatif)
// Heure effective = heureBase + offset minutes
int  heureFixeOuvH  = 7,  heureFixeOuvM  = 30;   // heure de base ouverture
int  offsetFixeOuv  = 0;                           // offset en minutes (−60 à +60)
int  heureFixeFerH  = 20, heureFixeFerM  = 0;     // heure de base fermeture
int  offsetFixeFer  = 0;                           // offset en minutes

// ============================================================
//  VARIABLES GLOBALES
// ============================================================
WiFiClient   espClient;
PubSubClient mqtt(espClient);
BH1750       bh1750;
Preferences  prefs;
EtatPorte   etatPorte   = PORTE_FERMEE;
ModeGestion modeActuel  = MODE_SOLEIL;
ModeGestion modePending = MODE_SOLEIL;  // appliqué après fin de mouvement
bool        modePendingActif = false;

unsigned long tempsDebutMouvement   = 0;
unsigned long dernierPublish        = 0;
unsigned long dernierCheckAuto      = 0;

// Confirmation luminosité
unsigned long debutConditionLux     = 0;
bool          conditionLuxActive    = false;
bool          conditionLuxCible     = false;  // true=ouverture, false=fermeture

// Override manuel temporaire
bool          forceManuel          = false;
unsigned long forceManuelExpire     = 0;   // millis() à partir duquel l'auto reprend

// Ignore les messages MQTT retain au démarrage
unsigned long ignoreRetainUntil     = 0;

// ============================================================
//  CALCUL LEVER / COUCHER DU SOLEIL (NOAA simplifié)
// ============================================================
HeureSoleil calculSoleil(int annee, int mois, int jour, float lat, float lon) {
  int a = (14 - mois) / 12;
  int y = annee - a;
  int m = mois + 12 * a - 3;
  long jdn = jour + (153 * m + 2) / 5 + 365L * y + y / 4 - y / 100 + y / 400 + 1721119;
  float jd  = jdn - 0.5;
  float n   = jd - 2451545.0;
  float L   = fmod(280.460 + 0.9856474 * n, 360.0);
  if (L < 0) L += 360.0;
  float g   = fmod(357.528 + 0.9856003 * n, 360.0) * M_PI / 180.0;
  float lam = (L + 1.915 * sin(g) + 0.020 * sin(2 * g)) * M_PI / 180.0;
  float eps = 23.439 * M_PI / 180.0;
  float decl = asin(sin(eps) * sin(lam));
  float latR  = lat * M_PI / 180.0;
  float cosH  = (cos(90.833 * M_PI / 180.0) - sin(latR) * sin(decl)) / (cos(latR) * cos(decl));

  HeureSoleil h = {0, 0, 0, 0};
  if (cosH < -1 || cosH > 1) return h;

  float H    = acos(cosH) * 180.0 / M_PI;
  float RA   = atan2(cos(eps) * sin(lam), cos(lam)) * 180.0 / M_PI;
  if (RA < 0) RA += 360.0;
  float EoT  = (L - 0.0057183 - RA) * 4.0;  // minutes — corrigé pour lon négative
  float tNoon    = 12.0 - EoT / 60.0 - lon / 15.0;
  float tLever   = tNoon - H / 15.0;
  float tCoucher = tNoon + H / 15.0;
  // Décalage UTC dynamique (gère heure été/hiver automatiquement)
  struct tm tNow; getLocalTime(&tNow);
  time_t t1 = mktime(&tNow);
  struct tm* utc = gmtime(&t1);
  float offsetH = (float)(tNow.tm_hour - utc->tm_hour);
  if (offsetH < -12) offsetH += 24;
  if (offsetH >  12) offsetH -= 24;
  tLever   += offsetH;
  tCoucher += offsetH;

  // Normaliser dans la plage 0-24h (longitude négative peut donner des valeurs < 0)
  while (tLever   < 0)    tLever   += 24.0;
  while (tLever   >= 24)  tLever   -= 24.0;
  while (tCoucher < 0)    tCoucher += 24.0;
  while (tCoucher >= 24)  tCoucher -= 24.0;

  h.leverH   = (int)tLever;
  h.leverM   = (int)((tLever   - h.leverH)   * 60);
  h.coucherH = (int)tCoucher;
  h.coucherM = (int)((tCoucher - h.coucherH) * 60);
  return h;
}

// Applique un offset en minutes à une heure H:M, retourne en minutes depuis minuit
int heureEnMinutes(int h, int m, int offsetMin) {
  int total = h * 60 + m + offsetMin;
  // Boucler proprement (ex: 00:30 - 45min = 23:45 de la veille → on garde dans journée)
  if (total < 0)    total += 1440;
  if (total >= 1440) total -= 1440;
  return total;
}

// ============================================================
//  MOTEUR
// ============================================================
void stopMoteur() {
  ledcWrite(PIN_RPWM, 0);
  ledcWrite(PIN_LPWM, 0);
}

void stopUrgence() {
  ledcWrite(PIN_RPWM, 0);
  ledcWrite(PIN_LPWM, 0);
  // Relire les FDC pour déterminer l'état réel
  delay(50);
  if      (digitalRead(PIN_FDC_OUV) == LOW) etatPorte = PORTE_OUVERTE;
  else if (digitalRead(PIN_FDC_FER) == LOW) etatPorte = PORTE_FERMEE;
  else                                       etatPorte = PORTE_ERREUR;
  publishEtat();
  wsBroadcastState();
  wsLog("Stop d'urgence — moteur arrêté", "warn");
  Serial.println("[STOP] Arrêt d'urgence");
}


// ============================================================
//  RESET ERREUR — relit les FDC et remet l'état à jour
// ============================================================
void resetErreur() {
  stopMoteur();
  delay(100);
  if      (digitalRead(PIN_FDC_OUV) == LOW)  { etatPorte = PORTE_OUVERTE; Serial.println("[RESET] Porte OUVERTE"); }
  else if (digitalRead(PIN_FDC_FER) == LOW)  { etatPorte = PORTE_FERMEE;  Serial.println("[RESET] Porte FERMÉE");  }
  else                                        { etatPorte = PORTE_ERREUR;  Serial.println("[RESET] Position toujours inconnue"); }
  publishEtat();
  wsBroadcastState();
  wsLog("Reset erreur — état relu depuis les fins de course", "info");
}

// Démarrage progressif — rampe PWM de 0 à PWM_DUTY_9V en RAMPE_MS ms
#define RAMPE_MS  300   // durée de la rampe en millisecondes

void demarrerMoteur(int pinActif, int pinInactif, int dutyMax) {
  ledcWrite(pinInactif, 0);
  int steps = 20;
  for (int i = 1; i <= steps; i++) {
    int duty = (dutyMax * i) / steps;
    ledcWrite(pinActif, duty);
    delay(RAMPE_MS / steps);
  }
}

// Ouverture = porte coulisse de droite vers gauche
void ouvrirPorte() {
  if (digitalRead(PIN_FDC_OUV) == LOW) {
    Serial.println("[MOTEUR] Déjà ouverte (gauche).");
    return;
  }
  if (etatPorte == PORTE_EN_OUVERTURE || etatPorte == PORTE_EN_FERMETURE) {
    Serial.println("[MOTEUR] Mouvement déjà en cours.");
    return;
  }
  Serial.println("[MOTEUR] Ouverture (droite → gauche)...");
  etatPorte = PORTE_EN_OUVERTURE;
  tempsDebutMouvement = millis();
  publishEtat();
  demarrerMoteur(PIN_RPWM, PIN_LPWM, PWM_DUTY_9V);
  wsLog("Ouverture déclenchée", "info");
}

// Fermeture = porte coulisse de gauche vers droite
void fermerPorte() {
  if (digitalRead(PIN_FDC_FER) == LOW) {
    Serial.println("[MOTEUR] Déjà fermée (droite).");
    return;
  }
  if (etatPorte == PORTE_EN_OUVERTURE || etatPorte == PORTE_EN_FERMETURE) {
    Serial.println("[MOTEUR] Mouvement déjà en cours.");
    return;
  }
  Serial.println("[MOTEUR] Fermeture (gauche → droite)...");
  etatPorte = PORTE_EN_FERMETURE;
  tempsDebutMouvement = millis();
  publishEtat();
  demarrerMoteur(PIN_LPWM, PIN_RPWM, PWM_DUTY_FER);
  wsLog("Fermeture déclenchée", "info");
}

// ============================================================
//  LECTURE ACS712
// ============================================================
float lireCourant() {
  int raw = analogRead(PIN_ACS712);
  return abs(raw - ACS712_ZERO_ADC) / 115.0;
}
bool surcourant() {
  // Moyenne sur 5 lectures pour filtrer le bruit ADC
  long somme = 0;
  for (int i = 0; i < 5; i++) {
    somme += analogRead(PIN_ACS712);
    delay(2);
  }
  int moyenne = somme / 5;
  int delta = abs(moyenne - ACS712_ZERO_ADC);
  return delta > ACS712_SEUIL_ADC;
}

// ============================================================
//  APPLICATION DU MODE PENDING
// ============================================================
void appliquerModePending() {
  if (!modePendingActif) return;
  if (etatPorte == PORTE_EN_OUVERTURE || etatPorte == PORTE_EN_FERMETURE) return;
  if (modePending != modeActuel) {
    modeActuel = modePending;
    publishMode();
  }
  modePendingActif = false;
  conditionLuxActive = false;  // reset confirmation lux
  Serial.printf("[MODE] Appliqué : %d\n", modeActuel);
  wsBroadcastState();
}



// ============================================================
//  PERSISTANCE CONFIG (NVS Flash)
// ============================================================
void sauvegarderConfig() {
  prefs.begin("poulailler", false);
  prefs.putInt("offsetOuv",      offsetOuv);
  prefs.putInt("offsetFer",      offsetFer);
  prefs.putFloat("luxOuv",       luxSeuilOuverture);
  prefs.putFloat("luxFer",       luxSeuilFermeture);
  prefs.putInt("luxConf",        luxConfirmationSec);
  prefs.putInt("hfxOuvH",        heureFixeOuvH);
  prefs.putInt("hfxOuvM",        heureFixeOuvM);
  prefs.putInt("hfxOuvOff",      offsetFixeOuv);
  prefs.putInt("hfxFerH",        heureFixeFerH);
  prefs.putInt("hfxFerM",        heureFixeFerM);
  prefs.putInt("hfxFerOff",      offsetFixeFer);
  prefs.putInt("mode",           (int)modeActuel);
  prefs.end();
  Serial.println("[NVS] Config sauvegardée");
}

void chargerConfig() {
  prefs.begin("poulailler", true);  // read-only
  offsetOuv          = prefs.getInt("offsetOuv",   offsetOuv);
  offsetFer          = prefs.getInt("offsetFer",   offsetFer);
  luxSeuilOuverture  = prefs.getFloat("luxOuv",    luxSeuilOuverture);
  luxSeuilFermeture  = prefs.getFloat("luxFer",    luxSeuilFermeture);
  luxConfirmationSec = prefs.getInt("luxConf",     luxConfirmationSec);
  heureFixeOuvH      = prefs.getInt("hfxOuvH",    heureFixeOuvH);
  heureFixeOuvM      = prefs.getInt("hfxOuvM",    heureFixeOuvM);
  offsetFixeOuv      = prefs.getInt("hfxOuvOff",  offsetFixeOuv);
  heureFixeFerH      = prefs.getInt("hfxFerH",    heureFixeFerH);
  heureFixeFerM      = prefs.getInt("hfxFerM",    heureFixeFerM);
  offsetFixeFer      = prefs.getInt("hfxFerOff",  offsetFixeFer);
  modeActuel         = (ModeGestion)prefs.getInt("mode", (int)modeActuel);
  prefs.end();
  Serial.println("[NVS] Config chargée");
}

// ============================================================
//  OVERRIDE MANUEL — suspend l'auto jusqu'à la prochaine échéance
// ============================================================
void activerForceManuel() {
  if (modeActuel == MODE_MANUEL) return;  // inutile en mode manuel

  // Calculer la durée jusqu'à la prochaine échéance (max 12h)
  unsigned long dureeMs = 12UL * 3600UL * 1000UL;  // fallback 12h

  struct tm t;
  if (getLocalTime(&t)) {
    int maintenant = t.tm_hour * 60 + t.tm_min;
    int cible = -1;

    if (modeActuel == MODE_SOLEIL) {
      HeureSoleil s = calculSoleil(t.tm_year+1900, t.tm_mon+1, t.tm_mday, LATITUDE, LONGITUDE);
      int heureOuv = heureEnMinutes(s.leverH, s.leverM, offsetOuv);
      int heureFer = heureEnMinutes(s.coucherH, s.coucherM, offsetFer);
      // Prochaine échéance = prochain lever ou coucher
      if (maintenant < heureOuv)      cible = heureOuv;
      else if (maintenant < heureFer) cible = heureFer;
      else                            cible = heureOuv + 1440;  // lever demain
    } else if (modeActuel == MODE_HEURE_FIXE) {
      int heureOuv = heureEnMinutes(heureFixeOuvH, heureFixeOuvM, offsetFixeOuv);
      int heureFer = heureEnMinutes(heureFixeFerH, heureFixeFerM, offsetFixeFer);
      if (maintenant < heureOuv)      cible = heureOuv;
      else if (maintenant < heureFer) cible = heureFer;
      else                            cible = heureOuv + 1440;
    } else if (modeActuel == MODE_LUMINOSITE) {
      // Pour luminosité : suspension de 30 minutes
      dureeMs = 30UL * 60UL * 1000UL;
      cible = -1;
    }

    if (cible >= 0) {
      int delta = cible - maintenant;
      if (delta < 0) delta += 1440;
      dureeMs = (unsigned long)delta * 60UL * 1000UL;
    }
  }

  forceManuel = true;
  forceManuelExpire = millis() + dureeMs;

  char msg[64];
  snprintf(msg, sizeof(msg), "Mode auto suspendu jusqu'à la prochaine échéance");
  Serial.println(msg);
  wsLog(msg, "info");
}

// ============================================================
//  GESTION AUTOMATIQUE
// ============================================================

// ── Mode SOLEIL ──────────────────────────────────────────────
void gestionSoleil() {
  // Vérifier expiration override manuel
  if (forceManuel && millis() >= forceManuelExpire) {
    forceManuel = false;
    Serial.println("[AUTO] Override manuel expiré — reprise automatique");
    wsLog("Reprise automatique", "info");
    wsBroadcastState();
  }
  if (forceManuel) return;

  struct tm t; if (!getLocalTime(&t)) return;
  HeureSoleil s = calculSoleil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, LATITUDE, LONGITUDE);
  int maintenant = t.tm_hour * 60 + t.tm_min;
  int heureOuv   = heureEnMinutes(s.leverH,   s.leverM,   offsetOuv);
  int heureFer   = heureEnMinutes(s.coucherH, s.coucherM, offsetFer);
  bool doitEtreOuverte = (maintenant >= heureOuv && maintenant < heureFer);
  if (doitEtreOuverte  && etatPorte == PORTE_FERMEE) ouvrirPorte();
  if (!doitEtreOuverte && etatPorte == PORTE_OUVERTE) fermerPorte();
}

// ── Mode LUMINOSITE ──────────────────────────────────────────
void gestionLuminosite() {
  // Vérifier expiration override manuel
  if (forceManuel && millis() >= forceManuelExpire) {
    forceManuel = false;
    Serial.println("[AUTO] Override manuel expiré — reprise automatique");
    wsLog("Reprise automatique", "info");
    wsBroadcastState();
  }
  if (forceManuel) return;

  float lux = bh1750.readLightLevel();

  // Déterminer quelle condition est active
  bool devraitOuvrir = (lux > luxSeuilOuverture && etatPorte == PORTE_FERMEE);
  bool devraitFermer = (lux < luxSeuilFermeture && etatPorte == PORTE_OUVERTE);

  if (!devraitOuvrir && !devraitFermer) {
    // Plus de condition — reset timer
    conditionLuxActive = false;
    debutConditionLux  = 0;
    return;
  }

  bool cibleActuelle = devraitOuvrir; // true=ouverture, false=fermeture

  if (!conditionLuxActive || conditionLuxCible != cibleActuelle) {
    // Nouvelle condition → démarrer le timer
    conditionLuxActive = true;
    conditionLuxCible  = cibleActuelle;
    debutConditionLux  = millis();
    Serial.printf("[LUX] Condition %s — confirmation dans %ds (%.0f lux)\n",
      cibleActuelle ? "ouverture" : "fermeture", luxConfirmationSec, lux);
    return;
  }

  // Condition stable depuis assez longtemps ?
  if (millis() - debutConditionLux >= (unsigned long)luxConfirmationSec * 1000) {
    conditionLuxActive = false;
    if (cibleActuelle) {
      Serial.printf("[LUX] Ouverture confirmée (%.0f lux > %.0f)\n", lux, luxSeuilOuverture);
      wsLog("Ouverture — luminosité confirmée", "info");
      ouvrirPorte();
    } else {
      Serial.printf("[LUX] Fermeture confirmée (%.0f lux < %.0f)\n", lux, luxSeuilFermeture);
      wsLog("Fermeture — luminosité confirmée", "info");
      fermerPorte();
    }
  }
}

// ── Mode HEURE FIXE ──────────────────────────────────────────
void gestionHeureFixe() {
  // Vérifier expiration override manuel
  if (forceManuel && millis() >= forceManuelExpire) {
    forceManuel = false;
    Serial.println("[AUTO] Override manuel expiré — reprise automatique");
    wsLog("Reprise automatique", "info");
    wsBroadcastState();
  }
  if (forceManuel) return;

  struct tm t; if (!getLocalTime(&t)) return;
  int maintenant = t.tm_hour * 60 + t.tm_min;
  int heureOuv   = heureEnMinutes(heureFixeOuvH, heureFixeOuvM, offsetFixeOuv);
  int heureFer   = heureEnMinutes(heureFixeFerH, heureFixeFerM, offsetFixeFer);
  bool doitEtreOuverte = (maintenant >= heureOuv && maintenant < heureFer);
  if (doitEtreOuverte  && etatPorte == PORTE_FERMEE) ouvrirPorte();
  if (!doitEtreOuverte && etatPorte == PORTE_OUVERTE) fermerPorte();
}

// ============================================================
//  MQTT
// ============================================================
void publishEtat() {
  if (!MQTT_ENABLED) return;
  const char* s;
  switch (etatPorte) {
    case PORTE_OUVERTE:        s = "open";    break;
    case PORTE_FERMEE:         s = "closed";  break;
    case PORTE_EN_OUVERTURE:   s = "opening"; break;
    case PORTE_EN_FERMETURE:   s = "closing"; break;
    default:                   s = "error";   break;
  }
  mqtt.publish(TOPIC_ETAT_PORTE, s, true);
}

void publishMode() {
  if (!MQTT_ENABLED) return;
  const char* modes[] = { "soleil", "luminosite", "heure_fixe", "manuel" };
  mqtt.publish(TOPIC_MODE, modes[modeActuel], false);  // pas de retain — évite les boucles
}

void publishCapteurs() {
  if (!MQTT_ENABLED) return;
  float lux = bh1750.readLightLevel();
  float amp = lireCourant();
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f", lux);
  mqtt.publish(TOPIC_LUMINOSITE, buf);
  snprintf(buf, sizeof(buf), "%.2f", amp);
  mqtt.publish(TOPIC_COURANT, buf);
}

void publishHeuresSoleil() {
  if (!MQTT_ENABLED) return;
  struct tm t; if (!getLocalTime(&t)) return;
  HeureSoleil s = calculSoleil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, LATITUDE, LONGITUDE);
  int ouvMin = heureEnMinutes(s.leverH,   s.leverM,   offsetOuv);
  int ferMin = heureEnMinutes(s.coucherH, s.coucherM, offsetFer);
  char buf[256];
  snprintf(buf, sizeof(buf),
    "{\"lever\":\"%02d:%02d\",\"coucher\":\"%02d:%02d\","
    "\"ouverture\":\"%02d:%02d\",\"fermeture\":\"%02d:%02d\"}",
    s.leverH, s.leverM, s.coucherH, s.coucherM,
    ouvMin / 60, ouvMin % 60, ferMin / 60, ferMin % 60);
  mqtt.publish(TOPIC_HEURE_SOLEIL, buf, true);
}


// ============================================================
//  COMMANDES MANUELLES (depuis MQTT ou WebSocket)
// ============================================================
void commandeOuvrir() {
  activerForceManuel();
  ouvrirPorte();
}

void commandeFermer() {
  activerForceManuel();
  fermerPorte();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  Serial.printf("[MQTT] %s → %s\n", topic, msg.c_str());

  // Ignorer les retain au démarrage (sauf commandes porte)
  if (millis() < ignoreRetainUntil && String(topic) == TOPIC_MODE) {
    Serial.println("[MQTT] Retain ignoré (démarrage)");
    return;
  }

  if (String(topic) == TOPIC_COMMANDE_PORTE) {
    if (msg == "OPEN")  commandeOuvrir();
    if (msg == "CLOSE") commandeFermer();
    if (msg == "STOP")  stopUrgence();
    if (msg == "RESET") resetErreur();
  }
  else if (String(topic) == TOPIC_MODE) {
    ModeGestion nouveau;
    if      (msg == "soleil")     nouveau = MODE_SOLEIL;
    else if (msg == "luminosite") nouveau = MODE_LUMINOSITE;
    else if (msg == "heure_fixe") nouveau = MODE_HEURE_FIXE;
    else                          nouveau = MODE_MANUEL;
    // Appliquer immédiatement si porte stable, sinon mettre en attente
    if (etatPorte == PORTE_EN_OUVERTURE || etatPorte == PORTE_EN_FERMETURE) {
      modePending = nouveau;
      modePendingActif = true;
      Serial.println("[MODE] En mouvement — mode mis en attente");
    } else {
      if (nouveau != modeActuel) {   // ne publier que si le mode change réellement
        modeActuel = nouveau;
        publishMode();
      }
      wsBroadcastState();
    }
  }
  else if (String(topic) == TOPIC_CONFIG) {
    // JSON de configuration complet
    // ex: {"offset_ouv":15,"offset_fer":-10,"lux_ouv":800,"lux_fer":100,"lux_conf":60,
    //       "hfx_ouv_h":7,"hfx_ouv_m":30,"hfx_ouv_off":0,
    //       "hfx_fer_h":20,"hfx_fer_m":0,"hfx_fer_off":0}
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, msg) == DeserializationError::Ok) {
      if (doc.containsKey("offset_ouv"))   offsetOuv           = doc["offset_ouv"];
      if (doc.containsKey("offset_fer"))   offsetFer           = doc["offset_fer"];
      if (doc.containsKey("lux_ouv"))      luxSeuilOuverture   = doc["lux_ouv"];
      if (doc.containsKey("lux_fer"))      luxSeuilFermeture   = doc["lux_fer"];
      if (doc.containsKey("lux_conf"))     luxConfirmationSec  = doc["lux_conf"];
      if (doc.containsKey("hfx_ouv_h"))   heureFixeOuvH       = doc["hfx_ouv_h"];
      if (doc.containsKey("hfx_ouv_m"))   heureFixeOuvM       = doc["hfx_ouv_m"];
      if (doc.containsKey("hfx_ouv_off")) offsetFixeOuv       = doc["hfx_ouv_off"];
      if (doc.containsKey("hfx_fer_h"))   heureFixeFerH       = doc["hfx_fer_h"];
      if (doc.containsKey("hfx_fer_m"))   heureFixeFerM       = doc["hfx_fer_m"];
      if (doc.containsKey("hfx_fer_off")) offsetFixeFer       = doc["hfx_fer_off"];
      sauvegarderConfig();
      wsBroadcastState();
    }
  }
}

bool connecterMQTT() {
  if (!MQTT_ENABLED) return true;
  if (mqtt.connected()) return true;
  Serial.print("[MQTT] Connexion...");
  if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS,
                   TOPIC_ETAT_PORTE, 1, true, "offline")) {
    Serial.println(" OK");
    mqtt.subscribe(TOPIC_COMMANDE_PORTE);
    mqtt.subscribe(TOPIC_MODE);
    mqtt.subscribe(TOPIC_CONFIG);
    publishEtat();
    publishMode();
    ignoreRetainUntil = millis() + 3000;  // ignorer les retain pendant 3s
    return true;
  }
  Serial.printf(" Échec (%d)\n", mqtt.state());
  return false;
}

// ============================================================
//  WEBSOCKET — déclarations (implémentation dans web_server.ino)
// ============================================================
// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== PORTE POULAILLER v2 — Démarrage ===");

  // Moteur
  // ESP32 Arduino v3.x : nouvelle API PWM (ledcAttach remplace ledcSetup+ledcAttachPin)
  ledcAttach(PIN_RPWM, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PIN_LPWM, PWM_FREQ, PWM_RESOLUTION);
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, HIGH);
  stopMoteur();

  // Fins de course NF
  pinMode(PIN_FDC_OUV, INPUT_PULLUP);
  pinMode(PIN_FDC_FER, INPUT_PULLUP);

  // ACS712
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // I2C + BH1750
  Wire.begin(PIN_SDA, PIN_SCL);
  bh1750.begin() ? Serial.println("[BH1750] OK") : Serial.println("[BH1750] Non trouvé");

  // État initial
  delay(100);
  if      (digitalRead(PIN_FDC_OUV) == LOW)  { etatPorte = PORTE_OUVERTE; Serial.println("[INIT] Porte OUVERTE (gauche)"); }
  else if (digitalRead(PIN_FDC_FER) == LOW)  { etatPorte = PORTE_FERMEE;  Serial.println("[INIT] Porte FERMÉE (droite)");  }
  else                                        { etatPorte = PORTE_ERREUR;  Serial.println("[INIT] Position inconnue");       }

  // Charger la config depuis NVS
  chargerConfig();

  // WiFi
  Serial.printf("[WiFi] Connexion à %s...", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 30) { delay(500); Serial.print("."); t++; }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("\n[WiFi] Échec — mode hors ligne");

  // NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", TZ_INFO, 1);
  tzset();
  struct tm timeinfo; t = 0;
  while (!getLocalTime(&timeinfo) && t < 10) { delay(1000); t++; }
  if (getLocalTime(&timeinfo))
    Serial.printf("[NTP] %02d/%02d/%04d %02d:%02d\n",
      timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900,
      timeinfo.tm_hour, timeinfo.tm_min);

  // MQTT
  if (MQTT_ENABLED) {
    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    connecterMQTT();
  }

  // Serveur web
  setupWebServer();

  publishHeuresSoleil();
  Serial.println("=== Prêt ===");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // MQTT
  if (MQTT_ENABLED) {
    if (!mqtt.connected()) {
      static unsigned long derniereTentative = 0;
      if (millis() - derniereTentative > 5000) {
        derniereTentative = millis();
        connecterMQTT();
      }
    }
    mqtt.loop();
  }

  // WebSocket
  ws.cleanupClients();
  static unsigned long dernierBroadcast = 0;
  if (millis() - dernierBroadcast > 2000) {
    dernierBroadcast = millis();
    wsBroadcastState();
  }

  // ── Fins de course ───────────────────────────────────────
  bool fdcOuv = (digitalRead(PIN_FDC_OUV) == LOW);
  bool fdcFer = (digitalRead(PIN_FDC_FER) == LOW);

  if (etatPorte == PORTE_EN_OUVERTURE && fdcOuv) {
    stopMoteur();
    etatPorte = PORTE_OUVERTE;
    Serial.println("[FDC] Porte ouverte ✓ (position gauche)");
    publishEtat();
    publishHeuresSoleil();
    wsLog("Porte ouverte — fin de course gauche atteint", "ok");
    appliquerModePending();
  }
  else if (etatPorte == PORTE_EN_FERMETURE && fdcFer) {
    stopMoteur();
    etatPorte = PORTE_FERMEE;
    Serial.println("[FDC] Porte fermée ✓ (position droite)");
    publishEtat();
    wsLog("Porte fermée — fin de course droit atteint", "ok");
    appliquerModePending();
  }

  // ── Sécurité — stopper le moteur si état non-mouvement ────
  if (etatPorte != PORTE_EN_OUVERTURE && etatPorte != PORTE_EN_FERMETURE) {
    ledcWrite(PIN_RPWM, 0);
    ledcWrite(PIN_LPWM, 0);
  }

  // ── Timeout moteur ───────────────────────────────────────
  if (etatPorte == PORTE_EN_OUVERTURE || etatPorte == PORTE_EN_FERMETURE) {
    unsigned long timeoutActuel = (etatPorte == PORTE_EN_OUVERTURE) ? TIMEOUT_OUV_MS : TIMEOUT_FER_MS;
    if (millis() - tempsDebutMouvement > timeoutActuel) {
      stopMoteur();
      etatPorte = PORTE_ERREUR;
      Serial.println("[TIMEOUT] Fin de course non atteint !");
      if (MQTT_ENABLED) mqtt.publish(TOPIC_ETAT_PORTE, "error", true);
      wsLog("Timeout moteur — vérifier les fins de course", "error");
      appliquerModePending();
    }
    // ── Surcourant (blocage) ─────────────────────────────
    if (surcourant()) {
      ledcWrite(PIN_RPWM, 0);   // stop immédiat sans rampe
      ledcWrite(PIN_LPWM, 0);
      stopMoteur();
      etatPorte = PORTE_ERREUR;
      Serial.println("[SURCOURANT] Blocage détecté !");
      if (MQTT_ENABLED) mqtt.publish(TOPIC_ETAT_PORTE, "error", true);
      wsLog("Surcourant — obstacle détecté, moteur arrêté", "error");
      appliquerModePending();
    }
  }

  // ── Gestion automatique (toutes les 60 s) ────────────────
  if (millis() - dernierCheckAuto > 60000) {
    dernierCheckAuto = millis();
    switch (modeActuel) {
      case MODE_SOLEIL:     gestionSoleil();     break;
      case MODE_HEURE_FIXE: gestionHeureFixe();  break;
      case MODE_MANUEL:     break;
      default: break;
    }
  }
  // Mode luminosité : vérification plus fréquente (toutes les 5 s)
  if (modeActuel == MODE_LUMINOSITE) {
    static unsigned long dernierLux = 0;
    if (millis() - dernierLux > 5000) {
      dernierLux = millis();
      gestionLuminosite();
    }
  }

  // ── Publication périodique capteurs (30 s) ───────────────
  if (millis() - dernierPublish > 30000) {
    dernierPublish = millis();
    publishCapteurs();
    static unsigned long dernierSoleil = 0;
    if (millis() - dernierSoleil > 3600000) {
      dernierSoleil = millis();
      publishHeuresSoleil();
    }
  }

  delay(100);
}

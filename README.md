# 🐔 Porte de poulailler automatique — Rétrofit Porte marque Omlet (FR)
[English version available below](#-automatic-chicken-coop-door--omlet-autodoor-retrofit---english-version)

Contrôleur basé sur ESP32 pour automatiser une porte de poulailler coulissante, en récupérant la mécanique d'une porte Omlet Autodoor existante. 
Intégration Home Assistant possible via MQTT, interface web mobile embarquée, 4 modes de gestion.

![Capture d'écran de l'interface web](https://github.com/bonnoa/retrofit_omlet_automatic_door/blob/15a76a0ba2ad137b84e32b59a12258cbaffb4ce7/ePoule.png)
---

## Contexte

Après deux portes HS (mais moteur Ok), et quelques poules croquées par un renard plus tard... je décide de supprimer le panneau de commande de la porte automatique Omlet pour ne conserver que la mécanique.

Le projet part d'une porte automatique de la marque Omlet dont on conserve uniquement la mécanique (moteur 014.0077, rail, porte coulissante). L'électronique d'origine est remplacée par un contrôleur ESP32.
Par contre exit le capteur de pression latérale qui détecte la présence d'un objet (poule ?) sur la route. C'est trop souvent la source de dysfonctionnement et d'erreur, ce principe sera géré par une surtension détectée sur le moteur.

La porte coulisse **horizontalement** : droite → gauche pour ouvrir, gauche → droite pour fermer.

---

## Fonctionnalités

- **4 modes de gestion** : soleil (calcul NOAA lever/coucher), luminosité (seuils lux + confirmation), heure fixe, manuel
- **Intégration MQTT** via MQTT (broker Home Assistant par exemple)
- **Interface web mobile-first** embarquée sur l'ESP32 (WebSocket, temps réel)
- **Sécurités** : fins de course magnétiques, détection surcourant (ACS712), timeout moteur
- **Override manuel** : commande manuelle suspend l'automatisme jusqu'à la prochaine échéance
- **Persistance des paramètres** : sauvegarde NVS Flash
- **Heure été/hiver automatique** : fuseau horaire POSIX France (CET-1CEST,M3.5.0,M10.5.0/3)
- **Démarrage progressif** : rampe PWM pour limiter le pic de courant

---

## Matériel requis

| Composant | Référence | Rôle |
|---|---|---|
| Microcontrôleur | ESP32-WROOM-32U DevKitC | Cerveau + WiFi + serveur web |
| Contrôleur moteur | BTS7960 / IBT-2 | Pilotage moteur bidirectionnel PWM |
| Fins de course | MC-38 × 2 | Détection position ouverte/fermée |
| Capteur courant | ACS712 5A | Détection surcourant / blocage |
| Capteur luminosité | BH1750 (GY-30) | Mode luminosité + log |
| Convertisseur | LM2596S | 12V → 5V alimentation ESP32 (finalement pas utilisé dans mon projet) |
| Alimentation | 12V 2A | Alimentation principale pour le moteur |
| Alimentation USB | 5V | Alimentation de l'ESP32 et composants associés |
| Boîtier | 190×150×75 mm IP67 ABS | Protection étanche extérieure |
| Câble signal | Câble alarme 4×0.22mm² | FDC et BH1750 vers boîtier |

La seule alimentation 12V pour le moteur, le convertisseur LM2596S (12 → 5V) n'est pas suffisante.
J'ai donc fait le choix de mettre en place 2 alimentations.
Mais la mise en place d'un condensateur sur la sortie 5V du LM2596S est à tester (pas testé pour ma part)
— ajouter 470µF à 1000µF / 10V entre la sortie 5V et GND, directement sur les pattes d'alimentation de l'ESP32. 

---

## Logiciel

### Structure du projet

```
porte_poulailler/
├── porte_poulailler.ino   ← Logique principale (moteur, modes, MQTT, NVS)
└── web_server_v2.ino      ← Serveur web + WebSocket + HTML embarqué
```

> Les deux fichiers `.ino` doivent être dans le même dossier. L'IDE Arduino les compile automatiquement ensemble.

### Dépendances

Installer via le gestionnaire de bibliothèques Arduino :

| Bibliothèque | Auteur |
|---|---|
| PubSubClient | Nick O'Leary |
| ArduinoJson | Benoit Blanchon |
| BH1750 | Christopher Laws |
| ESP Async WebServer | **ESP32Async** (github.com/ESP32Async/ESPAsyncWebServer) |
| AsyncTCP | **ESP32Async** (github.com/ESP32Async/AsyncTCP) |

> ⚠️ Utiliser les forks **ESP32Async** et non les dépôts originaux me-no-dev (archivés).

### Configuration Arduino IDE

Package ESP32 — ajouter dans Fichier → Préférences → URLs supplémentaires :
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Paramètres de carte :

| Paramètre | Valeur |
|---|---|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| CPU Frequency | 240MHz (WiFi/BT) |
| Flash Mode | DIO |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs |
| PSRAM | Disabled |

---

## Configuration

### Paramètres obligatoires dans `porte_poulailler.ino`

```cpp
#define MQTT_ENABLED  true   // true = MQTT actif, false = désactivé

const char* WIFI_SSID     = "TON_SSID";
const char* WIFI_PASSWORD = "TON_MOT_DE_PASSE";

const char* MQTT_SERVER   = "192.168.1.XX";  // IP du broker Home Assistant
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "mqtt_user";
const char* MQTT_PASS     = "mqtt_password";

const float LATITUDE      = 48.8566;  // Coordonnées GPS pour calcul soleil
const float LONGITUDE     = 2.3522;
```

### Paramètres à calibrer après montage

```cpp
#define ACS712_ZERO_ADC   3050  // Valeur ADC à 0A — mesurer à moteur arrêté
#define ACS712_SEUIL_ADC  310   // Delta déclenchant le surcourant (~2.7A)
#define PWM_DUTY_9V       191   // Duty cycle ouverture (~9V sur alim 12V)
#define PWM_DUTY_FER      140   // Duty cycle fermeture (~7V, plus doux)
#define TIMEOUT_OUV_MS    15000 // Timeout ouverture en ms
#define TIMEOUT_FER_MS    20000 // Timeout fermeture en ms
#define RAMPE_MS          300   // Durée rampe PWM démarrage moteur
```

---

## Câblage

### Affectation des pins ESP32

| GPIO | Composant | Signal |
|---|---|---|
| 25 | BTS7960 | RPWM (ouverture) |
| 26 | BTS7960 | LPWM (fermeture) |
| 27 | BTS7960 | EN (R_EN + L_EN reliés) |
| 32 | MC-38 gauche | FDC position ouverte |
| 33 | MC-38 droit | FDC position fermée |
| 34 | ACS712 | OUT (analogique) |
| 21 | BH1750 | SDA |
| 22 | BH1750 | SCL |
| VIN | LM2596S | 5V sortie |
| GND | Tous | Masse commune |

### ACS712 — câblage en série sur M+

```
BTS7960 (OUT+) → ACS712 (IP+) → (IP−) → Moteur (M+)
Moteur (M−) → BTS7960 (OUT−)
```

### MC-38 — fins de course

Câblage sans polarité (interchangeable) :
- Un fil → GPIO32 (OUV) ou GPIO33 (FER)
- L'autre fil → GND

Comportement NO : aimant présent → contact fermé → pin LOW.

### BH1750

```
VCC  → 3.3V ESP32
GND  → GND commun
SCL  → GPIO22
SDA  → GPIO21
ADDR → Non connecté (adresse 0x23)
```

---

## Topics MQTT

| Topic | Direction | Valeurs |
|---|---|---|
| `poulailler/porte/etat` | Publish | `open` / `closed` / `opening` / `closing` / `error` |
| `poulailler/porte/commande` | Subscribe | `OPEN` / `CLOSE` / `STOP` / `RESET` |
| `poulailler/capteurs/lux` | Publish | Float (lux) |
| `poulailler/capteurs/courant` | Publish | Float (ampères) |
| `poulailler/soleil/heures` | Publish | JSON lever/coucher/ouverture/fermeture |
| `poulailler/mode` | Pub/Sub | `soleil` / `luminosite` / `heure_fixe` / `manuel` |
| `poulailler/config` | Subscribe | JSON de configuration complet |

---

## Modes de gestion

### Mode Soleil
Calcul astronomique NOAA embarqué. Ouvre après le lever + offset, ferme après le coucher + offset. Offsets configurables de −60 à +120 minutes.

### Mode Luminosité
Ouvre si luminosité > seuil pendant une durée de confirmation (évite les fausses détections nuageuses). Ferme si luminosité < seuil pendant la même durée.

### Mode Heure Fixe
Heures d'ouverture et fermeture définies manuellement, avec offsets optionnels.

### Mode Manuel
Aucune automatisation. Commandes uniquement via interface web ou MQTT.

### Override manuel
En modes automatiques, une commande manuelle suspend l'automatisme jusqu'à la prochaine échéance naturelle (lever/coucher ou 30 min en mode luminosité). Un badge violet s'affiche dans l'interface.

---

## Interface web

Accessible à `http://[IP_ESP32]` depuis n'importe quel navigateur sur le réseau local.
IP visible au démarrage de l'ESP32 dans le moniteur Arduino.

- État porte temps réel + heure ESP32
- Boutons Ouvrir / Fermer / Stop d'urgence (double confirmation)
- 4 onglets de configuration des modes
- Graphique courant moteur
- Journal des événements (20 entrées)
- Bouton Réinitialiser l'erreur (visible uniquement en état erreur)

---

## Sécurités

| Protection | Condition | Action |
|---|---|---|
| Fins de course | FDC atteint pendant mouvement | Arrêt immédiat |
| Timeout ouverture | FDC non atteint après 15s | Arrêt + état ERREUR |
| Timeout fermeture | FDC non atteint après 20s | Arrêt + état ERREUR |
| Surcourant ACS712 | Courant > seuil (~2.7A) | Arrêt + état ERREUR |
| Garde universelle | Moteur actif hors mouvement | PWM forcé à 0 |

En état ERREUR : bouton **Réinitialiser** dans l'interface ou commande MQTT `RESET` sur `poulailler/porte/commande`.

---

## Calibration ACS712

```cpp
// Sketch de test rapide
#define PIN_ACS712    34
#define ACS712_ZERO_ADC 3050  // Valeur à remplacer

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}
void loop() {
  int raw = analogRead(PIN_ACS712);
  float amperes = abs(raw - ACS712_ZERO_ADC) / 115.0;
  Serial.printf("RAW: %d  |  Courant: %.2f A\n", raw, amperes);
  delay(500);
}
```

1. Téléverser ce sketch moteur **débranché**
2. Relever la valeur RAW moyenne → c'est `ACS712_ZERO_ADC`
3. Brancher le moteur, faire tourner sans blocage → noter le delta max
4. Bloquer manuellement la porte → noter le delta en blocage
5. Placer `ACS712_SEUIL_ADC` à mi-chemin entre les deux

---

## Dépannage

| Symptôme | Solution |
|---|---|
| `[BH1750] Device is not configured!` | Vérifier SDA→21, SCL→22, VCC→3.3V, GND |
| ESP32 reboot au démarrage moteur | Ajouter condensateur 470µF sur sortie 5V LM2596S, ou alimenter l'ESP32 via USB indépendant |
| Surcourant intempestif | Recalibrer ACS712_ZERO_ADC avec sketch de test |
| Boucle MQTT sur le mode | Purger le retain : publier payload vide sur `poulailler/mode` dans HA |
| `[INIT] Position inconnue` | Déplacer la porte sur un FDC manuellement puis redémarrer |
| Interface web inaccessible | Vérifier l'IP dans le moniteur série (115200 bauds) |

---

## Licence

Projet personnel DIY — libre de réutilisation et d'adaptation.


---


# 🐔 Automatic Chicken Coop Door — Omlet Autodoor Retrofit - ENGLISH VERSION

DIY ESP32-based controller to automate a sliding chicken coop door, reusing the mechanics of an existing Omlet Autodoor. Home Assistant integration via MQTT, embedded mobile-first web interface, 4 management modes.

**The web version isn't available in English. Feel free to open a new GitHub issue to request it.**

---

## Context

This project reuses the mechanics of an Omlet Autodoor (motor 014.0077, rail, sliding door) while replacing the original electronics with a fully DIY ESP32 controller.

The door slides **horizontally**: right → left to open, left → right to close.

---

## Features

- **4 management modes**: sun (NOAA sunrise/sunset calculation), luminosity (lux thresholds + confirmation delay), fixed time, manual
- **Home Assistant integration** via MQTT (HA broker)
- **Embedded mobile-first web interface** on the ESP32 (WebSocket, real-time)
- **Safety systems**: magnetic limit switches, overcurrent detection (ACS712), motor timeout, universal guard
- **Manual override**: a manual command temporarily suspends automation until the next scheduled event
- **Parameter persistence**: NVS Flash storage, survives reboots
- **Automatic DST handling**: POSIX timezone string for France (CET-1CEST,M3.5.0,M10.5.0/3)
- **Soft motor start**: PWM ramp to limit inrush current

---

## Hardware

| Component | Reference | Role |
|---|---|---|
| Microcontroller | ESP32-WROOM-32U DevKitC | Brain + WiFi + web server |
| Motor driver | BTS7960 / IBT-2 | Bidirectional PWM motor control |
| Limit switches | MC-38 × 2 | Open/closed position detection |
| Current sensor | ACS712 5A | Overcurrent / jam detection |
| Light sensor | BH1750 (GY-30) | Luminosity mode + logging |
| Voltage converter | LM2596S (adjustable) | 12V → 5V for ESP32 |
| Power supply | 12V 2A | Main power |
| Enclosure | 190×150×75 mm IP67 ABS | Weatherproof outdoor housing |
| Signal cable | 4-wire alarm cable 0.22mm² | Limit switches and BH1750 to enclosure |

---

## Software

**The web version isn't available in English. Feel free to open a new GitHub issue to request it.**

### Project structure

```
porte_poulailler/
├── porte_poulailler.ino   ← Main logic (motor, modes, MQTT, NVS)
└── web_server_v2.ino      ← Web server + WebSocket + embedded HTML
```

> Both `.ino` files must be in the same folder. The Arduino IDE compiles them together automatically.

### Dependencies

Install via the Arduino Library Manager:

| Library | Author |
|---|---|
| PubSubClient | Nick O'Leary |
| ArduinoJson | Benoit Blanchon |
| BH1750 | Christopher Laws |
| ESP Async WebServer | **ESP32Async** (github.com/ESP32Async/ESPAsyncWebServer) |
| AsyncTCP | **ESP32Async** (github.com/ESP32Async/AsyncTCP) |

> ⚠️ Use the **ESP32Async** forks — the original me-no-dev repositories are archived.

### Arduino IDE configuration

ESP32 package — add in File → Preferences → Additional Board Manager URLs:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Board settings:

| Parameter | Value |
|---|---|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| CPU Frequency | 240MHz (WiFi/BT) |
| Flash Mode | DIO |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs |
| PSRAM | Disabled |

---

## Configuration

### Required parameters in `porte_poulailler.ino`

```cpp
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

const char* MQTT_SERVER   = "192.168.1.XX";  // Home Assistant broker IP
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "mqtt_user";
const char* MQTT_PASS     = "mqtt_password";

const float LATITUDE      = 48.8566;  // GPS coordinates for sun calculation
const float LONGITUDE     = 2.3522;
```

### Parameters to calibrate after assembly

```cpp
#define MQTT_ENABLED      true   // true = MQTT active, false = disabled

#define ACS712_ZERO_ADC   3050   // ADC value at 0A — measure with motor stopped
#define ACS712_SEUIL_ADC  310    // Delta triggering overcurrent (~2.7A)
#define PWM_DUTY_9V       191    // Opening duty cycle (~9V on 12V supply)
#define PWM_DUTY_FER      140    // Closing duty cycle (~7V, softer)
#define TIMEOUT_OUV_MS    15000  // Opening timeout in ms
#define TIMEOUT_FER_MS    20000  // Closing timeout in ms
#define RAMPE_MS          300    // PWM ramp duration at motor start
```

---

## Wiring

### ESP32 pin assignment

| GPIO | Component | Signal |
|---|---|---|
| 25 | BTS7960 | RPWM (opening) |
| 26 | BTS7960 | LPWM (closing) |
| 27 | BTS7960 | EN (R_EN + L_EN tied together) |
| 32 | MC-38 left | Limit switch open position |
| 33 | MC-38 right | Limit switch closed position |
| 34 | ACS712 | OUT (analog input) |
| 21 | BH1750 | SDA |
| 22 | BH1750 | SCL |
| VIN | LM2596S | 5V output |
| GND | All | Common ground |

### ACS712 — in-series wiring on M+

```
BTS7960 (OUT+) → ACS712 (IP+) → (IP−) → Motor (M+)
Motor (M−) → BTS7960 (OUT−)
```

### MC-38 — limit switches

Wiring has no polarity (wires are interchangeable):
- One wire → GPIO32 (open) or GPIO33 (closed)
- Other wire → GND

NO behavior: magnet present → contact closed → pin LOW.

### BH1750

```
VCC  → 3.3V ESP32
GND  → Common GND
SCL  → GPIO22
SDA  → GPIO21
ADDR → Not connected (address 0x23)
```

---

## MQTT Topics

| Topic | Direction | Values |
|---|---|---|
| `poulailler/porte/etat` | Publish | `open` / `closed` / `opening` / `closing` / `error` |
| `poulailler/porte/commande` | Subscribe | `OPEN` / `CLOSE` / `STOP` / `RESET` |
| `poulailler/capteurs/lux` | Publish | Float (lux) |
| `poulailler/capteurs/courant` | Publish | Float (amperes) |
| `poulailler/soleil/heures` | Publish | JSON sunrise/sunset/opening/closing times |
| `poulailler/mode` | Pub/Sub | `soleil` / `luminosite` / `heure_fixe` / `manuel` |
| `poulailler/config` | Subscribe | Full configuration JSON |

---

## Management Modes

### Sun Mode
Embedded NOAA astronomical calculation. Opens after sunrise + offset, closes after sunset + offset. Offsets configurable from −60 to +120 minutes.

### Luminosity Mode
Opens if brightness > threshold for a confirmation duration (avoids false triggers from clouds). Closes if brightness < threshold for the same duration.

### Fixed Time Mode
Manually defined opening and closing times, with optional offsets.

### Manual Mode
No automation. Commands only via web interface or MQTT.

### Manual Override
In automatic modes, a manual command suspends automation until the next natural event (sunrise/sunset or 30 minutes in luminosity mode). A purple badge appears in the interface.

---

## Web Interface

Accessible at `http://[ESP32_IP]` from any browser on the local network.

- Real-time door status + ESP32 clock
- Open / Close / Emergency Stop buttons (double confirmation)
- 4 mode configuration tabs
- Motor current graph
- Event log (20 entries)
- Reset Error button (visible only in error state)

---

## Safety Systems

| Protection | Condition | Action |
|---|---|---|
| Limit switches | Limit switch reached during movement | Immediate stop |
| Opening timeout | Limit switch not reached after 15s | Stop + ERROR state |
| Closing timeout | Limit switch not reached after 20s | Stop + ERROR state |
| ACS712 overcurrent | Current > threshold (~2.7A) | Stop + ERROR state |
| Universal guard | Motor active outside movement state | PWM forced to 0 |

In ERROR state: **Reset** button in the web interface or MQTT command `RESET` on `poulailler/porte/commande`.

---

## ACS712 Calibration

```cpp
// Quick test sketch
#define PIN_ACS712      34
#define ACS712_ZERO_ADC 3050  // Value to replace

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}
void loop() {
  int raw = analogRead(PIN_ACS712);
  float amperes = abs(raw - ACS712_ZERO_ADC) / 115.0;
  Serial.printf("RAW: %d  |  Current: %.2f A\n", raw, amperes);
  delay(500);
}
```

1. Upload this sketch with motor **disconnected**
2. Record the average RAW value → this is `ACS712_ZERO_ADC`
3. Connect the motor, run without jamming → record max delta
4. Manually jam the door → record delta under jam
5. Set `ACS712_SEUIL_ADC` halfway between the two values

---

## Troubleshooting

| Symptom | Solution |
|---|---|
| `[BH1750] Device is not configured!` | Check SDA→21, SCL→22, VCC→3.3V, GND |
| ESP32 reboots at motor start | Add 470µF capacitor on LM2596S 5V output, or power ESP32 via independent USB charger |
| Spurious overcurrent detection | Recalibrate ACS712_ZERO_ADC with test sketch |
| MQTT mode loop | Clear retain: publish empty payload on `poulailler/mode` in HA Dev Tools |
| `[INIT] Position inconnue` | Manually move door onto a limit switch then reboot |
| Web interface unreachable | Check IP in serial monitor (115200 baud) |

---

## License

Personal DIY project — free to reuse and adapt.

/*
 * ============================================================
 *  PORTE POULAILLER — Serveur Web + WebSocket  v2
 *  À placer dans le même dossier que porte_poulailler.ino
 *
 *  Dépendances supplémentaires :
 *    - ESPAsyncWebServer  (ESP32Async) https://github.com/ESP32Async/ESPAsyncWebServer
 *    - AsyncTCP           (ESP32Async) https://github.com/ESP32Async/AsyncTCP
 * ============================================================
 */

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ── HTML embarqué en flash (PROGMEM) ────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
<title>Poulailler</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=Syne:wght@400;600;800&display=swap');
:root {
  --bg:#0d1117; --surface:#161b22; --surface2:#1c2128; --border:#30363d;
  --accent:#f0a500; --accent2:#e05a00; --green:#3fb950; --red:#f85149;
  --blue:#58a6ff; --purple:#a78bfa; --teal:#2dd4bf; --muted:#8b949e;
  --text:#e6edf3; --radius:16px;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:'Syne',sans-serif;min-height:100vh;padding:0 0 48px;overflow-x:hidden}
header{background:var(--surface);border-bottom:1px solid var(--border);padding:18px 20px 14px;position:sticky;top:0;z-index:100;display:flex;align-items:center;justify-content:space-between}
.header-left{display:flex;align-items:center;gap:12px}
.logo{width:40px;height:40px;background:var(--accent);border-radius:11px;display:flex;align-items:center;justify-content:center;font-size:20px;flex-shrink:0}
h1{font-size:19px;font-weight:800;letter-spacing:-.5px}h1 span{color:var(--accent)}
.status-pill{display:flex;align-items:center;gap:7px;background:var(--surface2);border:1px solid var(--border);border-radius:100px;padding:5px 12px;font-family:'Space Mono',monospace;font-size:10px;flex-shrink:0}
.dot{width:7px;height:7px;border-radius:50%;background:var(--green);box-shadow:0 0 7px var(--green);animation:pulse 2s infinite}
.dot.offline{background:var(--red);box-shadow:0 0 7px var(--red);animation:none}
.dot.moving{background:var(--accent);box-shadow:0 0 7px var(--accent)}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}
main{padding:16px 14px;display:flex;flex-direction:column;gap:14px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius);padding:18px}
.card-title{font-size:10px;font-weight:600;letter-spacing:1.5px;text-transform:uppercase;color:var(--muted);margin-bottom:14px}

/* ── PORTE ── */
.porte-card{background:linear-gradient(135deg,var(--surface),var(--surface2));position:relative;overflow:hidden}
.porte-card::before{content:'';position:absolute;top:-30px;right:-30px;width:130px;height:130px;background:var(--accent);opacity:.04;border-radius:50%}
.porte-etat{display:flex;align-items:center;gap:14px;margin-bottom:6px}
.porte-icon{width:58px;height:58px;border-radius:13px;display:flex;align-items:center;justify-content:center;font-size:28px;background:var(--surface2);border:1px solid var(--border);transition:all .4s ease;flex-shrink:0}
.porte-icon.open{background:rgba(63,185,80,.15);border-color:var(--green)}
.porte-icon.closed{background:rgba(248,81,73,.12);border-color:var(--red)}
.porte-icon.moving{background:rgba(240,165,0,.15);border-color:var(--accent)}
.porte-icon.error{background:rgba(248,81,73,.12);border-color:var(--red)}
.porte-info h2{font-size:24px;font-weight:800;letter-spacing:-1px;transition:color .3s}
.porte-info h2.open{color:var(--green)}.porte-info h2.closed{color:var(--red)}
.porte-info h2.moving{color:var(--accent)}.porte-info h2.error{color:var(--red)}
.porte-info p{font-size:11px;color:var(--muted);font-family:'Space Mono',monospace;margin-top:3px}

/* Direction indicator */

.override-badge{display:none;margin-top:8px;padding:6px 10px;background:rgba(167,139,250,.12);border:1px solid var(--purple);border-radius:8px;font-size:10px;font-family:'Space Mono',monospace;color:var(--purple);text-align:center}
.override-badge.visible{display:block}
.progress-bar{height:3px;background:var(--surface2);border-radius:100px;margin-top:12px;overflow:hidden;opacity:0;transition:opacity .3s}
.progress-bar.visible{opacity:1}
.progress-fill{height:100%;background:linear-gradient(90deg,var(--accent),var(--accent2));border-radius:100px;animation:progress 20s linear forwards;transform-origin:left}
@keyframes progress{from{width:0%}to{width:100%}}

/* ── BOUTONS ── */
.btn-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.btn{border:none;border-radius:13px;padding:18px 14px;font-family:'Syne',sans-serif;font-size:14px;font-weight:700;cursor:pointer;display:flex;align-items:center;justify-content:center;gap:9px;transition:all .15s ease;-webkit-tap-highlight-color:transparent;user-select:none}
.btn:active{transform:scale(.95)}
.btn-open{background:rgba(63,185,80,.15);border:1.5px solid var(--green);color:var(--green)}
.btn-open:active{background:rgba(63,185,80,.25)}
.btn-close{background:rgba(248,81,73,.12);border:1.5px solid var(--red);color:var(--red)}
.btn-close:active{background:rgba(248,81,73,.22)}
.btn-stop{grid-column:1/-1;background:var(--surface2);border:1.5px solid var(--border);color:var(--muted);padding:12px;font-size:12px;letter-spacing:.5px;text-transform:uppercase}
.btn-stop:active{background:var(--border);color:var(--text)}
.btn-stop.armed{border-color:var(--red);color:var(--red);background:rgba(248,81,73,.08)}
.btn-reset{grid-column:1/-1;background:rgba(248,81,73,.12);border:1.5px solid var(--red);color:var(--red);padding:12px;font-size:12px;letter-spacing:.5px;text-transform:uppercase;display:none}
.btn-reset.visible{display:block}

/* ── MODE SELECTOR ── */
.mode-tabs{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.mode-tab{border:none;border-radius:11px;padding:12px 10px;font-family:'Syne',sans-serif;font-size:12px;font-weight:600;cursor:pointer;background:var(--surface2);border:1.5px solid var(--border);color:var(--muted);transition:all .2s;-webkit-tap-highlight-color:transparent;user-select:none;display:flex;flex-direction:column;align-items:center;gap:4px}
.mode-tab .tab-icon{font-size:18px}
.mode-tab.active{color:var(--text);border-color:var(--accent);background:rgba(240,165,0,.1)}
.mode-tab:active{transform:scale(.96)}
.mode-pending{font-size:10px;color:var(--accent);font-family:'Space Mono',monospace;margin-top:6px;text-align:center;padding:5px 10px;background:rgba(240,165,0,.08);border-radius:8px;border:1px solid rgba(240,165,0,.2)}

/* Panneaux de config par mode */
.mode-panels{margin-top:14px}
.mode-panel{display:none}
.mode-panel.active{display:block}

/* ── SOLEIL ── */
.soleil-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.soleil-item{background:var(--surface2);border:1px solid var(--border);border-radius:11px;padding:12px}
.soleil-item .label{font-size:9px;letter-spacing:1px;text-transform:uppercase;color:var(--muted);margin-bottom:5px}
.soleil-item .time{font-family:'Space Mono',monospace;font-size:18px;font-weight:700;color:var(--accent)}
.soleil-item .sublabel{font-size:9px;color:var(--muted);margin-top:3px;font-family:'Space Mono',monospace}
.divider{border:none;border-top:1px solid var(--border);margin:14px 0}

/* ── OFFSET / SPINNER ── */
.offset-row{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
.offset-row:last-child{margin-bottom:0}
.offset-label{font-size:13px;font-weight:600}
.offset-label span{display:block;font-size:10px;color:var(--muted);font-weight:400;margin-top:1px;font-family:'Space Mono',monospace}
.offset-ctrl{display:flex;align-items:center;gap:8px}
.offset-val{font-family:'Space Mono',monospace;font-size:16px;font-weight:700;min-width:46px;text-align:center}
.offset-val.positive{color:var(--green)}.offset-val.negative{color:var(--red)}.offset-val.zero{color:var(--muted)}
.btn-offset{width:34px;height:34px;border-radius:9px;border:1.5px solid var(--border);background:var(--surface2);color:var(--text);font-size:17px;cursor:pointer;display:flex;align-items:center;justify-content:center;transition:all .15s;-webkit-tap-highlight-color:transparent}
.btn-offset:active{background:var(--border);transform:scale(.91)}

/* ── LUMINOSITE MODE ── */
.lux-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:12px}
.lux-item{background:var(--surface2);border:1px solid var(--border);border-radius:11px;padding:12px}
.lux-item .label{font-size:9px;letter-spacing:1px;text-transform:uppercase;color:var(--muted);margin-bottom:5px}
.lux-item .val{font-family:'Space Mono',monospace;font-size:17px;font-weight:700;color:var(--accent)}
.lux-slider{width:100%;margin-top:6px;accent-color:var(--accent)}
.confirmation-row{display:flex;align-items:center;justify-content:space-between}
.conf-bar{flex:1;height:4px;background:var(--border);border-radius:100px;margin:0 10px;overflow:hidden}
.conf-fill{height:100%;background:var(--teal);border-radius:100px;width:0%;transition:width .5s linear}
.conf-label{font-family:'Space Mono',monospace;font-size:11px;color:var(--teal)}

/* ── HEURE FIXE ── */
.hfx-row{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
.hfx-row:last-child{margin-bottom:0}
.hfx-label{font-size:13px;font-weight:600}
.hfx-label span{display:block;font-size:10px;color:var(--muted);font-weight:400;margin-top:1px;font-family:'Space Mono',monospace}
.hfx-time-ctrl{display:flex;align-items:center;gap:6px}
.time-input{background:var(--surface2);border:1.5px solid var(--border);border-radius:9px;color:var(--text);font-family:'Space Mono',monospace;font-size:15px;font-weight:700;padding:6px 10px;width:70px;text-align:center;outline:none}
.time-input:focus{border-color:var(--accent)}

/* ── CAPTEURS ── */
.capteurs-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.capteur-item{background:var(--surface2);border:1px solid var(--border);border-radius:11px;padding:12px}
.c-label{font-size:9px;letter-spacing:1px;text-transform:uppercase;color:var(--muted);margin-bottom:5px}
.c-val{font-family:'Space Mono',monospace;font-size:20px;font-weight:700}
.c-unit{font-size:10px;color:var(--muted);font-family:'Space Mono',monospace;margin-top:2px}
.c-val.ok{color:var(--green)}.c-val.warn{color:var(--accent)}.c-val.danger{color:var(--red)}
.fdc-row{display:flex;gap:8px;margin-bottom:12px}
.fdc-item{flex:1;background:var(--surface2);border:1px solid var(--border);border-radius:9px;padding:9px;text-align:center}
.fdc-label{font-size:9px;color:var(--muted);letter-spacing:.8px;text-transform:uppercase}
.fdc-state{font-size:12px;font-weight:700;margin-top:3px;font-family:'Space Mono',monospace}
.fdc-state.active{color:var(--green)}.fdc-state.inactive{color:var(--muted)}

/* ── GRAPHIQUE ── */
.chart-wrap{position:relative;height:90px;margin-top:4px}
canvas{width:100%;height:100%;display:block}
.seuil-line{position:absolute;top:20%;left:0;right:0;height:1px;background:var(--red);opacity:.4}
.seuil-label{position:absolute;top:calc(20% - 13px);right:4px;font-size:8px;color:var(--red);font-family:'Space Mono',monospace;opacity:.7}

/* ── LOG ── */
.log-list{display:flex;flex-direction:column;gap:5px;max-height:240px;overflow-y:auto}
.log-item{display:flex;align-items:flex-start;gap:9px;padding:9px 11px;background:var(--surface2);border-radius:9px;border-left:3px solid var(--border);font-size:12px;animation:slideIn .2s ease}
.log-item.info{border-left-color:var(--blue)}.log-item.ok{border-left-color:var(--green)}
.log-item.warn{border-left-color:var(--accent)}.log-item.error{border-left-color:var(--red)}
@keyframes slideIn{from{opacity:0;transform:translateX(-6px)}to{opacity:1;transform:translateX(0)}}
.log-time{font-family:'Space Mono',monospace;font-size:9px;color:var(--muted);white-space:nowrap;margin-top:1px;flex-shrink:0}
.log-msg{color:var(--text);line-height:1.4}
footer{text-align:center;font-size:10px;color:var(--muted);padding-top:6px;font-family:'Space Mono',monospace}
</style>
</head>
<body>
<header>
  <div class="header-left">
    <div class="logo">🐔</div>
    <h1>Poula<span>iller</span></h1>
  </div>
  <div id="heureEsp" style="font-family:'Space Mono',monospace;font-size:10px;color:var(--muted);flex:1;text-align:center;letter-spacing:.5px">--:--</div>
  <div class="status-pill">
    <div class="dot" id="dotStatus"></div>
    <span id="statusLabel">Connexion...</span>
  </div>
</header>

<main>

  <!-- ── ÉTAT PORTE ── -->
  <div class="card porte-card">
    <div class="card-title">État de la porte</div>
    <div class="porte-etat">
      <div class="porte-icon" id="porteIcon">🚪</div>
      <div class="porte-info">
        <h2 id="porteEtat">--</h2>
        <p id="porteSub">En attente...</p>
      </div>
    </div>
    <div class="progress-bar" id="progressBar">
      <div class="progress-fill" id="progressFill"></div>
    </div>
    <div class="override-badge" id="overrideBadge">⏸ Mode auto suspendu — commande manuelle active</div>
  </div>

  <!-- ── CONTRÔLE ── -->
  <div class="card">
    <div class="card-title">Contrôle manuel</div>
    <div class="btn-grid">
      <button class="btn btn-open" onclick="envoyerCmd('OPEN')">
        ← Ouvrir
      </button>
      <button class="btn btn-close" onclick="envoyerCmd('CLOSE')">
        Fermer →
      </button>
      <button class="btn btn-stop" id="btnStop" onclick="stopUrgence()">
        ⏹ Stop d'urgence
      </button>
      <button class="btn btn-reset" id="btnReset" onclick="resetErreur()">
        🔄 Réinitialiser l'erreur
      </button>
    </div>
  </div>

  <!-- ── MODE ── -->
  <div class="card">
    <div class="card-title">Mode de gestion</div>
    <div class="mode-tabs">
      <button class="mode-tab active" id="tab-soleil"     onclick="selectionnerMode('soleil')">
        <span class="tab-icon">🌅</span>Soleil
      </button>
      <button class="mode-tab" id="tab-luminosite" onclick="selectionnerMode('luminosite')">
        <span class="tab-icon">💡</span>Luminosité
      </button>
      <button class="mode-tab" id="tab-heure_fixe" onclick="selectionnerMode('heure_fixe')">
        <span class="tab-icon">🕐</span>Heure fixe
      </button>
      <button class="mode-tab" id="tab-manuel"     onclick="selectionnerMode('manuel')">
        <span class="tab-icon">🖐</span>Manuel
      </button>
    </div>
    <div id="modePendingMsg" class="mode-pending" style="display:none">
      ⏳ Changement appliqué après fin du mouvement en cours
    </div>

    <div class="mode-panels">

      <!-- SOLEIL -->
      <div class="mode-panel active" id="panel-soleil">
        <div class="soleil-grid">
          <div class="soleil-item"><div class="label">🌅 Lever</div><div class="time" id="hLever">--:--</div><div class="sublabel">Ouverture</div></div>
          <div class="soleil-item"><div class="label">🌇 Coucher</div><div class="time" id="hCoucher">--:--</div><div class="sublabel">Fermeture</div></div>
          <div class="soleil-item"><div class="label">🔓 Ouvre à</div><div class="time" id="hOuverture">--:--</div><div class="sublabel">+offset</div></div>
          <div class="soleil-item"><div class="label">🔒 Ferme à</div><div class="time" id="hFermeture">--:--</div><div class="sublabel">+offset</div></div>
        </div>
        <hr class="divider">
        <div class="card-title" style="margin-bottom:10px">Décalages</div>
        <div class="offset-row">
          <div class="offset-label">Ouverture<span>après le lever (min)</span></div>
          <div class="offset-ctrl">
            <button class="btn-offset" onclick="modOffset('ouv',-5)">−</button>
            <div class="offset-val" id="valOffsetOuv">+15</div>
            <button class="btn-offset" onclick="modOffset('ouv',+5)">+</button>
          </div>
        </div>
        <div class="offset-row">
          <div class="offset-label">Fermeture<span>après le coucher (min)</span></div>
          <div class="offset-ctrl">
            <button class="btn-offset" onclick="modOffset('fer',-5)">−</button>
            <div class="offset-val" id="valOffsetFer">+15</div>
            <button class="btn-offset" onclick="modOffset('fer',+5)">+</button>
          </div>
        </div>
      </div>

      <!-- LUMINOSITÉ -->
      <div class="mode-panel" id="panel-luminosite">
        <div class="lux-grid">
          <div class="lux-item">
            <div class="label">🔓 Ouvre si</div>
            <div class="val" id="valLuxOuv">800</div>
            <div style="font-size:9px;color:var(--muted);margin-top:2px;font-family:'Space Mono',monospace">lux &gt; seuil</div>
            <input type="range" class="lux-slider" id="sliderLuxOuv" min="100" max="2000" step="50" value="800"
              oninput="changeLuxSeuil('ouv',this.value)">
          </div>
          <div class="lux-item">
            <div class="label">🔒 Ferme si</div>
            <div class="val" id="valLuxFer">100</div>
            <div style="font-size:9px;color:var(--muted);margin-top:2px;font-family:'Space Mono',monospace">lux &lt; seuil</div>
            <input type="range" class="lux-slider" id="sliderLuxFer" min="10" max="500" step="10" value="100"
              oninput="changeLuxSeuil('fer',this.value)">
          </div>
        </div>
        <div class="offset-row" style="margin-bottom:8px">
          <div class="offset-label">Confirmation<span>durée avant action (s)</span></div>
          <div class="offset-ctrl">
            <button class="btn-offset" onclick="modLuxConf(-10)">−</button>
            <div class="offset-val zero" id="valLuxConf">60</div>
            <button class="btn-offset" onclick="modLuxConf(+10)">+</button>
          </div>
        </div>
        <div class="confirmation-row">
          <span style="font-size:10px;color:var(--muted);font-family:'Space Mono',monospace">Détection</span>
          <div class="conf-bar"><div class="conf-fill" id="confFill"></div></div>
          <span class="conf-label" id="confLabel">--</span>
        </div>
        <div style="margin-top:10px;font-size:11px;color:var(--muted);font-family:'Space Mono',monospace">
          Lux actuel : <span id="luxActuel" style="color:var(--accent)">--</span> lx
        </div>
      </div>

      <!-- HEURE FIXE -->
      <div class="mode-panel" id="panel-heure_fixe">
        <div class="hfx-row">
          <div class="hfx-label">🔓 Ouverture<span>heure de base</span></div>
          <div class="hfx-time-ctrl">
            <input type="time" class="time-input" id="hfxOuvBase" value="07:30"
              onchange="sendHfx()">
          </div>
        </div>
        <div class="offset-row" style="margin-bottom:14px;padding-left:16px">
          <div class="offset-label" style="font-size:12px">Décalage<span>minutes</span></div>
          <div class="offset-ctrl">
            <button class="btn-offset" onclick="modOffset('hfxOuv',-5)">−</button>
            <div class="offset-val" id="valOffsetHfxOuv">0</div>
            <button class="btn-offset" onclick="modOffset('hfxOuv',+5)">+</button>
          </div>
        </div>
        <hr class="divider" style="margin:10px 0">
        <div class="hfx-row">
          <div class="hfx-label">🔒 Fermeture<span>heure de base</span></div>
          <div class="hfx-time-ctrl">
            <input type="time" class="time-input" id="hfxFerBase" value="20:00"
              onchange="sendHfx()">
          </div>
        </div>
        <div class="offset-row" style="padding-left:16px">
          <div class="offset-label" style="font-size:12px">Décalage<span>minutes</span></div>
          <div class="offset-ctrl">
            <button class="btn-offset" onclick="modOffset('hfxFer',-5)">−</button>
            <div class="offset-val" id="valOffsetHfxFer">0</div>
            <button class="btn-offset" onclick="modOffset('hfxFer',+5)">+</button>
          </div>
        </div>
        <div id="hfxEffectif" style="margin-top:12px;padding:10px 12px;background:var(--surface2);border-radius:10px;font-family:'Space Mono',monospace;font-size:11px;color:var(--muted)">
          Effectif : ouvre à <span id="hfxOuvEff" style="color:var(--green)">07:30</span> · ferme à <span id="hfxFerEff" style="color:var(--red)">20:00</span>
        </div>
      </div>

      <!-- MANUEL -->
      <div class="mode-panel" id="panel-manuel">
        <div style="padding:14px;background:var(--surface2);border-radius:12px;text-align:center">
          <div style="font-size:28px;margin-bottom:8px">🖐</div>
          <div style="font-size:13px;color:var(--muted);line-height:1.5">Mode manuel actif.<br>Utilisez les boutons ci-dessus<br>pour contrôler la porte.</div>
        </div>
      </div>

    </div>
  </div>

  <!-- ── CAPTEURS ── -->
  <div class="card">
    <div class="card-title">Capteurs</div>
    <div class="fdc-row">
      <div class="fdc-item">
        <div class="fdc-label">FDC Gauche (ouvert)</div>
        <div class="fdc-state inactive" id="fdcOuv">--</div>
      </div>
      <div class="fdc-item">
        <div class="fdc-label">FDC Droit (fermé)</div>
        <div class="fdc-state inactive" id="fdcFer">--</div>
      </div>
    </div>
    <div class="capteurs-grid">
      <div class="capteur-item"><div class="c-label">💡 Luminosité</div><div class="c-val ok" id="valLux">--</div><div class="c-unit">lux</div></div>
      <div class="capteur-item"><div class="c-label">⚡ Courant</div><div class="c-val ok" id="valCourant">--</div><div class="c-unit">ampères</div></div>
    </div>
  </div>

  <!-- ── GRAPHIQUE ── -->
  <div class="card">
    <div class="card-title">Courant moteur — historique</div>
    <div class="chart-wrap">
      <canvas id="chartCourant"></canvas>
      <div class="seuil-line"></div>
      <div class="seuil-label">seuil 2A</div>
    </div>
  </div>

  <!-- ── LOG ── -->
  <div class="card">
    <div class="card-title">Journal des événements</div>
    <div class="log-list" id="logList"></div>
  </div>

  <footer>ESP32 Poulailler · WebSocket live · NTP sync</footer>
</main>

<script>
// ── État local ───────────────────────────────────────────────
let ws;
let courantHist = Array(40).fill(0);
let modeSelectionne = 'soleil';
let modePendingActif = false;
let luxConf = 60;
let offsetOuv = 15, offsetFer = 15;
let offsetHfxOuv = 0, offsetHfxFer = 0;
let luxSeuilOuv = 800, luxSeuilFer = 100;
let confDebutMs = null, confDureeMs = null;
let stopArmed = false, stopArmedTimer = null;



// ── WebSocket ────────────────────────────────────────────────
function connectWS() {
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen = () => {
    setStatus('connected');
    ws.send(JSON.stringify({ cmd: 'GET_STATE' }));
  };
  ws.onclose = () => { setStatus('offline'); setTimeout(connectWS, 3000); };
  ws.onmessage = e => { try { handleMsg(JSON.parse(e.data)); } catch(err) {} };
}
function send(obj) { if (ws && ws.readyState === 1) ws.send(JSON.stringify(obj)); }

// ── Commandes ────────────────────────────────────────────────
function resetErreur() {
  send({ cmd: 'RESET' });
  ajouterLog('Réinitialisation erreur demandée', 'info');
}

function envoyerCmd(cmd) {
  send({ cmd });
  ajouterLog(cmd === 'OPEN' ? '← Ouverture manuelle' : 'Fermeture manuelle →', 'info');
}

function stopUrgence() {
  const btn = document.getElementById('btnStop');
  if (!stopArmed) {
    // Premier clic : armer
    stopArmed = true;
    btn.classList.add('armed');
    btn.textContent = '⚠ Confirmer le stop ?';
    stopArmedTimer = setTimeout(() => {
      stopArmed = false;
      btn.classList.remove('armed');
      btn.textContent = '⏹ Stop d\'urgence';
    }, 3000);
  } else {
    // Deuxième clic : exécuter
    clearTimeout(stopArmedTimer);
    stopArmed = false;
    btn.classList.remove('armed');
    btn.textContent = '⏹ Stop d\'urgence';
    send({ cmd: 'STOP' });
    // Remettre l'UI en état stable
    const etatEl = document.getElementById('porteEtat');
    const cl = etatEl.className;
    if (cl === 'moving') {
      // Laisser en état inconnu — l'ESP32 renverra l'état réel
      etatEl.textContent = 'ARRÊTÉ';
      document.getElementById('porteIcon').className = 'porte-icon';
      document.getElementById('porteIcon').textContent = '⏹';
    }
    document.getElementById('progressBar').classList.remove('visible');
    setStatus('connected');
    ajouterLog('Stop d\'urgence — moteur arrêté', 'warn');
  }
}

// ── Sélection de mode ────────────────────────────────────────
function selectionnerMode(mode) {
  if (mode === modeSelectionne) return;
  modeSelectionne = mode;
  // Met à jour les onglets
  document.querySelectorAll('.mode-tab').forEach(t => t.classList.remove('active'));
  document.getElementById('tab-' + mode).classList.add('active');
  document.querySelectorAll('.mode-panel').forEach(p => p.classList.remove('active'));
  document.getElementById('panel-' + mode).classList.add('active');
  // Envoie au ESP32
  send({ mode });
}

// ── Offsets ──────────────────────────────────────────────────
function modOffset(type, delta) {
  if (type === 'ouv') {
    offsetOuv = Math.max(-60, Math.min(120, offsetOuv + delta));
    afficherOffset('valOffsetOuv', offsetOuv);
    send({ offset_ouv: offsetOuv });
    updateHeuresOuvertureFermeture();
  } else if (type === 'fer') {
    offsetFer = Math.max(-60, Math.min(120, offsetFer + delta));
    afficherOffset('valOffsetFer', offsetFer);
    send({ offset_fer: offsetFer });
    updateHeuresOuvertureFermeture();
  } else if (type === 'hfxOuv') {
    offsetHfxOuv = Math.max(-120, Math.min(120, offsetHfxOuv + delta));
    afficherOffset('valOffsetHfxOuv', offsetHfxOuv);
    sendHfx();
    updateHfxEffectif();
  } else if (type === 'hfxFer') {
    offsetHfxFer = Math.max(-120, Math.min(120, offsetHfxFer + delta));
    afficherOffset('valOffsetHfxFer', offsetHfxFer);
    sendHfx();
    updateHfxEffectif();
  }
}

function afficherOffset(id, val) {
  const el = document.getElementById(id);
  el.textContent = (val > 0 ? '+' : '') + val;
  el.className = 'offset-val ' + (val > 0 ? 'positive' : val < 0 ? 'negative' : 'zero');
}

// ── Luminosité ───────────────────────────────────────────────
function changeLuxSeuil(type, val) {
  if (type === 'ouv') {
    luxSeuilOuv = parseInt(val);
    document.getElementById('valLuxOuv').textContent = luxSeuilOuv;
  } else {
    luxSeuilFer = parseInt(val);
    document.getElementById('valLuxFer').textContent = luxSeuilFer;
  }
  send({ lux_ouv: luxSeuilOuv, lux_fer: luxSeuilFer });
}
function modLuxConf(delta) {
  luxConf = Math.max(10, Math.min(300, luxConf + delta));
  document.getElementById('valLuxConf').textContent = luxConf;
  send({ lux_conf: luxConf });
}

// ── Heure fixe ───────────────────────────────────────────────
function sendHfx() {
  const ouv = document.getElementById('hfxOuvBase').value.split(':');
  const fer = document.getElementById('hfxFerBase').value.split(':');
  send({
    hfx_ouv_h: parseInt(ouv[0]), hfx_ouv_m: parseInt(ouv[1]), hfx_ouv_off: offsetHfxOuv,
    hfx_fer_h: parseInt(fer[0]), hfx_fer_m: parseInt(fer[1]), hfx_fer_off: offsetHfxFer
  });
  updateHfxEffectif();
}
function updateHfxEffectif() {
  const ouv = document.getElementById('hfxOuvBase').value.split(':');
  const fer = document.getElementById('hfxFerBase').value.split(':');
  const ouvMin = parseInt(ouv[0]) * 60 + parseInt(ouv[1]) + offsetHfxOuv;
  const ferMin = parseInt(fer[0]) * 60 + parseInt(fer[1]) + offsetHfxFer;
  document.getElementById('hfxOuvEff').textContent = minutesToHHMM(ouvMin);
  document.getElementById('hfxFerEff').textContent = minutesToHHMM(ferMin);
}
function minutesToHHMM(min) {
  let m = ((min % 1440) + 1440) % 1440;
  return String(Math.floor(m / 60)).padStart(2,'0') + ':' + String(m % 60).padStart(2,'0');
}

// ── Soleil — heures effectives ────────────────────────────────
let leverData = { h: 0, m: 0 }, coucherData = { h: 0, m: 0 };
function updateHeuresOuvertureFermeture() {
  const ouvMin = leverData.h * 60 + leverData.m + offsetOuv;
  const ferMin = coucherData.h * 60 + coucherData.m + offsetFer;
  document.getElementById('hOuverture').textContent = minutesToHHMM(ouvMin);
  document.getElementById('hFermeture').textContent = minutesToHHMM(ferMin);
}

// ── Traitement messages ESP32 ─────────────────────────────────
function handleMsg(d) {
  if (d.heure !== undefined) document.getElementById('heureEsp').textContent = d.heure;
  if (d.porte !== undefined) mettreAJourPorte(d.porte);
  if (d.mode  !== undefined) mettreAJourMode(d.mode, d.mode_pending);
  if (d.lever  !== undefined) {
    document.getElementById('hLever').textContent   = d.lever;
    const p = d.lever.split(':');
    leverData = { h: parseInt(p[0]), m: parseInt(p[1]) };
  }
  if (d.coucher !== undefined) {
    document.getElementById('hCoucher').textContent = d.coucher;
    const p = d.coucher.split(':');
    coucherData = { h: parseInt(p[0]), m: parseInt(p[1]) };
  }
  if (d.ouverture !== undefined) document.getElementById('hOuverture').textContent = d.ouverture;
  if (d.fermeture !== undefined) document.getElementById('hFermeture').textContent = d.fermeture;
  if (d.offset_ouv !== undefined) { offsetOuv = d.offset_ouv; afficherOffset('valOffsetOuv', offsetOuv); }
  if (d.offset_fer !== undefined) { offsetFer = d.offset_fer; afficherOffset('valOffsetFer', offsetFer); }
  if (d.lux_ouv    !== undefined) { luxSeuilOuv = d.lux_ouv; document.getElementById('valLuxOuv').textContent = luxSeuilOuv; document.getElementById('sliderLuxOuv').value = luxSeuilOuv; }
  if (d.lux_fer    !== undefined) { luxSeuilFer = d.lux_fer; document.getElementById('valLuxFer').textContent = luxSeuilFer; document.getElementById('sliderLuxFer').value = luxSeuilFer; }
  if (d.lux_conf   !== undefined) { luxConf = d.lux_conf; document.getElementById('valLuxConf').textContent = luxConf; }
  if (d.hfx_ouv_h  !== undefined) {
    document.getElementById('hfxOuvBase').value = String(d.hfx_ouv_h).padStart(2,'0') + ':' + String(d.hfx_ouv_m||0).padStart(2,'0');
    offsetHfxOuv = d.hfx_ouv_off || 0;
    afficherOffset('valOffsetHfxOuv', offsetHfxOuv);
    updateHfxEffectif();
  }
  if (d.hfx_fer_h  !== undefined) {
    document.getElementById('hfxFerBase').value = String(d.hfx_fer_h).padStart(2,'0') + ':' + String(d.hfx_fer_m||0).padStart(2,'0');
    offsetHfxFer = d.hfx_fer_off || 0;
    afficherOffset('valOffsetHfxFer', offsetHfxFer);
    updateHfxEffectif();
  }
  if (d.lux !== undefined) {
    const lux = parseFloat(d.lux);
    document.getElementById('valLux').textContent = Math.round(lux);
    document.getElementById('luxActuel').textContent = Math.round(lux);
  }
  if (d.courant !== undefined) {
    const a = parseFloat(d.courant);
    document.getElementById('valCourant').textContent = a.toFixed(2);
    document.getElementById('valCourant').className = 'c-val ' + (a > 2 ? 'danger' : a > 0.8 ? 'warn' : 'ok');
    courantHist.push(a);
    if (courantHist.length > 40) courantHist.shift();
    dessinerGraphique();
  }
  if (d.force_manuel !== undefined) {
    const badge = document.getElementById('overrideBadge');
    badge.className = 'override-badge' + (d.force_manuel ? ' visible' : '');
  }
  if (d.fdc_ouv !== undefined) {
    const el = document.getElementById('fdcOuv');
    el.textContent = d.fdc_ouv ? 'ACTIF' : 'INACTIF';
    el.className = 'fdc-state ' + (d.fdc_ouv ? 'active' : 'inactive');
  }
  if (d.fdc_fer !== undefined) {
    const el = document.getElementById('fdcFer');
    el.textContent = d.fdc_fer ? 'ACTIF' : 'INACTIF';
    el.className = 'fdc-state ' + (d.fdc_fer ? 'active' : 'inactive');
  }
  if (d.conf_pct !== undefined) {
    document.getElementById('confFill').style.width = d.conf_pct + '%';
    document.getElementById('confLabel').textContent = d.conf_pct < 100 ? d.conf_pct + '%' : '✓';
  }
  if (d.log) ajouterLog(d.log.msg, d.log.type || 'info');
}

// ── Mise à jour état porte ────────────────────────────────────
function mettreAJourPorte(p) {
  const moving = p === 'opening' || p === 'closing';
  const labels = { open:'OUVERTE', closed:'FERMÉE', opening:'OUVERTURE…', closing:'FERMETURE…', error:'ERREUR' };
  const icons  = { open:'🔓', closed:'🔒', opening:'◀', closing:'▶', error:'⚠️' };
  const cls    = { open:'open', closed:'closed', opening:'moving', closing:'moving', error:'error' };
  const cl = cls[p] || 'moving';

  document.getElementById('porteEtat').textContent = labels[p] || p.toUpperCase();
  document.getElementById('porteEtat').className = cl;
  document.getElementById('porteIcon').textContent = icons[p] || '🚪';
  document.getElementById('porteIcon').className = 'porte-icon ' + cl;


  // Barre de progression
  document.getElementById('progressBar').className = 'progress-bar' + (moving ? ' visible' : '');
  const btnReset = document.getElementById('btnReset');
  btnReset.className = 'btn btn-reset' + (p === 'error' ? ' visible' : '');
  if (moving) {
    const fill = document.getElementById('progressFill');
    fill.style.animation = 'none';
    void fill.offsetWidth;
    fill.style.animation = 'progress 20s linear forwards';
  }

  setStatus(moving ? 'moving' : 'connected');
}

// ── Mise à jour mode ──────────────────────────────────────────
function mettreAJourMode(mode, pending) {
  modeSelectionne = mode;
  document.querySelectorAll('.mode-tab').forEach(t => t.classList.remove('active'));
  const tab = document.getElementById('tab-' + mode);
  if (tab) tab.classList.add('active');
  document.querySelectorAll('.mode-panel').forEach(p => p.classList.remove('active'));
  const panel = document.getElementById('panel-' + mode);
  if (panel) panel.classList.add('active');
  const msg = document.getElementById('modePendingMsg');
  msg.style.display = pending ? 'block' : 'none';
}

// ── Status dot ────────────────────────────────────────────────
function setStatus(s) {
  const dot = document.getElementById('dotStatus');
  const lbl = document.getElementById('statusLabel');
  if (s === 'connected') { dot.className = 'dot'; lbl.textContent = 'Connecté'; }
  if (s === 'moving')    { dot.className = 'dot moving'; lbl.textContent = 'En mouvement'; }
  if (s === 'offline')   { dot.className = 'dot offline'; lbl.textContent = 'Hors ligne'; }
}

// ── Log ───────────────────────────────────────────────────────
function ajouterLog(msg, type = 'info') {
  const now = new Date();
  const t = String(now.getHours()).padStart(2,'0') + ':' + String(now.getMinutes()).padStart(2,'0');
  const list = document.getElementById('logList');
  const item = document.createElement('div');
  item.className = 'log-item ' + type;
  item.innerHTML = `<span class="log-time">${t}</span><span class="log-msg">${msg}</span>`;
  list.prepend(item);
  while (list.children.length > 20) list.removeChild(list.lastChild);
}

// ── Graphique courant ─────────────────────────────────────────
function dessinerGraphique() {
  const c = document.getElementById('chartCourant');
  const ctx = c.getContext('2d');
  const W = c.offsetWidth, H = c.offsetHeight;
  c.width = W; c.height = H;
  const max = 3, pts = courantHist;
  ctx.clearRect(0, 0, W, H);
  ctx.strokeStyle = 'rgba(48,54,61,.5)'; ctx.lineWidth = 1;
  for (let i = 1; i < 4; i++) {
    const y = H - (i / max) * H;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
  }
  const g = ctx.createLinearGradient(0, 0, 0, H);
  g.addColorStop(0, 'rgba(240,165,0,.22)'); g.addColorStop(1, 'rgba(240,165,0,0)');
  ctx.beginPath();
  pts.forEach((v, i) => {
    const x = (i / (pts.length - 1)) * W, y = H - (v / max) * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.lineTo(W, H); ctx.lineTo(0, H); ctx.closePath();
  ctx.fillStyle = g; ctx.fill();
  ctx.beginPath(); ctx.strokeStyle = '#f0a500'; ctx.lineWidth = 2; ctx.lineJoin = 'round';
  pts.forEach((v, i) => {
    const x = (i / (pts.length - 1)) * W, y = H - (v / max) * H;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.stroke();
  const lx = W - 2, ly = H - (pts[pts.length - 1] / max) * H;
  ctx.beginPath(); ctx.arc(lx, ly, 4, 0, Math.PI * 2);
  ctx.fillStyle = '#f0a500'; ctx.fill();
}


window.addEventListener('load', () => {
  dessinerGraphique();
  afficherOffset('valOffsetOuv', offsetOuv);
  afficherOffset('valOffsetFer', offsetFer);
  afficherOffset('valOffsetHfxOuv', 0);
  afficherOffset('valOffsetHfxFer', 0);
  updateHfxEffectif();
  connectWS();
});
window.addEventListener('resize', dessinerGraphique);
</script>
</body>
</html>
)rawhtml";

// ── Envoi état complet JSON à tous les clients ───────────────
void wsBroadcastState() {
  if (ws.count() == 0) return;

  struct tm timeinfo;
  getLocalTime(&timeinfo);

  HeureSoleil s = calculSoleil(
    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
    LATITUDE, LONGITUDE
  );

  // Heures effectives selon mode soleil
  int ouvMin = heureEnMinutes(s.leverH,   s.leverM,   offsetOuv);
  int ferMin = heureEnMinutes(s.coucherH, s.coucherM, offsetFer);

  const char* etatStr;
  switch (etatPorte) {
    case PORTE_OUVERTE:        etatStr = "open";    break;
    case PORTE_FERMEE:         etatStr = "closed";  break;
    case PORTE_EN_OUVERTURE:   etatStr = "opening"; break;
    case PORTE_EN_FERMETURE:   etatStr = "closing"; break;
    default:                   etatStr = "error";   break;
  }

  const char* modeStr[] = { "soleil", "luminosite", "heure_fixe", "manuel" };

  float lux = bh1750.readLightLevel();
  float amp = lireCourant();

  char heureStr[6];
  snprintf(heureStr, sizeof(heureStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  char buf[700];
  snprintf(buf, sizeof(buf),
    "{\"heure\":\"%s\","
    "\"porte\":\"%s\","
    "\"mode\":\"%s\","
    "\"mode_pending\":%s,"
    "\"lever\":\"%02d:%02d\","
    "\"coucher\":\"%02d:%02d\","
    "\"ouverture\":\"%02d:%02d\","
    "\"fermeture\":\"%02d:%02d\","
    "\"offset_ouv\":%d,"
    "\"offset_fer\":%d,"
    "\"lux_ouv\":%.0f,"
    "\"lux_fer\":%.0f,"
    "\"lux_conf\":%d,"
    "\"hfx_ouv_h\":%d,\"hfx_ouv_m\":%d,\"hfx_ouv_off\":%d,"
    "\"hfx_fer_h\":%d,\"hfx_fer_m\":%d,\"hfx_fer_off\":%d,"
    "\"lux\":%.0f,"
    "\"courant\":%.2f,"
    "\"force_manuel\":%s,"
    "\"fdc_ouv\":%s,"
    "\"fdc_fer\":%s"
    "}",
    heureStr,
    etatStr,
    modeStr[modeActuel],
    modePendingActif ? "true" : "false",
    s.leverH, s.leverM,
    s.coucherH, s.coucherM,
    ouvMin / 60, ouvMin % 60,
    ferMin / 60, ferMin % 60,
    offsetOuv,
    offsetFer,
    luxSeuilOuverture,
    luxSeuilFermeture,
    luxConfirmationSec,
    heureFixeOuvH, heureFixeOuvM, offsetFixeOuv,
    heureFixeFerH, heureFixeFerM, offsetFixeFer,
    lux,
    amp,
    forceManuel ? "true" : "false",
    digitalRead(PIN_FDC_OUV) == LOW  ? "true" : "false",
    digitalRead(PIN_FDC_FER) == LOW  ? "true" : "false"
  );
  ws.textAll(buf);
}

// ── Envoi d'un log à tous les clients ───────────────────────
void wsLog(const char* msg, const char* type) {
  if (ws.count() == 0) return;
  char buf[256];
  snprintf(buf, sizeof(buf), "{\"log\":{\"msg\":\"%s\",\"type\":\"%s\"}}", msg, type);
  ws.textAll(buf);
}

// ── Traitement des messages WebSocket reçus ──────────────────
void wsHandleMessage(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;
  data[len] = 0;

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, (char*)data) != DeserializationError::Ok) return;

  // Commande porte
  if (doc.containsKey("cmd")) {
    String cmd = doc["cmd"].as<String>();
    if      (cmd == "OPEN")      commandeOuvrir();
    else if (cmd == "CLOSE")     commandeFermer();
    else if (cmd == "STOP")      stopUrgence();
    else if (cmd == "RESET")     resetErreur();
    else if (cmd == "GET_STATE") wsBroadcastState();
  }

  // Changement de mode
  if (doc.containsKey("mode")) {
    ModeGestion nouveau;
    String m = doc["mode"].as<String>();
    if      (m == "soleil")     nouveau = MODE_SOLEIL;
    else if (m == "luminosite") nouveau = MODE_LUMINOSITE;
    else if (m == "heure_fixe") nouveau = MODE_HEURE_FIXE;
    else                        nouveau = MODE_MANUEL;

    if (etatPorte == PORTE_EN_OUVERTURE || etatPorte == PORTE_EN_FERMETURE) {
      modePending = nouveau;
      modePendingActif = true;
    } else {
      if (nouveau != modeActuel) {  // ne publier que si le mode change réellement
        modeActuel = nouveau;
        publishMode();
        sauvegarderConfig();
      }
    }
    wsBroadcastState();
  }

  // Configuration — chaque clé est optionnelle
  if (doc.containsKey("offset_ouv"))   offsetOuv           = doc["offset_ouv"].as<int>();
  if (doc.containsKey("offset_fer"))   offsetFer           = doc["offset_fer"].as<int>();
  if (doc.containsKey("lux_ouv"))      luxSeuilOuverture   = doc["lux_ouv"].as<float>();
  if (doc.containsKey("lux_fer"))      luxSeuilFermeture   = doc["lux_fer"].as<float>();
  if (doc.containsKey("lux_conf"))     luxConfirmationSec  = doc["lux_conf"].as<int>();
  if (doc.containsKey("hfx_ouv_h"))   heureFixeOuvH       = doc["hfx_ouv_h"].as<int>();
  if (doc.containsKey("hfx_ouv_m"))   heureFixeOuvM       = doc["hfx_ouv_m"].as<int>();
  if (doc.containsKey("hfx_ouv_off")) offsetFixeOuv       = doc["hfx_ouv_off"].as<int>();
  if (doc.containsKey("hfx_fer_h"))   heureFixeFerH       = doc["hfx_fer_h"].as<int>();
  if (doc.containsKey("hfx_fer_m"))   heureFixeFerM       = doc["hfx_fer_m"].as<int>();
  if (doc.containsKey("hfx_fer_off")) offsetFixeFer       = doc["hfx_fer_off"].as<int>();
  sauvegarderConfig();
}

// ── Callback événements WebSocket ────────────────────────────
void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("[WS] Client #%u connecté\n", client->id());
      wsBroadcastState();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Client #%u déconnecté\n", client->id());
      break;
    case WS_EVT_DATA:
      wsHandleMessage(arg, data, len);
      break;
    default: break;
  }
}

// ── Initialisation — à appeler dans setup() après WiFi ───────
void setupWebServer() {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", HTML_PAGE);
  });

  server.begin();
  Serial.printf("[WEB] http://%s\n", WiFi.localIP().toString().c_str());
}

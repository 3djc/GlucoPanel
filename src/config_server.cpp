// Note: no Seeed_GFX header may be included before WebServer.h (FS_NO_GLOBALS breaks it).
#include <Arduino.h>
#ifdef ARDUINO
#include <FS.h>
#endif
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include "config.h"

static WebServer server(80);
static bool settingsChanged = false;

static String esc(const String& s) {
  String o;
  for (int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

// Iterates "value:Label|value:Label".
static bool nextOption(const char*& p, String& val, String& label) {
  if (!*p) return false;
  val = "";
  label = "";
  while (*p && *p != ':') val += *p++;
  if (*p == ':') p++;
  while (*p && *p != '|') label += *p++;
  if (*p == '|') p++;
  return true;
}

static const char* PAGE_HEAD =
  "<!doctype html><html><head><meta charset=utf-8>"
  "<meta name=viewport content='width=device-width,initial-scale=1'><title>GlucoPanel</title>"
  "<style>body{font:16px system-ui,sans-serif;max-width:30rem;margin:0 auto;padding:1rem}"
  "label{display:block;margin:.8rem 0 .2rem;font-weight:600}"
  "input,select{width:100%;padding:.5rem;font:inherit;box-sizing:border-box}"
  "button{margin-top:1rem;padding:.6rem 1rem;font:inherit}"
  ".ok{background:#d8f5d8;padding:.5rem}.err{background:#fbd8d8;padding:.5rem}"
  "hr{margin:2rem 0}.danger{color:#b00020}</style></head><body><h1>GlucoPanel</h1>";


// Visual dashboard-layout editor (progressive enhancement: without JavaScript the plain selects inside
// #blkfb are used). Everything is drawn in "cqw" units of its own block, so one preview markup serves both
// the miniature dashboard and the palette cards.
static const char* EDITOR_CSS = R"HTML(
.hint{color:#555;margin:.2rem 0 .6rem}
#dash{position:relative;aspect-ratio:5/3;border:2px solid #222;border-radius:8px;background:#fff;overflow:hidden}
#dash .band{position:absolute;left:.75%;top:2%;width:98.5%;height:27%;background:#e2e2e2;border-radius:10px;display:grid;grid-template-columns:repeat(3,1fr)}
#dash .mid{position:absolute;left:0;top:33%;width:100%;height:33%;display:grid;grid-template-columns:repeat(3,1fr)}
#dash .bot{position:absolute;left:0;top:68%;width:100%;height:30%}
.c,.pv{container-type:inline-size;display:flex;flex-direction:column;align-items:center;justify-content:center;overflow:hidden;box-sizing:border-box}
.c{border:0;background:none;padding:0;margin:0;cursor:pointer;height:100%;width:100%;font:inherit;color:#000;position:relative}
.band .c+.c{border-left:1px solid #444}
.c:hover{background:rgba(0,0,0,.07)}
.c.sel{background:rgba(0,110,220,.16);box-shadow:inset 0 0 0 3px #06c}
.c .tag{position:absolute;left:3px;top:2px;font-size:.6rem;color:#06c;opacity:.0}
.c:hover .tag,.c.sel .tag{opacity:1}
#pal{display:grid;grid-template-columns:repeat(auto-fill,minmax(7.2rem,1fr));gap:.5rem;margin-top:.7rem}
#pal h3{grid-column:1/-1;margin:.2rem 0;font-size:1rem}
.o{border:2px solid #bbb;border-radius:8px;background:#fff;padding:.3rem;font:inherit;font-size:.8rem;cursor:pointer;text-align:center;color:#000}
.o:hover{border-color:#06c}.o.on{border-color:#06c;background:#e8f2ff}
.o .pv{aspect-ratio:266/150;width:100%;background:#fafafa;border:1px solid #ddd;margin-bottom:.25rem}
.o.band .pv{background:#e2e2e2}
.big{font:500 46cqw/1 system-ui,sans-serif}.num{font:500 18cqw/1.1 system-ui,sans-serif}
.cap{font-style:normal;font-size:6.5cqw;margin-bottom:1cqw}.unit{font-style:normal;font-size:9cqw;margin-top:1cqw}
.date{font-style:normal;font-size:9cqw;margin-bottom:-1cqw}
.ar{width:34cqw;height:34cqw}.none{color:#999;font-size:7cqw;border:1.2cqw dashed #bbb;border-radius:3cqw;padding:4cqw 10cqw}
.fr{display:flex;flex-direction:column-reverse;gap:2cqw;width:37cqw}
.fr u{display:block;height:6.5cqw;border:1cqw solid #000;border-radius:2cqw;text-decoration:none}.fr u.f{background:#000}
.gr{width:100%;height:100%}
)HTML";

static const char* EDITOR_JS = R"HTML(
(function(){
const K=['blk_tl','blk_tm','blk_tr','blk_ml','blk_mc','blk_mr','blk_bottom'];
const sel=K.map(k=>document.querySelector('[name='+k+']'));
const dash=document.getElementById('dash'),pal=document.getElementById('pal'),fb=document.getElementById('blkfb');
if(!dash||sel.some(x=>!x))return;
fb.style.display='none';
const unit=document.querySelector('[name=unit]');
function pv(t){
 const mm=unit&&unit.value=='mmol',v=mm?'7.1':'128',u=mm?'mmol/L':'mg/dL',d1=mm?'-0.3':'-5',d15=mm?'+0.6':'+11';
 switch(t){
 case 'value':return '<b class=big>'+v+'</b><i class=unit>'+u+'</i>';
 case 'arrow':return '<svg class=ar viewBox="-50 -50 100 100"><g transform="rotate(45)"><rect x="-45" y="-3" width="62" height="6"/><polygon points="45,0 14,-25 14,25"/></g></svg>';
 case 'delta1':return '<i class=cap>vs last</i><b class=num>'+d1+'</b>';
 case 'delta15':return '<i class=cap>vs 15 min</i><b class=num>'+d15+'</b>';
 case 'clock':return '<i class=date>17 Oct</i><b class=num>09:52</b>';
 case 'readtime':return '<i class=cap>reading time</i><b class=num>09:50</b>';
 case 'age':return '<i class=cap>reading age</i><b class=num>2 min</b>';
 case 'fresh':return '<div class=fr><u class=f></u><u class=f></u><u class=f></u><u></u><u></u></div>';
 case 'avg':return '<i class=cap>average 16 h</i><b class=num>'+(mm?'7.4':'133')+'</b>';
 case 'tir':return '<i class=cap>in range 16 h</i><b class=num>72%</b><i class=cap>low 3%   high 25%</i>';
 case 'range':return '<i class=cap>lowest / highest</i><b class=num>'+(mm?'3.6 - 12.9':'65 - 232')+'</b>';
 case 'cv':return '<i class=cap>variability 16 h</i><b class=num>28%</b><i class=cap>SD '+(mm?'2.1':'38')+'</i>';
 case 'graph':return '<svg class=gr viewBox="0 0 200 60" preserveAspectRatio="none"><rect x="14" y="20" width="182" height="22" fill="#ddd"/><path d="M14 8V54H196" fill="none" stroke="#000" stroke-width="1.5"/><path d="M14 20H196M14 42H196" stroke="#000" stroke-width=".6" stroke-dasharray="3 3"/><path d="M16 46C40 50 60 40 85 34S130 14 150 22 180 34 190 30" fill="none" stroke="#000" stroke-width="2.4" stroke-linecap="round"/><circle cx="190" cy="30" r="3.4"/></svg>';
 default:return '<span class=none>empty</span>';}
}
const names=sel.map(s=>s.previousElementSibling.textContent);
dash.innerHTML='<div class=band></div><div class=mid></div><div class=bot></div>';
const parts=[dash.querySelector('.band'),dash.querySelector('.mid'),dash.querySelector('.bot')];
const cells=sel.map((s,i)=>{const b=document.createElement('button');b.type='button';b.className='c';b.title=names[i];
 b.onclick=()=>choose(i);parts[i<3?0:i<6?1:2].appendChild(b);return b;});
function paint(i){cells[i].innerHTML=pv(sel[i].value)+'<span class=tag>'+names[i]+'</span>';}
function paintAll(){sel.forEach((_,i)=>paint(i));}
function choose(i){
 cells.forEach((c,j)=>c.classList.toggle('sel',j==i));
 pal.innerHTML='<h3>'+names[i]+': show</h3>';
 Array.from(sel[i].options).forEach(o=>{
  const b=document.createElement('button');b.type='button';b.className='o'+(o.value==sel[i].value?' on':'')+(i<3?' band':'');
  b.innerHTML='<div class=pv>'+pv(o.value)+'</div>'+o.textContent;
  b.onclick=()=>{sel[i].value=o.value;paint(i);choose(i);};
  pal.appendChild(b);});
}
if(unit)unit.addEventListener('change',paintAll);
paintAll();
pal.innerHTML='<p class=hint style="grid-column:1/-1">Tap a block, then pick what it shows.</p>';
})();
)HTML";

static void handleRoot() {
  String h = PAGE_HEAD;
  h += String("<style>") + EDITOR_CSS + "</style>";
  if (server.hasArg("saved")) h += "<p class=ok>Saved.</p>";
  h += "<form method=post action=/save>";
  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) {
    const ConfigField& f = CONFIG_FIELDS[i];
    String cur = cfgGet(f.key);
    if (!strcmp(f.key, "blk_tl"))  // the visual layout editor goes here; the plain selects stay as a fallback
      h += "<h2>Dashboard layout</h2><div id=dash></div><div id=pal></div><div id=blkfb>";
    bool grouped = f.group[0];
    if (grouped) h += String("<div class='grp' data-g='") + f.group + "'>";
    h += String("<label>") + f.label + "</label>";
    if (!strcmp(f.type, "select")) {
      h += String("<select name='") + f.key + "'" + (i == 0 ? " id=source" : "") + ">";
      const char* p = f.options;
      String v, l;
      while (nextOption(p, v, l))
        h += String("<option value='") + v + "'" + (v == cur ? " selected" : "") + ">" + l + "</option>";
      h += "</select>";
    } else if (!strcmp(f.type, "password")) {
      // never echo stored secrets; blank keeps the current value
      h += String("<input type=password name='") + f.key + "' autocomplete=off placeholder='" +
           (cur.length() ? "(saved - leave blank to keep)" : "") + "'>";
    } else {
      h += String("<input type='") + f.type + "' name='" + f.key + "' value='" + esc(cur) + "'>";
    }
    if (grouped) h += "</div>";
    if (!strcmp(f.key, "blk_bottom")) h += "</div>";  // end of #blkfb
  }
  h += "<button>Save</button></form>";
  h += String("<p>Refresh: every ") + String(refreshMinutes()) + " min (set by the data source).</p>";
  h += "<hr><form method=post action=/reset-wifi "
       "onsubmit=\"return confirm('Erase saved WiFi and reboot into setup mode?')\">"
       "<button class=danger>Erase WiFi and reboot into setup</button></form>"
       "<script>const s=document.getElementById('source');"
       "function sync(){document.querySelectorAll('.grp').forEach(d=>"
       "d.style.display=d.dataset.g===s.value?'':'none')}"
       "s.onchange=sync;sync();</script>";
  h += String("<script>") + EDITOR_JS + "</script></body></html>";
  server.send(200, "text/html", h);
}

static void fail(const String& msg) {
  server.send(400, "text/html", String(PAGE_HEAD) + "<p class=err>" + esc(msg) +
                                  "</p><p><a href=/>Back</a></p></body></html>");
}

static void handleSave() {
  String vals[CONFIG_FIELD_COUNT];
  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) {
    const ConfigField& f = CONFIG_FIELDS[i];
    if (!server.hasArg(f.key)) { vals[i] = cfgGet(f.key); continue; }  // field not submitted: keep it
    String v = server.arg(f.key);
    String t;  // trim, and drop control characters
    for (int k = 0; k < v.length(); k++)
      if ((unsigned char)v[k] >= 32 && !(v[k] == ' ' && t.length() == 0)) t += v[k];
    while (t.length() && t[t.length() - 1] == ' ') t.remove(t.length() - 1);
    v = t;

    if (!strcmp(f.type, "password") && v.length() == 0) { vals[i] = cfgGet(f.key); continue; }
    if (!strcmp(f.type, "select")) {
      const char* p = f.options;
      String ov, ol;
      bool found = false;
      while (nextOption(p, ov, ol)) found |= (ov == v);
      if (!found) return fail(String("Invalid value for ") + f.label);
    } else if (!strcmp(f.type, "url") && v.length()) {
      if (!v.startsWith("http://") && !v.startsWith("https://"))
        return fail(String(f.label) + " must start with http:// or https://");
      while (v.endsWith("/")) v.remove(v.length() - 1);
    } else if (!strcmp(f.type, "number")) {
      if (atoi(v.c_str()) < 20 || atoi(v.c_str()) > 400)
        return fail(String(f.label) + " must be between 20 and 400");
      v = String(atoi(v.c_str()));
    }
    vals[i] = v;
  }
  int low = 0, high = 0;
  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) {
    if (!strcmp(CONFIG_FIELDS[i].key, "low")) low = atoi(vals[i].c_str());
    if (!strcmp(CONFIG_FIELDS[i].key, "high")) high = atoi(vals[i].c_str());
  }
  if (low >= high) return fail("Low threshold must be below the high threshold");

  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) cfgSet(CONFIG_FIELDS[i].key, vals[i]);
  settingsChanged = true;
  server.sendHeader("Location", "/?saved=1");
  server.send(303, "text/plain", "");
}

static void handleResetWifi() {
  server.send(200, "text/html", String(PAGE_HEAD) +
              "<p class=ok>WiFi erased. Rebooting: join the <b>GlucoPanel</b> hotspot to set it up again.</p>"
              "</body></html>");
  delay(1000);
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

void configServerBegin() {
  MDNS.begin("glucopanel");
  MDNS.addService("http", "tcp", 80);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset-wifi", HTTP_POST, handleResetWifi);
  server.begin();
}

void configServerHandle() { server.handleClient(); }

bool configServerTakeChanged() {
  bool c = settingsChanged;
  settingsChanged = false;
  return c;
}

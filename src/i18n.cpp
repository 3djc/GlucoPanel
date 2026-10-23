#include "i18n.h"
#include <string.h>
#include "config.h"

namespace {

Lang lang = LANG_EN;

struct Entry {
  const char* en;
  const char* tr[4];  // fr, de, it, es
};

// English is the key. Keep entries grouped as in the code that uses them.
const Entry TABLE[] = {
    // ---- panel: WiFi screens ----
    {"Cannot join WiFi", {"Connexion WiFi impossible", "Verbindung zum WLAN fehlgeschlagen", "Impossibile connettersi al WiFi", "No se puede conectar al WiFi"}},
    {"Saved network:", {"Réseau enregistré :", "Gespeichertes Netzwerk:", "Rete salvata:", "Red guardada:"}},
    {"Erase it and set up a new WiFi?", {"L'effacer et configurer un nouveau WiFi ?", "Löschen und neues WLAN einrichten?", "Cancellarla e configurare un nuovo WiFi?", "¿Borrarla y configurar un nuevo WiFi?"}},
    {"KEY0 (green): ERASE and set up new WiFi", {"KEY0 (vert) : EFFACER et configurer un nouveau WiFi", "KEY0 (grün): LÖSCHEN und neues WLAN einrichten", "KEY0 (verde): CANCELLA e configura un nuovo WiFi", "KEY0 (verde): BORRAR y configurar un nuevo WiFi"}},
    {"KEY1: retry saved WiFi", {"KEY1 : réessayer le WiFi enregistré", "KEY1: gespeichertes WLAN erneut versuchen", "KEY1: riprova la rete salvata", "KEY1: reintentar la red guardada"}},
    {"(retries automatically in 60 s)", {"(nouvel essai automatique dans 60 s)", "(automatischer neuer Versuch in 60 s)", "(nuovo tentativo automatico tra 60 s)", "(reintento automático en 60 s)"}},
    {"WiFi setup", {"Configuration WiFi", "WLAN-Einrichtung", "Configurazione WiFi", "Configuración WiFi"}},
    {"1. Join WiFi %s", {"1. Rejoindre le WiFi %s", "1. Mit dem WLAN %s verbinden", "1. Collegati al WiFi %s", "1. Conéctate al WiFi %s"}},
    {"2. Open setup page", {"2. Ouvrir la page de réglages", "2. Einstellungsseite öffnen", "2. Apri la pagina delle impostazioni", "2. Abre la página de ajustes"}},
    {"(or browse to 192.168.4.1)", {"(ou aller sur 192.168.4.1)", "(oder 192.168.4.1 aufrufen)", "(oppure vai su 192.168.4.1)", "(o ve a 192.168.4.1)"}},
    {"Settings", {"Réglages", "Einstellungen", "Impostazioni", "Ajustes"}},
    {"Scan to open the settings page:", {"Scannez pour ouvrir la page de réglages :", "Zum Öffnen der Einstellungsseite scannen:", "Scansiona per aprire la pagina delle impostazioni:", "Escanea para abrir la página de ajustes:"}},
    {"Hold the green button for 3 s to erase the WiFi", {"Maintenez le bouton vert 3 s pour effacer le WiFi", "Grüne Taste 3 s halten, um das WLAN zu löschen", "Tieni premuto il pulsante verde per 3 s per cancellare il WiFi", "Mantén pulsado el botón verde 3 s para borrar el WiFi"}},
    {"Press any button to close", {"Appuyez sur un bouton pour fermer", "Beliebige Taste zum Schließen drücken", "Premi un pulsante per chiudere", "Pulsa un botón para cerrar"}},
    {"estimated HbA1c", {"HbA1c estimée", "HbA1c (geschätzt)", "HbA1c stimata", "HbA1c estimada"}},
    {"Not available yet", {"Pas encore disponible", "Noch nicht verfügbar", "Non ancora disponibile", "Aún no disponible"}},
    {"over %d days", {"sur %d jours", "über %d Tage", "su %d giorni", "en %d días"}},
    {"%.1f / %d days", {"%.1f / %d jours", "%.1f / %d Tage", "%.1f / %d giorni", "%.1f / %d días"}},
    {"HbA1c unit", {"Unité de l'HbA1c", "HbA1c-Einheit", "Unità dell'HbA1c", "Unidad de la HbA1c"}},
    {"Estimated HbA1c", {"HbA1c estimée", "Geschätzter HbA1c", "HbA1c stimata", "HbA1c estimada"}},
    {"Code:", {"Code :", "Code:", "Codice:", "Código:"}},
    {"Enter the code shown on the panel. Press the green button briefly to show it.", {"Saisissez le code affiché sur le panneau. Appuyez brièvement sur le bouton vert pour l'afficher.", "Geben Sie den auf dem Panel angezeigten Code ein. Drücken Sie kurz die grüne Taste, um ihn anzuzeigen.", "Inserisci il codice mostrato sul pannello. Premi brevemente il pulsante verde per mostrarlo.", "Introduce el código que se muestra en el panel. Pulsa brevemente el botón verde para mostrarlo."}},
    {"Open", {"Ouvrir", "Öffnen", "Apri", "Abrir"}},
    {"Wrong code.", {"Code incorrect.", "Falscher Code.", "Codice errato.", "Código incorrecto."}},
    {"Too many attempts, wait a minute.", {"Trop de tentatives, attendez une minute.", "Zu viele Versuche, bitte eine Minute warten.", "Troppi tentativi, attendi un minuto.", "Demasiados intentos, espera un minuto."}},
    {"Erase and set up new WiFi", {"Effacer et reconfigurer le WiFi", "Löschen und neues WLAN einrichten", "Cancella e configura un nuovo WiFi", "Borrar y configurar un nuevo WiFi"}},
    {"Retry", {"Réessayer", "Erneut versuchen", "Riprova", "Reintentar"}},
    {"Hold 3 s to erase the WiFi", {"Maintenir 3 s pour effacer le WiFi", "3 s halten, um das WLAN zu löschen", "Tieni premuto 3 s per cancellare il WiFi", "Mantén pulsado 3 s para borrar el WiFi"}},
    {"Tap anywhere else to close", {"Touchez ailleurs pour fermer", "Woanders tippen zum Schließen", "Tocca altrove per chiudere", "Toca en otro lugar para cerrar"}},
    {"WiFi connected", {"WiFi connecté", "WLAN verbunden", "WiFi connesso", "WiFi conectado"}},
    {"Configure the panel at:", {"Réglages du panneau :", "Panel einrichten unter:", "Configura il pannello su:", "Configura el panel en:"}},
    {"or", {"ou", "oder", "oppure", "o"}},
    {"WiFi setup failed", {"Échec de la configuration WiFi", "WLAN-Einrichtung fehlgeschlagen", "Configurazione WiFi non riuscita", "Falló la configuración WiFi"}},
    {"Updating the firmware", {"Mise à jour du firmware", "Firmware wird aktualisiert", "Aggiornamento del firmware", "Actualizando el firmware"}},
    {"Do not unplug the power", {"Ne débranchez pas l'alimentation", "Stromversorgung nicht trennen", "Non scollegare l'alimentazione", "No desconectes la alimentación"}},
    // ---- firmware updates ----
    {"Install updates automatically", {"Installer les mises à jour automatiquement", "Updates automatisch installieren", "Installa gli aggiornamenti automaticamente", "Instalar las actualizaciones automáticamente"}},
    {"Install updates at (hour, 0-23)", {"Installer les mises à jour à (heure, 0-23)", "Updates installieren um (Stunde, 0-23)", "Installa gli aggiornamenti alle (ora, 0-23)", "Instalar las actualizaciones a las (hora, 0-23)"}},
    {"Firmware updates", {"Mises à jour du firmware", "Firmware-Updates", "Aggiornamenti del firmware", "Actualizaciones del firmware"}},
    {"Version: %s", {"Version : %s", "Version: %s", "Versione: %s", "Versión: %s"}},
    {"Not checked yet.", {"Pas encore vérifié.", "Noch nicht geprüft.", "Non ancora controllato.", "Aún no comprobado."}},
    {"Up to date.", {"À jour.", "Aktuell.", "Aggiornato.", "Actualizado."}},
    {"Update available: %s", {"Mise à jour disponible : %s", "Update verfügbar: %s", "Aggiornamento disponibile: %s", "Actualización disponible: %s"}},
    {"Last check: %s", {"Dernière vérification : %s", "Letzte Prüfung: %s", "Ultimo controllo: %s", "Última comprobación: %s"}},
    {"Update check failed: %s", {"Échec de la vérification : %s", "Prüfung fehlgeschlagen: %s", "Controllo non riuscito: %s", "Falló la comprobación: %s"}},
    {"Update failed: %s", {"Échec de la mise à jour : %s", "Update fehlgeschlagen: %s", "Aggiornamento non riuscito: %s", "Falló la actualización: %s"}},
    {"Check for updates", {"Rechercher des mises à jour", "Nach Updates suchen", "Cerca aggiornamenti", "Buscar actualizaciones"}},
    {"Install %s now", {"Installer %s maintenant", "%s jetzt installieren", "Installa %s ora", "Instalar %s ahora"}},
    {"Install this update now? The panel restarts.", {"Installer cette mise à jour maintenant ? Le panneau redémarre.", "Dieses Update jetzt installieren? Das Panel startet neu.", "Installare ora questo aggiornamento? Il pannello si riavvia.", "¿Instalar esta actualización ahora? El panel se reinicia."}},
    {"Updating. The panel restarts in about a minute; reload this page then.", {"Mise à jour en cours. Le panneau redémarre dans environ une minute ; rechargez alors cette page.", "Update läuft. Das Panel startet in etwa einer Minute neu; laden Sie die Seite danach neu.", "Aggiornamento in corso. Il pannello si riavvia tra circa un minuto; poi ricarica questa pagina.", "Actualizando. El panel se reinicia en un minuto aproximadamente; luego recarga esta página."}},
    {"No update to install.", {"Aucune mise à jour à installer.", "Kein Update zu installieren.", "Nessun aggiornamento da installare.", "No hay actualización que instalar."}},
    {"bad URL", {"URL invalide", "Ungültige URL", "URL non valida", "URL no válida"}},
    {"bad manifest", {"informations de mise à jour invalides", "ungültige Update-Informationen", "informazioni di aggiornamento non valide", "información de actualización no válida"}},
    {"size mismatch", {"taille incorrecte", "Größe stimmt nicht", "dimensione errata", "tamaño incorrecto"}},
    {"download incomplete", {"téléchargement incomplet", "Download unvollständig", "download incompleto", "descarga incompleta"}},
    {"hash mismatch", {"somme de contrôle incorrecte", "Prüfsumme stimmt nicht", "checksum errato", "suma de verificación incorrecta"}},
    // ---- panel: dashboard ----
    {"Not configured", {"Non configuré", "Nicht konfiguriert", "Non configurato", "No configurado"}},
    {"Open this page to choose your", {"Ouvrez cette page pour choisir votre", "Öffnen Sie diese Seite, um Ihre", "Apri questa pagina per scegliere la", "Abre esta página para elegir tu"}},
    {"glucose data source:", {"source de données glycémiques :", "Glukose-Datenquelle auszuwählen:", "fonte dei dati glicemici:", "fuente de datos de glucosa:"}},
    {"No data yet", {"Pas encore de données", "Noch keine Daten", "Ancora nessun dato", "Aún no hay datos"}},
    {"vs last", {"dernier", "ggü. letztem", "vs ultima", "vs última"}},
    {"vs 15 min", {"15 min", "ggü. 15 Min.", "vs 15 min", "vs 15 min"}},
    {"vs 30 min", {"30 min", "ggü. 30 Min.", "vs 30 min", "vs 30 min"}},
    {"reading time", {"heure de mesure", "Messzeit", "ora misura", "hora lectura"}},
    {"reading age", {"âge de la mesure", "Alter der Messung", "età misura", "edad lectura"}},
    {"%d min", {"%d min", "%d Min.", "%d min", "%d min"}},
    {"average", {"moyenne", "Durchschnitt", "media", "promedio"}},
    {"in range", {"dans la cible", "im Zielbereich", "in target", "en rango"}},
    {"lowest / highest", {"min / max", "Min / Max", "min / max", "mín / máx"}},
    {"variability", {"variabilité", "Variabilität", "variabilità", "variabilidad"}},
    {"low", {"bas", "niedrig", "basso", "bajo"}},
    {"high", {"haut", "hoch", "alto", "alto"}},
    {"SD", {"ET", "SA", "DS", "DE"}},
    // ---- fetch errors (after the "Provider: " prefix) ----
    {"Nightscout URL not set", {"URL Nightscout non renseignée", "Nightscout-URL nicht gesetzt", "URL Nightscout non impostato", "URL de Nightscout no definida"}},
    {"Dexcom username/password not set", {"Identifiant/mot de passe Dexcom non renseigné", "Dexcom-Benutzername/Passwort nicht gesetzt", "Nome utente/password Dexcom non impostati", "Usuario/contraseña de Dexcom no definidos"}},
    {"LibreLinkUp email/password not set", {"E-mail/mot de passe LibreLinkUp non renseigné", "LibreLinkUp-E-Mail/Passwort nicht gesetzt", "Email/password LibreLinkUp non impostate", "Correo/contraseña de LibreLinkUp no definidos"}},
    {"access denied (check token)", {"accès refusé (vérifiez le jeton)", "Zugriff verweigert (Token prüfen)", "accesso negato (controlla il token)", "acceso denegado (revisa el token)"}},
    {"unreadable response", {"réponse illisible", "Antwort nicht lesbar", "risposta illeggibile", "respuesta ilegible"}},
    {"no readings", {"aucune mesure", "keine Messwerte", "nessuna misurazione", "sin lecturas"}},
    {"reading has no value", {"mesure sans valeur", "Messwert ohne Wert", "misurazione senza valore", "lectura sin valor"}},
    {"login failed (HTTP %d)", {"échec de connexion (HTTP %d)", "Anmeldung fehlgeschlagen (HTTP %d)", "accesso non riuscito (HTTP %d)", "inicio de sesión fallido (HTTP %d)"}},
    {"no recent readings", {"aucune mesure récente", "keine aktuellen Messwerte", "nessuna misurazione recente", "sin lecturas recientes"}},
    {"session expired", {"session expirée", "Sitzung abgelaufen", "sessione scaduta", "sesión caducada"}},
    {"unreadable login response (HTTP %d)", {"réponse de connexion illisible (HTTP %d)", "Anmeldeantwort nicht lesbar (HTTP %d)", "risposta di accesso illeggibile (HTTP %d)", "respuesta de inicio de sesión ilegible (HTTP %d)"}},
    {"wrong email or password", {"e-mail ou mot de passe incorrect", "E-Mail oder Passwort falsch", "email o password errata", "correo o contraseña incorrectos"}},
    {"accept the new terms in the LibreLinkUp app", {"acceptez les nouvelles conditions dans l'app LibreLinkUp", "neue Bedingungen in der LibreLinkUp-App akzeptieren", "accetta i nuovi termini nell'app LibreLinkUp", "acepta los nuevos términos en la app LibreLinkUp"}},
    {"login status %d", {"statut de connexion %d", "Anmeldestatus %d", "stato di accesso %d", "estado de inicio de sesión %d"}},
    {"region redirect failed", {"échec de la redirection de région", "Regionsumleitung fehlgeschlagen", "reindirizzamento di regione non riuscito", "falló la redirección de región"}},
    {"login needs an extra step (open the LibreLinkUp app)", {"la connexion demande une étape de plus (ouvrez l'app LibreLinkUp)", "Anmeldung braucht einen weiteren Schritt (LibreLinkUp-App öffnen)", "l'accesso richiede un passaggio in più (apri l'app LibreLinkUp)", "el inicio de sesión necesita un paso más (abre la app LibreLinkUp)"}},
    {"region redirect loop", {"boucle de redirection de région", "Regionsumleitung in Endlosschleife", "ciclo di reindirizzamento di regione", "bucle de redirección de región"}},
    {"no connections (share your data in LibreLink)", {"aucune connexion (partagez vos données dans LibreLink)", "keine Verbindungen (Daten in LibreLink teilen)", "nessuna connessione (condividi i dati in LibreLink)", "sin conexiones (comparte tus datos en LibreLink)"}},
    {"no glucose measurement", {"aucune mesure de glycémie", "keine Glukosemessung", "nessuna misurazione di glucosio", "sin medición de glucosa"}},
    {"WiFi disconnected", {"WiFi déconnecté", "WLAN getrennt", "WiFi disconnesso", "WiFi desconectado"}},
    {"Waiting for the clock (NTP)", {"En attente de l'horloge (NTP)", "Warte auf die Uhrzeit (NTP)", "In attesa dell'orologio (NTP)", "Esperando el reloj (NTP)"}},
    // ---- web page: settings labels ----
    {"Data source", {"Source de données", "Datenquelle", "Fonte dati", "Fuente de datos"}},
    {"Nightscout URL", {"URL Nightscout", "Nightscout-URL", "URL Nightscout", "URL de Nightscout"}},
    {"Access token (optional)", {"Jeton d'accès (facultatif)", "Zugriffstoken (optional)", "Token di accesso (facoltativo)", "Token de acceso (opcional)"}},
    {"Email", {"E-mail", "E-Mail", "Email", "Correo electrónico"}},
    {"Password", {"Mot de passe", "Passwort", "Password", "Contraseña"}},
    {"Region", {"Région", "Region", "Regione", "Región"}},
    {"Username", {"Nom d'utilisateur", "Benutzername", "Nome utente", "Usuario"}},
    {"Server", {"Serveur", "Server", "Server", "Servidor"}},
    {"Display unit", {"Unité d'affichage", "Anzeigeeinheit", "Unità di visualizzazione", "Unidad de visualización"}},
    {"Time zone", {"Fuseau horaire", "Zeitzone", "Fuso orario", "Zona horaria"}},
    {"Trusted IP addresses (no code needed)", {"Adresses IP de confiance (sans code)", "Vertrauenswürdige IP-Adressen (ohne Code)", "Indirizzi IP fidati (senza codice)", "Direcciones IP de confianza (sin código)"}},
    {"%s: use IPv4 addresses or ranges such as 192.168.1.20 or 192.168.1.0/24 (at most 8, ranges /8 to /32)", {"%s : adresses IPv4 ou plages, par ex. 192.168.1.20 ou 192.168.1.0/24 (8 au plus, plages /8 à /32)", "%s: IPv4-Adressen oder Bereiche wie 192.168.1.20 oder 192.168.1.0/24 (höchstens 8, Bereiche /8 bis /32)", "%s: indirizzi IPv4 o intervalli come 192.168.1.20 o 192.168.1.0/24 (al massimo 8, intervalli da /8 a /32)", "%s: direcciones IPv4 o rangos como 192.168.1.20 o 192.168.1.0/24 (como máximo 8, rangos de /8 a /32)"}},
    {"Check server certificates", {"Vérifier les certificats des serveurs", "Serverzertifikate prüfen", "Verifica i certificati dei server", "Verificar los certificados de los servidores"}},
    {"Yes (recommended)", {"Oui (recommandé)", "Ja (empfohlen)", "Sì (consigliato)", "Sí (recomendado)"}},
    {"No (insecure)", {"Non (non sécurisé)", "Nein (unsicher)", "No (non sicuro)", "No (inseguro)"}},
    {"Brightness (%)", {"Luminosité (%)", "Helligkeit (%)", "Luminosità (%)", "Brillo (%)"}},
    {"Night mode (dark theme)", {"Mode nuit (thème sombre)", "Nachtmodus (dunkles Design)", "Modalità notte (tema scuro)", "Modo noche (tema oscuro)"}},
    {"Off", {"Désactivé", "Aus", "Disattivato", "Desactivado"}},
    {"On", {"Activé", "An", "Attivato", "Activado"}},
    {"Scheduled", {"Programmé", "Zeitgesteuert", "Programmato", "Programado"}},
    {"Night starts at", {"Début de la nuit", "Nacht beginnt um", "La notte inizia alle", "La noche empieza a las"}},
    {"Night ends at", {"Fin de la nuit", "Nacht endet um", "La notte finisce alle", "La noche termina a las"}},
    {"Night brightness (%)", {"Luminosité la nuit (%)", "Helligkeit nachts (%)", "Luminosità notturna (%)", "Brillo nocturno (%)"}},
    {"%s must be between %d and %d", {"%s doit être compris entre %d et %d", "%s muss zwischen %d und %d liegen", "%s deve essere compreso tra %d e %d", "%s debe estar entre %d y %d"}},
    {"%s must be a time (HH:MM)", {"%s doit être une heure (HH:MM)", "%s muss eine Uhrzeit sein (HH:MM)", "%s deve essere un orario (HH:MM)", "%s debe ser una hora (HH:MM)"}},
    {"Language", {"Langue", "Sprache", "Lingua", "Idioma"}},
    {"Low threshold (mg/dL)", {"Seuil bas (mg/dL)", "Unterer Grenzwert (mg/dL)", "Soglia bassa (mg/dL)", "Umbral bajo (mg/dL)"}},
    {"High threshold (mg/dL)", {"Seuil haut (mg/dL)", "Oberer Grenzwert (mg/dL)", "Soglia alta (mg/dL)", "Umbral alto (mg/dL)"}},
    {"Top row, left", {"Rangée du haut, gauche", "Obere Reihe, links", "Riga in alto, sinistra", "Fila superior, izquierda"}},
    {"Top row, middle", {"Rangée du haut, centre", "Obere Reihe, Mitte", "Riga in alto, centro", "Fila superior, centro"}},
    {"Top row, right", {"Rangée du haut, droite", "Obere Reihe, rechts", "Riga in alto, destra", "Fila superior, derecha"}},
    {"Middle row, left", {"Rangée du milieu, gauche", "Mittlere Reihe, links", "Riga centrale, sinistra", "Fila central, izquierda"}},
    {"Middle row, centre", {"Rangée du milieu, centre", "Mittlere Reihe, Mitte", "Riga centrale, centro", "Fila central, centro"}},
    {"Middle row, right", {"Rangée du milieu, droite", "Mittlere Reihe, rechts", "Riga centrale, destra", "Fila central, derecha"}},
    {"Bottom row", {"Rangée du bas", "Untere Reihe", "Riga in basso", "Fila inferior"}},
    // ---- web page: option labels ----
    {"Europe", {"Europe", "Europa", "Europa", "Europa"}},
    {"Europe 2", {"Europe 2", "Europa 2", "Europa 2", "Europa 2"}},
    {"United States", {"États-Unis", "USA", "Stati Uniti", "Estados Unidos"}},
    {"Outside US", {"Hors États-Unis", "Außerhalb der USA", "Fuori dagli USA", "Fuera de EE. UU."}},
    {"Germany", {"Allemagne", "Deutschland", "Germania", "Alemania"}},
    {"France", {"France", "Frankreich", "Francia", "Francia"}},
    {"Japan", {"Japon", "Japan", "Giappone", "Japón"}},
    {"Asia-Pacific", {"Asie-Pacifique", "Asien-Pazifik", "Asia-Pacifico", "Asia-Pacífico"}},
    {"Australia", {"Australie", "Australien", "Australia", "Australia"}},
    {"UAE", {"Émirats arabes unis", "VAE", "Emirati Arabi Uniti", "Emiratos Árabes Unidos"}},
    {"Canada", {"Canada", "Kanada", "Canada", "Canadá"}},
    {"London", {"Londres", "London", "Londra", "Londres"}},
    {"Helsinki / Athens", {"Helsinki / Athènes", "Helsinki / Athen", "Helsinki / Atene", "Helsinki / Atenas"}},
    {"New York", {"New York", "New York", "New York", "Nueva York"}},
    {"Empty", {"Vide", "Leer", "Vuoto", "Vacío"}},
    {"Glucose value", {"Valeur de glycémie", "Glukosewert", "Valore glicemico", "Valor de glucosa"}},
    {"Trend arrow", {"Flèche de tendance", "Trendpfeil", "Freccia di tendenza", "Flecha de tendencia"}},
    {"Change vs last reading", {"Variation vs mesure précédente", "Änderung zur letzten Messung", "Variazione rispetto all'ultima misura", "Cambio respecto a la última lectura"}},
    {"Change vs 30 min ago", {"Variation vs il y a 30 min", "Änderung zu vor 30 Min.", "Variazione rispetto a 30 min fa", "Cambio respecto a hace 30 min"}},
    {"Change vs 15 min ago", {"Variation vs il y a 15 min", "Änderung zu vor 15 Min.", "Variazione rispetto a 15 min fa", "Cambio respecto a hace 15 min"}},
    {"Date and time", {"Date et heure", "Datum und Uhrzeit", "Data e ora", "Fecha y hora"}},
    {"Time of the reading", {"Heure de la mesure", "Zeit der Messung", "Ora della misura", "Hora de la lectura"}},
    {"Age of the reading", {"Âge de la mesure", "Alter der Messung", "Età della misura", "Antigüedad de la lectura"}},
    {"Freshness bar", {"Barre de fraîcheur", "Aktualitätsbalken", "Barra di freschezza", "Barra de frescura"}},
    {"Average glucose", {"Glycémie moyenne", "Durchschnittlicher Glukosewert", "Glicemia media", "Glucosa media"}},
    {"Time in range", {"Temps dans la cible", "Zeit im Zielbereich", "Tempo in target", "Tiempo en rango"}},
    {"Lowest and highest", {"Minimum et maximum", "Niedrigster und höchster Wert", "Minimo e massimo", "Mínimo y máximo"}},
    {"Variability (CV)", {"Variabilité (CV)", "Variabilität (CV)", "Variabilità (CV)", "Variabilidad (CV)"}},
    {"Glucose graph (3 h)", {"Courbe de glycémie (3 h)", "Glukosekurve (3 Std.)", "Grafico glicemico (3 h)", "Gráfico de glucosa (3 h)"}},
    // ---- web page: texts ----
    {"Saved.", {"Enregistré.", "Gespeichert.", "Salvato.", "Guardado."}},
    {"Save", {"Enregistrer", "Speichern", "Salva", "Guardar"}},
    {"Dashboard layout", {"Disposition du tableau de bord", "Layout des Dashboards", "Layout del pannello", "Diseño del panel"}},
    {"Tap a block, then pick what it shows.", {"Touchez un bloc, puis choisissez ce qu'il affiche.", "Block antippen, dann auswählen, was er zeigt.", "Tocca un blocco, poi scegli cosa mostra.", "Toca un bloque y elige qué muestra."}},
    {"Choose what to show: %s", {"Contenu à afficher : %s", "Anzuzeigender Inhalt: %s", "Contenuto da mostrare: %s", "Contenido a mostrar: %s"}},
    {"empty", {"vide", "leer", "vuoto", "vacío"}},
    {"Refresh: every %d min (set by the data source).", {"Actualisation : toutes les %d min (selon la source de données).", "Aktualisierung: alle %d Min. (durch die Datenquelle festgelegt).", "Aggiornamento: ogni %d min (stabilito dalla fonte dati).", "Actualización: cada %d min (según la fuente de datos)."}},
    {"Erase WiFi and reboot into setup", {"Effacer le WiFi et redémarrer en mode configuration", "WLAN löschen und im Einrichtungsmodus neu starten", "Cancella il WiFi e riavvia in modalità configurazione", "Borrar el WiFi y reiniciar en modo configuración"}},
    {"Erase saved WiFi and reboot into setup mode?", {"Effacer le WiFi enregistré et redémarrer en mode configuration ?", "Gespeichertes WLAN löschen und im Einrichtungsmodus neu starten?", "Cancellare il WiFi salvato e riavviare in modalità configurazione?", "¿Borrar el WiFi guardado y reiniciar en modo configuración?"}},
    {"WiFi erased. Rebooting: join the %s hotspot to set it up again.", {"WiFi effacé. Redémarrage : rejoignez le point d'accès %s pour le reconfigurer.", "WLAN gelöscht. Neustart: Verbinden Sie sich mit dem Hotspot %s, um es neu einzurichten.", "WiFi cancellato. Riavvio: collegati all'hotspot %s per configurarlo di nuovo.", "WiFi borrado. Reiniciando: conéctate al punto de acceso %s para configurarlo de nuevo."}},
    {"Back", {"Retour", "Zurück", "Indietro", "Volver"}},
    {"(saved - leave blank to keep)", {"(enregistré - laisser vide pour conserver)", "(gespeichert - leer lassen zum Beibehalten)", "(salvata - lascia vuoto per mantenerla)", "(guardada - déjala vacía para conservarla)"}},
    {"Invalid value for %s", {"Valeur invalide pour %s", "Ungültiger Wert für %s", "Valore non valido per %s", "Valor no válido para %s"}},
    {"%s must start with http:// or https://", {"%s doit commencer par http:// ou https://", "%s muss mit http:// oder https:// beginnen", "%s deve iniziare con http:// o https://", "%s debe empezar por http:// o https://"}},
    {"%s must be between 20 and 400", {"%s doit être compris entre 20 et 400", "%s muss zwischen 20 und 400 liegen", "%s deve essere compreso tra 20 e 400", "%s debe estar entre 20 y 400"}},
    {"Low threshold must be below the high threshold", {"Le seuil bas doit être inférieur au seuil haut", "Der untere Grenzwert muss unter dem oberen liegen", "La soglia bassa deve essere inferiore a quella alta", "El umbral bajo debe ser menor que el alto"}},
};

const char* MONTHS[5][12] = {
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"},
    {"jan", "fév", "mars", "avr", "mai", "juin", "juil", "août", "sept", "oct", "nov", "déc"},
    {"Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"},
    {"gen", "feb", "mar", "apr", "mag", "giu", "lug", "ago", "set", "ott", "nov", "dic"},
    {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"},
};

}  // namespace

void i18nReload() {
  String l = cfgGet("lang");
  lang = l == "fr" ? LANG_FR : l == "de" ? LANG_DE : l == "it" ? LANG_IT : l == "es" ? LANG_ES : LANG_EN;
}

const char* i18nCode() {
  static const char* codes[] = {"en", "fr", "de", "it", "es"};
  return codes[lang];
}

const char* T(const char* en) {
  if (lang == LANG_EN) return en;
  for (const Entry& e : TABLE)
    if (!strcmp(e.en, en)) return e.tr[lang - 1] ? e.tr[lang - 1] : en;
  return en;
}

String i18nDate(const struct tm& t) {
  char buf[24];
  snprintf(buf, sizeof(buf), lang == LANG_DE ? "%d. %s" : "%d %s", t.tm_mday, MONTHS[lang][t.tm_mon]);
  return buf;
}

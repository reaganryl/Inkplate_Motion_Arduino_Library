/*
 * FieldOS 1 — Inkplate 6 MOTION / Scuderia prototype
 * 1024 x 758, high contrast B&W; no Wi-Fi, LEDs, or battery required.
 *
 * USER1  = next page
 * USER2  = previous page
 * WAKE   = Home
 *
 * Optional Pi telemetry: 115200 baud, KEY=VALUE\n, e.g.
 *   HOST=ZYNTHIAN
 *   CPU=51C
 *   LOAD=0.34
 *   VOLT=12.6V
 *   FRIDGE=4.2C
 *   AMBIENT=22C
 *   RMS=0.24
 *   CREST=3.1
 *   NOTE=BUILD SOMETHING GOOD
 *   PAGE=HOME|SIENNA|LAB|QUIET
 * Unknown/missing readings are shown as NO DATA.
 *
 * This file intentionally has the same name as Soldered's Simple_BW
 * example so the existing cloud workflow compiles it unchanged.
 */

#include <InkplateMotion.h>
#include <string.h>

Inkplate inkplate;

static const int SW = 1024;
static const int SH = 758;
static const uint32_t FEED_REFRESH_MS = 15000UL;
static const uint32_t IDLE_REFRESH_MS = 120000UL;
static const uint32_t FEED_STALE_MS = 180000UL;

static uint8_t page = 0;
static uint8_t lastButtons[3] = {HIGH, HIGH, HIGH};
static uint32_t lastButtonAt = 0;
static uint32_t lastDrawAt = 0;
static uint32_t lastFeedAt = 0;
static bool haveFeed = false;
static bool dirty = false;
static bool pageChanged = false;

static char serialLine[96];
static uint8_t serialLen = 0;

static char host[22] = "NO DATA";
static char cpu[18] = "NO DATA";
static char loadAvg[18] = "NO DATA";
static char voltage[18] = "NO DATA";
static char fridge[18] = "NO DATA";
static char ambient[18] = "NO DATA";
static char rms[18] = "NO DATA";
static char crest[18] = "NO DATA";
static char note[42] = "Waiting for your first field note";

static const char *pageNames[4] = {"HOME", "SIENNA", "LAB", "QUIET"};

static void textAt(int x, int y, uint8_t size, const char *s, uint16_t color = BLACK)
{
    inkplate.setTextColor(color);
    inkplate.setTextSize(size);
    inkplate.setTextWrap(false);
    inkplate.setCursor(x, y);
    inkplate.print(s);
}

static void copyValue(char *dest, size_t capacity, const char *src)
{
    if (!capacity) return;
    strncpy(dest, src, capacity - 1);
    dest[capacity - 1] = '\0';
}

static void drawHeader(const char *title, const char *subtitle)
{
    inkplate.fillRect(0, 0, SW, 125, BLACK);
    textAt(30, 23, 6, "FIELDOS", WHITE);
    textAt(33, 88, 2, "SCUDERIA  /  INKPLATE MOTION", WHITE);
    textAt(775, 29, 4, title, WHITE);
    textAt(774, 88, 2, subtitle, WHITE);
}

static void drawFooter()
{
    inkplate.drawFastHLine(30, 709, 964, BLACK);
    char p[38];
    snprintf(p, sizeof(p), "PAGE %u / 4", unsigned(page + 1));
    textAt(31, 726, 2, p);
    textAt(245, 726, 2, "USER1 NEXT    USER2 BACK    WAKE HOME");
}

static void drawCard(int x, int y, int w, int h, const char *label,
                     const char *value, const char *description)
{
    inkplate.drawRect(x, y, w, h, BLACK);
    inkplate.fillRect(x, y, w, 49, BLACK);
    textAt(x + 16, y + 12, 3, label, WHITE);
    // Value sizes are deliberately restricted to avoid clipping.
    size_t n = strlen(value);
    uint8_t valueSize = n > 18 ? 2 : (n > 13 ? 3 : (n > 9 ? 4 : 5));
    textAt(x + 18, y + 75, valueSize, value);
    textAt(x + 18, y + h - 35, 2, description);
}

static void drawHome()
{
    drawHeader("HOME", "OVERVIEW");
    drawCard(28, 152, 471, 247, "PI / LINK", host,
             haveFeed && (millis() - lastFeedAt < FEED_STALE_MS) ? "SERIAL FEED ACTIVE" : "AWAITING PI TELEMETRY");
    drawCard(525, 152, 471, 247, "PI / CPU TEMP", cpu, "ZYNTHIAN HOST");
    drawCard(28, 418, 471, 247, "SIENNA / VOLTS", voltage, "FIELD BUS INPUT");
    drawCard(525, 418, 471, 247, "FIELD / FRIDGE", fridge, "TEMPERATURE INPUT");
}

static void drawSienna()
{
    drawHeader("SIENNA", "FIELD BUS");
    drawCard(28, 152, 471, 247, "BATTERY", voltage, "SOURCE: SERIAL / VOLT");
    drawCard(525, 152, 471, 247, "FRIDGE", fridge, "SOURCE: SERIAL / FRIDGE");
    drawCard(28, 418, 471, 247, "AMBIENT", ambient, "SOURCE: SERIAL / AMBIENT");
    drawCard(525, 418, 471, 247, "RELAY BANK", "NOT LINKED", "NO OUTPUTS SWITCHED");
}

static void drawLab()
{
    drawHeader("LAB", "EDGE / AUDIO");
    drawCard(28, 152, 471, 247, "EDGERAVE / RMS", rms, "VIBRATION MONITOR INPUT");
    drawCard(525, 152, 471, 247, "EDGERAVE / CREST", crest, "FAULT ANALYSIS INPUT");
    drawCard(28, 418, 471, 247, "PI / LOAD", loadAvg, "COMPUTE LOAD / 1 MIN");
    drawCard(525, 418, 471, 247, "MIDI / ARRANGER", "STANDBY", "AUDIO SYSTEM UNTOUCHED");
}

static void drawQuiet()
{
    drawHeader("QUIET", "SANCTUARY");
    inkplate.drawRect(28, 154, 968, 509, BLACK);
    textAt(65, 200, 3, "TODAY'S PERMISSION");
    inkplate.drawFastHLine(65, 259, 888, BLACK);
    textAt(65, 302, 4, "I am allowed to have");
    textAt(65, 360, 4, "a nice, simple house.");
    textAt(65, 470, 3, "Small things. Beautifully done.");
    textAt(65, 595, 2, "FIELD NOTE:");
    textAt(250, 595, 2, note);
}

static void drawScreen()
{
    inkplate.clearDisplay();
    switch (page) {
        case 0: drawHome(); break;
        case 1: drawSienna(); break;
        case 2: drawLab(); break;
        default: drawQuiet(); break;
    }
    drawFooter();
    // Full updates deliberately limited: slow and steady, no LED animation.
    inkplate.display();
    lastDrawAt = millis();
    dirty = false;
    pageChanged = false;
}

static void handleLine(char *line)
{
    char *eq = strchr(line, '=');
    if (!eq) return;
    *eq++ = '\0';
    if (strcmp(line, "PAGE") == 0) {
        const char *p = eq;
        for (uint8_t i = 0; i < 4; ++i) {
            if (strcmp(p, pageNames[i]) == 0) {
                page = i;
                pageChanged = true;
                return;
            }
        }
        return;
    }
    char *dest = NULL;
    size_t cap = 0;
#define FIELD(k, v) if (strcmp(line, k) == 0) { dest = v; cap = sizeof(v); }
    FIELD("HOST", host)
    else FIELD("CPU", cpu)
    else FIELD("LOAD", loadAvg)
    else FIELD("VOLT", voltage)
    else FIELD("FRIDGE", fridge)
    else FIELD("AMBIENT", ambient)
    else FIELD("RMS", rms)
    else FIELD("CREST", crest)
    else FIELD("NOTE", note)
#undef FIELD
    if (dest) {
        copyValue(dest, cap, eq);
        haveFeed = true;
        lastFeedAt = millis();
        dirty = true;
    }
}

static void pollSerial()
{
    while (Serial.available() > 0) {
        char c = char(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            serialLine[serialLen] = '\0';
            handleLine(serialLine);
            serialLen = 0;
        } else if (c >= 32 && c <= 126) {
            if (serialLen < sizeof(serialLine) - 1) {
                serialLine[serialLen++] = c;
            } else {
                serialLen = 0; // discard an oversized line
            }
        }
    }
}

static void pollButtons()
{
    const int pins[3] = {INKPLATE_USER1, INKPLATE_USER2, INKPLATE_WAKE};
    if (millis() - lastButtonAt < 160UL) return;
    for (uint8_t i = 0; i < 3; ++i) {
        uint8_t now = digitalRead(pins[i]);
        if (now == LOW && lastButtons[i] == HIGH) {
            if (i == 0) page = (page + 1) % 4;
            if (i == 1) page = (page + 3) % 4;
            if (i == 2) page = 0;
            pageChanged = true;
            lastButtonAt = millis();
        }
        lastButtons[i] = now;
    }
}

void setup()
{
    Serial.begin(115200);
    inkplate.begin(INKPLATE_BLACKWHITE);
    pinMode(INKPLATE_USER1, INPUT_PULLUP);
    pinMode(INKPLATE_USER2, INPUT_PULLUP);
    pinMode(INKPLATE_WAKE, INPUT_PULLUP);
    drawScreen();
}

void loop()
{
    pollButtons();
    pollSerial();
    const uint32_t elapsed = millis() - lastDrawAt;
    if (pageChanged || (dirty && elapsed >= FEED_REFRESH_MS) || elapsed >= IDLE_REFRESH_MS)
        drawScreen();
    delay(15);
}

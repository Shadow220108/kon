/*
  kon - ESP32-S3 e-reader firmware

  Arduino IDE setup:
    Board package: "esp32" by Espressif
    Board: "ESP32S3 Dev Module"
      Flash Size: 16MB, PSRAM: "OPI PSRAM", USB CDC On Boot: Enabled
    Library: GxEPD2 by Jean-Marc Zingg (installs Adafruit GFX too)
    Upload: hold BOOT, tap RESET, upload over USB-C

  What it does:
    - reads .txt books from the root of the microSD card
    - MENU opens the book list, PREV / NEXT move, BACK opens the book
    - in a book PREV / NEXT turn pages, BACK goes back to the list
    - remembers the page you were on for every book
    - shows the battery percent at the bottom
    - goes into deep sleep after 3 minutes, any button wakes it up
      (e-ink keeps the last page on screen with no power)

  Display: 4.26" GDEY0426T82 (800 x 480, SSD1677) on the 24 pin FPC.
  GxEPD2 drives it with the GDEQ0426T82 class (same panel and controller).
*/

#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD_MMC.h>
#include <Preferences.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSerif12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// ---------------- pins (from the schematic) ----------------
// e-paper
#define EPD_BUSY  9
#define EPD_CS    10
#define EPD_MOSI  11
#define EPD_SCK   12
#define EPD_DC    13
#define EPD_RST   14

// microSD (4 bit SD_MMC)
#define SD_D1     4
#define SD_D0     5
#define SD_CLK    6
#define SD_CMD    7
#define SD_D3     15
#define SD_D2     16
#define SD_EN     47   // P-MOSFET gate, LOW = card powered
#define SD_DET    48   // LOW = card inserted

// buttons (100k pull-up, pressed = LOW)
#define BTN_BACK  3
#define BTN_MENU  8
#define BTN_PREV  17
#define BTN_NEXT  18

// sensing
#define VBAT_ADC  1    // 1M / 1M divider, battery / 2
#define VBUS_DET  2    // 100k / 100k divider, HIGH when USB is plugged in

// ---------------- display ----------------
// paged drawing, 1/4 of the screen at a time so it fits in RAM easily
GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT / 4>
  display(GxEPD2_426_GDEQ0426T82(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

const int MARGIN_X   = 24;
const int TOP_Y      = 36;
const int LINE_H     = 30;
const int FOOTER_H   = 28;

// ---------------- state ----------------
enum Screen { SCREEN_LIST, SCREEN_BOOK };
Screen screen = SCREEN_LIST;

const int MAX_BOOKS = 40;
String books[MAX_BOOKS];
int bookCount = 0;
int listIndex = 0;

File   bookFile;
String bookName;
uint32_t pageStarts[2000];     // file offset where each page starts
int    pageCount = 0;          // pages found so far
int    currentPage = 0;

Preferences prefs;
uint32_t lastActivity = 0;
const uint32_t SLEEP_AFTER_MS = 3UL * 60UL * 1000UL;

// ---------------- helpers ----------------
int batteryPercent() {
  uint32_t mv = analogReadMilliVolts(VBAT_ADC) * 2;   // undo the divider
  int pct = map((int)mv, 3300, 4200, 0, 100);
  return constrain(pct, 0, 100);
}

bool usbConnected() {
  return digitalRead(VBUS_DET) == HIGH;
}

String prefKey(const String &name) {
  // Preferences keys max 15 chars, so hash the file name
  uint32_t h = 5381;
  for (size_t i = 0; i < name.length(); i++) h = h * 33 + name[i];
  return "p" + String(h, HEX);
}

void drawFooter(const String &left) {
  display.setFont(nullptr);
  display.setTextColor(GxEPD_BLACK);
  int y = display.height() - FOOTER_H + 10;
  display.drawFastHLine(0, display.height() - FOOTER_H, display.width(), GxEPD_BLACK);
  display.setCursor(MARGIN_X, y);
  display.print(left);
  String batt = String(batteryPercent()) + "%" + (usbConnected() ? " +" : "");
  display.setCursor(display.width() - MARGIN_X - batt.length() * 6, y);
  display.print(batt);
}

void showMessage(const String &title, const String &line) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(MARGIN_X, 90);
    display.print(title);
    display.setFont(&FreeSerif12pt7b);
    display.setCursor(MARGIN_X, 130);
    display.print(line);
  } while (display.nextPage());
}

// ---------------- sd card ----------------
bool startSD() {
  pinMode(SD_EN, OUTPUT);
  digitalWrite(SD_EN, LOW);       // power the card
  delay(20);
  if (digitalRead(SD_DET) == HIGH) return false;   // no card
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3);
  return SD_MMC.begin("/sdcard", false);           // 4 bit mode
}

void scanBooks() {
  bookCount = 0;
  File root = SD_MMC.open("/");
  if (!root) return;
  File f = root.openNextFile();
  while (f && bookCount < MAX_BOOKS) {
    String n = f.name();
    if (!f.isDirectory() && (n.endsWith(".txt") || n.endsWith(".TXT")) && !n.startsWith(".")) {
      books[bookCount++] = n;
    }
    f = root.openNextFile();
  }
}

// ---------------- book list ----------------
void drawList() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(MARGIN_X, TOP_Y);
    display.print("kon  -  library");

    display.setFont(&FreeSerif12pt7b);
    int perScreen = (display.height() - TOP_Y - FOOTER_H - 10) / LINE_H;
    int first = (listIndex / perScreen) * perScreen;
    for (int i = 0; i < perScreen && first + i < bookCount; i++) {
      int idx = first + i;
      int y = TOP_Y + 20 + (i + 1) * LINE_H;
      if (idx == listIndex) {
        display.fillRect(0, y - 22, display.width(), LINE_H, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
      } else {
        display.setTextColor(GxEPD_BLACK);
      }
      display.setCursor(MARGIN_X, y);
      display.print(books[idx]);
    }
    if (bookCount == 0) {
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(MARGIN_X, TOP_Y + 40);
      display.print("no .txt files on the card");
    }
    drawFooter("BACK open   PREV/NEXT move");
  } while (display.nextPage());
}

// ---------------- reading ----------------
// lays out one page of text starting at 'start', draws it if 'draw' is true,
// and returns the file offset where the next page begins
uint32_t layoutPage(uint32_t start, bool draw) {
  bookFile.seek(start);
  display.setFont(&FreeSerif12pt7b);
  display.setTextColor(GxEPD_BLACK);

  int maxW = display.width() - MARGIN_X * 2;
  int lines = (display.height() - TOP_Y - FOOTER_H) / LINE_H;
  int line = 0;
  String word, text;
  uint32_t pos = start;
  uint32_t lineStartPos = start;

  auto flushLine = [&](const String &s) {
    if (draw) {
      display.setCursor(MARGIN_X, TOP_Y + line * LINE_H);
      display.print(s);
    }
    line++;
  };

  while (bookFile.available() && line < lines) {
    char c = bookFile.read();
    pos++;
    if (c == '\r') continue;

    if (c == ' ' || c == '\n') {
      String test = text.length() ? text + " " + word : word;
      int16_t x1, y1; uint16_t w, h;
      display.getTextBounds(test, 0, 0, &x1, &y1, &w, &h);
      if ((int)w > maxW && text.length()) {
        flushLine(text);
        lineStartPos = pos - word.length() - 1;
        text = word;
        if (line >= lines) return lineStartPos;
      } else {
        text = test;
      }
      word = "";
      if (c == '\n') {
        flushLine(text);
        text = "";
        lineStartPos = pos;
      }
    } else {
      word += c;
    }
  }
  if (line < lines && (text.length() || word.length())) {
    flushLine(text.length() ? text + " " + word : word);
    return bookFile.size();
  }
  return bookFile.available() ? lineStartPos : bookFile.size();
}

void drawPage() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    uint32_t next = layoutPage(pageStarts[currentPage], true);
    if (currentPage + 1 >= pageCount && next < bookFile.size() && pageCount < 2000) {
      pageStarts[pageCount++] = next;
    }
    drawFooter(bookName + "   p" + String(currentPage + 1));
  } while (display.nextPage());

  prefs.putUInt(prefKey(bookName).c_str(), currentPage);
}

void openBook(const String &name) {
  bookFile = SD_MMC.open("/" + name);
  if (!bookFile) { showMessage("couldnt open", name); return; }
  bookName = name;
  pageCount = 1;
  pageStarts[0] = 0;
  currentPage = 0;

  // jump back to the saved page by laying out the pages before it
  int saved = prefs.getUInt(prefKey(name).c_str(), 0);
  while (currentPage < saved) {
    uint32_t next = layoutPage(pageStarts[currentPage], false);
    if (next >= bookFile.size()) break;
    pageStarts[pageCount++] = next;
    currentPage++;
  }
  screen = SCREEN_BOOK;
  drawPage();
}

// ---------------- buttons and sleep ----------------
bool pressed(int pin) {
  static uint32_t lastPress[49] = {0};
  if (digitalRead(pin) == LOW && millis() - lastPress[pin] > 250) {
    lastPress[pin] = millis();
    lastActivity = millis();
    return true;
  }
  return false;
}

void goToSleep() {
  if (screen == SCREEN_BOOK) bookFile.close();
  display.hibernate();
  SD_MMC.end();
  digitalWrite(SD_EN, HIGH);          // cut power to the card

  uint64_t mask = (1ULL << BTN_BACK) | (1ULL << BTN_MENU) |
                  (1ULL << BTN_PREV) | (1ULL << BTN_NEXT);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

// ---------------- setup / loop ----------------
void setup() {
  Serial.begin(115200);
  pinMode(BTN_BACK, INPUT);
  pinMode(BTN_MENU, INPUT);
  pinMode(BTN_PREV, INPUT);
  pinMode(BTN_NEXT, INPUT);
  pinMode(SD_DET, INPUT_PULLUP);
  pinMode(VBUS_DET, INPUT);
  analogReadResolution(12);

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(115200, true, 2, false);
  display.setRotation(0);             // landscape, buttons on the right of the case

  prefs.begin("kon", false);

  if (!startSD()) {
    showMessage("no sd card", "put a card with .txt files in and press RESET");
    goToSleep();
  }

  scanBooks();

  // reopen the last book if there is one
  String last = prefs.getString("last", "");
  bool found = false;
  for (int i = 0; i < bookCount; i++) if (books[i] == last) { listIndex = i; found = true; }
  if (found) openBook(last); else drawList();

  lastActivity = millis();
}

void loop() {
  if (screen == SCREEN_LIST) {
    if (pressed(BTN_NEXT) && bookCount) { listIndex = (listIndex + 1) % bookCount; drawList(); }
    if (pressed(BTN_PREV) && bookCount) { listIndex = (listIndex - 1 + bookCount) % bookCount; drawList(); }
    if (pressed(BTN_BACK) && bookCount) { prefs.putString("last", books[listIndex]); openBook(books[listIndex]); }
    if (pressed(BTN_MENU)) { scanBooks(); drawList(); }
  } else {
    if (pressed(BTN_NEXT) && currentPage + 1 < pageCount) { currentPage++; drawPage(); }
    if (pressed(BTN_PREV) && currentPage > 0) { currentPage--; drawPage(); }
    if (pressed(BTN_BACK) || pressed(BTN_MENU)) {
      bookFile.close();
      screen = SCREEN_LIST;
      drawList();
    }
  }

  if (millis() - lastActivity > SLEEP_AFTER_MS && !usbConnected()) {
    goToSleep();
  }
  delay(10);
}

// ATLAS CREATOR v1 - Inkplate 6 MOTION
// Offline interactive woodland story / story creator.
// Wheel: highlight; USER1: choose; USER2: continue/choose; WAKE: back/menu.
// Core: Inkplate_Motion:stm32:Inkplate6Motion @ 1.0.2
#include <InkplateMotion.h>

Inkplate inkplate;

const int W = 1024;
const int H = 758;
const uint8_t N_CHOICES = 3;
enum Page { INTRO, PICK_MOOD, PICK_HERO, PICK_GIFT, CHAPTER_ONE, CHAPTER_TWO, ENDING };
Page page = INTRO;
int cursorIndex = 0;
int mood = 0, hero = 0, gift = 0, firstChoice = 0, secondChoice = 0;
uint8_t partialCount = 0;

const char *moods[] = { "Wonder", "Courage", "Kindness" };
const char *heroes[] = { "A little fox", "A watchful wren", "A young inventor" };
const char *gifts[] = { "Listening", "Making", "Remembering" };
const char *firstOptions[] = { "Follow the silver footprints", "Call out to the stranger", "Examine the lantern" };
const char *secondOptions[] = { "Share your light", "Follow the distant bell", "Make a bridge" };

int lastAngle = 0;
int wheelRemainder = 0;
unsigned long nextWheelMs = 0;
bool wheelReady = false;
const int WHEEL_STEP = 120; // 10.5 degrees per selection, modulo 4096
const uint8_t buttonPins[3] = { INKPLATE_USER1, INKPLATE_USER2, INKPLATE_WAKE };
bool lastRaw[3] = { false, false, false };
bool stable[3] = { false, false, false };
unsigned long changedAt[3] = { 0, 0, 0 };

String firstLine() {
  if (hero == 0) return "A small fox found a lantern beneath the old ash tree.";
  if (hero == 1) return "A watchful wren found a lantern caught among the roots.";
  return "A young inventor found a lantern that glowed without a flame.";
}

String moodLine() {
  if (mood == 0) return "The stars were near enough to hear, or so it seemed.";
  if (mood == 1) return "Beyond the hedge, something unknown was waiting.";
  return "Somewhere in the wood, someone needed help.";
}

String secondLine() {
  if (firstChoice == 0) return "The footprints led to a clearing where the trees whispered softly.";
  if (firstChoice == 1) return "A tiny voice answered: 'I have been looking for a friend.'";
  return "Inside the lantern was a map of places that had been forgotten.";
}

String giftLine() {
  if (gift == 0) return "You listen. Beneath the silence, you hear what matters.";
  if (gift == 1) return "You can make useful things from whatever the wood gives you.";
  return "You remember an old promise: leave each place brighter.";
}

String endingLine() {
  if (secondChoice == 0) return "You hold out the lantern. Two lights rise where there was only one.";
  if (secondChoice == 1) return "You follow the bell to a cottage that had lost its way home.";
  return "You build a little bridge. Soon every creature can cross the stream.";
}

String epilogue() {
  if (mood == 0) return "And the wood became a little more wonderful.";
  if (mood == 1) return "You were brave enough to begin, and wise enough to listen.";
  return "The kindness you chose stayed behind, like a warm window at dusk.";
}

void textAt(int x, int y, const String &s, uint8_t size = 2) {
  inkplate.setTextSize(size);
  inkplate.setTextColor(BLACK);
  inkplate.setCursor(x, y);
  inkplate.print(s);
}

// Adafruit GFX built-in font: ~6*textSize pixels per character.
int wrappedText(const String &s, int x, int y, int width, int lineHeight, int size = 2) {
  int maxChars = width / (6 * size);
  if (maxChars < 12) maxChars = 12;
  int start = 0;
  while (start < (int)s.length()) {
    int stop = start + maxChars;
    if (stop >= (int)s.length()) stop = s.length();
    else {
      int breakAt = stop;
      while (breakAt > start && s[breakAt] != ' ') --breakAt;
      if (breakAt > start) stop = breakAt;
    }
    textAt(x, y, s.substring(start, stop), size);
    y += lineHeight;
    start = stop;
    while (start < (int)s.length() && s[start] == ' ') start++;
  }
  return y;
}

void ornament(int offset) {
  // Small, deliberately simple line-art illustration. No external assets.
  int cx = 856, cy = 152;
  inkplate.drawCircle(cx, cy, 55, BLACK);
  inkplate.drawCircle(cx, cy, 46, BLACK);
  for (int i = 0; i < 6; i++) {
    int dx = (i % 3) * 57;
    int baseX = 715 + dx;
    int baseY = 288 + (i % 2) * 9;
    inkplate.drawLine(baseX, baseY, baseX + 21, baseY - 72 - offset, BLACK);
    inkplate.drawLine(baseX + 21, baseY - 72 - offset, baseX + 45, baseY, BLACK);
    inkplate.drawLine(baseX + 21, baseY - 72 - offset, baseX + 21, baseY + 18, BLACK);
  }
  inkplate.drawLine(684, 317, 963, 317, BLACK);
}

void frame(const String &caption) {
  inkplate.clearDisplay();
  inkplate.drawRect(22, 22, W - 44, H - 44, BLACK);
  inkplate.drawRect(29, 29, W - 58, H - 58, BLACK);
  textAt(58, 51, "ATLAS", 3);
  textAt(59, 93, "the little book of possibilities", 1);
  inkplate.drawLine(58, 125, 965, 125, BLACK);
  inkplate.drawLine(58, 695, 965, 695, BLACK);
  textAt(58, 713, caption, 1);
  textAt(816, 713, "CREATOR  /  v1", 1);
}

int currentOptionCount() {
  if (page == PICK_MOOD || page == PICK_HERO || page == PICK_GIFT ||
      page == CHAPTER_ONE || page == CHAPTER_TWO) return N_CHOICES;
  return 0;
}

void optionRow(int index, const String &label) {
  const int y = 414 + index * 80;
  if (index == cursorIndex) {
    inkplate.fillRoundRect(84, y, 856, 64, 8, BLACK);
    inkplate.setTextColor(WHITE);
  } else {
    inkplate.drawRoundRect(84, y, 856, 64, 8, BLACK);
    inkplate.setTextColor(BLACK);
  }
  inkplate.setTextSize(2);
  inkplate.setCursor(111, y + 21);
  inkplate.print(index == cursorIndex ? "> " : "  ");
  inkplate.print(label);
  inkplate.setTextColor(BLACK);
}

void drawPage(bool fullRefresh) {
  frame("wheel: choose    USER1: enter    USER2: next    WAKE: back");
  if (page == INTRO) {
    textAt(65, 177, "The Lantern at the Edge", 3);
    textAt(65, 221, "of the Wood", 3);
    wrappedText("A story is not only something you read. It is something you help make.",
                65, 340, 840, 32, 2);
    inkplate.drawRoundRect(84, 520, 856, 82, 10, BLACK);
    textAt(245, 551, "PRESS USER1 TO BEGIN", 2);
    ornament(0);
  } else if (page == PICK_MOOD) {
    textAt(65, 177, "First: what kind of story?", 3);
    wrappedText("Choose the feeling of the journey.", 65, 299, 820, 30, 2);
    ornament(5);
    for (int i = 0; i < 3; i++) optionRow(i, moods[i]);
  } else if (page == PICK_HERO) {
    textAt(65, 177, "Who will carry the lantern?", 3);
    wrappedText("Every hero sees the world in a different way.", 65, 299, 820, 30, 2);
    ornament(11);
    for (int i = 0; i < 3; i++) optionRow(i, heroes[i]);
  } else if (page == PICK_GIFT) {
    textAt(65, 177, "Choose a quiet strength", 3);
    wrappedText("No strength is worth more than another.", 65, 299, 820, 30, 2);
    ornament(7);
    for (int i = 0; i < 3; i++) optionRow(i, gifts[i]);
  } else if (page == CHAPTER_ONE) {
    textAt(65, 177, "Chapter I: A small light", 3);
    int y = wrappedText(firstLine(), 65, 264, 870, 30, 2);
    wrappedText(moodLine(), 65, y + 15, 870, 30, 2);
    for (int i = 0; i < 3; i++) optionRow(i, firstOptions[i]);
  } else if (page == CHAPTER_TWO) {
    textAt(65, 177, "Chapter II: A choice", 3);
    int y = wrappedText(secondLine(), 65, 264, 870, 30, 2);
    wrappedText(giftLine(), 65, y + 15, 870, 30, 2);
    for (int i = 0; i < 3; i++) optionRow(i, secondOptions[i]);
  } else {
    textAt(65, 177, "The first chapter of you", 3);
    int y = wrappedText(endingLine(), 65, 286, 860, 34, 2);
    wrappedText(epilogue(), 65, y + 23, 850, 34, 2);
    inkplate.drawRoundRect(84, 520, 856, 82, 10, BLACK);
    textAt(192, 551, "USER1: CREATE ANOTHER STORY", 2);
    ornament(6);
  }
  if (fullRefresh || partialCount >= 12) {
    inkplate.display();
    partialCount = 0;
  } else {
    inkplate.partialUpdate(true);
    partialCount++;
  }
}

void go(Page next) {
  page = next;
  cursorIndex = 0;
  drawPage(true);
}

void confirm() {
  switch (page) {
    case INTRO: go(PICK_MOOD); break;
    case PICK_MOOD: mood = cursorIndex; go(PICK_HERO); break;
    case PICK_HERO: hero = cursorIndex; go(PICK_GIFT); break;
    case PICK_GIFT: gift = cursorIndex; go(CHAPTER_ONE); break;
    case CHAPTER_ONE: firstChoice = cursorIndex; go(CHAPTER_TWO); break;
    case CHAPTER_TWO: secondChoice = cursorIndex; go(ENDING); break;
    case ENDING: go(PICK_MOOD); break;
  }
}

void back() {
  switch (page) {
    case INTRO: break;
    case PICK_MOOD: go(INTRO); break;
    case PICK_HERO: go(PICK_MOOD); break;
    case PICK_GIFT: go(PICK_HERO); break;
    case CHAPTER_ONE: go(PICK_GIFT); break;
    case CHAPTER_TWO: go(CHAPTER_ONE); break;
    case ENDING: go(CHAPTER_TWO); break;
  }
}

void updateWheel() {
  if (!wheelReady || millis() < nextWheelMs) return;
  nextWheelMs = millis() + 35;
  int now = (int)inkplate.rotaryEncoder.rawAngle();
  int diff = now - lastAngle;
  if (diff > 2048) diff -= 4096;
  if (diff < -2048) diff += 4096;
  lastAngle = now;
  if (abs(diff) > 650) { wheelRemainder = 0; return; } // reject glitches
  if (!currentOptionCount()) { wheelRemainder = 0; return; }
  wheelRemainder += diff;
  int delta = 0;
  while (wheelRemainder >= WHEEL_STEP) { wheelRemainder -= WHEEL_STEP; delta++; }
  while (wheelRemainder <= -WHEEL_STEP) { wheelRemainder += WHEEL_STEP; delta--; }
  if (delta != 0) {
    const int count = currentOptionCount();
    cursorIndex = (cursorIndex + delta % count + count) % count;
    drawPage(false);
  }
}

void updateButtons() {
  for (int i = 0; i < 3; i++) {
    bool raw = digitalRead(buttonPins[i]) == LOW;
    if (raw != lastRaw[i]) { lastRaw[i] = raw; changedAt[i] = millis(); }
    if (millis() - changedAt[i] >= 35 && raw != stable[i]) {
      stable[i] = raw;
      if (stable[i]) {
        wheelRemainder = 0;
        if (i == 2) back();
        else confirm();  // USER1 select; USER2 continue/select
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  inkplate.begin(INKPLATE_BLACKWHITE);
  pinMode(INKPLATE_USER1, INPUT_PULLUP);
  pinMode(INKPLATE_USER2, INPUT_PULLUP);
  pinMode(INKPLATE_WAKE, INPUT_PULLUP);
  inkplate.peripheralState(INKPLATE_PERIPHERAL_ROTARY_ENCODER, true);
  delay(150);
  wheelReady = inkplate.rotaryEncoder.begin();
  if (wheelReady) lastAngle = (int)inkplate.rotaryEncoder.rawAngle();
  drawPage(true);
  Serial.println("ATLAS CREATOR v1 started");
}

void loop() {
  updateButtons();
  updateWheel();
  delay(8);
}

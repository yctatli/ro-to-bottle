// Damacana aktarma kontrolcüsü — Arduino Nano
// Arıtmanın basınçlı tankındaki suyu belirli aralıklarla 12 V selenoid vanayı açarak
// damacanaya aktarır. XKC-Y25-V sensörü damacana dolunca vanayı anında kapatır.
// Vana 12 V röle modülüyle sürülür. Kartın tek beslemesi 12 V adaptördür (VIN).

const uint8_t PIN_SENSOR = 2;   // XKC-Y25-V sarı kablo (sensör Nano'nun 5V'u ile beslenir)
const uint8_t PIN_BUTTON = 4;   // elle aktarım düğmesi, diğer ucu GND
const uint8_t PIN_BUZZER = 6;   // aktif buzzer: damacana dolunca uyarır (isteğe bağlı)
const uint8_t PIN_VALVE  = 9;   // röle modülü IN girişi
const uint8_t PIN_LED    = 13;  // kart üstü LED: vana açıkken yanar

// --- Ayarlar ---------------------------------------------------------------
const unsigned long INTERVAL_MS   = 6UL * 60 * 60 * 1000;  // aktarım aralığı: 6 saat
const unsigned long OPEN_MS       = 5UL * 60 * 1000;       // vana açık kalma: 5 dk
const unsigned long FIRST_RUN_MS  = 60UL * 1000;           // açılıştan 1 dk sonra ilk aktarım
const unsigned long SWAP_STABLE_MS = 5000;                 // sensör 5 sn boş görürse damacana değişti say
const unsigned long BEEP_EVERY_MS = 10UL * 60 * 1000;      // damacana doluyken 10 dk'da bir uyar
const bool SENSOR_ACTIVE_HIGH = true;  // testte ters çalışırsa false yap
const bool RELAY_ACTIVE_LOW   = false; // röle LOW ile çekiyorsa (çoğu 5 V modül) true yap
const bool BUZZER_ENABLED     = true;
// ---------------------------------------------------------------------------

enum State { WAITING, FILLING, FULL };
State state = WAITING;
unsigned long lastTransfer;   // son aktarımın başladığı an
unsigned long openedAt;
unsigned long lastBeep = 0;
unsigned long emptySince = 0;
uint16_t transfers = 0;

bool bottleFull() {
  bool high = digitalRead(PIN_SENSOR) == HIGH;
  return SENSOR_ACTIVE_HIGH ? high : !high;
}

void setValve(bool open) {
  digitalWrite(PIN_VALVE, open != RELAY_ACTIVE_LOW ? HIGH : LOW);
  digitalWrite(PIN_LED, open ? HIGH : LOW);
}

// Üç kısa bip: "damacana dolu, değiştir"
void beep() {
  if (!BUZZER_ENABLED) return;
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(120);
    digitalWrite(PIN_BUZZER, LOW);
    delay(120);
  }
}

// Düğmeye basıldığı anı bir kez bildirir (50 ms parazit filtresi)
bool buttonPressed() {
  static bool last = HIGH;
  static unsigned long changedAt = 0;
  bool now = digitalRead(PIN_BUTTON);
  if (now != last && millis() - changedAt > 50) {
    changedAt = millis();
    last = now;
    return now == LOW;
  }
  return false;
}

void startFilling(const char* why) {
  lastTransfer = millis();
  if (bottleFull()) {
    Serial.println(F("Damacana dolu, aktarim atlandi"));
    state = FULL;
    return;
  }
  setValve(true);
  openedAt = millis();
  state = FILLING;
  transfers++;
  Serial.print(F("Vana acildi ("));
  Serial.print(why);
  Serial.print(F("), aktarim #"));
  Serial.println(transfers);
}

void stopFilling(const char* why) {
  setValve(false);
  state = bottleFull() ? FULL : WAITING;
  Serial.print(F("Vana kapandi: "));
  Serial.println(why);
}

void setup() {
  setValve(false);                 // pini çıkış yapmadan önce kapalı seviyeye ayarla
  pinMode(PIN_VALVE, OUTPUT);      // böylece açılışta röle bir an bile çekmez
  pinMode(PIN_SENSOR, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  Serial.begin(9600);
  lastTransfer = millis() - INTERVAL_MS + FIRST_RUN_MS;  // ilk aktarım 1 dk sonra
  Serial.println(F("Damacana kontrolcusu basladi"));
}

void loop() {
  unsigned long now = millis();
  bool full = bottleFull();
  bool pressed = buttonPressed();

  switch (state) {
    case WAITING:
      if (full) {
        state = FULL;
        Serial.println(F("Damacana dolu"));
      } else if (pressed) {
        startFilling("dugme");
      } else if (now - lastTransfer >= INTERVAL_MS) {
        startFilling("zamanlayici");
      }
      break;

    case FILLING:
      if (full)                          stopFilling("damacana doldu");
      else if (now - openedAt >= OPEN_MS) stopFilling("sure bitti");
      else if (pressed)                  stopFilling("dugme");
      break;

    case FULL:
      if (full) {
        emptySince = 0;
        if (lastBeep == 0 || now - lastBeep >= BEEP_EVERY_MS) {
          lastBeep = now;
          beep();
        }
      } else {
        if (emptySince == 0) emptySince = now;
        if (now - emptySince >= SWAP_STABLE_MS) {
          emptySince = 0;
          state = WAITING;
          lastTransfer = now - INTERVAL_MS + FIRST_RUN_MS;
          Serial.println(F("Yeni damacana algilandi, 1 dk sonra aktarim"));
        }
      }
      break;
  }
}

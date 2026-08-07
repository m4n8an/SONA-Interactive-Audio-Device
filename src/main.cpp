#include <M5Unified.h>
#include <math.h>
#include <string.h>
#include <Wire.h>
#include <MFRC522_I2C.h>
#include "logo.h"

// ═══════════════════════════════════════════════════
//  SONA – Music Player for M5StickS3
//  3 Aphex Twin songs  |  Transpose & BPM controls
//  Double‑buffered via M5Canvas (no flicker)
// ═══════════════════════════════════════════════════

// ─── Layout ───
static const int   FPS      = 30;

// ─── HAT Vibrator ───
//  StickS3 Hat2-Bus Pin5 (Boot/G0) → Vibrator Motor (Fixed Connect)
//  If you plug it elsewhere (e.g. HY2.0-4P G9/G10) change VIB_PIN.
#define VIB_PIN       0     // GPIO driving the motor control signal
#define VIB_PWM_CH    5     // LEDC channel (unused by M5.Speaker I2S)
#define VIB_PWM_FREQ  5000  // PWM carrier Hz
#define VIB_PWM_RES   8     // 8-bit duty (0..255)

// ─── M5Stack RFID2 Unit (U031-B, WS1850S, I2C 0x28) ───
//  Bottom HY2.0-4P Grove port: Yellow=G9(SDA) White=G10(SCL)
#define RFID2_SDA  9
#define RFID2_SCL  10
// (chipAddr 0x28, "reset" pin 2 unused — RFID2 RST is handled on-module)
static MFRC522_I2C mfrc522(0x28, 2, &Wire); // WS1850S on Grove I2C

static uint32_t     gRfidUid      = 0;    // card UID currently held
static bool         gRfidCard     = false; // a card is currently held (playing)
static unsigned long gRfidLastScan = 0;    // last RFID poll time
static bool         gRecording    = false; // streaming frames to PC

// ─── Unified blue palette  (main: #5ACBFF) ───
static const uint16_t C_BLACK      = 0x0000;
static const uint16_t C_WHITE      = 0xFFFF;
static const uint16_t C_MAIN       = 0x5E5F;   // #5ACBFF – brand blue
static const uint16_t C_MAIN_LT    = 0x8EDF;   // #8EDBFF – light accent
static const uint16_t C_MAIN_DK    = 0x3CDA;   // #3A9BD0 – dark accent
static const uint16_t C_GLASS      = 0x1906;   // dark navy card fill
static const uint16_t C_BORDER     = 0x3C0A;   // medium blue border
static const uint16_t C_MUTED      = 0xADF7;   // cool gray text
static const uint16_t C_DIM        = 0x6B6D;   // dim gray text
static const uint16_t C_GREEN      = 0x07E0;   // status dot
static const uint16_t C_PROG_BG    = 0x1906;   // dark navy
static const uint16_t C_PROG_FG    = 0x5E5F;   // main blue

// ═══════════════════════════════════════════════════
//  Note frequencies  (A major / F# minor scale)
// ═══════════════════════════════════════════════════
#define REST 0
#define A2  110
#define B2  123
#define Cs3 139
#define D3  147
#define E3  165
#define Fs3 185
#define Gs3 208
#define A3  220
#define B3  247
#define Cs4 277
#define D4  294
#define E4  330
#define F4  349
#define Fs4 370
#define G4  392
#define Gs4 415
#define A4  440
#define B4  494
#define Cs5 554
#define D5  587
#define E5  659
#define Fs5 740

struct Note { uint16_t freq; uint16_t dur; };

// ═══════════════════════════════════════════════════
//  Song 1 – "Pulse"  –  steady driving bass rhythm
//  Clear 8th-note pulse → tempo change is unmistakable
// ═══════════════════════════════════════════════════
static const Note S1[] = {
  // ── A minor pulse ──
  {A2,200},{A2,200},{A2,200},{A2,200},
  {A2,200},{A2,200},{E3,200},{A2,200},
  {A2,200},{A2,200},{A2,200},{A2,200},
  {A2,200},{A2,200},{Cs4,200},{A2,200},
  {A2,200},{A2,200},{A2,200},{A2,200},
  {A2,200},{A2,200},{E3,200},{A2,200},
  {A2,200},{A2,200},{A2,200},{A2,200},
  {A2,200},{A2,200},{Fs4,200},{A2,200},
  // ── D pulse ──
  {D3,200},{D3,200},{D3,200},{D3,200},
  {D3,200},{D3,200},{A3,200},{D3,200},
  {D3,200},{D3,200},{D3,200},{D3,200},
  {D3,200},{D3,200},{F4,200},{D3,200},
  // ── E pulse ──
  {E3,200},{E3,200},{E3,200},{E3,200},
  {E3,200},{E3,200},{B3,200},{E3,200},
  {E3,200},{E3,200},{E3,200},{E3,200},
  {E3,200},{E3,200},{Gs4,200},{E3,200},
  {REST,400}
};
static const int S1_N = sizeof(S1) / sizeof(S1[0]);

// ═══════════════════════════════════════════════════
//  Song 2 – "Beat"  –  heavy boom … boom … pulse
//  Rests make each beat clearly separated → very audible
// ═══════════════════════════════════════════════════
static const Note S2[] = {
  // ── heavy A pulse ──
  {A2,240},{REST,240},{A2,240},{REST,240},
  {A2,240},{REST,240},{A2,240},{E3,240},
  {A2,240},{REST,240},{A2,240},{REST,240},
  {A2,240},{REST,240},{A2,240},{Cs4,240},
  {A2,240},{REST,240},{A2,240},{REST,240},
  {A2,240},{REST,240},{A2,240},{E3,240},
  {A2,240},{REST,240},{A2,240},{REST,240},
  {A2,240},{REST,240},{A2,240},{Fs4,240},
  // ── D build ──
  {D3,240},{D3,240},{D3,240},{D3,240},
  {D3,240},{D3,240},{D3,240},{A3,240},
  {D3,240},{D3,240},{D3,240},{D3,240},
  {D3,240},{D3,240},{D3,240},{F4,240},
  // ── E build ──
  {E3,240},{E3,240},{E3,240},{E3,240},
  {E3,240},{E3,240},{E3,240},{Gs4,240},
  {E3,240},{E3,240},{E3,240},{E3,240},
  {E3,240},{E3,240},{E3,240},{A4,240},
  {REST,480}
};
static const int S2_N = sizeof(S2) / sizeof(S2[0]);

// ═══════════════════════════════════════════════════
//  Song 3 – "Groove"  –  oom-pah bass + melody hits
//  Bass on beats, melody off-beats → swinging rhythm
// ═══════════════════════════════════════════════════
static const Note S3[] = {
  // ── A groove ──
  {A2,200},{E3,200},{A2,200},{Cs4,200},
  {A2,200},{E3,200},{A2,200},{E4,200},
  {A2,200},{E3,200},{A2,200},{Fs4,200},
  {A2,200},{E3,200},{A2,200},{E4,200},
  {A2,200},{E3,200},{A2,200},{Cs4,200},
  {A2,200},{E3,200},{A2,200},{A4,200},
  {A2,200},{E3,200},{A2,200},{Fs4,200},
  {A2,200},{E3,200},{A2,200},{E4,200},
  // ── D groove ──
  {D3,200},{A3,200},{D3,200},{F4,200},
  {D3,200},{A3,200},{D3,200},{A4,200},
  {D3,200},{A3,200},{D3,200},{F4,200},
  {D3,200},{A3,200},{D3,200},{D5,200},
  // ── E groove ──
  {E3,200},{B3,200},{E3,200},{Gs4,200},
  {E3,200},{B3,200},{E3,200},{B4,200},
  {E3,200},{B3,200},{E3,200},{Gs4,200},
  {E3,200},{B3,200},{E3,200},{E5,200},
  {REST,400}
};
static const int S3_N = sizeof(S3) / sizeof(S3[0]);

// ═══════════════════════════════════════════════════
//  Song registry
// ═══════════════════════════════════════════════════
struct Song { const char* title; const char* artist; const Note* data; int len; };
static const Song SONGS[] = {
  {"Pulse",  "SONA Rhythms", S1, S1_N},
  {"Beat",   "SONA Rhythms", S2, S2_N},
  {"Groove", "SONA Rhythms", S3, S3_N},
};
static const int SONG_N = sizeof(SONGS) / sizeof(SONGS[0]);

// ═══════════════════════════════════════════════════
//  State
// ═══════════════════════════════════════════════════
enum Screen    { SCR_IDLE, SCR_PLAYER };
enum PlayState { ST_STOPPED, ST_PLAYING, ST_PAUSED };

Screen    gScr     = SCR_IDLE;
PlayState gSt      = ST_STOPPED;
int       gSongIdx = 0;           // current song index
int       gNoteIdx = 0;
unsigned long gT0 = 0;           // playback start (absolute)
unsigned long gNoteT0 = 0;       // note start (absolute)

int   gTranspose = 0;            // semitones: 0,±2,±4
float gBpm       = 1.0f;         // multiplier
float gTiltAngle = 0.0f;         // current IMU tilt (deg) for display
float gShake     = 0.0f;         // shake intensity 0..1
float gLastAx = 0, gLastAy = 0, gLastAz = 0;  // prev accel for shake

// ── button helpers ──
static unsigned long gBtnAPressMs = 0;
static unsigned long gBtnBPressMs = 0;

// ── HAT vibrator state ──
static bool   gVibOn       = true;   // vibrator enable (default ON)
static float  gVibDutyF    = 0.0f;   // motor duty 0..255 (smoothed)
static unsigned long gBLastRelMs = 0;   // last B short-release (double-click)
static bool   gBPendingClick = false;   // awaiting possible 2nd click
static const unsigned long B_DBL_MS = 350;

// ═══════════════════════════════════════════════════
//  Canvas  (double buffer)
// ═══════════════════════════════════════════════════
static M5Canvas gCanvas(&M5.Display);

// ═══════════════════════════════════════════════════
//  Helpers: transpose factor
// ═══════════════════════════════════════════════════
static float transposeFactor()   { return powf(2.0f, gTranspose / 12.0f); }

// ═══════════════════════════════════════════════════
//  Cycle transpose  (0 → +2 → +4 → -2 → -4 → 0 …)
// ═══════════════════════════════════════════════════
static void cycleTranspose() {
  static const int tbl[] = {0, 2, 4, -2, -4};
  static int idx = 0;
  idx = (idx + 1) % 5;
  gTranspose = tbl[idx];
}

// ═══════════════════════════════════════════════════
//  IMU motion  –  tilt → BPM (aggressive)
//  + shake → extra energy + visual amplitude
// ═══════════════════════════════════════════════════
static void updateMotion() {
  float ax, ay, az;
  M5.Imu.getAccel(&ax, &ay, &az);

  // shake intensity = how fast the accel vector changes
  float d = fabsf(ax - gLastAx) + fabsf(ay - gLastAy) + fabsf(az - gLastAz);
  gLastAx = ax; gLastAy = ay; gLastAz = az;
  gShake += (d - gShake) * 0.15f;

  // tilt angle (deg), positive = up/faster
  float angle = atan2f(ax, fabsf(az)) * 180.0f / 3.14159f;
  gTiltAngle = angle;

  // dead zone: ±1° tilt AND low shake → stay
  if (fabsf(angle) < 1.0f && gShake < 0.2f) return;

  // ASYMMETRIC ultra‑aggressive mapping — HUGE audible range:
  //   up:   +4° → ×2.0,   +12° → ×8.0   (fast = high pitched)
  //   down: −2.5° → ×0.5, −8° → ×0.1    (slow = deep & slow)
  // slow side steeper so deceleration is unmistakable
  float target;
  if (angle >= 0)
    target = powf(2.0f, angle / 4.0f);
  else
    target = powf(2.0f, angle / 2.5f);
  target *= (1.0f + gShake * 0.25f);   // shake adds extra energy
  if (target < 0.1f) target = 0.1f;    // slowest 1/10×
  if (target > 8.0f) target = 8.0f;    // fastest 8×

  gBpm += (target - gBpm) * 0.25f;
}

// ═══════════════════════════════════════════════════
//  Tiny 3x5 pixel font  –  mirrors the web edition
// ═══════════════════════════════════════════════════
struct PixChar { char c; uint16_t b; };
static const PixChar PIX[] = {
  {'A',0b111101111101101},{'B',0b110101110101110},{'C',0b111100100100111},
  {'D',0b110101101101110},{'E',0b111100110100111},{'F',0b111100110100100},
  {'G',0b111100101101111},{'H',0b101101111101101},{'I',0b111010010010111},
  {'J',0b001001001101111},{'K',0b101101110101101},{'L',0b100100100100111},
  {'M',0b101111111101101},{'N',0b101111111111101},{'O',0b111101101101111},
  {'P',0b110101110100100},{'Q',0b111101101110011},{'R',0b110101110101101},
  {'S',0b111100111001111},{'T',0b111010010010010},{'U',0b101101101101111},
  {'V',0b101101101101010},{'W',0b101101111111101},{'X',0b101101010101101},
  {'Y',0b101101010010010},{'Z',0b111001010100111},
  {'0',0b111101101101111},{'1',0b010110010010111},{'2',0b111001111100111},
  {'3',0b111001011001111},{'4',0b101101111001001},{'5',0b111100111001111},
  {'6',0b111100111101111},{'7',0b111001010010010},{'8',0b111101111101111},
  {'9',0b111101111001111},
  {'.',0b000000000000010},{':',0b000010000010000},{'x',0b000101010101000},
  {'<',0b000010111010000},
};
static int pixWidth(const char* t, int s) { return (int)strlen(t) * 4 * s; }
template <typename D>
static void drawPixText(D& dst, const char* t, int x, int y, int s, uint16_t col) {
  for (const char* p = t; *p; p++) {
    char ch = *p;
    if (ch >= 'a' && ch <= 'z') ch -= 32;        // uppercase
    uint16_t b = 0;
    if (ch != ' ') {
      for (const PixChar& pc : PIX) if (pc.c == ch) { b = pc.b; break; }
    }
    if (b == 0) { x += 4 * s; continue; }        // space / unknown
    for (int r = 0; r < 5; r++)
      for (int c = 0; c < 3; c++)
        if (b & (1 << (14 - (r * 3 + c))))
          dst.fillRect(x + c * s, y + r * s, s, s, col);
    x += 4 * s;
  }
}
template <typename D>
static void drawLogo(D& dst, int x, int y) {
  for (int yy = 0; yy < LOGO_H; yy++)
    for (int xx = 0; xx < LOGO_W; xx++) {
      uint16_t c = pgm_read_word(&LOGO[yy * LOGO_W + xx]);
      if (c) dst.drawPixel(x + xx, y + yy, c);
    }
}
static void drawCapsule(M5Canvas& dst, int cx, int cy, int w, int h,
                        const char* text, uint16_t col) {
  dst.fillRoundRect(cx, cy, w, h, h / 2, C_GLASS);
  dst.drawRoundRect(cx, cy, w, h, h / 2, C_MAIN);
  dst.setTextSize(1);
  dst.setTextColor(col, C_GLASS);
  dst.setCursor(cx + (w - dst.textWidth(text)) / 2, cy + (h - 8) / 2);
  dst.print(text);
}

// ═══════════════════════════════════════════════════
//  Idle screen  –  dark + banner logo + pixel hint
// ═══════════════════════════════════════════════════
static void drawIdle() {
  int cx = gCanvas.width() / 2;       // 67 (portrait 135x240)
  gCanvas.fillScreen(0x0021);         // near-black #04070B

  // ── original text logo: SONA + ™ + blue line ──
  gCanvas.setTextSize(4);
  gCanvas.setTextColor(C_WHITE, C_BLACK);
  gCanvas.setCursor(cx - 48, 96);
  gCanvas.print("SONA");
  gCanvas.setTextSize(1);
  gCanvas.setTextColor(C_MAIN, C_BLACK);
  gCanvas.setCursor(cx - 48 + 92, 88);
  gCanvas.print("TM");
  gCanvas.drawFastHLine(cx - 30, 132, 60, C_MAIN);

  // pixel hint
  drawPixText(gCanvas, "Tap to start",
              cx - pixWidth("Tap to start", 1) / 2, 210, 1, C_DIM);

  gCanvas.pushSprite(0, 0);
}

// ═══════════════════════════════════════════════════
//  Waveform visual  –  white dotted vertical wave
//  Reference style (Colorpong "Sound wave - 3")
//  Waves with the melody (BPM) & reacts to tilt/shake
// ═══════════════════════════════════════════════════
static void drawAurora(M5Canvas& dst, unsigned long t) {
  int w = dst.width(), h = dst.height();
  int cx = w / 2;

  // continuous forward flow — phase NEVER reverses, so no sudden jerks
  float flow = t * 0.004f * gBpm;
  float amp  = 12.0f + gShake * 16.0f;         // shake → wider wave
  float breathe = 0.75f + 0.25f * sinf(t * 0.003f * gBpm);  // melody pulse

  dst.fillSprite(C_BLACK);

  // ── dense dotted vertical waveform ──
  // 3 interleaved sine lines → dotted band like the reference image
  for (int li = 0; li < 3; li++) {
    float phase = flow + li * 2.1f;
    uint16_t col = (li == 1) ? C_WHITE : 0x6B6D;   // centre line bright
    for (int y = 2; y < h - 2; y += 2) {
      float xoff = amp * breathe * sinf(y * 0.055f + phase);
      int x = cx + (int)xoff;
      if (x < 1 || x >= w - 1) continue;
      dst.drawPixel(x, y, col);
    }
  }

  // ── drifting particles, endless looping motion ──
  for (int i = 0; i < 30; i++) {
    float a = (float)i * 0.21f + t * 0.002f * gBpm;   // always advances
    float px = cx + amp * breathe * sinf(a * 0.7f + i);
    float py = (float)(i * 4 % h);
    int xi = (int)px, yi = (int)py;
    if (xi >= 0 && xi < w)
      dst.drawPixel(xi, yi, (i % 4 == 0) ? C_WHITE : 0x3CDA);
  }

  // ── thin centre guide (faint) ──
  dst.drawFastVLine(cx, 4, h - 8, 0x18C4);
}

// ═══════════════════════════════════════════════════
//  Minimal HUD  –  speed badge + SONA brand
// ═══════════════════════════════════════════════════
static void drawHUD(M5Canvas& dst) {
  int w = dst.width(), h = dst.height();

  // top-left back pill (decorative — long-press A actually returns)
  drawCapsule(dst, 3, 3, 24, 12, "<", C_WHITE);

  // top-right speed pill (same size & style)
  char sp[8]; sprintf(sp, "x%.1f", (double)gBpm);
  drawCapsule(dst, w - 3 - 24, 3, 24, 12, sp, C_WHITE);

  // play/pause dot – bottom right
  dst.fillCircle(w - 7, h - 8, 2, (gSt == ST_PLAYING) ? C_GREEN : C_DIM);

  // song title – uppercase, top centre
  char t[16];
  strncpy(t, SONGS[gSongIdx].title, 15); t[15] = 0;
  for (char* q = t; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
  dst.setTextSize(1);
  dst.setTextColor(C_MUTED, C_BLACK);
  dst.setCursor((w - dst.textWidth(t)) / 2, 3);
  dst.print(t);

  // pixel "Tap to play" (stopped or paused)
  if (gSt != ST_PLAYING) {
    drawPixText(dst, "Tap to play",
                (w - pixWidth("Tap to play", 1)) / 2, h / 2 + 40, 1, C_MAIN);
  }
}

// ═══════════════════════════════════════════════════
//  Draw abstract frame  –  double‑buffered
// ═══════════════════════════════════════════════════
static void drawAbstractFrame() {
  unsigned long t = millis();
  gCanvas.fillSprite(C_BLACK);
  drawAurora(gCanvas, t);
  drawHUD(gCanvas);
  gCanvas.pushSprite(0, 0);
}

// ═══════════════════════════════════════════════════
//  HAT vibrator  –  follows the audio envelope
//  Each sounding note: quick attack → exponential decay,
//  just like the sound wave.  Rests let it wind down.
// ═══════════════════════════════════════════════════
static void updateVibrator() {
  if (!gVibOn) {                    // disabled → off
    if (gVibDutyF > 0.0f) { gVibDutyF = 0.0f; ledcWrite(VIB_PWM_CH, 0); }
    return;
  }
  float target = 0.0f;
  if (gSt == ST_PLAYING) {
    const Song& s = SONGS[gSongIdx];
    uint16_t f = s.data[gNoteIdx].freq;
    if (f >= 20) {   // a note is sounding → follow its volume envelope
      unsigned long dt = millis() - gNoteT0;
      unsigned long dur = (unsigned long)(s.data[gNoteIdx].dur / gBpm);
      if (dur > 0) {
        float phase = (float)dt / (float)dur;         // 0..1 into the note
        if (phase < 0.0f) phase = 0.0f; else if (phase > 1.0f) phase = 1.0f;
        const float attack = 0.08f;                   // very quick attack
        float env = (phase < attack)
                  ? phase / attack                     // rise to peak
                  : 1.0f - (phase - attack) / (1.0f - attack) * 0.35f;  // decay → 65%
        float pitch = (f - 110.0f) / 740.0f;          // A2..Fs5 → 0..1
        if (pitch < 0.0f) pitch = 0.0f; else if (pitch > 1.0f) pitch = 1.0f;
        float strength = 0.65f + pitch * 0.35f;       // higher notes buzz harder
        target = env * strength * 255.0f;
      }
    }
    // rests → target stays 0, motor winds down
  }
  // follow the envelope: fast up, slower decay (stronger feel)
  float k = (target > gVibDutyF) ? 0.7f : 0.28f;
  gVibDutyF += (target - gVibDutyF) * k;
  if (gVibDutyF < 1.0f) gVibDutyF = 0.0f;
  ledcWrite(VIB_PWM_CH, (uint32_t)gVibDutyF);
}

static void vibratorOff() {
  gVibDutyF = 0.0f;
  ledcWrite(VIB_PWM_CH, 0);
}

// ═══════════════════════════════════════════════════
//  Start / stop / pause helpers
// ═══════════════════════════════════════════════════
static void startSong(int idx) {
  M5.Speaker.stop();
  gSongIdx = idx;
  gNoteIdx = 0;
  gT0      = millis();
  gNoteT0  = gT0;
  gSt      = ST_PLAYING;

  uint16_t f = SONGS[idx].data[0].freq;
  uint16_t d = (uint16_t)(SONGS[idx].data[0].dur / gBpm);
  if (f >= 20) M5.Speaker.tone((uint16_t)(f * transposeFactor()), d);
}

static void togglePlayPause() {
  if (gSt == ST_PLAYING) {
    M5.Speaker.stop();
    gSt = ST_PAUSED;
  } else {
    gSt = ST_PLAYING;
    uint16_t f = SONGS[gSongIdx].data[gNoteIdx].freq;
    uint16_t d = (uint16_t)(SONGS[gSongIdx].data[gNoteIdx].dur / gBpm);
    if (f >= 20) M5.Speaker.tone((uint16_t)(f * transposeFactor()), d);
  }
}

static void nextSong() {
  M5.Speaker.stop();
  int next = (gSongIdx + 1) % SONG_N;
  startSong(next);
}

// ═══════════════════════════════════════════════════
//  RFID2  –  card on reader → play;  card off → stop
//  WUPA polls every ~80ms: while a card sits on the reader it
//  stays detected; when it leaves we stop playback.
//  (per-card audio not loaded yet → plays built-in melody)
// ═══════════════════════════════════════════════════
static void updateRfid() {
  unsigned long now = millis();
  if (now - gRfidLastScan < 80) return;   // throttle scan rate
  gRfidLastScan = now;

  // WUPA sees cards in IDLE *and* HALT → true while held on the reader
  byte atqa[2]; byte atqaLen = sizeof(atqa);
  byte r = mfrc522.PICC_WakeupA(atqa, &atqaLen);
  bool present = (r == mfrc522.STATUS_OK || r == mfrc522.STATUS_COLLISION);

  if (!present) {
    if (gRfidCard) {          // card just left → stop
      M5.Speaker.stop();
      gSt = ST_STOPPED;
      gRfidCard = false;
      gRfidUid  = 0;
    }
    return;
  }

  // card present → read its UID
  uint32_t uid = 0;
  if (mfrc522.PICC_ReadCardSerial()) {
    for (byte i = 0; i < mfrc522.uid.size; i++) uid = uid * 256 + mfrc522.uid.uidByte[i];
  }
  mfrc522.PICC_HaltA();      // → HALT so the next WUPA still detects it

  if (gRfidCard && uid == gRfidUid) return;   // same card → keep playing

  gRfidUid  = uid;
  gRfidCard = true;

  // (re)start playback for this card
  M5.Speaker.stop();
  gScr = SCR_PLAYER;
  gSt  = ST_STOPPED;
  gNoteIdx = 0;
  gTranspose = 0;
  gBpm = 1.0f;
  gVibOn = true;
  startSong(uid % SONG_N);   // map card UID → built-in melody
}

// ═══════════════════════════════════════════════════
//  Screenshot over serial  –  PC sends 'S' to capture
//  Sends the canvas as PPM P6 (RGB) so a PC script can
//  reconstruct the exact screen as an image.
// ═══════════════════════════════════════════════════
static void sendScreenshot() {
  int w = gCanvas.width();
  int h = gCanvas.height();
  const uint16_t* buf = (const uint16_t*)gCanvas.getBuffer();

  Serial.print("P6\n");
  Serial.print(w); Serial.print(' '); Serial.print(h); Serial.print('\n');
  Serial.print("255\n");

  for (int y = 0; y < h; y++) {
    const uint16_t* row = buf + (size_t)y * w;
    for (int x = 0; x < w; x++) {
      uint16_t p = row[x];
      uint8_t r = (uint8_t)((p >> 11) & 0x1F);
      uint8_t g = (uint8_t)((p >>  5) & 0x3F);
      uint8_t b = (uint8_t)( p        & 0x1F);
      Serial.write((uint8_t)(r << 3 | r >> 2));
      Serial.write((uint8_t)(g << 2 | g >> 4));
      Serial.write((uint8_t)(b << 3 | b >> 2));
    }
  }
}

// ═══════════════════════════════════════════════════
//  Continuous recording frame  –  PC sends 'R'/'E' to
//  start/stop.  Streams half-res frames back-to-back,
//  each prefixed with "FRAME\n", for a smooth video.
// ═══════════════════════════════════════════════════
static void sendFrameScaled() {    // full-resolution frame for recording
  int w = gCanvas.width();         // 135
  int h = gCanvas.height();        // 240
  const uint16_t* buf = (const uint16_t*)gCanvas.getBuffer();

  Serial.print("FRAME\n");
  Serial.print("P6\n");
  Serial.print(w); Serial.print(' '); Serial.print(h); Serial.print('\n');
  Serial.print("255\n");

  for (int y = 0; y < h; y++) {
    const uint16_t* row = buf + (size_t)y * w;
    for (int x = 0; x < w; x++) {
      uint16_t p = row[x];
      uint8_t r = (uint8_t)((p >> 11) & 0x1F);
      uint8_t g = (uint8_t)((p >>  5) & 0x3F);
      uint8_t b = (uint8_t)( p        & 0x1F);
      Serial.write((uint8_t)(r << 3 | r >> 2));
      Serial.write((uint8_t)(g << 2 | g >> 4));
      Serial.write((uint8_t)(b << 3 | b >> 2));
    }
  }
}

// ═══════════════════════════════════════════════════
//  setup
// ═══════════════════════════════════════════════════
void setup() {
  M5.begin();
  Serial.begin(115200);
  Serial.println("[ScreenshotReady]");   // boot signal for the PC tool
  M5.Display.setRotation(0);   // portrait 135x240
  M5.Speaker.setVolume(255);   // max software volume (0–255)

  // 5× louder: raise ES8311 DAC gain from 0xBF(±0 dB) to 0xCF(+16 dB ≈ 6×).
  // M5Unified writes 0xBF when the speaker initialises; we override it here.
  {
    uint8_t dac_gain = 0xCF;   // +16 dB (same value M5PaperColor uses)
    M5.In_I2C.writeRegister(0x18, 0x32, &dac_gain, 1, 100000);
  }

  // RFID2 card reader on the Grove I2C port
  Wire.begin(RFID2_SDA, RFID2_SCL);
  mfrc522.PCD_Init();

  ledcSetup(VIB_PWM_CH, VIB_PWM_FREQ, VIB_PWM_RES);
  ledcAttachPin(VIB_PIN, VIB_PWM_CH);
  ledcWrite(VIB_PWM_CH, 0);
  gCanvas.createSprite(M5.Display.width(), M5.Display.height());
  drawIdle();
}

// ═══════════════════════════════════════════════════
//  loop
// ═══════════════════════════════════════════════════
void loop() {
  M5.update();

  // serial commands:
  //   'S' → one full-res PPM screenshot
  //   'R' → start continuous recording (half-res frames)
  //   'E' → stop recording
  if (Serial.available()) {
    char c = Serial.read();
    if      (c == 'S') sendScreenshot();
    else if (c == 'R') gRecording = true;
    else if (c == 'E') gRecording = false;
  }

  // continuous recording: stream one frame per loop iteration
  if (gRecording) {
    sendFrameScaled();
    delay(1);
  }

  // RFID2: a card tap auto-plays that card's audio (from idle or player)
  updateRfid();

  // ================================================
  //  IDLE screen
  // ================================================
  if (gScr == SCR_IDLE) {
    if (M5.BtnA.wasPressed()) {
      gScr = SCR_PLAYER;
      gSt  = ST_STOPPED;
      gSongIdx = 0;
      gNoteIdx = 0;
      gTranspose = 0;
      gBpm = 1.0f;
      gVibOn = true;   // vibrator starts enabled on entering player
      drawAbstractFrame();
    }
    delay(50);
    return;
  }

  // ================================================
  //  PLAYER screen  –  IMU + buttons
  // ================================================

  // always track motion so the visual reacts to tilt/shake
  updateMotion();

  // HAT vibrator: strength/frequency follows force
  updateVibrator();

  // ── BtnA: track press time ──
  if (M5.BtnA.wasPressed()) gBtnAPressMs = millis();

  // ── BtnA long press → back to level 1 menu ──
  if (M5.BtnA.wasHold()) {
    M5.Speaker.stop();
    vibratorOff();
    gSt = ST_STOPPED;
    gScr = SCR_IDLE;
    drawIdle();
    delay(50);
    return;
  }

  // ── BtnA short press → play/pause ──
  if (M5.BtnA.wasReleased()) {
    unsigned long held = millis() - gBtnAPressMs;
    if (held < 500) {
      if (gSt == ST_STOPPED) startSong(gSongIdx);
      else                   togglePlayPause();
    }
  }

  // ── BtnB: track press time ──
  if (M5.BtnB.wasPressed()) gBtnBPressMs = millis();

  // ── BtnB long press → transpose (moved from BtnA) ──
  if (M5.BtnB.wasHold()) {
    cycleTranspose();
    if (gSt == ST_PLAYING) {
      M5.Speaker.stop();
      uint16_t f = SONGS[gSongIdx].data[gNoteIdx].freq;
      uint16_t d = (uint16_t)(SONGS[gSongIdx].data[gNoteIdx].dur / gBpm);
      if (f >= 20) M5.Speaker.tone((uint16_t)(f * transposeFactor()), d);
    }
  }

  // ── BtnB short press → single or double click ──
  //   single click: next song / back to idle (delayed for double-click)
  //   double click: toggle vibrator
  if (M5.BtnB.wasReleased()) {
    unsigned long held = millis() - gBtnBPressMs;
    if (held < 500) {
      unsigned long now = millis();
      if (gBPendingClick && (now - gBLastRelMs <= B_DBL_MS)) {
        // double click → vibrator on/off
        gBPendingClick = false;
        gBLastRelMs = 0;
        gVibOn = !gVibOn;
        if (!gVibOn) vibratorOff();
      } else {
        gBPendingClick = true;   // first click → wait for possible 2nd
        gBLastRelMs = now;
      }
    }
  }
  // resolve pending single click (no 2nd press in time)
  if (gBPendingClick && (millis() - gBLastRelMs > B_DBL_MS)) {
    gBPendingClick = false;
    M5.Speaker.stop();
    if (gSt == ST_STOPPED) {
      vibratorOff();
      gScr = SCR_IDLE;
      drawIdle();
      delay(50);
      return;
    }
    int next = (gSongIdx + 1) % SONG_N;
    startSong(next);
  }

  // ================================================
  //  Playback engine
  // ================================================
  if (gSt == ST_PLAYING) {
    // ── melody engine ──
    unsigned long now   = millis();
    const Song& s       = SONGS[gSongIdx];
    unsigned long noteRealDur = (unsigned long)(s.data[gNoteIdx].dur / gBpm);

    if (now - gNoteT0 >= noteRealDur) {
      gNoteIdx++;
      if (gNoteIdx >= s.len) gNoteIdx = 0;   // loop: restart melody forever
      uint16_t f = s.data[gNoteIdx].freq;
      uint16_t d = (uint16_t)(s.data[gNoteIdx].dur / gBpm);
      if (f >= 20) M5.Speaker.tone((uint16_t)(f * transposeFactor()), d);
      else         M5.Speaker.stop();
      gNoteT0 = now;
    }

    drawAbstractFrame();
    delay(1000 / FPS);
    return;
  }

  // ── stopped / paused – keep abstract visual flowing ──
  drawAbstractFrame();
  delay(50);
}

// ============================================================================
//  EINSTEIN-ROSEN RUNNER (Enhanced Edition) - v1.7  (PROGRESS & MENUS)
//
//
//  Build + run (MSYS2 MinGW64) - save this file as EinsteinRosen_Runner.cpp, then paste the line below:
//    g++ -O2 -std=c++17 EinsteinRosen_Runner.cpp -o EinsteinRosen_Runner_v1.7.exe -lfreeglut -lopengl32 -lglu32 -lwinmm -lgdi32; ./EinsteinRosen_Runner_v1.7.exe
// ============================================================================

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <xinput.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <vector>
#include <algorithm>
#include <GL/freeglut.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <string>
#include <map>
#include <chrono>
#ifndef _WIN32
#include <unistd.h>
#endif

#ifndef GLUT_KEY_SHIFT_L
#define GLUT_KEY_SHIFT_L 0x0070
#endif

// ----------------------------------------------------------------------------
//  Constants
// ----------------------------------------------------------------------------
const float PI                = 3.14159265f;
const int   MAX_LEVEL         = 5;
const int   START_HEALTH      = 3;
const int   MAX_HEALTH        = 5;     // max hull points achievable via heart pickups
const float INVULN_TIME       = 3.0f;  // seconds of invincibility after a hit
const float SHIP_RADIUS       = 1.3f;
const float MOUTH_RADIUS      = 6.0f;
const float GRAV_RADIUS       = 34.0f; // gravity field of an OPEN bridge
const float LOCK_DIST         = 7.0f;  // point of no return
const float SUCK_TIME         = 1.6f;
const float TRANSIT_TIME      = 5.0f;
const float EMERGE_TIME       = 1.2f;
const float SEG_LEN           = 110.0f;// distance between course waypoints
const float TICK              = 0.025f;// reference tick the tuning constants use
const float FIRE_INTERVAL     = 0.13f; // seconds between player shots
const float PLAYER_BOLT_SPEED = 1.65f; // units per reference tick
const float ENEMY_BOLT_SPEED  = 0.75f; // units per reference tick

// ---- v1.2 mechanics ----
const float MAX_ENERGY        = 100.0f;
const float BOOST_DRAIN       = 30.0f; // energy per second while boosting
const float ENERGY_REGEN      = 18.0f; // energy per second while recovering
const float ENERGY_UNLOCK     = 25.0f; // after full depletion, boost stays locked until this much is back
const float ENERGY_DELAY      = 0.9f;  // seconds after boosting before regen starts
const int   MAX_MISSILES      = 6;
const int   START_MISSILES    = 3;
const float MISSILE_SPEED     = 1.25f; // units per reference tick
const float MISSILE_COOLDOWN  = 0.6f;
const float MISSILE_BLAST     = 14.0f;
const float MINE_BLAST        = 11.0f;
const float COMBO_WINDOW      = 3.2f;  // seconds to keep a chain alive
const float POWERUP_TIME      = 12.0f;
const float SHIELD_TIME       = 25.0f;

// ---- v1.3 mechanics ----
const float DASH_COST         = 25.0f; // energy
const float DASH_TIME         = 0.45f; // seconds of i-frames
const float DASH_TAP_WINDOW   = 0.28f; // double-tap window
const float GRAZE_DIST        = 5.0f;
const float BOMB_RADIUS       = 55.0f;
const float DRONE_TIME        = 22.0f;
const float RING_RADIUS       = 7.5f;
const float ION_DRAIN         = 30.0f; // energy per second inside an ion storm
const int   MAX_UPGRADE_LEVEL = 3;

// ----------------------------------------------------------------------------
//  Math helpers
// ----------------------------------------------------------------------------
struct Vec3 { float x, y, z; };

static inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static inline Vec3& operator+=(Vec3& a, Vec3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }

static inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float length(Vec3 a) { return sqrtf(dot(a, a)); }
static inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static inline Vec3 normalize(Vec3 a) {
    float l = length(a);
    if (l < 1e-6f) return {0, 0, 0};
    return a * (1.0f / l);
}
static inline float clampf(float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float frand() { return (float)rand() / (float)RAND_MAX; }
static inline float rnd(float lo, float hi) { return lo + (hi - lo) * frand(); }
static inline float wrapAngle(float a) { return atan2f(sinf(a), cosf(a)); }

// ----------------------------------------------------------------------------
//  Level themes
// ----------------------------------------------------------------------------
struct Theme {
    const char* name;
    float nebA[3], nebB[3];   // nebula colours
    float light[3];           // sunlight colour
    float accent[3];          // HUD / tunnel / open-bridge colour
    int   center;             // 0 ringed planet, 1 sun, 2 black hole
    float cc[3];              // centerpiece colour
};

const Theme themes[MAX_LEVEL] = {
    {"AZURE DRIFT",   {0.10f, 0.35f, 0.90f}, {0.00f, 0.70f, 0.80f}, {1.00f, 0.95f, 0.90f}, {0.30f, 0.75f, 1.00f}, 0, {0.30f, 0.50f, 0.95f}},
    {"EMBER REACH",   {0.90f, 0.25f, 0.10f}, {0.80f, 0.50f, 0.10f}, {1.00f, 0.80f, 0.60f}, {1.00f, 0.55f, 0.20f}, 1, {1.00f, 0.55f, 0.15f}},
    {"VERDANT VEIL",  {0.10f, 0.80f, 0.40f}, {0.00f, 0.50f, 0.70f}, {0.90f, 1.00f, 0.90f}, {0.30f, 1.00f, 0.60f}, 0, {0.25f, 0.85f, 0.45f}},
    {"VIOLET ABYSS",  {0.60f, 0.15f, 0.90f}, {0.90f, 0.20f, 0.60f}, {0.95f, 0.85f, 1.00f}, {0.80f, 0.40f, 1.00f}, 1, {0.70f, 0.50f, 1.00f}},
    {"EVENT HORIZON", {0.90f, 0.70f, 0.20f}, {0.50f, 0.20f, 0.70f}, {1.00f, 0.95f, 0.80f}, {1.00f, 0.85f, 0.40f}, 2, {1.00f, 0.80f, 0.30f}},
};

// ----------------------------------------------------------------------------
//  Game data structures
// ----------------------------------------------------------------------------
enum State { S_MENU, S_FLYING, S_SUCKING, S_TRANSIT, S_EMERGING, S_GAMEOVER, S_VICTORY, S_UPGRADE };
enum ObType { OB_ASTEROID, OB_MINE, OB_ORBITER };
enum PowType { PW_SHIELD = 0, PW_RAPID, PW_MAGNET, PW_DOUBLE, PW_AMMO, PW_DRONE, PW_BOMB, PW_COUNT };
enum EType { ET_FIGHTER = 0, ET_KAMIKAZE, ET_GUNSHIP, ET_BOSS };
enum UpType { UP_ENERGY = 0, UP_CANNON, UP_RACK, UP_HULL, UP_SALVAGE, UP_SHIELDGEN, UP_BOMBBAY, UP_COUNT };

struct Mouth {
    Vec3 pos;
    Vec3 normal;      // horizontal unit vector
    const char* name;
};

struct Particle {
    Vec3 pos, vel;
    float life, decay;
    float r, g, b;
};

struct Popup {
    Vec3 pos;
    char text[24];
    float life;
    float r, g, b;
};

struct Coin {
    Vec3 pos;
    bool taken;
    float phase;
};

struct HeartPickup {
    Vec3 pos;
    bool taken;
    float phase;
};

struct PowerUp {
    Vec3 pos;
    int type;
    bool taken;
    float phase;
};

struct Beacon {
    Vec3 pos;
    bool active;
    float anim;
};

struct Obstacle {
    ObType type;
    Vec3 pos, vel;
    float radius;
    Vec3 scl;
    Vec3 axis;
    float rot, rotSpeed;
    float r, g, b;
    Vec3 center;
    float orbitR, theta, omega;
    int plane;
    bool active;
    float hp, maxHp;   // v1.2: asteroids/orbiters can be destroyed
};

struct Planet {
    Vec3 pos;
    float radius;
    float r, g, b;
    int kind;      // v1.4: 0 gas giant, 1 ocean world, 2 lava/rock
    float seed;
};

struct Puff {
    Vec3 pos;
    float size, t, a;
};

struct Bolt {
    Vec3 pos, vel;
    float life;
    bool enemy;       // true = fired by an enemy ship
    bool graze;       // v1.3: graze reward already paid
    float minD;       // v1.3: closest approach to the player so far
};

struct Missile {
    Vec3 pos, vel;
    float life;
    int target;       // index into enemies, -1 = none
};

struct Enemy {
    Vec3 pos, vel, fwd, anchor;
    float hp, maxHp, radius;
    float phase, fireTimer, fireInterval, maxSpeed, flash;
    bool active, hunting;
    int type;          // EType
    int stage;         // boss phase 0..2
    float special;     // boss ring-burst timer
    float aux;         // boss summon timer
    bool counts;       // counts toward the sector objective
};

struct Shock {
    Vec3 pos;
    float t, dur, maxR;
    float r, g, b;
};

struct Ring {
    Vec3 pos, dir;
    bool taken;
    float phase, lastSide;
};

struct IonCloud {
    Vec3 pos;
    float radius, phase;
};

// ----------------------------------------------------------------------------
//  Globals
// ----------------------------------------------------------------------------
State state = S_MENU;
float stateTimer = 0.0f;
float gameTime = 0.0f;
bool  paused = false;
int   lastTime = 0;

int   level = 1;
int   score = 0;
int   health = START_HEALTH;
float invuln = 0.0f;
float hitFlash = 0.0f;
float camShake = 0.0f;

int   coinsGot = 0, coinsReq = 1;
int   beaconsGot = 0;
bool  bridgeOpen = false;
float openAmt = 0.0f;

Vec3  ship = {0, 0, 4};
Vec3  drift = {0, 0, 0};
float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
float rollVel = 0.0f;
float speed = 0.0f;
float shipScale = 1.0f;
bool  shipVisible = true;
bool  boosting = false;
bool  aiming = false;
Vec3  deathPos = {0, 0, 0};

// Aim-assist lock state
const Enemy* lockedEnemy = nullptr;
Vec3  aimAssistPoint = {0, 0, 0};
bool  hasAimLock = false;

float flashAlpha = 0.0f;
float flashR = 0.6f, flashG = 0.8f, flashB = 1.0f;

char  bannerText[96] = "";
char  bannerSub[128] = "";
float bannerTimer = 0.0f;
float bannerR = 1.0f, bannerG = 1.0f, bannerB = 1.0f;
float sealCooldown = 0.0f;

float nearIntensity = 0.0f;
float nearestDist = 1e9f;
bool  outOfBounds = false;

Vec3  suckRel = {0, 0, 0};

Mouth bridge;
Mouth entry;

int   segs = 4;
std::vector<Vec3>        wps;
std::vector<Coin>        coins;
std::vector<HeartPickup> hearts;
std::vector<PowerUp>     powerups;
std::vector<Beacon>      beacons;
std::vector<Obstacle>    obstacles;
std::vector<Planet>      planets;
std::vector<Particle>    particles;
std::vector<Popup>       popups;
std::vector<Enemy>       enemies;
std::vector<Bolt>        bolts;
std::vector<Missile>     missiles;

float fireTimer = 0.0f;
int   fireSide = 1;
int   enemiesKilled = 0, enemiesReq = 0;

// ---- v1.2 state ----
float energy = MAX_ENERGY;
bool  energyLocked = false;
float energyDelay = 0.0f;
int   missileAmmo = START_MISSILES;
float missileCooldown = 0.0f;
bool  missileRequest = false;
float shieldTimer = 0.0f, rapidTimer = 0.0f, magnetTimer = 0.0f, doubleTimer = 0.0f;
int   combo = 0, comboBest = 0;
float comboTimer = 0.0f;
float levelTime = 0.0f;
bool  tookHit = false;

// ---- v1.3 state ----
std::vector<Ring>     rings;
std::vector<IonCloud> clouds;
std::vector<Vec3>     summonQueue;
int   ringsGot = 0, ringsTotal = 0;
bool  ringBonusGiven = false;
bool  inIon = false;
int   upg[UP_COUNT] = {};
int   upChoice[3] = {0, 1, 2};
int   upSelected = 0;
int   bombAmmo = 1;
float bombCooldown = 0.0f;
bool  bombRequest = false;
float droneTimer = 0.0f, droneAngle = 0.0f, droneFireTimer = 0.0f;
Vec3  dronePos = {0, 0, 0};
float dashTimer = 0.0f, dashDir = 0.0f;
float lastRollTap[2] = {-10.0f, -10.0f};
bool  prevRollL = false, prevRollR = false;
bool  uiLeft = false, uiRight = false, uiConfirm = false, uiClick = false;
int   uiPick = -1;
int   mouseX = 0, mouseY = 0;

// ---- v1.4 renderer state ----
Vec3  sunDirW = {0.45f, 0.55f, 0.55f};
bool  gfxOK = false;
bool  postOn = true;
const char* shotPath = nullptr;
int   shotAt = 0, shotFrame = 0, startLevel = 0, winW = 1280, winH = 720, benchN = 0;
int   curWinW = 1280, curWinH = 720;   // cached by reshape(): glutGet(GLUT_WINDOW_*) is a blocking round trip

// ---- v1.6 user settings (all editable in the in-game Settings screen) ----
bool  cfgFullscreen = false, cfgVsync = true, cfgShowFps = false, cfgRadar = true, cfgHints = true, cfgPopups = true;
bool  cfgMSAA = true, cfgGodRays = true, cfgInvertY = false, cfgAutoCruise = true;
float cfgRenderScale = 1.0f, cfgFov = 65.0f, cfgExposure = 0.92f, cfgVignette = 0.55f, cfgCA = 1.0f, cfgGrain = 1.0f, cfgBlur = 1.0f;
float volMaster = 1.0f, volMusic = 1.0f, volSfx = 1.0f;
float cfgMouseSens = 1.0f, cfgPadSens = 1.0f, cfgPadDead = 0.15f, cfgRumble = 1.0f, cfgAimAssist = 1.0f;
int   cfgDifficulty = 1, cfgFpsIdx = 0;
float cfgInvuln = 3.0f, cfgComboWin = 3.2f, cfgShake = 1.0f, cfgFlash = 1.0f;
const int FPS_VALUES[8] = {0, 30, 60, 90, 120, 144, 165, 240};
bool  settingsOpen = false;
bool  uiUp = false, uiDown = false, uiBack = false, uiTabPrev = false, uiTabNext = false, uiReset = false;
bool  startSettings = false;
int   startTab = 0;
float fpsSmooth = 0.0f;
// difficulty: 0 easy, 1 normal, 2 hard, 3 insane (taken from the run in progress, see runDiff)
extern int runDiff;
float diffFire()        { static const float v[4] = {1.35f, 1.0f, 0.80f, 0.62f}; return v[runDiff]; }
float diffHp()          { static const float v[4] = {0.80f, 1.0f, 1.25f, 1.50f}; return v[runDiff]; }
float diffScore()       { static const float v[4] = {0.80f, 1.0f, 1.25f, 1.50f}; return v[runDiff]; }
int   diffStartHealth() { static const int   v[4] = {4, 3, 3, 2}; return v[runDiff]; }

// ---- v1.7 run tracking, scoreboard + menus ----
int   runDiff = 1;                       // difficulty of the run in progress
float runTime = 0.0f;                    // sum of the sector times completed in this run
float sectorTimes[MAX_LEVEL] = {0, 0, 0, 0, 0};
float bestSector[MAX_LEVEL]  = {0, 0, 0, 0, 0};
bool  lastSectorPB = false;
bool  resuming = false;                  // true while beginLevel() restores a saved checkpoint
int   menuSel = 0, menuPage = 0;         // menuPage: 0 none, 1 scoreboard, 2 how to play
int   confirmId = 0, confirmSel = 0;     // confirmId: 1 overwrite save, 2 clear scores
bool  lastRunValid = false, lastRunWon = false;
int   lastRunRank = -1, lastRunLevel = 0;
float lastRunTotal = 0.0f;
float lastRunTimes[MAX_LEVEL] = {0, 0, 0, 0, 0};
int   startPage = 0, startConfirm = 0, testEnd = 0;
bool  startPause = false;
static inline const char* fmtTime(float t, char* buf, size_t n) {
    if (t < 0.0f) t = 0.0f;
    int m = (int)(t / 60.0f);
    snprintf(buf, n, "%02d:%04.1f", m, t - m * 60.0f);
    return buf;
}
bool  noVsync = false;

// upgrade-derived stats
float maxEnergyVal()   { return MAX_ENERGY + 25.0f * upg[UP_ENERGY]; }
float energyRegenVal() { return ENERGY_REGEN + 5.0f * upg[UP_ENERGY]; }
float boltDamageVal()  { return 1.0f + 0.5f * upg[UP_CANNON]; }
int   maxMissilesVal() { return MAX_MISSILES + 2 * upg[UP_RACK]; }
int   maxHealthVal()   { return MAX_HEALTH + upg[UP_HULL]; }
float baseMagnetR()    { return 8.0f + 5.0f * upg[UP_SALVAGE]; }
int   maxBombsVal()    { return 2 + upg[UP_BOMBBAY]; }

float dispScore = 0.0f;                 // animated score counter
bool  bloomOn = true;                   // toggled with B or Y button
float bloomStrength = 1.15f;
GLuint texFull = 0, texSmall[2] = {0, 0};
const int FULL_TEX = 2048, SMALL_TEX = 512;
const int TRAIL_LEN = 72;
std::vector<Vec3>  trailL, trailR;      // engine ribbon history
std::vector<Shock> shocks;

Vec3  centerPos = {0, 0, 0};
float centerRadius = 40.0f;
Vec3  boundMin = {-85, -50, -500};
Vec3  boundMax = { 85,  50,   40};

Vec3  camCur = {0, 3, 14};
Vec3  camPos = {0, 3, 14};
bool  camSnap = true;
float currentFov = 65.0f;
float camYawOff = 0.0f, camPitchOff = 0.0f; // right-stick camera orbit offset

bool keys[256] = {};
bool specialKeys[256] = {};

// Mouse input state
bool  mouseLocked = true;
bool  mouseLeftDown = false;
bool  mouseRightDown = false;
float mouseDx = 0.0f, mouseDy = 0.0f;
bool  warpingPointer = false;

// Xbox 360 Controller state (smoothed radial sticks)
bool  padConnected = false;
float padLX = 0.0f, padLY = 0.0f;
float padRX = 0.0f, padRY = 0.0f;
float padLT = 0.0f, padRT = 0.0f;
bool  padA = false, padB = false, padX = false, padY = false;
bool  padLB = false, padRB = false;
bool  padUp = false, padDown = false;
#ifdef _WIN32
WORD  prevPadButtons = 0;
#endif
float rumbleTimer = 0.0f;
float rumbleL = 0.0f, rumbleR = 0.0f;

const int NUM_STARS = 3200;
Vec3  starPos[NUM_STARS];
float starBright[NUM_STARS];
float starHue[NUM_STARS];       // 0=blue, 0.5=white, 1.0=warm gold/red
const int NUM_PUFFS = 32;
Puff  puffs[NUM_PUFFS];
const int NUM_DUST = 650;
Vec3  dustBase[NUM_DUST];
const int NUM_STREAKS = 300;
float streak[NUM_STREAKS][3];

GLdouble hudMV[16], hudProj[16];
GLint    hudVP[4];

// ----------------------------------------------------------------------------
//  Procedural Real-Time Audio Engine (BGM + Polyphonic SFX)
// ----------------------------------------------------------------------------
enum SfxType {
    SFX_LASER = 0,
    SFX_ENEMY_LASER,
    SFX_COIN,
    SFX_HEART,
    SFX_BEACON,
    SFX_EXPLOSION,
    SFX_HIT,
    SFX_BRIDGE_OPEN,
    SFX_WORMHOLE,
    SFX_MISSILE,
    SFX_POWERUP,
    SFX_DASH
};

struct SfxVoice {
    bool active;
    int type;
    float t;
    float dur;
    float phase;
    float phase2;
};

bool audioMuted = false;

#ifdef _WIN32
const int AUDIO_SR = 44100;
const int AUDIO_BUF_SAMPLES = 1470; // ~33ms per buffer
const int AUDIO_NUM_BUFS = 4;
const int MAX_VOICES = 16;

HWAVEOUT hWaveOut = nullptr;
WAVEHDR waveHeaders[AUDIO_NUM_BUFS];
short audioBuffers[AUDIO_NUM_BUFS][AUDIO_BUF_SAMPLES * 2];
SfxVoice voices[MAX_VOICES] = {};
CRITICAL_SECTION audioCS;
bool audioRunning = false;
HANDLE audioThread = nullptr;
double bgmTime = 0.0;
double bgmPhase1 = 0.0, bgmPhase2 = 0.0, bgmPhase3 = 0.0, bgmThrusterPhase = 0.0;
unsigned int audioRng = 2463534242u;

static inline float fastRandAudio() {
    audioRng ^= audioRng << 13;
    audioRng ^= audioRng >> 17;
    audioRng ^= audioRng << 5;
    return (float)(audioRng & 0xFFFF) / 32768.0f - 1.0f;
}

void playSfx(SfxType type) {
    if (!audioRunning || audioMuted) return;
    EnterCriticalSection(&audioCS);
    int slot = -1;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].active) { slot = i; break; }
    }
    if (slot == -1) slot = rand() % MAX_VOICES;
    SfxVoice& v = voices[slot];
    v.active = true;
    v.type = type;
    v.t = 0.0f;
    v.phase = 0.0f;
    v.phase2 = 0.0f;
    switch (type) {
        case SFX_LASER:       v.dur = 0.14f; break;
        case SFX_ENEMY_LASER: v.dur = 0.18f; break;
        case SFX_COIN:        v.dur = 0.26f; break;
        case SFX_HEART:       v.dur = 0.55f; break;
        case SFX_BEACON:      v.dur = 0.75f; break;
        case SFX_EXPLOSION:   v.dur = 0.70f; break;
        case SFX_HIT:         v.dur = 0.45f; break;
        case SFX_BRIDGE_OPEN: v.dur = 1.30f; break;
        case SFX_WORMHOLE:    v.dur = 1.50f; break;
        case SFX_MISSILE:     v.dur = 0.55f; break;
        case SFX_POWERUP:     v.dur = 0.50f; break;
        case SFX_DASH:        v.dur = 0.30f; break;
    }
    LeaveCriticalSection(&audioCS);
}

void fillAudioBuffer(short* buf, int numSamples) {
    const float dt = 1.0f / (float)AUDIO_SR;
    const float bassNotes[4] = { 55.0f, 43.65f, 65.41f, 49.0f }; // A1, F1, C2, G1
    const float arpNotes[8]  = { 220.0f, 261.63f, 329.63f, 392.0f, 440.0f, 392.0f, 329.63f, 261.63f };

    State curState = state;
    bool isPaused = paused;
    bool isBoosting = boosting && (curState == S_FLYING);
    float curSpeed = fabsf(speed);
    float levelPitch = 1.0f + 0.05f * (level - 1);

    EnterCriticalSection(&audioCS);
    for (int i = 0; i < numSamples; ++i) {
        float mixL = 0.0f, mixR = 0.0f;

        if (!audioMuted && !isPaused) {
            bgmTime += dt;
            float tempo = (curState == S_TRANSIT) ? 8.0f : (curState == S_FLYING ? 6.0f : 4.0f);
            int bar = (int)(bgmTime * 0.5) & 3;
            int step = (int)(bgmTime * tempo) & 7;

            float bassFreq = bassNotes[bar] * levelPitch;
            float arpFreq  = arpNotes[step] * levelPitch * (bar == 1 ? 0.8909f : (bar == 2 ? 1.1892f : 1.0f));
            float stepFrac = fmodf((float)(bgmTime * tempo), 1.0f);
            float arpEnv   = expf(-stepFrac * 4.5f);

            bgmPhase1 += 2.0 * PI * bassFreq * dt;
            bgmPhase2 += 2.0 * PI * arpFreq * dt;
            bgmPhase3 += 2.0 * PI * (bassFreq * 1.5f) * dt;

            float bass = 0.5f * sinf((float)bgmPhase1) + 0.25f * sinf((float)bgmPhase1 * 2.0f);
            float pad  = sinf((float)bgmPhase3 + 0.4f * sinf((float)bgmTime * 1.5f));
            float arp  = sinf((float)bgmPhase2 + 0.6f * sinf((float)bgmPhase2 * 2.0f)) * arpEnv;

            float bgmVol = (curState == S_MENU || curState == S_GAMEOVER) ? 0.09f : 0.13f;
            float pan = 0.5f + 0.35f * sinf((float)bgmTime * 1.2f);
            mixL += volMusic * bgmVol * (bass * 0.7f + pad * 0.35f + arp * pan);
            mixR += volMusic * bgmVol * (bass * 0.7f + pad * 0.35f + arp * (1.0f - pan));

            if (curState == S_FLYING && curSpeed > 0.05f) {
                float thFreq = (isBoosting ? 78.0f : 52.0f) + curSpeed * 30.0f;
                bgmThrusterPhase += 2.0 * PI * thFreq * dt;
                float th = (0.6f * sinf((float)bgmThrusterPhase) + 0.4f * fastRandAudio()) * 0.035f * curSpeed * (isBoosting ? 1.8f : 1.0f);
                mixL += th * volSfx;
                mixR += th * volSfx;
            }
        }

        if (!audioMuted) {
            for (int vIdx = 0; vIdx < MAX_VOICES; ++vIdx) {
                SfxVoice& v = voices[vIdx];
                if (!v.active) continue;
                float p = v.t / v.dur;
                if (p >= 1.0f) { v.active = false; continue; }

                float env = 1.0f - p;
                float s = 0.0f;
                switch (v.type) {
                    case SFX_LASER: {
                        float freq = 1350.0f * expf(-p * 5.2f) + 160.0f;
                        v.phase += 2.0f * PI * freq * dt;
                        s = (sinf(v.phase) + 0.35f * sinf(v.phase * 2.0f)) * env * 0.22f;
                        break;
                    }
                    case SFX_ENEMY_LASER: {
                        float freq = 620.0f * expf(-p * 3.8f) + 95.0f;
                        v.phase += 2.0f * PI * freq * dt;
                        s = (sinf(v.phase) > 0.0f ? 0.6f : -0.6f) * env * 0.16f;
                        break;
                    }
                    case SFX_COIN: {
                        float freq = (p < 0.35f) ? 987.77f : 1318.51f;
                        v.phase += 2.0f * PI * freq * dt;
                        s = sinf(v.phase) * env * env * 0.25f;
                        break;
                    }
                    case SFX_HEART: {
                        float freq = (p < 0.28f) ? 523.25f : (p < 0.56f ? 659.25f : 783.99f);
                        v.phase += 2.0f * PI * freq * dt;
                        v.phase2 += 2.0f * PI * (freq * 1.5f) * dt;
                        s = (0.7f * sinf(v.phase) + 0.3f * sinf(v.phase2)) * env * 0.28f;
                        break;
                    }
                    case SFX_BEACON: {
                        float freq = 330.0f + 330.0f * p;
                        v.phase += 2.0f * PI * freq * dt;
                        v.phase2 += 2.0f * PI * (freq * 1.25f) * dt;
                        s = (sinf(v.phase) + sinf(v.phase2)) * 0.5f * env * 0.26f;
                        break;
                    }
                    case SFX_EXPLOSION: {
                        float rumbleFreq = 110.0f * expf(-p * 3.0f) + 28.0f;
                        v.phase += 2.0f * PI * rumbleFreq * dt;
                        s = (0.65f * fastRandAudio() * expf(-p * 3.5f) + 0.55f * sinf(v.phase)) * env * 0.38f;
                        break;
                    }
                    case SFX_HIT: {
                        float alarm = (fmodf(p * 6.0f, 1.0f) < 0.5f) ? 480.0f : 320.0f;
                        v.phase += 2.0f * PI * alarm * dt;
                        s = (0.5f * fastRandAudio() * expf(-p * 5.0f) + 0.5f * sinf(v.phase)) * env * 0.35f;
                        break;
                    }
                    case SFX_BRIDGE_OPEN: {
                        float freq = 165.0f + 495.0f * (p * p);
                        v.phase += 2.0f * PI * freq * dt;
                        v.phase2 += 2.0f * PI * (freq * 1.5f) * dt;
                        s = (sinf(v.phase) + 0.6f * sinf(v.phase2)) * sinf(p * PI) * 0.26f;
                        break;
                    }
                    case SFX_WORMHOLE: {
                        float freq = 220.0f + 160.0f * sinf(p * 4.0f * PI);
                        v.phase += 2.0f * PI * freq * dt;
                        s = (0.7f * sinf(v.phase) + 0.3f * fastRandAudio()) * sinf(p * PI) * 0.25f;
                        break;
                    }
                    case SFX_MISSILE: {
                        float freq = 120.0f + 520.0f * p;
                        v.phase += 2.0f * PI * freq * dt;
                        s = (0.55f * fastRandAudio() * env + 0.45f * sinf(v.phase)) * sinf(fminf(p * 6.0f, 1.0f) * PI * 0.5f) * env * 0.30f;
                        break;
                    }
                    case SFX_POWERUP: {
                        float freq = (p < 0.25f) ? 523.25f : (p < 0.5f ? 659.25f : (p < 0.75f ? 783.99f : 1046.5f));
                        v.phase += 2.0f * PI * freq * dt;
                        v.phase2 += 2.0f * PI * (freq * 2.0f) * dt;
                        s = (0.75f * sinf(v.phase) + 0.25f * sinf(v.phase2)) * env * 0.26f;
                        break;
                    }
                    case SFX_DASH: {
                        float freq = 260.0f + 1100.0f * p;
                        v.phase += 2.0f * PI * freq * dt;
                        s = (0.5f * fastRandAudio() + 0.5f * sinf(v.phase)) * sinf(p * PI) * 0.22f;
                        break;
                    }
                }
                mixL += s * volSfx;
                mixR += s * volSfx;
                v.t += dt;
            }
        }

        int outL = (int)(clampf(mixL * volMaster, -0.98f, 0.98f) * 32767.0f);
        int outR = (int)(clampf(mixR * volMaster, -0.98f, 0.98f) * 32767.0f);
        buf[i * 2 + 0] = (short)outL;
        buf[i * 2 + 1] = (short)outR;
    }
    LeaveCriticalSection(&audioCS);
}

DWORD WINAPI audioThreadProc(LPVOID) {
    while (audioRunning) {
        bool queuedAny = false;
        for (int i = 0; i < AUDIO_NUM_BUFS; ++i) {
            if (waveHeaders[i].dwFlags & WHDR_DONE) {
                fillAudioBuffer(audioBuffers[i], AUDIO_BUF_SAMPLES);
                waveOutWrite(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
                queuedAny = true;
            }
        }
        if (!queuedAny) Sleep(4);
    }
    return 0;
}

void initAudio() {
    InitializeCriticalSection(&audioCS);
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = AUDIO_SR;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 4;
    wfx.nAvgBytesPerSec = AUDIO_SR * 4;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        hWaveOut = nullptr;
        return;
    }
    audioRunning = true;
    for (int i = 0; i < AUDIO_NUM_BUFS; ++i) {
        memset(&waveHeaders[i], 0, sizeof(WAVEHDR));
        waveHeaders[i].lpData = (LPSTR)audioBuffers[i];
        waveHeaders[i].dwBufferLength = AUDIO_BUF_SAMPLES * 4;
        waveOutPrepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
        fillAudioBuffer(audioBuffers[i], AUDIO_BUF_SAMPLES);
        waveOutWrite(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
    }
    audioThread = CreateThread(nullptr, 0, audioThreadProc, nullptr, 0, nullptr);
}
#else
void initAudio() {}
void playSfx(SfxType) {}
#endif

// ----------------------------------------------------------------------------
//  Xbox 360 Controller (Dynamic XInput Loader + Radial Curve + Rumble)
// ----------------------------------------------------------------------------
#ifdef _WIN32
typedef DWORD (WINAPI *PFN_XInputGetState)(DWORD, XINPUT_STATE*);
typedef DWORD (WINAPI *PFN_XInputSetState)(DWORD, XINPUT_VIBRATION*);
PFN_XInputGetState pXInputGetState = nullptr;
PFN_XInputSetState pXInputSetState = nullptr;

void initXInput() {
    const char* dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for (int i = 0; i < 3; ++i) {
        HMODULE h = LoadLibraryA(dlls[i]);
        if (h) {
            pXInputGetState = (PFN_XInputGetState)GetProcAddress(h, "XInputGetState");
            pXInputSetState = (PFN_XInputSetState)GetProcAddress(h, "XInputSetState");
            if (pXInputGetState) break;
        }
    }
}

void triggerRumble(float leftMotor, float rightMotor, float duration) {
    if (!padConnected || !pXInputSetState || cfgRumble <= 0.0f) return;
    rumbleL = std::max(rumbleL, clampf(leftMotor * cfgRumble, 0.0f, 1.0f));
    rumbleR = std::max(rumbleR, clampf(rightMotor * cfgRumble, 0.0f, 1.0f));
    rumbleTimer = std::max(rumbleTimer, duration);
}

// Radial 2D deadzone + non-linear power curve for smooth precision aiming
static void applyRadialStick(short rawX, short rawY, float dz, float curveExp, float& outX, float& outY) {
    float fx = clampf((float)rawX / 32767.0f, -1.0f, 1.0f);
    float fy = clampf((float)rawY / 32767.0f, -1.0f, 1.0f);
    float mag = sqrtf(fx * fx + fy * fy);
    if (mag < dz) {
        outX = 0.0f;
        outY = 0.0f;
        return;
    }
    float norm = clampf((mag - dz) / (1.0f - dz), 0.0f, 1.0f);
    float curved = powf(norm, curveExp);
    outX = (fx / mag) * curved;
    outY = (fy / mag) * curved;
}
#else
void initXInput() {}
void triggerRumble(float, float, float) {}
#endif

// Forward declarations
void startGame();
void openSettings();
void saveCheckpoint();

void pollGamepad(float dt) {
#ifdef _WIN32
    if (!pXInputGetState) return;
    XINPUT_STATE xs;
    memset(&xs, 0, sizeof(xs));
    if (pXInputGetState(0, &xs) == ERROR_SUCCESS) {
        padConnected = true;
        float targetLX = 0.0f, targetLY = 0.0f, targetRX = 0.0f, targetRY = 0.0f;
        applyRadialStick(xs.Gamepad.sThumbLX, xs.Gamepad.sThumbLY, cfgPadDead, 1.55f, targetLX, targetLY);
        applyRadialStick(xs.Gamepad.sThumbRX, xs.Gamepad.sThumbRY, cfgPadDead, 1.50f, targetRX, targetRY);

        // Low-pass filter stick inputs for fluid ship control
        float smooth = clampf(dt * 22.0f, 0.0f, 1.0f);
        padLX = lerpf(padLX, targetLX, smooth);
        padLY = lerpf(padLY, targetLY, smooth);
        padRX = lerpf(padRX, targetRX, smooth);
        padRY = lerpf(padRY, targetRY, smooth);

        padLT = (xs.Gamepad.bLeftTrigger  > 30) ? (xs.Gamepad.bLeftTrigger  - 30) / 225.0f : 0.0f;
        padRT = (xs.Gamepad.bRightTrigger > 30) ? (xs.Gamepad.bRightTrigger - 30) / 225.0f : 0.0f;

        WORD btns = xs.Gamepad.wButtons;
        WORD justPressed = btns & ~prevPadButtons;
        prevPadButtons = btns;

        padA  = (btns & XINPUT_GAMEPAD_A) != 0;
        padB  = (btns & XINPUT_GAMEPAD_B) != 0;
        padX  = (btns & XINPUT_GAMEPAD_X) != 0;
        padY  = (btns & XINPUT_GAMEPAD_Y) != 0;
        padLB = (btns & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
        padRB = (btns & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
        padUp = (btns & XINPUT_GAMEPAD_DPAD_UP) != 0;
        padDown = (btns & XINPUT_GAMEPAD_DPAD_DOWN) != 0;

        // v1.2: R-Stick click fires a homing missile
        if (justPressed & XINPUT_GAMEPAD_RIGHT_THUMB) missileRequest = true;
        if (justPressed & XINPUT_GAMEPAD_DPAD_LEFT) { uiLeft = true; if (state == S_FLYING && !settingsOpen) bombRequest = true; }
        if (justPressed & XINPUT_GAMEPAD_DPAD_RIGHT) uiRight = true;
        if (justPressed & XINPUT_GAMEPAD_DPAD_UP)    uiUp = true;
        if (justPressed & XINPUT_GAMEPAD_DPAD_DOWN)  uiDown = true;
        if (justPressed & XINPUT_GAMEPAD_LEFT_SHOULDER)  uiTabPrev = true;
        if (justPressed & XINPUT_GAMEPAD_RIGHT_SHOULDER) uiTabNext = true;
        if (justPressed & XINPUT_GAMEPAD_B) uiBack = true;
        if (justPressed & XINPUT_GAMEPAD_X) uiReset = true;

        // Whole-game menu & system controls on Xbox 360 controller
        if (settingsOpen) {
            if (justPressed & XINPUT_GAMEPAD_A) uiConfirm = true;
            if (justPressed & XINPUT_GAMEPAD_START) uiBack = true;
        } else if (justPressed & (XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_A)) {
            if (state == S_MENU) {
                uiConfirm = true;                                   // menu / overlay navigation (see update())
            } else if (state == S_GAMEOVER || state == S_VICTORY) {
                paused = false;
                startGame();
            } else if (state == S_UPGRADE) {
                if (justPressed & XINPUT_GAMEPAD_A) uiConfirm = true;
            } else if (paused && (justPressed & XINPUT_GAMEPAD_A)) {
                uiConfirm = true;
            } else if (justPressed & XINPUT_GAMEPAD_START) {
                paused = !paused;
                menuSel = 0;
            }
        }
        if (justPressed & XINPUT_GAMEPAD_BACK) {
            if (settingsOpen) uiBack = true;
            else openSettings();
        }
        if (justPressed & XINPUT_GAMEPAD_Y) {
            bloomOn = !bloomOn;
        }

        // Update controller rumble motors
        if (rumbleTimer > 0.0f && !paused && !settingsOpen) {
            rumbleTimer -= dt;
            XINPUT_VIBRATION vib;
            vib.wLeftMotorSpeed  = (WORD)(rumbleL * 65535.0f);
            vib.wRightMotorSpeed = (WORD)(rumbleR * 65535.0f);
            pXInputSetState(0, &vib);
        } else {
            rumbleL = rumbleR = 0.0f;
            XINPUT_VIBRATION vib = {0, 0};
            pXInputSetState(0, &vib);
        }
    } else {
        padConnected = false;
        padLX = padLY = padRX = padRY = padLT = padRT = 0.0f;
        padA = padB = padX = padY = padLB = padRB = padUp = padDown = false;
        prevPadButtons = 0;
    }
#else
    (void)dt;
#endif
}

// ----------------------------------------------------------------------------
//  Small utilities
// ----------------------------------------------------------------------------
Vec3 forwardVec() {
    return {-sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch)};
}

// Convert a point from ship-local space to world space (same order as the GL transforms)
Vec3 shipLocalToWorld(Vec3 v) {
    float cr = cosf(roll), sr = sinf(roll);
    float x = v.x * cr - v.y * sr, y = v.x * sr + v.y * cr, z = v.z;
    float cp = cosf(pitch), sp = sinf(pitch);
    float y2 = y * cp - z * sp, z2 = y * sp + z * cp;
    float cy = cosf(yaw), sy = sinf(yaw);
    float x3 = x * cy + z2 * sy, z3 = -x * sy + z2 * cy;
    return ship + Vec3{x3, y2, z3} * shipScale;
}

const Theme& curTheme() {
    int i = level - 1;
    if (i < 0) i = 0;
    if (i >= MAX_LEVEL) i = MAX_LEVEL - 1;
    return themes[i];
}

// Power-up presentation data
const char* POW_NAMES[PW_COUNT] = { "SHIELD", "RAPID FIRE", "MAGNET", "SCORE x2", "MISSILES", "DRONE", "PULSE BOMB" };
const float POW_COL[PW_COUNT][3] = {
    {0.30f, 0.85f, 1.00f},   // shield  - cyan
    {1.00f, 0.90f, 0.20f},   // rapid   - yellow
    {0.80f, 0.40f, 1.00f},   // magnet  - purple
    {0.40f, 1.00f, 0.50f},   // x2      - green
    {1.00f, 0.50f, 0.15f},   // ammo    - orange
    {0.30f, 1.00f, 0.80f},   // drone   - teal
    {1.00f, 0.30f, 0.60f}    // bomb    - hot pink
};

const char* UP_NAMES[UP_COUNT] = { "ENERGY CELL", "CANNON OVERCHARGE", "MISSILE RACK", "HULL PLATING",
                                   "SALVAGE MAGNET", "SHIELD GENERATOR", "BOMB BAY" };
const char* UP_DESC_A[UP_COUNT] = { "+25 max energy and", "+50% cannon damage", "+2 max missiles,", "+1 max hull and",
                                    "Bigger permanent pickup", "Start every sector", "+1 max pulse bomb," };
const char* UP_DESC_B[UP_COUNT] = { "faster recharge", "(bolts, drone, all)", "+2 missiles now", "repairs 1 hull now",
                                    "range for coins/hearts", "with a shield", "+1 pulse bomb now" };
const float UP_COL[UP_COUNT][3] = {
    {0.30f, 0.85f, 1.00f}, {1.00f, 0.85f, 0.25f}, {1.00f, 0.50f, 0.15f}, {1.00f, 0.30f, 0.45f},
    {0.80f, 0.40f, 1.00f}, {0.40f, 1.00f, 0.70f}, {1.00f, 0.30f, 0.60f}
};

void setBanner(const char* text, const char* sub, float secs, float r, float g, float b) {
    snprintf(bannerText, sizeof(bannerText), "%s", text);
    snprintf(bannerSub, sizeof(bannerSub), "%s", sub);
    bannerTimer = secs;
    bannerR = r; bannerG = g; bannerB = b;
}

void spawnBurst(Vec3 center, int count, float r, float g, float b, float spdMin = 0.1f, float spdMax = 0.7f) {
    for (int i = 0; i < count; ++i) {
        if (particles.size() > 6000) return;
        float z = frand() * 2.0f - 1.0f;
        float a = frand() * 2.0f * PI;
        float s = sqrtf(1.0f - z * z);
        Vec3 dir = {s * cosf(a), s * sinf(a), z};
        Particle p;
        p.pos = center;
        p.vel = dir * rnd(spdMin, spdMax);
        p.life = 1.0f;
        p.decay = 0.012f + 0.012f * frand();
        p.r = r; p.g = g; p.b = b;
        particles.push_back(p);
    }
}

void spawnShock(Vec3 pos, float maxR, float r, float g, float b, float dur = 0.6f) {
    Shock sh;
    sh.pos = pos; sh.t = 0.0f; sh.dur = dur; sh.maxR = maxR;
    sh.r = r; sh.g = g; sh.b = b;
    shocks.push_back(sh);
}

void spawnTrail(float intensity) {
    if (particles.size() > 6000) return;
    Vec3 f = forwardVec();
    for (int side = -1; side <= 1; side += 2) {
        Particle p;
        p.pos = shipLocalToWorld({0.7f * side, -0.05f, 1.7f});
        p.vel = f * (-0.05f * intensity) + Vec3{(frand() - 0.5f) * 0.03f,
                                                (frand() - 0.5f) * 0.03f,
                                                (frand() - 0.5f) * 0.03f};
        p.life = 0.7f;
        p.decay = 0.03f;
        p.r = 0.2f; p.g = 0.65f + 0.2f * intensity; p.b = 1.0f;
        particles.push_back(p);
    }
}

void addPopup(Vec3 pos, const char* text, float r, float g, float b) {
    if (!cfgPopups) return;
    if (popups.size() > 40) popups.erase(popups.begin());
    Popup p;
    p.pos = pos;
    snprintf(p.text, sizeof(p.text), "%s", text);
    p.life = 1.2f;
    p.r = r; p.g = g; p.b = b;
    popups.push_back(p);
}

// ---- v1.3 helpers ----
Bolt makeBolt(Vec3 pos, Vec3 vel, float life, bool enemy) {
    Bolt b;
    b.pos = pos; b.vel = vel; b.life = life; b.enemy = enemy;
    b.graze = false;
    b.minD = 1e9f;
    return b;
}

Enemy makeEnemy(int type, Vec3 p, int L, bool counts) {
    Enemy e{};
    e.type = type;
    e.pos = p;
    e.anchor = p;
    e.vel = {0, 0, 0};
    e.fwd = {0, 0, 1};
    e.phase = frand() * 6.28f;
    e.flash = 0.0f;
    e.active = true;
    e.hunting = false;
    e.counts = counts;
    e.stage = 0;
    e.special = 2.0f;
    e.aux = 6.0f;
    switch (type) {
        case ET_KAMIKAZE:
            e.maxHp = (L >= 3) ? 2.0f : 1.0f; e.radius = 1.6f;
            e.fireInterval = 99.0f; e.maxSpeed = 0.50f + 0.03f * L; break;
        case ET_GUNSHIP:
            e.maxHp = 5.0f + L; e.radius = 3.4f;
            e.fireInterval = std::max(1.1f, 3.0f - 0.3f * L); e.maxSpeed = 0.18f + 0.02f * L; break;
        case ET_BOSS:
            e.maxHp = 45.0f; e.radius = 8.0f; e.fireInterval = 1.8f; e.maxSpeed = 0.22f; break;
        default:
            e.maxHp = (float)(2 + (L + 1) / 2); e.radius = 2.2f;
            e.fireInterval = std::max(0.7f, 2.4f - 0.3f * L); e.maxSpeed = 0.30f + 0.04f * L; break;
    }
    e.maxHp *= diffHp();
    e.hp = e.maxHp;
    e.fireTimer = rnd(0.5f, e.fireInterval);
    return e;
}

const char* enemyLabel(int t) {
    switch (t) {
        case ET_KAMIKAZE: return "KAMIKAZE";
        case ET_GUNSHIP:  return "GUNSHIP";
        case ET_BOSS:     return "GUARDIAN";
        default:          return "ENEMY";
    }
}

static float hash01(unsigned int v) {
    v *= 2654435761u; v ^= v >> 13; v *= 1274126177u; v ^= v >> 16;
    return (float)(v & 0xFFFF) / 65535.0f;
}

void upgradeCardRect(int i, int w, int h, float& x0, float& y0, float& x1, float& y1) {
    float cw = std::min(300.0f, ((float)w - 120.0f) / 3.0f);
    float gap = 24.0f;
    float total = cw * 3.0f + gap * 2.0f;
    float sx = ((float)w - total) * 0.5f;
    x0 = sx + i * (cw + gap);
    x1 = x0 + cw;
    y0 = h * 0.30f;
    y1 = y0 + 240.0f;
}

void generateUpgradeChoices() {
    int avail[UP_COUNT], n = 0;
    for (int i = 0; i < UP_COUNT; ++i) if (upg[i] < MAX_UPGRADE_LEVEL) avail[n++] = i;
    for (int i = n - 1; i > 0; --i) { int j = rand() % (i + 1); std::swap(avail[i], avail[j]); }
    for (int k = 0; k < 3; ++k) upChoice[k] = (n > 0) ? avail[k % n] : 0;
    upSelected = 1;
}

void applyUpgrade(int u) {
    if (u < 0 || u >= UP_COUNT || upg[u] >= MAX_UPGRADE_LEVEL) return;
    upg[u]++;
    switch (u) {
        case UP_RACK:    missileAmmo = std::min(maxMissilesVal(), missileAmmo + 2); break;
        case UP_HULL:    health = std::min(maxHealthVal(), health + 1); break;
        case UP_BOMBBAY: bombAmmo = std::min(maxBombsVal(), bombAmmo + 1); break;
        default: break;
    }
}

// ---- v1.2: combo-aware scoring ----
int comboMult() { return std::min(5, 1 + combo / 4); }

int addScore(int base, bool useCombo = true) {
    if (useCombo) {
        combo++;
        if (combo > comboBest) comboBest = combo;
        comboTimer = cfgComboWin;
    }
    int m = useCombo ? comboMult() : 1;
    if (doubleTimer > 0.0f) m *= 2;
    int gained = (int)(base * m * diffScore() + 0.5f);
    score += gained;
    return gained;
}

void spawnPowerUp(Vec3 pos, int type) {
    if (powerups.size() > 40) return;
    PowerUp p;
    p.pos = pos;
    p.type = type;
    p.taken = false;
    p.phase = frand() * 6.28f;
    powerups.push_back(p);
}

void applyPowerUp(int type, Vec3 pos) {
    float r = POW_COL[type][0], g = POW_COL[type][1], b = POW_COL[type][2];
    switch (type) {
        case PW_SHIELD: shieldTimer = SHIELD_TIME; break;
        case PW_RAPID:  rapidTimer  = POWERUP_TIME; break;
        case PW_MAGNET: magnetTimer = POWERUP_TIME + 2.0f; break;
        case PW_DOUBLE: doubleTimer = POWERUP_TIME; break;
        case PW_AMMO:   missileAmmo = std::min(maxMissilesVal(), missileAmmo + 3); break;
        case PW_DRONE:  droneTimer = DRONE_TIME; dronePos = ship; droneFireTimer = 0.3f; break;
        case PW_BOMB:   bombAmmo = std::min(maxBombsVal(), bombAmmo + 1); break;
    }
    spawnBurst(pos, 50, r, g, b, 0.08f, 0.5f);
    spawnShock(pos, 9.0f, r, g, b, 0.5f);
    char t[24];
    snprintf(t, sizeof(t), "%s!", POW_NAMES[type]);
    addPopup(pos + Vec3{0, 2, 0}, t, r, g, b);
    playSfx(SFX_POWERUP);
    triggerRumble(0.2f, 0.35f, 0.15f);
}

void updateParticles(float tk) {
    float damp = powf(0.985f, tk);
    for (auto& p : particles) {
        p.pos += p.vel * tk;
        p.vel = p.vel * damp;
        p.life -= p.decay * tk;
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
                       [](const Particle& p) { return p.life <= 0.0f; }),
        particles.end());
}

void updatePopups(float dt) {
    for (auto& p : popups) {
        p.life -= dt;
        p.pos.y += dt * 4.0f;
    }
    popups.erase(
        std::remove_if(popups.begin(), popups.end(),
                       [](const Popup& p) { return p.life <= 0.0f; }),
        popups.end());
}

// ----------------------------------------------------------------------------
//  Level construction
// ----------------------------------------------------------------------------
Vec3 pathPoint(float t) {
    int s = (int)floorf(t);
    if (s >= segs) s = segs - 1;
    if (s < 0) s = 0;
    float f = t - (float)s;
    return wps[s] * (1.0f - f) + wps[s + 1] * f;
}

bool freeSpot(Vec3 p, float rad) {
    if (length(p - Vec3{0, 0, 8}) < rad + 24.0f) return false;
    if (length(p - entry.pos) < rad + 18.0f) return false;
    if (length(p - bridge.pos) < rad + 20.0f) return false;
    for (const auto& c : coins)
        if (length(p - c.pos) < rad + 4.0f) return false;
    for (const auto& h : hearts)
        if (length(p - h.pos) < rad + 6.0f) return false;
    for (const auto& b : beacons)
        if (length(p - b.pos) < rad + 11.0f) return false;
    return true;
}

void updateObstacles(float dt) {
    for (auto& o : obstacles) {
        if (!o.active) continue;
        o.rot += o.rotSpeed * dt;
        if (o.type == OB_ASTEROID) {
            o.pos += o.vel * dt;
        } else if (o.type == OB_ORBITER) {
            o.theta += o.omega * dt;
            float c = cosf(o.theta) * o.orbitR;
            float s = sinf(o.theta) * o.orbitR;
            if (o.plane == 0)      o.pos = o.center + Vec3{c, 0, s};
            else if (o.plane == 1) o.pos = o.center + Vec3{c, s, 0};
            else                   o.pos = o.center + Vec3{0, c, s};
        }
    }
}

void buildLevel(int L) {
    srand(9000 + L * 7919);
    coins.clear(); hearts.clear(); beacons.clear(); obstacles.clear();
    planets.clear(); wps.clear(); particles.clear(); popups.clear();
    enemies.clear(); bolts.clear();
    powerups.clear(); missiles.clear();
    rings.clear(); clouds.clear(); summonQueue.clear();
    shocks.clear(); trailL.clear(); trailR.clear();
    lockedEnemy = nullptr;
    hasAimLock = false;

    const Theme& th = themes[L - 1];

    // ---- course waypoints ----
    segs = 3 + L;
    wps.push_back({0, 0, 0});
    for (int k = 1; k <= segs; ++k) {
        float x = rnd(-38.0f, 38.0f), y = rnd(-16.0f, 16.0f);
        if (k == segs) { x *= 0.5f; y *= 0.5f; }
        wps.push_back({x, y, -(float)k * SEG_LEN});
    }

    bridge = Mouth{wps[segs], {0, 0, 1}, "BRIDGE"};
    entry  = Mouth{{0, 0, 18}, {0, 0, -1}, "ENTRY"};

    // ---- beacons ----
    int interior = segs - 1;
    int B = 2 + (L + 1) / 2;
    if (B > interior) B = interior;
    for (int i = 0; i < B; ++i) {
        int idx = 1;
        if (B > 1) idx = 1 + (int)floorf(i * (interior - 1) / (float)(B - 1) + 0.5f);
        Beacon b;
        b.pos = wps[idx];
        b.active = false;
        b.anim = frand() * 6.28f;
        beacons.push_back(b);
    }

    // ---- coins ----
    for (int s = 0; s < segs; ++s) {
        Vec3 a = wps[s], b = wps[s + 1];
        Vec3 dir = normalize(b - a);
        Vec3 u = normalize(cross(dir, Vec3{0, 1, 0}));
        Vec3 v = cross(u, dir);
        int pattern = (s + L) % 3;
        const int N = 14;
        for (int i = 0; i < N; ++i) {
            float t = 0.08f + 0.84f * i / (float)(N - 1);
            Vec3 base = a + (b - a) * t;
            Vec3 off = {0, 0, 0};
            if (pattern == 0) {
                float ang = i * 0.8f;
                off = u * (cosf(ang) * 7.0f) + v * (sinf(ang) * 7.0f);
            } else if (pattern == 2) {
                off = u * (sinf(i * 0.7f) * 12.0f) + v * (cosf(i * 0.35f) * 4.0f);
            }
            Coin c;
            c.pos = base + off;
            c.taken = false;
            c.phase = frand() * 6.28f;
            coins.push_back(c);
        }
    }
    coinsReq = std::max(1, (int)(coins.size() * 0.7f));

    // ---- collectible hearts in the middle of the map ----
    int numHearts = 2 + (L >= 3 ? 1 : 0);
    for (int i = 0; i < numHearts; ++i) {
        float t = (0.28f + 0.44f * (float)i / std::max(1, numHearts - 1)) * segs;
        Vec3 base = pathPoint(t);
        Vec3 off = {rnd(-9.0f, 9.0f), rnd(-5.0f, 6.0f), 0.0f};
        HeartPickup hp;
        hp.pos = base + off;
        hp.taken = false;
        hp.phase = frand() * 6.28f;
        hearts.push_back(hp);
    }

    // ---- obstacles ----
    int nAst = 20 + 15 * L;
    int nMine = 2 + 3 * (L - 1);
    int nOrb = 2 + 3 * (L - 1);
    float total = (float)segs;

    for (int n = 0; n < nAst; ++n) {
        for (int tries = 0; tries < 60; ++tries) {
            Vec3 c = pathPoint(frand() * total);
            float ang = frand() * 2.0f * PI, rr = rnd(4.0f, 48.0f);
            Vec3 p = c + Vec3{cosf(ang) * rr, sinf(ang) * rr * 0.6f, rnd(-12.0f, 12.0f)};
            float rad = rnd(1.6f, 4.0f + 0.3f * L);
            if (!freeSpot(p, rad)) continue;
            Obstacle o{};
            o.type = OB_ASTEROID;
            o.pos = p;
            o.vel = {rnd(-0.8f, 0.8f), rnd(-0.8f, 0.8f), rnd(-0.8f, 0.8f)};
            o.radius = rad;
            o.scl = {rnd(0.8f, 1.25f), rnd(0.8f, 1.25f), rnd(0.8f, 1.25f)};
            o.axis = normalize(Vec3{rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)});
            o.rot = rnd(0, 360);
            o.rotSpeed = rnd(8.0f, 45.0f);
            float gr = rnd(0.35f, 0.6f);
            o.r = gr * (0.85f + 0.4f * th.nebA[0]);
            o.g = gr * (0.85f + 0.2f * th.nebA[1]);
            o.b = gr * (0.80f + 0.3f * th.nebA[2]);
            o.maxHp = 1.0f + rad * 0.9f;
            o.hp = o.maxHp;
            o.active = true;
            obstacles.push_back(o);
            break;
        }
    }

    for (int n = 0; n < nMine; ++n) {
        for (int tries = 0; tries < 60; ++tries) {
            Vec3 c = pathPoint(frand() * total);
            float ang = frand() * 2.0f * PI, rr = rnd(3.0f, 30.0f);
            Vec3 p = c + Vec3{cosf(ang) * rr, sinf(ang) * rr * 0.6f, rnd(-10.0f, 10.0f)};
            if (!freeSpot(p, 2.0f)) continue;
            Obstacle o{};
            o.type = OB_MINE;
            o.pos = p;
            o.radius = 1.8f;
            o.scl = {1, 1, 1};
            o.axis = {0, 1, 0};
            o.rot = rnd(0, 360);
            o.rotSpeed = rnd(15.0f, 40.0f);
            o.maxHp = o.hp = 1.0f;
            o.active = true;
            obstacles.push_back(o);
            break;
        }
    }

    for (int n = 0; n < nOrb; ++n) {
        for (int tries = 0; tries < 60; ++tries) {
            Vec3 c = pathPoint(frand() * total);
            float ang = frand() * 2.0f * PI, rr = rnd(5.0f, 35.0f);
            Vec3 center = c + Vec3{cosf(ang) * rr, sinf(ang) * rr * 0.6f, rnd(-10.0f, 10.0f)};
            float orR = rnd(6.0f, 13.0f);
            if (!freeSpot(center, orR + 1.8f)) continue;
            Obstacle o{};
            o.type = OB_ORBITER;
            o.radius = rnd(1.4f, 2.1f);
            o.scl = {1, 1, 1};
            o.axis = normalize(Vec3{rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)});
            o.rotSpeed = rnd(60.0f, 140.0f);
            o.center = center;
            o.orbitR = orR;
            o.theta = rnd(0.0f, 2.0f * PI);
            o.omega = rnd(0.6f, 1.3f) * ((rand() % 2) ? 1.0f : -1.0f);
            o.plane = rand() % 3;
            o.maxHp = o.hp = 5.0f;
            o.active = true;
            obstacles.push_back(o);
            break;
        }
    }
    updateObstacles(0.0f);

    // ---- enemy ships ----
    int nEnemies = (L == MAX_LEVEL) ? 4 : 2 + L;
    for (int i = 0; i < nEnemies; ++i) {
        Vec3 p = {0, 0, 0};
        for (int tries = 0; tries < 40; ++tries) {
            float t = 0.9f + (segs - 1.2f) * (i + 0.5f) / (float)nEnemies;
            Vec3 c = pathPoint(t);
            float ang = frand() * 2.0f * PI, rr = rnd(14.0f, 30.0f);
            p = c + Vec3{cosf(ang) * rr, sinf(ang) * rr * 0.5f, rnd(-8.0f, 8.0f)};
            if (freeSpot(p, 6.0f)) break;
        }
        int et = ET_FIGHTER;
        if (L >= 2 && i % 3 == 1) et = ET_KAMIKAZE;
        if (L >= 3 && i % 3 == 2) et = ET_GUNSHIP;
        enemies.push_back(makeEnemy(et, p, L, true));
    }

    // ---- v1.3: the Guardian boss guards the final bridge ----
    if (L == MAX_LEVEL) {
        Vec3 bp = bridge.pos + Vec3{0.0f, 4.0f, 70.0f};
        enemies.push_back(makeEnemy(ET_BOSS, bp, L, true));
    }

    // ---- power-ups (generated last so the rest of the layout is unchanged) ----
    {
        int nPow = 3 + L / 2;
        for (int i = 0; i < nPow; ++i) {
            Vec3 p = {0, 0, 0};
            for (int tries = 0; tries < 40; ++tries) {
                float t = (0.35f + 0.6f * (i + frand() * 0.8f) / (float)nPow) * segs;
                Vec3 c = pathPoint(std::min(t, (float)segs - 0.01f));
                p = c + Vec3{rnd(-14.0f, 14.0f), rnd(-8.0f, 8.0f), rnd(-6.0f, 6.0f)};
                if (freeSpot(p, 3.0f)) break;
            }
            spawnPowerUp(p, (i + L) % PW_COUNT);
        }
    }

    // ---- v1.3: fly-through rings & ion storms ----
    {
        ringsTotal = 5 + L;
        for (int i = 0; i < ringsTotal; ++i) {
            float t = 0.3f + ((float)segs - 0.7f) * (i + 0.5f) / (float)ringsTotal;
            Vec3 c = pathPoint(t);
            Vec3 dir = normalize(pathPoint(t + 0.08f) - pathPoint(t - 0.08f));
            Ring r;
            r.pos = c + Vec3{rnd(-3.0f, 3.0f), rnd(-2.0f, 2.0f), 0.0f};
            r.dir = dir;
            r.taken = false;
            r.phase = frand() * 6.28f;
            r.lastSide = 1e9f;
            rings.push_back(r);
        }
        int nCl = 1 + L / 2;
        for (int i = 0; i < nCl; ++i) {
            IonCloud ic;
            ic.radius = rnd(14.0f, 22.0f);
            ic.phase = frand() * 6.28f;
            Vec3 p = {0, 0, -100};
            for (int tries = 0; tries < 40; ++tries) {
                Vec3 c = pathPoint(frand() * (float)segs);
                float ang = frand() * 2.0f * PI, rr = rnd(12.0f, 34.0f);
                p = c + Vec3{cosf(ang) * rr, sinf(ang) * rr * 0.5f, rnd(-8.0f, 8.0f)};
                if (freeSpot(p, ic.radius * 0.5f)) break;
            }
            ic.pos = p;
            clouds.push_back(ic);
        }
    }

    // ---- scenery ----
    float totalLen = segs * SEG_LEN;
    float side = (L % 2) ? -1.0f : 1.0f;
    centerPos = {side * 170.0f, 50.0f, -0.5f * totalLen};
    centerRadius = (th.center == 1) ? 38.0f : (th.center == 2 ? 22.0f : 48.0f);
    if (th.center == 0) sunDirW = normalize(Vec3{0.45f, 0.55f, 0.55f});
    else                sunDirW = normalize(centerPos - Vec3{0.0f, 0.0f, -0.5f * totalLen});

    Planet m1 = {{-115.0f, -35.0f, -0.25f * totalLen}, 10.0f,
                 0.5f * th.nebA[0] + 0.3f, 0.5f * th.nebA[1] + 0.3f, 0.5f * th.nebA[2] + 0.3f, 0, 0.0f};
    Planet m2 = {{120.0f, 30.0f, -0.8f * totalLen}, 14.0f,
                 0.5f * th.nebB[0] + 0.3f, 0.5f * th.nebB[1] + 0.3f, 0.5f * th.nebB[2] + 0.3f, 0, 0.0f};
    m1.kind = 1; m1.seed = L * 2.3f;
    m2.kind = (L % 2) ? 2 : 0; m2.seed = L * 4.1f + 1.0f;
    planets.push_back(m1);
    planets.push_back(m2);

    // ---- bounds ----
    boundMin = {-85.0f, -50.0f, bridge.pos.z - 60.0f};
    boundMax = { 85.0f,  50.0f, 40.0f};

    // ---- per-level state ----
    coinsGot = 0;
    beaconsGot = 0;
    enemiesKilled = 0;
    enemiesReq = (int)enemies.size();
    bridgeOpen = false;
    openAmt = 0.0f;
    sealCooldown = 0.0f;
    levelTime = 0.0f;
    tookHit = false;
    comboBest = 0;
    ringsGot = 0;
    ringBonusGiven = false;
}

void beginLevel() {
    ship = entry.pos + entry.normal * 0.5f;
    yaw = atan2f(-entry.normal.x, -entry.normal.z);
    pitch = 0.0f;
    roll = 0.0f;
    rollVel = 0.0f;
    camYawOff = 0.0f;
    camPitchOff = 0.0f;
    speed = 0.0f;
    drift = {0, 0, 0};
    shipScale = 0.2f;
    shipVisible = true;
    invuln = 0.0f;
    state = S_EMERGING;
    stateTimer = 0.0f;
    flashAlpha = 1.0f;
    flashR = 0.6f; flashG = 0.8f; flashB = 1.0f;
    camSnap = true;

    // v1.2: refill energy, resupply a little ammo on later sectors
    energy = maxEnergyVal();
    energyLocked = false;
    energyDelay = 0.0f;
    missileRequest = false;
    missileCooldown = 0.0f;
    bombRequest = false;
    bombCooldown = 0.0f;
    dashTimer = 0.0f;
    inIon = false;
    if (level > 1 && !resuming) {
        missileAmmo = std::min(maxMissilesVal(), missileAmmo + 2);
        bombAmmo = std::min(maxBombsVal(), bombAmmo + 1);
    }
    if (upg[UP_SHIELDGEN] > 0) shieldTimer = std::max(shieldTimer, 6.0f + 6.0f * upg[UP_SHIELDGEN]);

    const Theme& th = curTheme();
    char t1[96], t2[128];
    snprintf(t1, sizeof(t1), "SECTOR %d: %s", level, th.name);
    snprintf(t2, sizeof(t2), "Collect %d coins - Activate %d beacons - Destroy %d enemy ships - Enter the bridge",
             coinsReq, (int)beacons.size(), enemiesReq);
    setBanner(t1, t2, 5.0f, th.accent[0], th.accent[1], th.accent[2]);
    spawnBurst(entry.pos, 120, 0.3f, 0.7f, 1.0f);
    spawnShock(entry.pos, 32.0f, th.accent[0], th.accent[1], th.accent[2], 1.0f);
    playSfx(SFX_WORMHOLE);
    saveCheckpoint();      // v1.7: progress is saved at the start of every sector
}

void startGame() {
    runDiff = cfgDifficulty;
    runTime = 0.0f;
    for (int i = 0; i < MAX_LEVEL; ++i) sectorTimes[i] = 0.0f;
    score = 0;
    health = diffStartHealth();
    level = 1;
    fireTimer = 0.0f;
    dispScore = 0.0f;

    // v1.2 state reset
    for (int i = 0; i < UP_COUNT; ++i) upg[i] = 0;
    missileAmmo = START_MISSILES;
    bombAmmo = 1;
    droneTimer = 0.0f;
    dashTimer = 0.0f;
    shieldTimer = rapidTimer = magnetTimer = doubleTimer = 0.0f;
    combo = 0;
    comboTimer = 0.0f;

    buildLevel(level);
    beginLevel();
}

// ============================================================================
//  v1.4 CINEMATIC RENDERER: GLSL shaders, HDR render targets, bloom, tone mapping
// ============================================================================
static PFNGLCREATESHADERPROC p_glCreateShader = nullptr;
static PFNGLSHADERSOURCEPROC p_glShaderSource = nullptr;
static PFNGLCOMPILESHADERPROC p_glCompileShader = nullptr;
static PFNGLGETSHADERIVPROC p_glGetShaderiv = nullptr;
static PFNGLGETSHADERINFOLOGPROC p_glGetShaderInfoLog = nullptr;
static PFNGLCREATEPROGRAMPROC p_glCreateProgram = nullptr;
static PFNGLATTACHSHADERPROC p_glAttachShader = nullptr;
static PFNGLLINKPROGRAMPROC p_glLinkProgram = nullptr;
static PFNGLGETPROGRAMIVPROC p_glGetProgramiv = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC p_glGetProgramInfoLog = nullptr;
static PFNGLUSEPROGRAMPROC p_glUseProgram = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC p_glGetUniformLocation = nullptr;
static PFNGLUNIFORM1FPROC p_glUniform1f = nullptr;
static PFNGLUNIFORM2FPROC p_glUniform2f = nullptr;
static PFNGLUNIFORM3FPROC p_glUniform3f = nullptr;
static PFNGLUNIFORM4FPROC p_glUniform4f = nullptr;
static PFNGLUNIFORM1IPROC p_glUniform1i = nullptr;
static PFNGLUNIFORMMATRIX3FVPROC p_glUniformMatrix3fv = nullptr;
static PFNGLACTIVETEXTUREPROC p_glActiveTexture = nullptr;
static PFNGLGENFRAMEBUFFERSPROC p_glGenFramebuffers = nullptr;
static PFNGLBINDFRAMEBUFFERPROC p_glBindFramebuffer = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC p_glFramebufferTexture2D = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC p_glCheckFramebufferStatus = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC p_glDeleteFramebuffers = nullptr;
static PFNGLGENRENDERBUFFERSPROC p_glGenRenderbuffers = nullptr;
static PFNGLBINDRENDERBUFFERPROC p_glBindRenderbuffer = nullptr;
static PFNGLRENDERBUFFERSTORAGEPROC p_glRenderbufferStorage = nullptr;
static PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC p_glRenderbufferStorageMultisample = nullptr;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC p_glFramebufferRenderbuffer = nullptr;
static PFNGLDELETERENDERBUFFERSPROC p_glDeleteRenderbuffers = nullptr;
static PFNGLBLITFRAMEBUFFERPROC p_glBlitFramebuffer = nullptr;
#define glCreateShader p_glCreateShader
#define glShaderSource p_glShaderSource
#define glCompileShader p_glCompileShader
#define glGetShaderiv p_glGetShaderiv
#define glGetShaderInfoLog p_glGetShaderInfoLog
#define glCreateProgram p_glCreateProgram
#define glAttachShader p_glAttachShader
#define glLinkProgram p_glLinkProgram
#define glGetProgramiv p_glGetProgramiv
#define glGetProgramInfoLog p_glGetProgramInfoLog
#define glUseProgram p_glUseProgram
#define glGetUniformLocation p_glGetUniformLocation
#define glUniform1f p_glUniform1f
#define glUniform2f p_glUniform2f
#define glUniform3f p_glUniform3f
#define glUniform4f p_glUniform4f
#define glUniform1i p_glUniform1i
#define glUniformMatrix3fv p_glUniformMatrix3fv
#define glActiveTexture p_glActiveTexture
#define glGenFramebuffers p_glGenFramebuffers
#define glBindFramebuffer p_glBindFramebuffer
#define glFramebufferTexture2D p_glFramebufferTexture2D
#define glCheckFramebufferStatus p_glCheckFramebufferStatus
#define glDeleteFramebuffers p_glDeleteFramebuffers
#define glGenRenderbuffers p_glGenRenderbuffers
#define glBindRenderbuffer p_glBindRenderbuffer
#define glRenderbufferStorage p_glRenderbufferStorage
#define glRenderbufferStorageMultisample p_glRenderbufferStorageMultisample
#define glFramebufferRenderbuffer p_glFramebufferRenderbuffer
#define glDeleteRenderbuffers p_glDeleteRenderbuffers
#define glBlitFramebuffer p_glBlitFramebuffer
static void gfxLoadFunctions() {
    p_glCreateShader = (PFNGLCREATESHADERPROC)glutGetProcAddress("glCreateShader");
    p_glShaderSource = (PFNGLSHADERSOURCEPROC)glutGetProcAddress("glShaderSource");
    p_glCompileShader = (PFNGLCOMPILESHADERPROC)glutGetProcAddress("glCompileShader");
    p_glGetShaderiv = (PFNGLGETSHADERIVPROC)glutGetProcAddress("glGetShaderiv");
    p_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)glutGetProcAddress("glGetShaderInfoLog");
    p_glCreateProgram = (PFNGLCREATEPROGRAMPROC)glutGetProcAddress("glCreateProgram");
    p_glAttachShader = (PFNGLATTACHSHADERPROC)glutGetProcAddress("glAttachShader");
    p_glLinkProgram = (PFNGLLINKPROGRAMPROC)glutGetProcAddress("glLinkProgram");
    p_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)glutGetProcAddress("glGetProgramiv");
    p_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)glutGetProcAddress("glGetProgramInfoLog");
    p_glUseProgram = (PFNGLUSEPROGRAMPROC)glutGetProcAddress("glUseProgram");
    p_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)glutGetProcAddress("glGetUniformLocation");
    p_glUniform1f = (PFNGLUNIFORM1FPROC)glutGetProcAddress("glUniform1f");
    p_glUniform2f = (PFNGLUNIFORM2FPROC)glutGetProcAddress("glUniform2f");
    p_glUniform3f = (PFNGLUNIFORM3FPROC)glutGetProcAddress("glUniform3f");
    p_glUniform4f = (PFNGLUNIFORM4FPROC)glutGetProcAddress("glUniform4f");
    p_glUniform1i = (PFNGLUNIFORM1IPROC)glutGetProcAddress("glUniform1i");
    p_glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)glutGetProcAddress("glUniformMatrix3fv");
    p_glActiveTexture = (PFNGLACTIVETEXTUREPROC)glutGetProcAddress("glActiveTexture");
    p_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)glutGetProcAddress("glGenFramebuffers");
    p_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)glutGetProcAddress("glBindFramebuffer");
    p_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glutGetProcAddress("glFramebufferTexture2D");
    p_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glutGetProcAddress("glCheckFramebufferStatus");
    p_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)glutGetProcAddress("glDeleteFramebuffers");
    p_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)glutGetProcAddress("glGenRenderbuffers");
    p_glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)glutGetProcAddress("glBindRenderbuffer");
    p_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)glutGetProcAddress("glRenderbufferStorage");
    p_glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)glutGetProcAddress("glRenderbufferStorageMultisample");
    p_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glutGetProcAddress("glFramebufferRenderbuffer");
    p_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)glutGetProcAddress("glDeleteRenderbuffers");
    p_glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)glutGetProcAddress("glBlitFramebuffer");
}

// ----------------------------------------------------------------------------
//  GLSL sources (GLSL 1.20, compatibility profile so the old geometry still works)
// ----------------------------------------------------------------------------
static const char* GLSL_NOISE = R"GLSL(
float hash13(vec3 p){ p=fract(p*0.1031); p+=dot(p,p.yzx+33.33); return fract((p.x+p.y)*p.z); }
float vnoise(vec3 p){
  vec3 i=floor(p); vec3 f=fract(p); f=f*f*(3.0-2.0*f);
  float a=hash13(i), b=hash13(i+vec3(1.0,0.0,0.0)), c=hash13(i+vec3(0.0,1.0,0.0)), d=hash13(i+vec3(1.0,1.0,0.0));
  float e=hash13(i+vec3(0.0,0.0,1.0)), g=hash13(i+vec3(1.0,0.0,1.0)), h=hash13(i+vec3(0.0,1.0,1.0)), k=hash13(i+vec3(1.0,1.0,1.0));
  return mix(mix(mix(a,b,f.x),mix(c,d,f.x),f.y), mix(mix(e,g,f.x),mix(h,k,f.x),f.y), f.z);
}
float fbm3(vec3 p){ float s=0.0; float a=0.5; for(int i=0;i<3;i++){ s+=a*vnoise(p); p=p*2.03+vec3(1.7,9.2,3.1); a*=0.5; } return s*1.14; }
float fbm(vec3 p){ float s=0.0; float a=0.5; for(int i=0;i<5;i++){ s+=a*vnoise(p); p=p*2.03+vec3(1.7,9.2,3.1); a*=0.5; } return s*1.03; }
)GLSL";

// ---- plain / glow / point ----
static const char* VS_GLOW = R"GLSL(
void main(){ gl_FrontColor=gl_Color; gl_Position=ftransform(); }
)GLSL";
static const char* FS_GLOW = R"GLSL(
uniform float uGain;
void main(){ gl_FragColor=vec4(gl_Color.rgb*uGain,gl_Color.a); }
)GLSL";
static const char* VS_POINT = R"GLSL(
void main(){ gl_FrontColor=gl_Color; gl_Position=ftransform(); }
)GLSL";
static const char* FS_POINT = R"GLSL(
uniform float uGain;
void main(){
  vec2 c=gl_PointCoord*2.0-1.0; float r2=dot(c,c);
  float a=exp(-r2*3.2)*(1.0-smoothstep(0.7,1.0,r2));
  gl_FragColor=vec4(gl_Color.rgb*uGain,gl_Color.a*a);
}
)GLSL";

// ---- sky box ----
static const char* VS_SKY = R"GLSL(
varying vec2 vUV;
void main(){ vUV=gl_Vertex.xy; gl_Position=vec4(gl_Vertex.xy,0.0,1.0); }
)GLSL";
static const char* FS_SKY = R"GLSL(
uniform mat3 uRot; uniform float uTan; uniform float uAspect; uniform float uTime; uniform samplerCube uNeb;
uniform vec3 uNebA; uniform vec3 uNebB; uniform vec3 uSunDir; uniform vec3 uSunCol; uniform float uSunPow;
varying vec2 vUV;
)GLSL";
static const char* FS_SKY_BODY = R"GLSL(
vec3 starLayer(vec3 d,float scale,float thr,float size,float bright){
  vec3 p=d*scale; vec3 c=floor(p); vec3 f=fract(p);
  float m=hash13(c+89.1);
  if(m<thr) return vec3(0.0);
  vec3 sp=vec3(hash13(c),hash13(c+17.3),hash13(c+41.7))*0.7+0.15;
  float dd=length(f-sp);
  float k=(m-thr)/(1.0-thr);
  float core=smoothstep(size,0.0,dd);
  float halo=exp(-dd*dd/(size*size*5.0))*0.35;
  float t=hash13(c+5.0);
  vec3 tint=(t<0.33)?vec3(0.65,0.78,1.0):((t<0.66)?vec3(1.0,0.97,0.92):vec3(1.0,0.78,0.55));
  return tint*(core+halo)*bright*(0.25+k*k*2.5);
}
void main(){
  vec3 dir=normalize(uRot*vec3(vUV.x*uAspect*uTan,vUV.y*uTan,-1.0));
  vec3 gn=normalize(vec3(0.25,0.9,0.35)); float lat=dot(dir,gn); float band=exp(-lat*lat*14.0);
  vec4 nb=textureCube(uNeb,dir); vec3 col=nb.rgb; float dens=nb.a;
  vec3 st=starLayer(dir,38.0,0.985,0.10,2.6)+starLayer(dir,90.0,0.986,0.12,1.9)+starLayer(dir,210.0,0.990,0.16,1.4)+starLayer(dir,480.0,0.993,0.22,1.0);
  col+=st*(1.0-dens*0.5)*(1.0+band*1.5);
  float sd=max(dot(dir,uSunDir),0.0);
  col+=uSunCol*(pow(sd,900.0)*8.0+pow(sd,60.0)*0.5*uSunPow+pow(sd,7.0)*0.10*uSunPow);
  gl_FragColor=vec4(col,1.0);
}
)GLSL";

static const char* FS_BAKE_HEAD = R"GLSL(
uniform float uFace; uniform vec3 uNebA; uniform vec3 uNebB;
)GLSL";
static const char* FS_BAKE_BODY = R"GLSL(
void main(){
  vec2 uv=gl_TexCoord[0].xy*2.0-1.0; float u=uv.x; float v=uv.y; vec3 d;
  if(uFace<0.5) d=vec3(1.0,-v,-u); else if(uFace<1.5) d=vec3(-1.0,-v,u); else if(uFace<2.5) d=vec3(u,1.0,v);
  else if(uFace<3.5) d=vec3(u,-1.0,-v); else if(uFace<4.5) d=vec3(u,-v,1.0); else d=vec3(-u,-v,-1.0);
  vec3 dir=normalize(d);
  vec3 q=dir*1.9;
  float w=fbm(q*1.3);
  float n=fbm(q+vec3(w*1.8,fbm(q*1.7+3.1)*1.4,w*1.2));
  float n2=fbm(q*2.6+vec3(7.0,2.0,5.0)+w);
  vec3 neb=mix(uNebA,uNebB,smoothstep(0.25,0.75,n2));
  float dens=smoothstep(0.42,0.9,n); dens=dens*dens;
  vec3 gn=normalize(vec3(0.25,0.9,0.35)); float lat=dot(dir,gn); float band=exp(-lat*lat*14.0);
  float dust=fbm(dir*5.0+9.0);
  float lane=smoothstep(0.52,0.78,dust);
  vec3 col=neb*dens*(0.28+band*0.9)*1.1;
  col+=vec3(1.0,0.9,0.78)*band*(0.025+0.05*fbm(dir*9.0))*(1.0-lane*0.8);
  col*=1.0-band*lane*0.55;
  gl_FragColor=vec4(col,dens);
}
)GLSL";

// ---- lit material (PBR-ish) ----
static const char* VS_MAT = R"GLSL(
varying vec3 vP; varying vec3 vN; varying vec3 vNO; varying vec3 vO; varying vec4 vC; varying float vFog;
void main(){
  vec4 ep=gl_ModelViewMatrix*gl_Vertex;
  vP=ep.xyz; vN=gl_NormalMatrix*gl_Normal; vNO=gl_Normal; vO=gl_Vertex.xyz; vC=gl_Color;
  vFog=length(ep.xyz);
  gl_Position=gl_ProjectionMatrix*ep;
}
)GLSL";
static const char* FS_MAT_HEAD = R"GLSL(
uniform float uMetal; uniform float uRough; uniform float uKind; uniform float uSeed; uniform float uDamage; uniform float uLit; uniform float uLod;
uniform mat3 uInvRot; uniform vec3 uSunW; uniform vec3 uEnvA; uniform vec3 uEnvB;
varying vec3 vP; varying vec3 vN; varying vec3 vNO; varying vec3 vO; varying vec4 vC; varying float vFog;
)GLSL";
static const char* FS_MAT_BODY = R"GLSL(
vec3 envCol(vec3 d,float rough){
  float h=d.y*0.5+0.5;
  vec3 sky=mix(uEnvB,uEnvA,h)*0.22;
  float s=max(dot(d,uSunW),0.0);
  float sharp=mix(512.0,6.0,rough);
  sky+=gl_LightSource[0].diffuse.rgb*(pow(s,sharp)*(sharp/80.0+0.6)*mix(2.5,0.15,rough)+pow(s,3.0)*0.15);
  return sky;
}
void main(){
  vec3 alb=vC.rgb;
  float fogf=clamp((gl_Fog.end-vFog)*gl_Fog.scale,0.0,1.0);
  if(uLit<0.5){ gl_FragColor=vec4(mix(gl_Fog.color.rgb,alb*1.7,fogf),vC.a); return; }
  vec3 N=normalize(vN); if(!gl_FrontFacing) N=-N;
  float rough=uRough; float metal=uMetal; vec3 emisCol=vec3(0.0);
  if(uKind>0.5 && uKind<2.5){
    vec3 p=vO*3.2+uSeed*13.7; float e=0.07;
    if(uLod>0.5){
    float h0=fbm3(p);
    vec3 g=vec3(fbm3(p+vec3(e,0.0,0.0))-h0,fbm3(p+vec3(0.0,e,0.0))-h0,fbm3(p+vec3(0.0,0.0,e))-h0)/e;
    vec3 nO=normalize(vNO); g-=nO*dot(g,nO);
    vec3 p2=vO*15.0+uSeed*7.1; float f0=vnoise(p2);
    vec3 g2=vec3(vnoise(p2+vec3(0.1,0.0,0.0))-f0,vnoise(p2+vec3(0.0,0.1,0.0))-f0,vnoise(p2+vec3(0.0,0.0,0.1))-f0)*10.0; g2-=nO*dot(g2,nO);
    vec3 nObj=normalize(nO-g*0.5-g2*0.16);
    N=normalize(gl_NormalMatrix*nObj); if(!gl_FrontFacing) N=-N;
    }
    float v=fbm3(p*0.7+3.0);
    alb*=0.5+0.8*v;
    alb*=mix(vec3(1.0),vec3(0.82,0.76,0.70),smoothstep(0.4,0.7,fbm3(vO*5.0+uSeed)));
    float cr=smoothstep(0.62,0.72,vnoise(vO*4.0+uSeed*3.0)); alb*=1.0-0.4*cr;
    rough=0.88; metal=0.0;
    if(uKind>1.5){
      float c=1.0-abs(fbm3(vO*3.0+uSeed)*2.0-1.0); c=pow(c,6.0);
      emisCol=vec3(1.0,0.36,0.08)*c*3.4+vec3(0.5,0.12,0.02)*pow(c,2.0)*2.0; alb*=0.32;
    }
    if(uDamage>0.01){
      float c=1.0-abs(fbm3(vO*4.5+uSeed*2.0)*2.0-1.0); c=pow(c,8.0);
      emisCol+=vec3(1.0,0.42,0.1)*c*uDamage*3.0;
    }
  } else if(uKind>2.5 && uKind<3.5){
    vec3 gr=abs(fract(vO*vec3(5.0,9.0,4.0))-0.5);
    float seam=smoothstep(0.455,0.5,max(gr.x,max(gr.y,gr.z)));
    alb*=1.0-0.42*seam;
    alb*=0.88+0.22*vnoise(vO*55.0);
    rough=clamp(rough+0.22*seam+0.18*(vnoise(vO*22.0)-0.5),0.06,1.0);
  }
  vec3 V=normalize(-vP); vec3 L=normalize(gl_LightSource[0].position.xyz);
  vec3 F0=mix(vec3(0.04),alb,metal);
  float NoL=max(dot(N,L),0.0); float NoV=max(dot(N,V),1e-3);
  vec3 H=normalize(L+V); float NoH=max(dot(N,H),0.0); float VoH=max(dot(V,H),0.0);
  float a=rough*rough; float a2=a*a; float dd=NoH*NoH*(a2-1.0)+1.0; float D=a2/(3.14159*dd*dd+1e-4);
  float k=(rough+1.0)*(rough+1.0)/8.0; float G=(NoL/(NoL*(1.0-k)+k))*(NoV/(NoV*(1.0-k)+k));
  vec3 F=F0+(1.0-F0)*pow(1.0-VoH,5.0);
  vec3 spec=D*G*F/(4.0*NoL*NoV+1e-3);
  vec3 diff=(1.0-F)*(1.0-metal)*alb/3.14159;
  vec3 col=(diff+spec)*gl_LightSource[0].diffuse.rgb*3.4*NoL;
  vec3 L1=normalize(gl_LightSource[1].position.xyz);
  col+=(1.0-metal)*alb*gl_LightSource[1].diffuse.rgb*max(dot(N,L1),0.0)*0.55;
  vec3 Rw=uInvRot*reflect(-V,N); vec3 Nw=uInvRot*N;
  vec3 amb=mix(uEnvB,uEnvA,Nw.y*0.5+0.5)*0.32+vec3(0.018);
  vec3 Fr=F0+(max(vec3(1.0-rough),F0)-F0)*pow(1.0-NoV,5.0);
  col+=amb*alb*(1.0-metal)*0.9+envCol(Rw,rough)*Fr*(0.5+0.5*metal);
  col+=emisCol;
  col+=pow(1.0-NoV,3.0)*uEnvA*0.10;
  gl_FragColor=vec4(mix(gl_Fog.color.rgb,col,fogf),vC.a);
}
)GLSL";

// ---- planets ----
static const char* VS_PLANET = R"GLSL(
varying vec3 vP; varying vec3 vN; varying vec3 vO;
void main(){ vec4 ep=gl_ModelViewMatrix*gl_Vertex; vP=ep.xyz; vN=gl_NormalMatrix*gl_Normal; vO=gl_Vertex.xyz; gl_Position=gl_ProjectionMatrix*ep; }
)GLSL";
static const char* FS_PLANET_HEAD = R"GLSL(
uniform float uKind; uniform float uSeed; uniform float uTime; uniform vec3 uCol1; uniform vec3 uCol2; uniform vec3 uAtmo;
varying vec3 vP; varying vec3 vN; varying vec3 vO;
)GLSL";
static const char* FS_PLANET_BODY = R"GLSL(
void main(){
  vec3 p=normalize(vO); vec3 c=vec3(0.5); vec3 night=vec3(0.0); float spec=0.0; float cloudAmt=0.0;
  if(uKind<0.5){
    float turb=fbm(vec3(p.x*2.0,p.y*7.0,p.z*2.0)+uSeed+vec3(uTime*0.004,0.0,0.0));
    float bands=sin((p.y+turb*0.32)*(13.0+5.0*fract(uSeed*0.37)))*0.5+0.5;
    float fine=fbm(vec3(p.x*6.0,p.y*42.0,p.z*6.0)+uSeed*3.0);
    c=mix(uCol1,uCol2,smoothstep(0.2,0.8,bands*0.7+fine*0.5));
    c*=0.7+0.55*fine;
    vec3 sp=normalize(vec3(0.6,-0.25,0.75)); float sd=length((p-sp)*vec3(1.0,2.4,1.0));
    float storm=smoothstep(0.30,0.1,sd); c=mix(c,uCol2*1.25+vec3(0.14,0.05,0.0),storm*0.85);
  } else if(uKind<1.5){
    float h=fbm(p*2.3+uSeed); float sea=0.52;
    vec3 ocean=mix(vec3(0.015,0.06,0.20),vec3(0.05,0.26,0.50),smoothstep(0.30,0.52,h));
    float ele=smoothstep(sea,0.78,h);
    vec3 land=mix(vec3(0.13,0.30,0.09),vec3(0.46,0.38,0.26),ele);
    land=mix(land,vec3(0.55,0.53,0.50),smoothstep(0.72,0.88,h));
    land*=0.8+0.4*fbm(p*14.0);
    float polar=smoothstep(0.76,0.90,abs(p.y)+0.10*(fbm(p*5.0)-0.5));
    float isLand=step(sea,h);
    c=mix(ocean,land,isLand); c=mix(c,vec3(0.92,0.96,1.0),polar);
    c=mix(c,c*uCol1*1.8,0.3);
    spec=(1.0-isLand)*(1.0-polar);
    cloudAmt=smoothstep(0.50,0.80,fbm(p*4.6+vec3(uTime*0.004,0.0,uSeed*2.0)));
    float cell=hash13(floor(p*150.0)); night=vec3(1.0,0.78,0.42)*step(0.9,cell)*isLand*(1.0-polar)*smoothstep(0.55,0.7,fbm(p*6.0));
  } else {
    float h=fbm(p*3.0+uSeed); float cr=smoothstep(0.55,0.66,vnoise(p*7.0+uSeed));
    c=mix(vec3(0.20,0.17,0.15),vec3(0.50,0.42,0.34),h)*uCol1*1.6;
    c*=1.0-0.4*cr; c*=0.75+0.5*fbm(p*20.0);
    float crack=1.0-abs(fbm(p*4.0+uSeed*2.0)*2.0-1.0); crack=pow(crack,7.0);
    night=vec3(1.0,0.35,0.08)*crack*2.5; c*=1.0-crack*0.6;
  }
  vec3 N=normalize(vN); vec3 V=normalize(-vP); vec3 L=normalize(gl_LightSource[0].position.xyz);
  float ndl=dot(N,L); float day=smoothstep(-0.07,0.32,ndl);
  vec3 sun=gl_LightSource[0].diffuse.rgb*2.7;
  vec3 col=c*(day*sun+0.012);
  vec3 H=normalize(L+V); col+=sun*spec*pow(max(dot(N,H),0.0),90.0)*0.7*day;
  col=mix(col,vec3(1.0)*(day*sun*0.9+0.01),cloudAmt*0.85);
  float cNight=1.0-day;
  col+=night*cNight*cNight*1.2;
  float fres=pow(1.0-max(dot(N,V),0.0),3.0);
  col+=uAtmo*fres*(0.10+day*1.3);
  gl_FragColor=vec4(col,1.0);
}
)GLSL";

static const char* FS_ATMO = R"GLSL(
uniform vec3 uCol; uniform float uStr;
varying vec3 vP; varying vec3 vN;
void main(){
  vec3 N=normalize(vN); vec3 V=normalize(-vP); vec3 L=normalize(gl_LightSource[0].position.xyz);
  float ndv=clamp(dot(N,V),0.0,1.0); float f=pow(1.0-ndv,3.0); float lit=smoothstep(-0.25,0.45,dot(N,L));
  float a=f*(0.12+lit)*uStr;
  gl_FragColor=vec4(uCol*a*1.8,1.0);
}
)GLSL";

static const char* FS_RING = R"GLSL(
uniform float uRin; uniform float uRout; uniform float uSeed; uniform vec3 uCol; uniform vec3 uPC; uniform float uPR;
varying vec3 vP; varying vec3 vN; varying vec3 vO;
)GLSL";
static const char* FS_RING_BODY = R"GLSL(
void main(){
  float r=length(vO.xy); float t=(r-uRin)/(uRout-uRin);
  float n1=vnoise(vec3(t*70.0,uSeed,0.5)); float n2=vnoise(vec3(t*190.0,uSeed*2.0,1.5));
  float d=0.25+0.55*n1+0.35*n2;
  d*=smoothstep(0.0,0.05,t)*smoothstep(1.0,0.92,t);
  d*=1.0-0.92*smoothstep(0.50,0.52,t)*smoothstep(0.60,0.58,t);
  d=clamp(d,0.0,1.0);
  vec3 L=normalize(gl_LightSource[0].position.xyz);
  float tt=dot(uPC-vP,L); float sh=1.0;
  if(tt>0.0){ float dd=length(vP+L*tt-uPC); sh=smoothstep(uPR*0.97,uPR*1.04,dd); }
  float fs=pow(max(dot(normalize(vP),L),0.0),2.0);
  vec3 col=uCol*(0.30+sh*(1.5+2.0*fs))*gl_LightSource[0].diffuse.rgb*(0.7+0.5*n2);
  gl_FragColor=vec4(col,d*0.88);
}
)GLSL";

// ---- sun ----
static const char* FS_SUN_HEAD = R"GLSL(
uniform float uTime; uniform float uSeed; uniform vec3 uCol;
varying vec3 vP; varying vec3 vN; varying vec3 vO;
)GLSL";
static const char* FS_SUN_BODY = R"GLSL(
void main(){
  vec3 p=normalize(vO);
  float g=fbm(p*6.0+vec3(uTime*0.05,0.0,uTime*0.03)); float g2=fbm(p*19.0-uTime*0.08);
  float spots=smoothstep(0.64,0.72,fbm(p*3.0+uSeed));
  vec3 N=normalize(vN); vec3 V=normalize(-vP); float mu=max(dot(N,V),0.0);
  float limb=pow(mu,0.45);
  vec3 base=mix(uCol*1.15,vec3(1.0,0.96,0.82),mu*mu);
  vec3 col=base*(0.78+0.62*g+0.25*g2)*(1.0-0.55*spots)*(0.5+0.75*limb)*2.5;
  gl_FragColor=vec4(col,1.0);
}
)GLSL";

// ---- accretion disk ----
static const char* FS_DISK_HEAD = R"GLSL(
uniform float uTime; uniform float uSeed; uniform float uRin; uniform float uRout;
varying vec3 vP; varying vec3 vN; varying vec3 vO;
)GLSL";
static const char* FS_DISK_BODY = R"GLSL(
void main(){
  float r=length(vO.xy); float t=(r-uRin)/(uRout-uRin); float ang=atan(vO.y,vO.x);
  float sw=fbm(vec3(r*0.35,ang*1.6+uTime*(3.0/(r*0.06+0.6)),uSeed));
  float sw2=fbm(vec3(r*1.4,ang*4.0-uTime*(5.0/(r*0.06+0.6)),uSeed+3.0));
  float temp=pow(clamp(1.0-t,0.0,1.0),2.0);
  vec3 hot=mix(vec3(1.0,0.32,0.05),vec3(1.0,0.92,0.65),temp)*(1.0+temp*4.0);
  vec3 tang=normalize(gl_NormalMatrix*vec3(-vO.y,vO.x,0.0)); vec3 V=normalize(-vP);
  float dop=dot(tang,V);
  float beam=pow(clamp(1.0+0.55*dop,0.25,2.0),3.0);
  hot=mix(hot,hot*vec3(0.75,0.9,1.3),clamp(dop,0.0,1.0)*0.5);
  float dens=smoothstep(0.0,0.06,t)*smoothstep(1.0,0.6,t)*(0.35+0.9*sw)*(0.7+0.5*sw2);
  gl_FragColor=vec4(hot*beam*dens*1.5,dens);
}
)GLSL";

// ---- post: bloom ----
static const char* VS_POST = R"GLSL(
void main(){ gl_TexCoord[0]=gl_MultiTexCoord0; gl_Position=vec4(gl_Vertex.xy,0.0,1.0); }
)GLSL";
static const char* FS_BRIGHT = R"GLSL(
uniform sampler2D uTex; uniform vec2 uTexel; uniform float uThr;
void main(){
  vec2 uv=gl_TexCoord[0].xy; vec3 c=vec3(0.0);
  c+=texture2D(uTex,uv+vec2(-0.5,-0.5)*uTexel).rgb; c+=texture2D(uTex,uv+vec2(0.5,-0.5)*uTexel).rgb;
  c+=texture2D(uTex,uv+vec2(-0.5,0.5)*uTexel).rgb;  c+=texture2D(uTex,uv+vec2(0.5,0.5)*uTexel).rgb;
  c*=0.25;
  float br=max(max(c.r,c.g),c.b);
  float soft=clamp(br-uThr+0.25,0.0,0.5); soft=soft*soft*1.0;
  float contrib=max(soft,br-uThr)/max(br,1e-4);
  gl_FragColor=vec4(c*contrib,1.0);
}
)GLSL";
static const char* FS_DOWN = R"GLSL(
uniform sampler2D uTex; uniform vec2 uTexel;
void main(){
  vec2 uv=gl_TexCoord[0].xy; vec2 t=uTexel;
  vec3 a=texture2D(uTex,uv+vec2(-2.0,2.0)*t).rgb, b=texture2D(uTex,uv+vec2(0.0,2.0)*t).rgb, c=texture2D(uTex,uv+vec2(2.0,2.0)*t).rgb;
  vec3 d=texture2D(uTex,uv+vec2(-2.0,0.0)*t).rgb, e=texture2D(uTex,uv).rgb, f=texture2D(uTex,uv+vec2(2.0,0.0)*t).rgb;
  vec3 g=texture2D(uTex,uv+vec2(-2.0,-2.0)*t).rgb, h=texture2D(uTex,uv+vec2(0.0,-2.0)*t).rgb, i=texture2D(uTex,uv+vec2(2.0,-2.0)*t).rgb;
  vec3 j=texture2D(uTex,uv+vec2(-1.0,1.0)*t).rgb, k=texture2D(uTex,uv+vec2(1.0,1.0)*t).rgb;
  vec3 l=texture2D(uTex,uv+vec2(-1.0,-1.0)*t).rgb, m=texture2D(uTex,uv+vec2(1.0,-1.0)*t).rgb;
  vec3 r=e*0.125+(a+c+g+i)*0.03125+(b+d+f+h)*0.0625+(j+k+l+m)*0.125;
  gl_FragColor=vec4(r,1.0);
}
)GLSL";
static const char* FS_UP = R"GLSL(
uniform sampler2D uTex; uniform vec2 uTexel; uniform float uWeight;
void main(){
  vec2 uv=gl_TexCoord[0].xy; vec2 t=uTexel;
  vec3 s=texture2D(uTex,uv).rgb*4.0;
  s+=(texture2D(uTex,uv+vec2(-1.0,0.0)*t).rgb+texture2D(uTex,uv+vec2(1.0,0.0)*t).rgb+texture2D(uTex,uv+vec2(0.0,-1.0)*t).rgb+texture2D(uTex,uv+vec2(0.0,1.0)*t).rgb)*2.0;
  s+=texture2D(uTex,uv+vec2(-1.0,-1.0)*t).rgb+texture2D(uTex,uv+vec2(1.0,-1.0)*t).rgb+texture2D(uTex,uv+vec2(-1.0,1.0)*t).rgb+texture2D(uTex,uv+vec2(1.0,1.0)*t).rgb;
  gl_FragColor=vec4(s/16.0*uWeight,1.0);
}
)GLSL";

// ---- post: composite (lensing, blur, CA, bloom, god rays, tone map, grain) ----
static const char* FS_COMP = R"GLSL(
uniform sampler2D uScene; uniform sampler2D uBloom;
uniform vec2 uRes; uniform float uTime; uniform float uExposure; uniform float uBloomStr; uniform float uVig;
uniform float uCA; uniform float uBlur; uniform vec2 uSun; uniform float uSunStr; uniform vec4 uBH; uniform float uAspect; uniform float uGrain;
float hsh(vec2 p){ return fract(sin(dot(p,vec2(12.9898,78.233)))*43758.5453); }
vec3 aces(vec3 x){ float a=2.51; float b=0.03; float c=2.43; float d=0.59; float e=0.14; return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0); }
void main(){
  vec2 uv=gl_TexCoord[0].xy; vec2 uv2=uv; float mask=1.0;
  if(uBH.w>0.0){
    vec2 asp=vec2(uAspect,1.0); vec2 d=(uv-uBH.xy)*asp; float r=length(d); float R=uBH.z;
    float RE=R*1.9; float rr=max(r,R*0.9);
    uv2=uv-(d/max(r,1e-5))/asp*(RE*RE/rr)*uBH.w;
    mask=smoothstep(R*0.97,R*1.03,r);
  }
  vec2 dir=uv2-vec2(0.5);
  float ca=uCA*(0.35+dot(dir,dir)*5.0);
  vec3 col;
  col.r=texture2D(uScene,uv2+dir*ca).r; col.g=texture2D(uScene,uv2).g; col.b=texture2D(uScene,uv2-dir*ca).b;
  if(uBlur>0.001){
    vec3 acc=col;
    for(int i=1;i<8;i++){ float s=float(i)/8.0; acc+=texture2D(uScene,uv2-dir*uBlur*s).rgb; }
    col=acc/8.0;
  }
  col+=texture2D(uBloom,uv2).rgb*uBloomStr;
  if(uSunStr>0.001){
    vec2 toSun=uSun-uv2; vec3 acc=vec3(0.0); float w=1.0;
    for(int i=0;i<28;i++){ float s=float(i)/28.0; acc+=texture2D(uBloom,uv2+toSun*s*0.9).rgb*w; w*=0.93; }
    col+=acc*uSunStr*0.032;
  }
  col*=mask;
  col*=uExposure;
  col=aces(col);
  float vg=1.0-uVig*pow(length((uv-0.5)*vec2(1.0,0.92))*1.38,2.4);
  col*=clamp(vg,0.0,1.0);
  col+=(hsh(uv*uRes+fract(uTime)*91.7)-0.5)*0.028*uGrain;
  gl_FragColor=vec4(col,1.0);
}
)GLSL";

// ----------------------------------------------------------------------------
//  Programs, uniforms, render targets
// ----------------------------------------------------------------------------
static GLuint progMat = 0, progGlow = 0, progPoint = 0, progSky = 0, progBake = 0, progPlanet = 0, progAtmo = 0,
              progRing = 0, progSun = 0, progDisk = 0, progBright = 0, progDown = 0, progUp = 0, progComp = 0;
static GLuint curProg = 0;
static float  viewM[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static float  invRot[9] = {1,0,0, 0,1,0, 0,0,1};
static float  glowGain = 1.7f;
static GLuint sphereList96 = 0, sphereList48 = 0, coinList = 0, heartBodyList = 0, heartRingList = 0;
static bool   noMSAA = false;
static bool   targetsDirty = false;

static struct { GLint metal, rough, kind, seed, damage, lit, invRot, sunW, envA, envB, lod; } MU;

static void useProg(GLuint p) {
    if (!gfxOK) return;
    if (p != curProg) { glUseProgram(p); curProg = p; }
}
struct ULEnt { GLuint p; const char* k; GLint l; };
static ULEnt ulCache[512];
static int   ulCount = 0;
static GLint ul(const char* n) {
    for (int i = 0; i < ulCount; ++i) if (ulCache[i].p == curProg && ulCache[i].k == n) return ulCache[i].l;
    GLint l = glGetUniformLocation(curProg, n);
    if (ulCount < 512) { ulCache[ulCount].p = curProg; ulCache[ulCount].k = n; ulCache[ulCount].l = l; ++ulCount; }
    return l;
}
static void u1(const char* n, float a)                       { glUniform1f(ul(n), a); }
static void u2(const char* n, float a, float b)              { glUniform2f(ul(n), a, b); }
static void u3(const char* n, float a, float b, float c)     { glUniform3f(ul(n), a, b, c); }
static void u4(const char* n, float a, float b, float c, float d) { glUniform4f(ul(n), a, b, c, d); }
static void ui(const char* n, int a)                         { glUniform1i(ul(n), a); }

// every glEnable/glDisable(GL_LIGHTING) in the game goes through here so the material shader knows
static void gfxLight(bool on) {
    if (on) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
    if (gfxOK && curProg == progMat) glUniform1f(MU.lit, on ? 1.0f : 0.0f);
}

static double nowSec() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
static void sleepMs(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}
static int   gfxFrame = 0;
static float billM[16];
static Vec3  camFwd = {0.0f, 0.0f, -1.0f};
static void captureView() {
    ++gfxFrame;
    glGetFloatv(GL_MODELVIEW_MATRIX, viewM);
    invRot[0] = viewM[0]; invRot[1] = viewM[4]; invRot[2] = viewM[8];
    invRot[3] = viewM[1]; invRot[4] = viewM[5]; invRot[5] = viewM[9];
    invRot[6] = viewM[2]; invRot[7] = viewM[6]; invRot[8] = viewM[10];
    for (int i = 0; i < 16; ++i) billM[i] = 0.0f;
    billM[0] = invRot[0]; billM[1] = invRot[1]; billM[2]  = invRot[2];
    billM[4] = invRot[3]; billM[5] = invRot[4]; billM[6]  = invRot[5];
    billM[8] = invRot[6]; billM[9] = invRot[7]; billM[10] = invRot[8];
    billM[15] = 1.0f;
    camFwd = Vec3{-viewM[2], -viewM[6], -viewM[10]};
}
// true when a sphere of radius r around p is entirely behind the camera plane
static inline bool behindCam(Vec3 p, float r) { return dot(camFwd, p - camPos) < -r; }

static void matBegin(float metal, float rough, int kind = 0, float seed = 0.0f, float damage = 0.0f, float lod = 1.0f) {
    if (!gfxOK) return;
    static int   stamp = -1;
    static float cm = -1.0f, cr = -1.0f, ck = -1.0f, cs = -1.0f, cd = -1.0f, cl = -1.0f;
    bool wasBound = (curProg == progMat);
    useProg(progMat);
    if (stamp != gfxFrame) {          // per-frame values: view rotation, sun, environment colours
        const Theme& th = curTheme();
        glUniformMatrix3fv(MU.invRot, 1, GL_FALSE, invRot);
        glUniform3f(MU.sunW, sunDirW.x, sunDirW.y, sunDirW.z);
        glUniform3f(MU.envA, th.nebA[0], th.nebA[1], th.nebA[2]);
        glUniform3f(MU.envB, th.nebB[0], th.nebB[1], th.nebB[2]);
        stamp = gfxFrame;
    }
    if (metal != cm)         { glUniform1f(MU.metal, metal);   cm = metal; }
    if (rough != cr)         { glUniform1f(MU.rough, rough);   cr = rough; }
    if ((float)kind != ck)   { glUniform1f(MU.kind, (float)kind); ck = (float)kind; }
    if (seed != cs)          { glUniform1f(MU.seed, seed);     cs = seed; }
    if (damage != cd)        { glUniform1f(MU.damage, damage); cd = damage; }
    if (lod != cl)           { glUniform1f(MU.lod, lod);       cl = lod; }
    (void)wasBound;
    glUniform1f(MU.lit, 1.0f);
}
static void matEnd() { if (gfxOK) useProg(0); }

static void gfxGlow(bool on) {
    if (!gfxOK) return;
    if (on) { useProg(progGlow); u1("uGain", glowGain); }
    else    useProg(0);
}
static void gfxPoints(bool on) {
    if (!gfxOK) return;
    if (on) { useProg(progPoint); u1("uGain", glowGain); glEnable(GL_POINT_SPRITE); }
    else    { glDisable(GL_POINT_SPRITE); useProg(progGlow); u1("uGain", glowGain); }
}

static GLuint compileSh(GLenum type, const std::string& src, const char* name) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096]; GLsizei n = 0;
        glGetShaderInfoLog(s, 4096, &n, log);
        fprintf(stderr, "[gfx] shader '%s' failed to compile:\n%s\n", name, log);
        return 0;
    }
    return s;
}
static GLuint makeProg(const std::string& vs, const std::string& fs, const char* name) {
    GLuint v = compileSh(GL_VERTEX_SHADER, vs, name);
    GLuint f = compileSh(GL_FRAGMENT_SHADER, fs, name);
    if (!v || !f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096]; GLsizei n = 0;
        glGetProgramInfoLog(p, 4096, &n, log);
        fprintf(stderr, "[gfx] program '%s' failed to link:\n%s\n", name, log);
        return 0;
    }
    return p;
}

// ---- render targets: HDR scene (MSAA + resolve) and a 5-level bloom chain ----
static GLuint sceneFbo = 0, sceneTex = 0, sceneDepth = 0, msFbo = 0, msColor = 0, msDepth = 0;
static bool   msOK = false;
static int    fbW = 0, fbH = 0;
static GLuint bloomTex[5] = {0,0,0,0,0}, bloomFbo[5] = {0,0,0,0,0};
static int    bloomW[5], bloomH[5];

static GLuint makeTex(int w, int h) {
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    return t;
}

static void freeTargets() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (sceneFbo)  glDeleteFramebuffers(1, &sceneFbo);
    if (msFbo)     glDeleteFramebuffers(1, &msFbo);
    if (sceneDepth) glDeleteRenderbuffers(1, &sceneDepth);
    if (msColor)   glDeleteRenderbuffers(1, &msColor);
    if (msDepth)   glDeleteRenderbuffers(1, &msDepth);
    if (sceneTex)  glDeleteTextures(1, &sceneTex);
    for (int i = 0; i < 5; ++i) {
        if (bloomFbo[i]) glDeleteFramebuffers(1, &bloomFbo[i]);
        if (bloomTex[i]) glDeleteTextures(1, &bloomTex[i]);
        bloomFbo[i] = bloomTex[i] = 0;
    }
    sceneFbo = msFbo = sceneDepth = msColor = msDepth = sceneTex = 0;
    msOK = false;
    fbW = fbH = 0;
}

static bool ensureTargets(int w, int h) {
    if (w == fbW && h == fbH && sceneFbo && !targetsDirty) return true;
    targetsDirty = false;
    freeTargets();
    sceneTex = makeTex(w, h);
    glGenRenderbuffers(1, &sceneDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glGenFramebuffers(1, &sceneFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sceneDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[gfx] HDR framebuffer incomplete - falling back to the classic renderer\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    // 4x MSAA scene buffer (optional)
    if (!noMSAA && p_glRenderbufferStorageMultisample && p_glBlitFramebuffer) {
        glGenRenderbuffers(1, &msColor);
        glBindRenderbuffer(GL_RENDERBUFFER, msColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA16F, w, h);
        glGenRenderbuffers(1, &msDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, msDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, w, h);
        glGenFramebuffers(1, &msFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth);
        msOK = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        if (!msOK) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &msFbo); glDeleteRenderbuffers(1, &msColor); glDeleteRenderbuffers(1, &msDepth);
            msFbo = msColor = msDepth = 0;
        }
    }
    // bloom chain
    int bw = std::max(1, w / 2), bh = std::max(1, h / 2);
    for (int i = 0; i < 5; ++i) {
        bloomW[i] = bw; bloomH[i] = bh;
        bloomTex[i] = makeTex(bw, bh);
        glGenFramebuffers(1, &bloomFbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTex[i], 0);
        bw = std::max(1, bw / 2); bh = std::max(1, bh / 2);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    fbW = w; fbH = h;
    return true;
}

// ---- projected position of the sector centrepiece (sun / planet / black hole) ----
static float cUV[2] = {0.5f, 0.5f};
static float cUVR = 0.0f;
static bool  cVis = false;
static void computeCenterScreen(int w, int h) {
    cVis = false;
    float x = centerPos.x, y = centerPos.y, z = centerPos.z;
    float ex = viewM[0] * x + viewM[4] * y + viewM[8]  * z + viewM[12];
    float ey = viewM[1] * x + viewM[5] * y + viewM[9]  * z + viewM[13];
    float ez = viewM[2] * x + viewM[6] * y + viewM[10] * z + viewM[14];
    if (ez > -1.0f) return;
    float th = tanf(currentFov * 0.5f * PI / 180.0f);
    float asp = (float)w / (float)std::max(1, h);
    float nx = ex / (-ez) / (th * asp), ny = ey / (-ez) / th;
    cUV[0] = nx * 0.5f + 0.5f;
    cUV[1] = ny * 0.5f + 0.5f;
    cUVR = (centerRadius / (-ez)) / th * 0.5f;
    cVis = (fabsf(nx) < 2.5f && fabsf(ny) < 2.5f);
}

static void eyePos(Vec3 w, float* o) {
    o[0] = viewM[0] * w.x + viewM[4] * w.y + viewM[8]  * w.z + viewM[12];
    o[1] = viewM[1] * w.x + viewM[5] * w.y + viewM[9]  * w.z + viewM[13];
    o[2] = viewM[2] * w.x + viewM[6] * w.y + viewM[10] * w.z + viewM[14];
}

static void fsQuad() {
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f( 1, -1);
    glTexCoord2f(1, 1); glVertex2f( 1,  1);
    glTexCoord2f(0, 1); glVertex2f(-1,  1);
    glEnd();
}

static void bindTexUnit(int unit, GLuint tex) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex);
}

// ---- bloom chain + composite (lensing, speed blur, CA, god rays, ACES, grain) ----
static void runPost(int w, int h, float tsec) {
    GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST), wasFog = glIsEnabled(GL_FOG), wasCull = glIsEnabled(GL_CULL_FACE),
              wasBlend = glIsEnabled(GL_BLEND), wasLight = glIsEnabled(GL_LIGHTING);
    gfxLight(false);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    // 1) bright pass + downsample
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[0]);
    glViewport(0, 0, bloomW[0], bloomH[0]);
    useProg(progBright);
    bindTexUnit(0, sceneTex); ui("uTex", 0);
    u2("uTexel", 1.0f / fbW, 1.0f / fbH);
    u1("uThr", 0.85f);
    fsQuad();
    // 2) downsample chain
    for (int i = 1; i < 5; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[i]);
        glViewport(0, 0, bloomW[i], bloomH[i]);
        useProg(progDown);
        bindTexUnit(0, bloomTex[i - 1]); ui("uTex", 0);
        u2("uTexel", 1.0f / bloomW[i - 1], 1.0f / bloomH[i - 1]);
        fsQuad();
    }
    // 3) upsample additively
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = 3; i >= 0; --i) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[i]);
        glViewport(0, 0, bloomW[i], bloomH[i]);
        useProg(progUp);
        bindTexUnit(0, bloomTex[i + 1]); ui("uTex", 0);
        u2("uTexel", 1.0f / bloomW[i + 1], 1.0f / bloomH[i + 1]);
        u1("uWeight", 0.85f);
        fsQuad();
    }
    glDisable(GL_BLEND);

    // 4) composite to the window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    computeCenterScreen(w, h);
    const Theme& th = curTheme();
    float asp = (float)w / (float)std::max(1, h);
    bool flying = (state == S_FLYING);
    float blur = 0.0f;
    if (flying) blur = (boosting ? 0.050f : 0.012f) * clampf(fabsf(speed) / 0.6f, 0.0f, 1.6f);
    float ca = 0.0016f + 0.0035f * camShake + 0.0030f * hitFlash + (boosting && flying ? 0.0018f : 0.0f);
    float sunStr = 0.0f;
    if (th.center == 1 && cVis) {
        float dx = cUV[0] - 0.5f, dy = cUV[1] - 0.5f;
        sunStr = clampf(1.35f - sqrtf(dx * dx + dy * dy) * 1.2f, 0.0f, 1.0f);
    }
    float lens = (th.center == 2 && cVis && (state == S_FLYING || state == S_EMERGING)) ? 1.0f : 0.0f;
    useProg(progComp);
    bindTexUnit(0, sceneTex); ui("uScene", 0);
    bindTexUnit(1, bloomTex[0]); ui("uBloom", 1);
    u2("uRes", (float)w, (float)h);
    u1("uTime", tsec);
    u1("uExposure", cfgExposure);
    u1("uBloomStr", bloomOn ? bloomStrength * 0.75f : 0.0f);
    u1("uVig", cfgVignette);
    u1("uCA", ca * cfgCA);
    u1("uBlur", blur * cfgBlur);
    u2("uSun", cUV[0], cUV[1]);
    u1("uSunStr", cfgGodRays ? sunStr : 0.0f);
    u4("uBH", cUV[0], cUV[1], cUVR, lens);
    u1("uAspect", asp);
    u1("uGrain", cfgGrain);
    fsQuad();
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
    useProg(0);
    if (wasDepth) glEnable(GL_DEPTH_TEST);
    if (wasFog)   glEnable(GL_FOG);
    if (wasCull)  glEnable(GL_CULL_FACE);
    if (wasBlend) glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (wasLight) gfxLight(true);
}

// ---- the nebula/galaxy/dust is static per sector: bake it once into a cubemap ----
static GLuint nebCube = 0, bakeFbo = 0;
static int    nebBakedFor = -1;
static void ensureSkyBaked(int w, int h) {
    if (!gfxOK || nebBakedFor == level) return;
    const int N = 768;
    if (!nebCube) {
        glGenTextures(1, &nebCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, nebCube);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        for (int f = 0; f < 6; ++f)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGBA16F, N, N, 0, GL_RGBA, GL_FLOAT, nullptr);
        glGenFramebuffers(1, &bakeFbo);
    }
    GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST), wasBlend = glIsEnabled(GL_BLEND), wasLight = glIsEnabled(GL_LIGHTING);
    gfxLight(false);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    const Theme& th = curTheme();
    useProg(progBake);
    u3("uNebA", th.nebA[0], th.nebA[1], th.nebA[2]);
    u3("uNebB", th.nebB[0], th.nebB[1], th.nebB[2]);
    for (int f = 0; f < 6; ++f) {
        glBindFramebuffer(GL_FRAMEBUFFER, bakeFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, nebCube, 0);
        glViewport(0, 0, N, N);
        u1("uFace", (float)f);
        fsQuad();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    useProg(0);
    if (wasDepth) glEnable(GL_DEPTH_TEST);
    if (wasBlend) glEnable(GL_BLEND);
    if (wasLight) gfxLight(true);
    nebBakedFor = level;
}

// ---- procedural sky box (baked nebula + live stars + sun glare) ----
static void drawSkyShader(float tsec) {
    const Theme& th = curTheme();
    int w = curWinW, h = std::max(1, curWinH);
    useProg(progSky);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, nebCube);
    ui("uNeb", 0);
    glUniformMatrix3fv(ul("uRot"), 1, GL_FALSE, invRot);
    u1("uTan", tanf(currentFov * 0.5f * PI / 180.0f));
    u1("uAspect", (float)w / (float)h);
    u1("uTime", tsec);
    u3("uNebA", th.nebA[0], th.nebA[1], th.nebA[2]);
    u3("uNebB", th.nebB[0], th.nebB[1], th.nebB[2]);
    u3("uSunDir", sunDirW.x, sunDirW.y, sunDirW.z);
    u3("uSunCol", th.light[0], th.light[1], th.light[2]);
    u1("uSunPow", th.center == 1 ? 1.0f : (th.center == 2 ? 0.45f : 0.30f));
    gfxLight(false);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBegin(GL_QUADS);
    glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(1, 1); glVertex2f(-1, 1);
    glEnd();
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    useProg(0);
    gfxLight(true);
}

// ---- planet / ring / atmosphere helpers ----
static void planetUniforms(float kind, const float* c1, const float* c2, float seed, float tsec, const float* atmo) {
    useProg(progPlanet);
    u1("uKind", kind);
    u1("uSeed", seed);
    u1("uTime", tsec);
    u3("uCol1", c1[0], c1[1], c1[2]);
    u3("uCol2", c2[0], c2[1], c2[2]);
    u3("uAtmo", atmo[0], atmo[1], atmo[2]);
}
static void drawAtmoShell(Vec3 pos, float R, const float* col, float str) {
    useProg(progAtmo);
    u3("uCol", col[0], col[1], col[2]);
    u1("uStr", str);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    glScalef(R * 1.07f, R * 1.07f, R * 1.07f);
    glCallList(sphereList48);
    glPopMatrix();
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    useProg(0);
}

// ---- detailed procedural rock meshes for asteroids (display lists) ----
static GLuint rockList[3] = {0, 0, 0};     // 1280 triangles (near)
static GLuint rockListLo[3] = {0, 0, 0};   // 320 triangles (far)

static float h3i(int x, int y, int z) {
    unsigned int n = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u + (unsigned int)z * 1274126177u;   // unsigned: signed overflow is UB
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= n >> 16;
    return (float)(n & 0xFFFF) / 65535.0f;
}
static float vn3c(float x, float y, float z) {
    int xi = (int)floorf(x), yi = (int)floorf(y), zi = (int)floorf(z);
    float fx = x - xi, fy = y - yi, fz = z - zi;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
    float a = lerpf(h3i(xi, yi, zi), h3i(xi + 1, yi, zi), fx);
    float b = lerpf(h3i(xi, yi + 1, zi), h3i(xi + 1, yi + 1, zi), fx);
    float c = lerpf(h3i(xi, yi, zi + 1), h3i(xi + 1, yi, zi + 1), fx);
    float d = lerpf(h3i(xi, yi + 1, zi + 1), h3i(xi + 1, yi + 1, zi + 1), fx);
    return lerpf(lerpf(a, b, fy), lerpf(c, d, fy), fz);
}
static float fbmc(float x, float y, float z) {
    float s = 0.0f, a = 0.5f;
    for (int i = 0; i < 4; ++i) { s += a * vn3c(x, y, z); x = x * 2.03f + 1.7f; y = y * 2.03f + 9.2f; z = z * 2.03f + 3.1f; a *= 0.5f; }
    return s * 1.07f;
}

static void buildRock(int variant, int subdiv, GLuint& outList) {
    const float t = (1.0f + sqrtf(5.0f)) / 2.0f;
    std::vector<Vec3> v = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
                           {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    for (auto& p : v) p = normalize(p);
    std::vector<int> f = {0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11, 1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
                          3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9, 4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1};
    for (int s = 0; s < subdiv; ++s) {
        std::map<unsigned long long, int> cache;
        std::vector<int> nf;
        auto mid = [&](int a, int b) -> int {
            unsigned long long k = ((unsigned long long)std::min(a, b) << 32) | (unsigned long long)std::max(a, b);
            auto it = cache.find(k);
            if (it != cache.end()) return it->second;
            Vec3 m = normalize((v[a] + v[b]) * 0.5f);
            v.push_back(m);
            int idx = (int)v.size() - 1;
            cache[k] = idx;
            return idx;
        };
        for (size_t i = 0; i < f.size(); i += 3) {
            int a = f[i], b = f[i + 1], c = f[i + 2];
            int ab = mid(a, b), bc = mid(b, c), ca = mid(c, a);
            int tri[12] = {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca};
            for (int k = 0; k < 12; ++k) nf.push_back(tri[k]);
        }
        f = nf;
    }
    static const float EL[3][3] = {{1.00f, 0.82f, 1.15f}, {1.12f, 1.00f, 0.80f}, {0.88f, 1.12f, 1.00f}};
    struct Crater { Vec3 c; float r, d; };
    std::vector<Crater> craters;
    for (int k = 0; k < 11; ++k) {
        float a = h3i(k, variant, 11) * 2.0f * PI, z = h3i(k, variant, 23) * 2.0f - 1.0f, s2 = sqrtf(1.0f - z * z);
        Crater cr;
        cr.c = {s2 * cosf(a), s2 * sinf(a), z};
        cr.r = 0.22f + 0.30f * h3i(k, variant, 37);
        cr.d = 0.05f + 0.09f * h3i(k, variant, 51);
        craters.push_back(cr);
    }
    std::vector<Vec3> pos(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        Vec3 n = v[i];
        float d = 1.0f + 0.62f * (fbmc(n.x * 1.7f + variant * 11.3f, n.y * 1.7f, n.z * 1.7f) - 0.5f);
        d += 0.08f * (fbmc(n.x * 6.0f + variant * 5.1f, n.y * 6.0f, n.z * 6.0f) - 0.5f);
        for (const auto& cr : craters) {
            float dd = acosf(clampf(dot(n, cr.c), -1.0f, 1.0f));
            if (dd < cr.r * 1.25f) {
                float tt = dd / cr.r;
                if (tt < 1.0f) d -= cr.d * (1.0f - tt * tt);
                d += cr.d * 0.35f * expf(-powf((tt - 1.0f) * 5.0f, 2.0f));
            }
        }
        pos[i] = {n.x * d * EL[variant][0], n.y * d * EL[variant][1], n.z * d * EL[variant][2]};
    }
    std::vector<Vec3> nrm(v.size(), Vec3{0, 0, 0});
    for (size_t i = 0; i < f.size(); i += 3) {
        Vec3 fn = cross(pos[f[i + 1]] - pos[f[i]], pos[f[i + 2]] - pos[f[i]]);
        for (int k = 0; k < 3; ++k) nrm[f[i + k]] += fn;
    }
    for (auto& n : nrm) n = normalize(n);
    outList = glGenLists(1);
    glNewList(outList, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < f.size(); ++i) {
        glNormal3f(nrm[f[i]].x, nrm[f[i]].y, nrm[f[i]].z);
        glVertex3f(pos[f[i]].x, pos[f[i]].y, pos[f[i]].z);
    }
    glEnd();
    glEndList();
}

// ---- display lists for things freeglut would otherwise rebuild on every single call ----
static void buildLists() {
    sphereList96 = glGenLists(1);
    glNewList(sphereList96, GL_COMPILE);
    glutSolidSphere(1.0, 96, 64);
    glEndList();
    sphereList48 = glGenLists(1);
    glNewList(sphereList48, GL_COMPILE);
    glutSolidSphere(1.0, 48, 32);
    glEndList();

    coinList = glGenLists(1);
    glNewList(coinList, GL_COMPILE);
    glColor3f(1.0f, 0.80f, 0.12f);
    glPushMatrix();
    glTranslatef(0, 0, -0.09f);
    glutSolidCylinder(0.85, 0.18, 24, 1);
    glPopMatrix();
    glColor3f(1.0f, 0.95f, 0.55f);
    glutSolidTorus(0.07, 0.62, 8, 24);
    glEndList();

    heartBodyList = glGenLists(1);
    glNewList(heartBodyList, GL_COMPILE);
    glColor3f(1.0f, 0.16f, 0.32f);
    glPushMatrix(); glTranslatef(-0.38f, 0.28f, 0.0f); glutSolidSphere(0.48, 18, 14); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.38f, 0.28f, 0.0f); glutSolidSphere(0.48, 18, 14); glPopMatrix();
    glPushMatrix();
    glTranslatef(0.0f, 0.14f, 0.0f);
    glRotatef(90.0f, 1, 0, 0);
    glScalef(1.0f, 0.58f, 1.0f);
    glutSolidCone(0.76, 1.05, 20, 8);
    glPopMatrix();
    glEndList();

    heartRingList = glGenLists(1);
    glNewList(heartRingList, GL_COMPILE);
    glColor3f(1.0f, 0.65f, 0.8f);
    glutWireTorus(0.05, 1.25, 8, 24);
    glEndList();
}

// ---- initialisation: load GL entry points, build every program, bake meshes ----
static void gfxInit() {
    gfxOK = false;
    gfxLoadFunctions();
    if (!p_glCreateShader || !p_glUseProgram || !p_glGenFramebuffers || !p_glActiveTexture || !p_glFramebufferTexture2D ||
        !p_glRenderbufferStorage || !p_glUniformMatrix3fv || !p_glGetUniformLocation) {
        fprintf(stderr, "[gfx] OpenGL 2.1+ with framebuffer objects not available - using the classic renderer\n");
        return;
    }
    const std::string H = "#version 120\n";
    std::string N = GLSL_NOISE;
    progGlow   = makeProg(H + VS_GLOW,  H + FS_GLOW, "glow");
    progPoint  = makeProg(H + VS_POINT, H + FS_POINT, "point");
    progSky    = makeProg(H + VS_SKY,   H + FS_SKY + N + FS_SKY_BODY, "sky");
    progBake   = makeProg(H + VS_POST, H + FS_BAKE_HEAD + N + FS_BAKE_BODY, "skybake");
    progMat    = makeProg(H + VS_MAT,   H + FS_MAT_HEAD + N + FS_MAT_BODY, "material");
    progPlanet = makeProg(H + VS_PLANET, H + FS_PLANET_HEAD + N + FS_PLANET_BODY, "planet");
    progAtmo   = makeProg(H + VS_PLANET, H + FS_ATMO, "atmosphere");
    progRing   = makeProg(H + VS_PLANET, H + FS_RING + N + FS_RING_BODY, "rings");
    progSun    = makeProg(H + VS_PLANET, H + FS_SUN_HEAD + N + FS_SUN_BODY, "sun");
    progDisk   = makeProg(H + VS_PLANET, H + FS_DISK_HEAD + N + FS_DISK_BODY, "disk");
    progBright = makeProg(H + VS_POST,  H + FS_BRIGHT, "bright");
    progDown   = makeProg(H + VS_POST,  H + FS_DOWN, "down");
    progUp     = makeProg(H + VS_POST,  H + FS_UP, "up");
    progComp   = makeProg(H + VS_POST,  H + FS_COMP, "composite");
    if (!progGlow || !progPoint || !progSky || !progBake || !progMat || !progPlanet || !progAtmo || !progRing || !progSun ||
        !progDisk || !progBright || !progDown || !progUp || !progComp) {
        fprintf(stderr, "[gfx] shader setup failed - using the classic renderer\n");
        return;
    }
    MU.metal  = glGetUniformLocation(progMat, "uMetal");
    MU.rough  = glGetUniformLocation(progMat, "uRough");
    MU.kind   = glGetUniformLocation(progMat, "uKind");
    MU.seed   = glGetUniformLocation(progMat, "uSeed");
    MU.damage = glGetUniformLocation(progMat, "uDamage");
    MU.lit    = glGetUniformLocation(progMat, "uLit");
    MU.invRot = glGetUniformLocation(progMat, "uInvRot");
    MU.sunW   = glGetUniformLocation(progMat, "uSunW");
    MU.envA   = glGetUniformLocation(progMat, "uEnvA");
    MU.envB   = glGetUniformLocation(progMat, "uEnvB");
    MU.lod    = glGetUniformLocation(progMat, "uLod");
    for (int i = 0; i < 3; ++i) { buildRock(i, 3, rockList[i]); buildRock(i, 2, rockListLo[i]); }
    gfxOK = true;
    fprintf(stderr, "[gfx] HDR shader renderer online (%s)\n", (const char*)glGetString(GL_RENDERER));
}

// ----------------------------------------------------------------------------
//  GL helpers
// ----------------------------------------------------------------------------
void setProjection() {
    int w = curWinW;
    int h = std::max(1, curWinH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(currentFov, (double)w / (double)h, 0.1, 1400.0);
    glMatrixMode(GL_MODELVIEW);
}

void setLight() {
    const Theme& th = curTheme();
    GLfloat pos0[] = {sunDirW.x, sunDirW.y, sunDirW.z, 0.0f};
    GLfloat dif0[] = {th.light[0], th.light[1], th.light[2], 1.0f};
    GLfloat spc0[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, pos0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif0);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spc0);

    GLfloat pos1[] = {-0.6f, -0.3f, -0.5f, 0.0f};
    GLfloat dif1[] = {th.accent[0] * 0.4f, th.accent[1] * 0.4f, th.accent[2] * 0.45f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, pos1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, dif1);
}

void beginAdditive() {
    gfxLight(false);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    gfxGlow(true);
}

void endAdditive() {
    gfxGlow(false);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    gfxLight(true);
}

void beginBillboard(Vec3 p) {
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glMultMatrixf(billM);   // cancels the view rotation: local axes == screen axes (no glGetFloatv stall)
}

void endBillboard() { glPopMatrix(); }

void glowDisc(Vec3 p, float rad, float r, float g, float b, float a) {
    static float cs[29], sn[29];
    static bool init = false;
    if (!init) {
        for (int i = 0; i <= 28; ++i) { float ang = 2.0f * PI * i / 28.0f; cs[i] = cosf(ang); sn[i] = sinf(ang); }
        init = true;
    }
    Vec3 rt = {viewM[0] * rad, viewM[4] * rad, viewM[8] * rad};
    Vec3 up = {viewM[1] * rad, viewM[5] * rad, viewM[9] * rad};
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r, g, b, a);
    glVertex3f(p.x, p.y, p.z);
    glColor4f(r, g, b, 0.0f);
    for (int i = 0; i <= 28; ++i)
        glVertex3f(p.x + rt.x * cs[i] + up.x * sn[i], p.y + rt.y * cs[i] + up.y * sn[i], p.z + rt.z * cs[i] + up.z * sn[i]);
    glEnd();
}

void drawBeam(Vec3 b, float h, float w, float r, float g, float bl, float a) {
    glBegin(GL_QUADS);
    glColor4f(r, g, bl, a);
    glVertex3f(b.x - w, b.y, b.z); glVertex3f(b.x + w, b.y, b.z);
    glColor4f(r, g, bl, 0.0f);
    glVertex3f(b.x + w, b.y + h, b.z); glVertex3f(b.x - w, b.y + h, b.z);

    glColor4f(r, g, bl, a);
    glVertex3f(b.x, b.y, b.z - w); glVertex3f(b.x, b.y, b.z + w);
    glColor4f(r, g, bl, 0.0f);
    glVertex3f(b.x, b.y + h, b.z + w); glVertex3f(b.x, b.y + h, b.z - w);
    glEnd();
}

void alignZ(Vec3 dir) {
    Vec3 z = {0, 0, 1};
    float d = clampf(dot(z, dir), -1.0f, 1.0f);
    if (d > 0.9999f) return;
    if (d < -0.9999f) { glRotatef(180.0f, 1, 0, 0); return; }
    Vec3 ax = normalize(cross(z, dir));
    glRotatef(acosf(d) * 180.0f / PI, ax.x, ax.y, ax.z);
}

// ----------------------------------------------------------------------------
//  Scene drawing
// ----------------------------------------------------------------------------
static void setMat(float spec, float shin) {
    GLfloat sp[] = {spec, spec, spec, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, sp);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shin);
}

void drawBandedSphere(float R, float cr, float cg, float cb, float bandFreq) {
    const int ST = 40, SL = 64;
    for (int i = 0; i < ST; ++i) {
        float la0 = PI * (-0.5f + (float)i / ST);
        float la1 = PI * (-0.5f + (float)(i + 1) / ST);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= SL; ++j) {
            float lo = 2.0f * PI * j / SL;
            for (int k = 0; k < 2; ++k) {
                float la = k ? la1 : la0;
                float x = cosf(la) * cosf(lo), y = sinf(la), z = cosf(la) * sinf(lo);
                float band = 0.78f + 0.22f * sinf(la * bandFreq)
                           + 0.08f * sinf(la * bandFreq * 2.7f + 1.3f + 0.3f * sinf(lo * 3.0f));
                glColor3f(cr * band, cg * band, cb * band);
                glNormal3f(x, y, z);
                glVertex3f(R * x, R * y, R * z);
            }
        }
        glEnd();
    }
}

void drawAtmosphere(Vec3 pos, float R, float r, float g, float b) {
    beginAdditive();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    glColor4f(r, g, b, 0.30f); glutSolidSphere(R * 1.03f, 40, 28);
    glColor4f(r, g, b, 0.15f); glutSolidSphere(R * 1.08f, 40, 28);
    glColor4f(r, g, b, 0.07f); glutSolidSphere(R * 1.16f, 40, 28);
    glPopMatrix();
    glDisable(GL_CULL_FACE);
    endAdditive();
}

void drawFlare(Vec3 p, float R, float r, float g, float b, float tsec) {
    beginBillboard(p);
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 18; ++i) {
        float a = i * 2.0f * PI / 18.0f + tsec * 0.04f;
        float len = R * (2.0f + 1.4f * (0.5f + 0.5f * sinf(i * 3.1f + tsec * 0.6f)));
        float w = 0.05f;
        glColor4f(r, g, b, 0.45f);
        glVertex3f(R * 0.9f * cosf(a - w), R * 0.9f * sinf(a - w), 0.0f);
        glVertex3f(R * 0.9f * cosf(a + w), R * 0.9f * sinf(a + w), 0.0f);
        glColor4f(r, g, b, 0.0f);
        glVertex3f(len * cosf(a), len * sinf(a), 0.0f);
    }
    glEnd();
    endBillboard();
}

void drawStar(Vec3 p, float len, float angDeg, float r, float g, float b, float a) {
    beginBillboard(p);
    glRotatef(angDeg, 0, 0, 1);
    float w = len * 0.12f;
    glBegin(GL_TRIANGLES);
    for (int k = 0; k < 4; ++k) {
        float ang = k * PI * 0.5f;
        float c = cosf(ang), sn = sinf(ang);
        glColor4f(r, g, b, a);
        glVertex3f(-sn * w, c * w, 0.0f);
        glVertex3f(sn * w, -c * w, 0.0f);
        glColor4f(r, g, b, 0.0f);
        glVertex3f(c * len, sn * len, 0.0f);
    }
    glEnd();
    endBillboard();
}

void drawRibbon(const std::vector<Vec3>& pts, float width, float r, float g, float b) {
    size_t n = pts.size();
    if (n < 2) return;
    glBegin(GL_TRIANGLE_STRIP);
    for (size_t i = 0; i < n; ++i) {
        float t = (float)i / (float)(n - 1);
        Vec3 d = (i + 1 < n) ? pts[i + 1] - pts[i] : pts[i] - pts[i - 1];
        Vec3 side = normalize(cross(d, camPos - pts[i]));
        float w = width * t;
        glColor4f(r, g, b, t * t * 0.85f);
        glVertex3f(pts[i].x + side.x * w, pts[i].y + side.y * w, pts[i].z + side.z * w);
        glVertex3f(pts[i].x - side.x * w, pts[i].y - side.y * w, pts[i].z - side.z * w);
    }
    glEnd();
}

void drawPortalFx(const Mouth& m, float tsec, float open, float activity) {
    const Theme& th = curTheme();
    float cr = lerpf(1.0f, th.accent[0], open);
    float cg = lerpf(0.25f, th.accent[1], open);
    float cb = lerpf(0.10f, th.accent[2], open);
    float R = MOUTH_RADIUS;

    glPushMatrix();
    glTranslatef(m.pos.x, m.pos.y, m.pos.z);
    glRotatef(atan2f(m.normal.x, m.normal.z) * 180.0f / PI, 0, 1, 0);

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 96; ++i) {
        float a = 2.0f * PI * i / 96.0f;
        float swirl = 0.5f + 0.5f * sinf(a * 3.0f - tsec * (1.5f + 2.0f * open + 2.0f * activity));
        glColor4f(cr, cg, cb, 0.20f + 0.40f * swirl);
        glVertex3f(R * 0.88f * cosf(a), R * 0.88f * sinf(a), 0.0f);
        glColor4f(cr, cg, cb, 0.0f);
        glVertex3f(R * 1.55f * cosf(a), R * 1.55f * sinf(a), 0.0f);
    }
    glEnd();

    glPointSize(3.5f);
    glBegin(GL_POINTS);
    for (int k = 0; k < 140; ++k) {
        float s2 = fmodf(k / 140.0f + tsec * 0.15f * (1.0f + open), 1.0f);
        float r = R * 0.95f * (1.0f - s2);
        float a = k * 2.4f + s2 * 7.0f - tsec * 2.0f * (1.0f + open);
        float sparkle = 0.5f + 0.5f * sinf(k * 3.7f + tsec * 8.0f);
        glColor4f(cr * 1.3f, cg * 1.3f, cb * 1.3f, (0.95f * (1.0f - s2 * 0.6f)) * (0.6f + 0.4f * sparkle));
        glVertex3f(r * cosf(a), r * sinf(a), 0.05f);
    }
    glEnd();

    glPopMatrix();
}

void drawSky(float tsec) {
    const Theme& th = curTheme();
    glPushMatrix();
    glTranslatef(camPos.x, camPos.y, camPos.z);
    gfxLight(false);
    glDepthMask(GL_FALSE);

    // Dim stars (subtle color tinting by temperature)
    glPointSize(1.8f);
    glBegin(GL_POINTS);
    for (int i = 0; i < NUM_STARS; ++i) {
        if (starBright[i] > 0.85f) continue;
        float b = starBright[i] * (0.85f + 0.15f * sinf(tsec * 2.0f + i));
        float hue = starHue[i];
        float sr, sg, sb;
        if (hue < 0.33f) { sr = 0.6f; sg = 0.75f; sb = 1.0f; }       // cool blue
        else if (hue < 0.66f) { sr = 0.95f; sg = 0.95f; sb = 1.0f; }  // white
        else { sr = 1.0f; sg = 0.85f; sb = 0.65f; }                    // warm gold
        glColor3f(b * sr, b * sg, b * sb);
        glVertex3f(starPos[i].x, starPos[i].y, starPos[i].z);
    }
    glEnd();

    // Bright stars (sparkle + color)
    glPointSize(3.2f);
    glBegin(GL_POINTS);
    for (int i = 0; i < NUM_STARS; ++i) {
        if (starBright[i] <= 0.85f) continue;
        float b = starBright[i] * (0.75f + 0.25f * sinf(tsec * 3.5f + i * 1.7f));
        float hue = starHue[i];
        float sr, sg, sb;
        if (hue < 0.2f) { sr = 0.55f; sg = 0.7f; sb = 1.0f; }
        else if (hue < 0.5f) { sr = 1.0f; sg = 0.98f; sb = 1.0f; }
        else if (hue < 0.8f) { sr = 1.0f; sg = 0.9f; sb = 0.7f; }
        else { sr = 1.0f; sg = 0.6f; sb = 0.5f; }                     // reddish
        glColor3f(b * sr, b * sg, b * sb);
        glVertex3f(starPos[i].x, starPos[i].y, starPos[i].z);
    }
    glEnd();

    // Bright star cross-flares (the brightest stars get small lens crosses)
    // We're already in additive-friendly state (lighting off, depthMask off)
    // so just switch blend mode without touching lighting/depth
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (int i = 0; i < NUM_STARS; ++i) {
        if (starBright[i] <= 0.94f) continue;
        float twinkle = 0.5f + 0.5f * sinf(tsec * 4.5f + i * 2.3f);
        if (twinkle < 0.55f) continue;
        float hue = starHue[i];
        float sr = 0.9f, sg = 0.9f, sb = 1.0f;
        if (hue > 0.6f) { sr = 1.0f; sg = 0.85f; sb = 0.7f; }
        drawStar(starPos[i], 4.5f * twinkle, tsec * 5.0f + i * 37.0f,
                 sr, sg, sb, 0.30f * twinkle);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (int i = 0; i < NUM_PUFFS; ++i) {
        float r = lerpf(th.nebA[0], th.nebB[0], puffs[i].t);
        float g = lerpf(th.nebA[1], th.nebB[1], puffs[i].t);
        float b = lerpf(th.nebA[2], th.nebB[2], puffs[i].t);
        // Boost saturation by pushing toward the dominant channel
        float mx = std::max({r, g, b});
        r = lerpf(r, mx > 0.01f ? r / mx : r, 0.3f) * 1.2f;
        g = lerpf(g, mx > 0.01f ? g / mx : g, 0.3f) * 1.2f;
        b = lerpf(b, mx > 0.01f ? b / mx : b, 0.3f) * 1.2f;
        float pulse = 1.0f + 0.06f * sinf(tsec * 0.5f + i * 1.3f);
        beginBillboard(puffs[i].pos);
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(r, g, b, puffs[i].a * 1.35f * pulse);
        glVertex3f(0, 0, 0);
        glColor4f(r, g, b, 0.0f);
        for (int k = 0; k <= 28; ++k) {
            float ang = 2.0f * PI * k / 28.0f;
            float warp = 1.0f + 0.08f * sinf(ang * 3.0f + tsec * 0.3f + i);
            glVertex3f(puffs[i].size * cosf(ang) * warp, puffs[i].size * sinf(ang) * 0.72f * warp, 0.0f);
        }
        glEnd();
        endBillboard();
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDepthMask(GL_TRUE);
    gfxLight(true);
    glPopMatrix();
}

void drawAnnulus(float r1, float r2, int segN, float cr, float cg, float cb, float a1, float a2) {
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segN; ++i) {
        float ang = 2.0f * PI * i / segN;
        glColor4f(cr, cg, cb, a1);
        glVertex3f(r1 * cosf(ang), r1 * sinf(ang), 0);
        glColor4f(cr, cg, cb, a2);
        glVertex3f(r2 * cosf(ang), r2 * sinf(ang), 0);
    }
    glEnd();
}

void drawCenterpieceOld(float tsec) {
    const Theme& th = curTheme();
    Vec3 c = centerPos;
    float R = centerRadius;

    if (th.center == 0) {
        gfxLight(true);
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glRotatef(15.0f, 0, 0, 1);
        setMat(0.15f, 20.0f);
        drawBandedSphere(R, th.cc[0], th.cc[1], th.cc[2], 7.0f);
        setMat(0.0f, 0.0f);
        glPopMatrix();

        gfxLight(false);
        glDepthMask(GL_FALSE);
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glRotatef(20.0f, 0, 0, 1);
        glRotatef(90.0f, 1, 0, 0);
        drawAnnulus(R * 1.30f, R * 1.45f, 120, th.cc[0] * 0.8f, th.cc[1] * 0.8f, th.cc[2] * 0.8f, 0.25f, 0.30f);
        drawAnnulus(R * 1.45f, R * 1.62f, 120, th.cc[0] * 1.1f, th.cc[1] * 1.1f, th.cc[2] * 1.1f, 0.55f, 0.50f);
        drawAnnulus(R * 1.68f, R * 1.88f, 120, 0.9f, 0.9f, 0.95f, 0.35f, 0.30f);
        drawAnnulus(R * 1.94f, R * 2.35f, 120, th.cc[0], th.cc[1], th.cc[2], 0.42f, 0.10f);
        glPopMatrix();
        glDepthMask(GL_TRUE);
        gfxLight(true);

        drawAtmosphere(c, R, th.cc[0], th.cc[1], th.cc[2]);
        beginAdditive();
        glowDisc(c, R * 1.5f, th.cc[0], th.cc[1], th.cc[2], 0.22f);
        endAdditive();
    } else if (th.center == 1) {
        gfxLight(false);
        glColor3f(th.cc[0], th.cc[1], th.cc[2]);
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glutSolidSphere(R, 48, 32);
        glPopMatrix();
        gfxLight(true);

        beginAdditive();
        float pulse = 1.0f + 0.06f * sinf(tsec * 1.3f);
        float pulse2 = 1.0f + 0.04f * sinf(tsec * 2.7f + 1.2f);
        glowDisc(c, R * 1.15f, 1.0f, 0.97f, 0.85f, 0.65f);
        glowDisc(c, R * 1.7f * pulse, th.cc[0], th.cc[1], th.cc[2], 0.70f);
        glowDisc(c, R * 3.2f * pulse, th.cc[0], th.cc[1], th.cc[2], 0.35f);
        glowDisc(c, R * 5.5f * pulse2, th.cc[0] * 0.8f, th.cc[1] * 0.7f, th.cc[2] * 0.6f, 0.18f);
        glowDisc(c, R * 8.0f * pulse, th.cc[0] * 0.5f, th.cc[1] * 0.4f, th.cc[2] * 0.3f, 0.08f);
        drawFlare(c, R, th.cc[0], th.cc[1], th.cc[2], tsec);
        drawFlare(c, R * 0.85f, 1.0f, 0.95f, 0.8f, tsec * 0.7f + 2.0f);
        endAdditive();
    } else {
        gfxLight(false);
        glColor3f(0.0f, 0.0f, 0.0f);
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glutSolidSphere(R, 48, 32);
        glPopMatrix();
        gfxLight(true);

        beginAdditive();
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glRotatef(68.0f, 1, 0, 0);
        glRotatef(14.0f, 0, 1, 0);
        const int SEGN = 180;
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= SEGN; ++i) {
            float ang = 2.0f * PI * i / SEGN;
            float swirl = 0.5f + 0.5f * sinf(ang * 3.0f - tsec * 2.0f);
            float r1 = R * 1.35f, r2 = R * 3.6f;
            glColor4f(1.0f, 0.95f, 0.7f, 0.55f + 0.35f * swirl);
            glVertex3f(r1 * cosf(ang), r1 * sinf(ang), 0);
            glColor4f(1.0f, 0.35f + 0.2f * swirl, 0.08f, 0.0f);
            glVertex3f(r2 * cosf(ang), r2 * sinf(ang), 0);
        }
        glEnd();
        glPopMatrix();

        beginBillboard(c);
        glLineWidth(3.5f);
        glColor4f(1.0f, 0.85f, 0.55f, 0.85f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 96; ++i) {
            float a = 2.0f * PI * i / 96.0f;
            glVertex3f(R * 1.07f * cosf(a), R * 1.07f * sinf(a), 0.0f);
        }
        glEnd();
        glLineWidth(1.0f);
        endBillboard();

        glowDisc(c, R * 1.9f, 1.0f, 0.65f, 0.25f, 0.30f);
        glowDisc(c, R * 3.5f, 1.0f, 0.5f, 0.2f, 0.12f);
        endAdditive();
    }
}

void drawPlanetsOld() {
    gfxLight(true);
    int idx = 0;
    for (const auto& p : planets) {
        glPushMatrix();
        glTranslatef(p.pos.x, p.pos.y, p.pos.z);
        glRotatef(20.0f * (idx + 1), 0, 0, 1);
        setMat(0.12f, 18.0f);
        drawBandedSphere(p.radius, p.r, p.g, p.b, 6.0f + 2.0f * idx);
        setMat(0.0f, 0.0f);
        glPopMatrix();
        ++idx;
    }
    for (const auto& p : planets) drawAtmosphere(p.pos, p.radius, p.r, p.g, p.b);
    beginAdditive();
    for (const auto& p : planets) glowDisc(p.pos, p.radius * 1.5f, p.r, p.g, p.b, 0.18f);
    endAdditive();
}

// ---- v1.4: shader-based centrepiece & moons ----
void drawCenterpieceGfx(float tsec) {
    const Theme& th = curTheme();
    Vec3 c = centerPos;
    float R = centerRadius;
    float atmo[3] = {th.cc[0] * 0.7f + 0.25f, th.cc[1] * 0.7f + 0.30f, th.cc[2] * 0.7f + 0.40f};

    if (th.center == 0) {
        float c2[3] = {th.nebB[0] * 0.8f + 0.15f, th.nebB[1] * 0.8f + 0.15f, th.nebB[2] * 0.8f + 0.15f};
        gfxLight(false);
        planetUniforms(0.0f, th.cc, c2, (float)level * 1.7f, tsec, atmo);
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glRotatef(15.0f, 0, 0, 1);
        glScalef(R, R, R);
        glCallList(sphereList96);
        glPopMatrix();
        useProg(0);

        // ring system with planet shadow
        useProg(progRing);
        float pe[3];
        eyePos(c, pe);
        u1("uRin", R * 1.28f);
        u1("uRout", R * 2.4f);
        u1("uSeed", (float)level * 3.1f);
        u3("uCol", th.cc[0] * 0.85f + 0.30f, th.cc[1] * 0.85f + 0.28f, th.cc[2] * 0.85f + 0.25f);
        u3("uPC", pe[0], pe[1], pe[2]);
        u1("uPR", R);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glRotatef(20.0f, 0, 0, 1);
        glRotatef(90.0f, 1, 0, 0);
        drawAnnulus(R * 1.28f, R * 2.4f, 220, 1, 1, 1, 1, 1);
        glPopMatrix();
        glDepthMask(GL_TRUE);
        useProg(0);

        drawAtmoShell(c, R, atmo, 1.0f);
        beginAdditive();
        glowDisc(c, R * 1.5f, th.cc[0], th.cc[1], th.cc[2], 0.08f);
        endAdditive();
    } else if (th.center == 1) {
        gfxLight(false);
        useProg(progSun);
        u1("uTime", tsec);
        u1("uSeed", (float)level * 2.9f);
        u3("uCol", th.cc[0], th.cc[1], th.cc[2]);
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glScalef(R, R, R);
        glCallList(sphereList96);
        glPopMatrix();
        useProg(0);

        beginAdditive();
        float pulse = 1.0f + 0.06f * sinf(tsec * 1.3f);
        float pulse2 = 1.0f + 0.04f * sinf(tsec * 2.7f + 1.2f);
        glowDisc(c, R * 1.25f, 1.0f, 0.97f, 0.85f, 0.35f);
        glowDisc(c, R * 1.7f * pulse, th.cc[0], th.cc[1], th.cc[2], 0.40f);
        glowDisc(c, R * 3.2f * pulse, th.cc[0], th.cc[1], th.cc[2], 0.20f);
        glowDisc(c, R * 5.5f * pulse2, th.cc[0] * 0.8f, th.cc[1] * 0.7f, th.cc[2] * 0.6f, 0.10f);
        glowDisc(c, R * 8.0f * pulse, th.cc[0] * 0.5f, th.cc[1] * 0.4f, th.cc[2] * 0.3f, 0.05f);
        drawFlare(c, R, th.cc[0], th.cc[1], th.cc[2], tsec);
        drawFlare(c, R * 0.85f, 1.0f, 0.95f, 0.8f, tsec * 0.7f + 2.0f);
        endAdditive();
    } else {
        // black hole: event horizon, Doppler-beamed accretion disk, photon ring (lensing is done in post)
        gfxLight(false);
        useProg(0);
        glColor3f(0.0f, 0.0f, 0.0f);
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glScalef(R, R, R);
        glCallList(sphereList96);
        glPopMatrix();

        useProg(progDisk);
        u1("uTime", tsec);
        u1("uSeed", (float)level * 1.3f);
        u1("uRin", R * 1.3f);
        u1("uRout", R * 4.2f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glRotatef(68.0f, 1, 0, 0);
        glRotatef(14.0f, 0, 1, 0);
        drawAnnulus(R * 1.3f, R * 4.2f, 260, 1, 1, 1, 1, 1);
        glPopMatrix();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
        useProg(0);

        beginAdditive();
        beginBillboard(c);
        glLineWidth(3.0f);
        glColor4f(1.0f, 0.85f, 0.55f, 0.55f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 128; ++i) {
            float a = 2.0f * PI * i / 128.0f;
            glVertex3f(R * 1.06f * cosf(a), R * 1.06f * sinf(a), 0.0f);
        }
        glEnd();
        glLineWidth(1.0f);
        endBillboard();
        glowDisc(c, R * 1.9f, 1.0f, 0.65f, 0.25f, 0.12f);
        glowDisc(c, R * 3.5f, 1.0f, 0.5f, 0.2f, 0.05f);
        endAdditive();
    }
}

void drawPlanetsGfx(float tsec) {
    const Theme& th = curTheme();
    for (const auto& p : planets) {
        float c1[3] = {p.r, p.g, p.b};
        float c2[3] = {th.nebA[0] * 0.8f + 0.1f, th.nebA[1] * 0.8f + 0.1f, th.nebA[2] * 0.8f + 0.1f};
        float atmo[3] = {0.30f, 0.55f, 1.0f};
        if (p.kind == 0) { atmo[0] = p.r * 0.8f + 0.2f; atmo[1] = p.g * 0.8f + 0.2f; atmo[2] = p.b * 0.8f + 0.2f; }
        gfxLight(false);
        planetUniforms((float)p.kind, c1, c2, p.seed, tsec, atmo);
        glPushMatrix();
        glTranslatef(p.pos.x, p.pos.y, p.pos.z);
        glRotatef(20.0f + p.seed * 7.0f, 0, 0, 1);
        glScalef(p.radius, p.radius, p.radius);
        glCallList(sphereList48);
        glPopMatrix();
        useProg(0);
        if (p.kind < 2) drawAtmoShell(p.pos, p.radius, atmo, 0.9f);
    }
}

void drawCenterpiece(float tsec) { if (gfxOK) drawCenterpieceGfx(tsec); else drawCenterpieceOld(tsec); }
void drawPlanets(float tsec)     { if (gfxOK) drawPlanetsGfx(tsec);     else drawPlanetsOld(); }

static float wrap60(float v) {
    v = fmodf(v, 60.0f);
    if (v < 0.0f) v += 60.0f;
    return v - 30.0f;
}

void drawDust() {
    gfxLight(false);
    bool moving = (state == S_FLYING || state == S_EMERGING) && fabsf(speed) > 0.12f;
    Vec3 f = forwardVec();
    float sgn = speed >= 0.0f ? 1.0f : -1.0f;
    float spdMag = fabsf(speed);
    bool isBoosting = boosting && state == S_FLYING;
    float len = clampf(spdMag * 9.0f, 0.0f, 9.0f) * (isBoosting ? 2.2f : 1.0f);

    if (moving) {
        glLineWidth(isBoosting ? 1.8f : 1.3f);
        glBegin(GL_LINES);
    } else {
        glPointSize(1.8f);
        glBegin(GL_POINTS);
    }
    for (int i = 0; i < NUM_DUST; ++i) {
        Vec3 d = {wrap60(dustBase[i].x - ship.x),
                  wrap60(dustBase[i].y - ship.y),
                  wrap60(dustBase[i].z - ship.z)};
        float fade = clampf(1.0f - length(d) / 52.0f, 0.0f, 1.0f);
        if (isBoosting) fade *= 1.4f;
        Vec3 p0 = ship + d;
        // Tint dust with speed-dependent color
        float sr = 0.8f, sg = 0.9f, sb = 1.0f;
        if (isBoosting) { sr = 0.95f; sg = 0.75f; sb = 0.45f; } // warm orange streaks
        glColor4f(sr, sg, sb, fade * 0.75f);
        glVertex3f(p0.x, p0.y, p0.z);
        if (moving) {
            Vec3 p1 = p0 + f * (len * sgn);
            glColor4f(sr, sg, sb, 0.0f);
            glVertex3f(p1.x, p1.y, p1.z);
        }
    }
    glEnd();
    glLineWidth(1.0f);
    gfxLight(true);
}

void drawCoins(float tsec) {
    gfxLight(true);
    for (const auto& c : coins) {
        if (c.taken || behindCam(c.pos, 3.0f)) continue;
        float bob = sinf(tsec * 2.0f + c.phase) * 0.3f;
        glPushMatrix();
        glTranslatef(c.pos.x, c.pos.y + bob, c.pos.z);
        glRotatef(tsec * 140.0f + c.phase * 57.0f, 0, 1, 0);
        glCallList(coinList);
        glPopMatrix();
    }
}

void drawHearts3D(float tsec) {
    gfxLight(true);
    setMat(0.85f, 80.0f);
    for (const auto& h : hearts) {
        if (h.taken || behindCam(h.pos, 4.0f)) continue;
        float bob = sinf(tsec * 2.6f + h.phase) * 0.45f;
        float pulse = 1.0f + 0.12f * sinf(tsec * 6.0f + h.phase);
        glPushMatrix();
        glTranslatef(h.pos.x, h.pos.y + bob, h.pos.z);
        glRotatef(tsec * 110.0f + h.phase * 50.0f, 0, 1, 0);
        glScalef(pulse, pulse, pulse);
        glCallList(heartBodyList);
        gfxLight(false);
        glRotatef(tsec * 90.0f, 1, 1, 0);
        glCallList(heartRingList);
        gfxLight(true);
        glPopMatrix();
    }
    setMat(0.0f, 0.0f);
}

// v1.2: floating power-up capsules (each type has its own shape + colour)
void drawPowerUps3D(float tsec) {
    gfxLight(true);
    setMat(0.9f, 90.0f);
    for (const auto& p : powerups) {
        if (p.taken || behindCam(p.pos, 4.0f)) continue;
        float bob = sinf(tsec * 2.2f + p.phase) * 0.5f;
        const float* c = POW_COL[p.type];
        glPushMatrix();
        glTranslatef(p.pos.x, p.pos.y + bob, p.pos.z);
        glRotatef(tsec * 80.0f + p.phase * 40.0f, 0, 1, 0);
        glRotatef(20.0f * sinf(tsec + p.phase), 1, 0, 0);
        glColor3f(c[0], c[1], c[2]);
        switch (p.type) {
            case PW_SHIELD: glPushMatrix(); glScalef(0.85f, 0.85f, 0.85f); glutSolidIcosahedron(); glPopMatrix(); break;
            case PW_RAPID:  glPushMatrix(); glScalef(0.95f, 1.2f, 0.95f);  glutSolidOctahedron();  glPopMatrix(); break;
            case PW_MAGNET: glutSolidTorus(0.22, 0.62, 12, 24); break;
            case PW_DOUBLE: glutSolidCube(1.15); break;
            case PW_AMMO:   glPushMatrix(); glTranslatef(0, 0, -0.8f); glutSolidCone(0.55, 1.6, 16, 4); glPopMatrix(); break;
            case PW_DRONE:  glPushMatrix(); glScalef(0.9f, 0.9f, 0.9f); glutSolidTetrahedron(); glPopMatrix(); break;
            case PW_BOMB:   glutSolidSphere(0.62, 18, 14); break;
        }
        gfxLight(false);
        glColor3f(1.0f, 1.0f, 1.0f);
        glPushMatrix();
        glRotatef(tsec * 120.0f, 0, 0, 1);
        glutWireCube(1.9);
        glPopMatrix();
        gfxLight(true);
        glPopMatrix();
    }
    setMat(0.0f, 0.0f);
}

void drawBeacons(float tsec) {
    gfxLight(false);
    for (const auto& b : beacons) {
        float act = b.active ? 1.0f : 0.0f;
        float cr = lerpf(1.0f, 0.2f, act), cg = lerpf(0.55f, 1.0f, act), cb = lerpf(0.10f, 0.5f, act);
        float pulse = 0.65f + 0.35f * sinf(tsec * 4.0f + b.anim);

        glPushMatrix();
        glTranslatef(b.pos.x, b.pos.y, b.pos.z);

        glLineWidth(3.0f);
        glColor3f(cr * pulse, cg * pulse, cb * pulse);
        for (int a = 0; a < 6; ++a) {
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i <= 14; ++i) {
                float ang = a * PI / 3.0f + tsec * (act ? 0.5f : 1.5f) + i * (PI / 3.0f * 0.7f) / 14.0f;
                glVertex3f(5.0f * cosf(ang), 5.0f * sinf(ang), 0.0f);
            }
            glEnd();
        }
        glLineWidth(2.0f);
        for (int a = 0; a < 4; ++a) {
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i <= 12; ++i) {
                float ang = a * PI / 2.0f - tsec * 2.2f + i * (PI / 2.0f * 0.6f) / 12.0f;
                glVertex3f(3.4f * cosf(ang), 3.4f * sinf(ang), 0.0f);
            }
            glEnd();
        }
        glRotatef(tsec * 90.0f, 0, 1, 0);
        glRotatef(tsec * 50.0f, 1, 0, 0);
        glColor3f(cr, cg, cb);
        glutSolidOctahedron();
        glColor3f(1, 1, 1);
        glLineWidth(1.0f);
        glPushMatrix();
        glScalef(1.4f, 1.4f, 1.4f);
        glutWireOctahedron();
        glPopMatrix();
        glPopMatrix();
    }
    glLineWidth(1.0f);
    gfxLight(true);
}

void drawObstacles(float tsec) {
    gfxLight(true);
    for (const auto& o : obstacles) {
        if (!o.active || behindCam(o.pos, o.radius * 2.0f + 4.0f)) continue;
        glPushMatrix();
        glTranslatef(o.pos.x, o.pos.y, o.pos.z);
        float lodNear = (length(o.pos - camPos) < 110.0f) ? 1.0f : 0.0f;

        if (o.type == OB_ASTEROID) {
            glRotatef(o.rot, o.axis.x, o.axis.y, o.axis.z);
            float k = o.radius / 1.73f;
            glScalef(o.scl.x * k, o.scl.y * k, o.scl.z * k);
            // v1.2: damaged asteroids glow slightly red-hot
            float dmg = (o.maxHp > 0.0f) ? 1.0f - clampf(o.hp / o.maxHp, 0.0f, 1.0f) : 0.0f;
            float rr = o.r + dmg * 0.45f, gg = o.g * (1.0f - dmg * 0.35f), bb = o.b * (1.0f - dmg * 0.45f);
            if (gfxOK) {
                glScalef(1.73f, 1.73f, 1.73f);
                matBegin(0.0f, 0.9f, 1, fmodf(o.rotSpeed * 0.37f + o.radius, 10.0f), dmg, lodNear);
                glColor3f(rr, gg, bb);
                glCallList((lodNear > 0.5f ? rockList : rockListLo)[((int)(o.rotSpeed * 10.0f)) % 3]);
            } else {
            glColor3f(rr, gg, bb);
            glutSolidDodecahedron();
            glPushMatrix();
            glRotatef(37.0f, 1.0f, 0.3f, 0.2f);
            glTranslatef(0.45f, 0.25f, -0.3f);
            glScalef(0.7f, 0.65f, 0.75f);
            glColor3f(rr * 0.85f, gg * 0.85f, bb * 0.9f);
            glutSolidDodecahedron();
            glPopMatrix();
            glPushMatrix();
            glRotatef(-61.0f, 0.2f, 1.0f, 0.4f);
            glTranslatef(-0.5f, -0.2f, 0.45f);
            glScalef(0.5f, 0.55f, 0.5f);
            glColor3f(rr * 1.1f, gg * 1.1f, bb * 1.05f);
            glutSolidDodecahedron();
            glPopMatrix();
            }
        } else if (o.type == OB_MINE) {
            matBegin(0.75f, 0.45f, 3);
            float R = o.radius;
            glRotatef(o.rot, 0, 1, 0);
            glColor3f(0.22f, 0.22f, 0.28f);
            glutSolidSphere(R * 0.7f, 16, 12);
            for (int sx = -1; sx <= 1; ++sx)
                for (int sy = -1; sy <= 1; ++sy)
                    for (int sz = -1; sz <= 1; ++sz) {
                        if (sx == 0 && sy == 0 && sz == 0) continue;
                        int nz = (sx != 0) + (sy != 0) + (sz != 0);
                        if (nz == 2) continue;
                        Vec3 d = normalize(Vec3{(float)sx, (float)sy, (float)sz});
                        glPushMatrix();
                        alignZ(d);
                        glTranslatef(0, 0, R * 0.55f);
                        glColor3f(0.45f, 0.45f, 0.5f);
                        glutSolidCone(R * 0.2f, R * 0.7f, 8, 2);
                        glPopMatrix();
                    }
            float pulse = 0.5f + 0.5f * sinf(tsec * 6.0f + o.pos.x);
            gfxLight(false);
            glColor3f(1.0f, 0.1f + 0.2f * pulse, 0.1f);
            glutSolidSphere(R * 0.32f, 12, 8);
            gfxLight(true);
        } else {
            glRotatef(o.rot, o.axis.x, o.axis.y, o.axis.z);
            if (gfxOK) {
                glScalef(o.radius * 1.5f, o.radius * 1.5f, o.radius * 1.5f);
                matBegin(0.0f, 0.8f, 2, fmodf(o.rotSpeed * 0.21f, 10.0f), 0.0f, lodNear);
                glColor3f(0.55f, 0.35f, 0.28f);
                glCallList((lodNear > 0.5f ? rockList : rockListLo)[((int)(o.rotSpeed * 10.0f)) % 3]);
            } else {
                glScalef(o.radius, o.radius, o.radius);
                glColor3f(1.0f, 0.35f, 0.2f);
                glutSolidIcosahedron();
            }
        }
        glPopMatrix();
    }
    matEnd();
}

void drawPortalBody(const Mouth& m, float tsec, float activity, float open) {
    const Theme& th = curTheme();
    float cr = lerpf(1.0f, th.accent[0], open);
    float cg = lerpf(0.25f, th.accent[1], open);
    float cb = lerpf(0.10f, th.accent[2], open);
    float R = MOUTH_RADIUS;

    glPushMatrix();
    glTranslatef(m.pos.x, m.pos.y, m.pos.z);
    glRotatef(atan2f(m.normal.x, m.normal.z) * 180.0f / PI, 0, 1, 0);
    gfxLight(false);

    glBegin(GL_TRIANGLE_FAN);
    glColor3f(0.0f, 0.0f, 0.02f);
    glVertex3f(0, 0, 0);
    glColor3f(cr * 0.22f, cg * 0.22f, cb * 0.30f + 0.05f);
    for (int i = 0; i <= 56; ++i) {
        float a = 2.0f * PI * i / 56.0f;
        glVertex3f(R * 0.92f * cosf(a), R * 0.92f * sinf(a), 0.0f);
    }
    glEnd();

    float spin = tsec * (0.4f + open * 1.8f + activity * 2.0f);
    glLineWidth(2.0f);
    for (int j = 0; j < 8; ++j) {
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 44; ++i) {
            float s = i / 44.0f;
            float r = 0.3f + s * (R * 0.92f - 0.3f);
            float a = j * PI / 4.0f + s * 3.2f - spin * 1.5f;
            glColor3f(cr * (0.3f + 0.7f * s), cg * (0.3f + 0.7f * s), cb * (0.4f + 0.6f * s));
            glVertex3f(r * cosf(a), r * sinf(a), 0.03f);
        }
        glEnd();
    }

    for (int ring = 0; ring < 5; ++ring) {
        float radius = R + ring * 0.32f;
        float phase = tsec * (ring % 2 ? -1.0f : 1.0f) * (0.6f + open + activity * 2.0f) + ring * 0.7f;
        glLineWidth(ring == 0 ? 4.5f : 2.0f);
        if (ring % 2 == 0) glColor3f(cr, cg, cb);
        else               glColor3f(cr * 0.6f + 0.35f, cg * 0.5f, cb * 0.6f + 0.4f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 110; ++i) {
            float a = 2.0f * PI * i / 110.0f;
            float r = radius + 0.1f * sinf(a * 8.0f + phase * 2.0f);
            glVertex3f(r * cosf(a), r * sinf(a), 0.0f);
        }
        glEnd();
    }

    if (open < 0.98f) {
        glLineWidth(3.0f);
        float ka = 1.0f - open;
        for (int s = 0; s < 12; ++s) {
            float a = s * PI / 6.0f + tsec * 0.3f;
            glColor4f(1.0f, 0.3f, 0.1f, 0.8f * ka);
            glBegin(GL_LINES);
            glVertex3f(R * 0.25f * cosf(a), R * 0.25f * sinf(a), 0.05f);
            glVertex3f(R * 0.98f * cosf(a), R * 0.98f * sinf(a), 0.05f);
            glEnd();
        }
    }

    glLineWidth(1.5f);
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 1; i <= 9; ++i) {
            float z = side * i * 0.8f;
            float rad = R * (1.0f - i * 0.09f) * (0.97f + 0.03f * sinf(tsec * 2.0f + i));
            float f = 1.0f - i / 10.0f;
            glColor3f(cr * f * 0.8f, cg * f * 0.8f, cb * f);
            glBegin(GL_LINE_LOOP);
            for (int k = 0; k < 48; ++k) {
                float a = 2.0f * PI * k / 48.0f + i * 0.2f;
                glVertex3f(rad * cosf(a), rad * sinf(a), z);
            }
            glEnd();
        }
    }

    glLineWidth(1.0f);
    gfxLight(true);
    glPopMatrix();
}

void drawShipModel(float tsec, float boostK) {
    float flame = 0.5f + 0.5f * sinf(tsec * 40.0f);
    gfxLight(true);

    setMat(0.9f, 80.0f);
    glColor3f(0.72f, 0.78f, 0.88f);
    glPushMatrix();
    glScalef(0.55f, 0.42f, 1.65f);
    glutSolidSphere(1.0, 28, 18);
    glPopMatrix();

    glColor3f(0.42f, 0.48f, 0.60f);
    glPushMatrix();
    glTranslatef(0.0f, 0.30f, 0.35f);
    glScalef(0.17f, 0.14f, 1.05f);
    glutSolidSphere(1.0, 16, 10);
    glPopMatrix();

    glColor3f(0.88f, 0.94f, 1.0f);
    glPushMatrix();
    glTranslatef(0, 0, -1.45f);
    glRotatef(180.0f, 0, 1, 0);
    glutSolidCone(0.2, 1.0, 20, 12);
    glPopMatrix();

    setMat(1.0f, 120.0f);
    glColor3f(0.05f, 0.75f, 1.0f);
    glPushMatrix();
    glTranslatef(0, 0.30f, -0.45f);
    glScalef(0.29f, 0.25f, 0.58f);
    glutSolidSphere(1.0, 24, 16);
    glPopMatrix();

    setMat(0.5f, 50.0f);
    glColor3f(0.36f, 0.44f, 0.58f);
    glBegin(GL_TRIANGLES);
    glNormal3f(0, 1, 0);
    glVertex3f( 0.25f, 0.0f,  0.0f); glVertex3f( 2.0f, 0.0f, 0.85f); glVertex3f( 0.45f, 0.0f, -0.75f);
    glVertex3f(-0.25f, 0.0f,  0.0f); glVertex3f(-0.45f, 0.0f, -0.75f); glVertex3f(-2.0f, 0.0f, 0.85f);
    glEnd();
    glColor3f(0.20f, 0.26f, 0.36f);
    glBegin(GL_TRIANGLES);
    glNormal3f(0, -1, 0);
    glVertex3f( 0.25f, -0.06f,  0.0f); glVertex3f( 2.0f, -0.06f, 0.85f); glVertex3f( 0.45f, -0.06f, -0.75f);
    glVertex3f(-0.25f, -0.06f,  0.0f); glVertex3f(-0.45f, -0.06f, -0.75f); glVertex3f(-2.0f, -0.06f, 0.85f);
    glEnd();

    glColor3f(0.50f, 0.57f, 0.70f);
    for (int s2 = -1; s2 <= 1; s2 += 2) {
        glPushMatrix();
        glTranslatef(0.55f * s2, -0.02f, 0.15f);
        glScalef(0.4f, 0.16f, 0.9f);
        glutSolidSphere(1.0, 14, 10);
        glPopMatrix();
    }

    glColor3f(0.30f, 0.38f, 0.52f);
    glBegin(GL_TRIANGLES);
    glNormal3f(1, 0, 0);
    for (int s2 = -1; s2 <= 1; s2 += 2) {
        glVertex3f(2.0f * s2, 0.0f, 0.85f);
        glVertex3f(2.0f * s2, 0.55f, 1.05f);
        glVertex3f(1.7f * s2, 0.0f, 0.95f);
    }
    glVertex3f(0.0f, 0.2f, 0.8f); glVertex3f(0.0f, 0.95f, 1.5f); glVertex3f(0.0f, 0.2f, 1.5f);
    glEnd();

    setMat(0.7f, 60.0f);
    for (int s2 = -1; s2 <= 1; s2 += 2) {
        glColor3f(0.25f, 0.28f, 0.36f);
        glPushMatrix();
        glTranslatef(0.7f * s2, -0.05f, 0.5f);
        glutSolidCylinder(0.2, 1.1, 16, 1);
        glPopMatrix();
        glColor3f(0.55f, 0.60f, 0.70f);
        glPushMatrix();
        glTranslatef(0.7f * s2, -0.05f, 0.5f);
        glRotatef(180.0f, 0, 1, 0);
        glutSolidCone(0.2, 0.35, 14, 2);
        glPopMatrix();
        glColor3f(0.80f, 0.85f, 0.92f);
        glPushMatrix();
        glTranslatef(0.7f * s2, -0.05f, 1.6f);
        glutSolidTorus(0.045, 0.2, 8, 20);
        glPopMatrix();
    }
    setMat(0.0f, 0.0f);

    gfxLight(false);
    glLineWidth(2.0f);
    glColor3f(0.2f, 0.9f, 1.0f);
    glBegin(GL_LINES);
    glVertex3f( 0.35f, 0.02f, -0.5f); glVertex3f( 1.9f, 0.02f, 0.75f);
    glVertex3f(-0.35f, 0.02f, -0.5f); glVertex3f(-1.9f, 0.02f, 0.75f);
    glEnd();
    glLineWidth(1.0f);

    glColor3f(1.0f, 0.15f, 0.12f);
    glPushMatrix(); glTranslatef(-2.0f, 0.05f, 0.85f); glutSolidSphere(0.09, 10, 8); glPopMatrix();
    glColor3f(0.1f, 0.8f, 0.3f);
    glPushMatrix(); glTranslatef( 2.0f, 0.05f, 0.85f); glutSolidSphere(0.09, 10, 8); glPopMatrix();

    for (int s2 = -1; s2 <= 1; s2 += 2) {
        glPushMatrix();
        glTranslatef(0.7f * s2, -0.05f, 1.6f);
        glColor3f(0.1f, 0.5f + flame * 0.5f, 1.0f);
        glutSolidCone(0.17f + flame * 0.05f, (0.7f + flame * 0.5f) * boostK, 12, 4);
        glColor3f(0.85f, 0.95f, 1.0f);
        glutSolidCone(0.08f, (0.4f + flame * 0.25f) * boostK, 10, 3);
        glPopMatrix();
    }
    gfxLight(true);
}

void drawShipWorld(float tsec) {
    if (!shipVisible) return;
    if (invuln > 0.0f && fmodf(invuln, 0.2f) > 0.13f) return;

    float boostK = (boosting && state == S_FLYING) ? 1.7f : 1.0f;
    glPushMatrix();
    glTranslatef(ship.x, ship.y, ship.z);
    glRotatef(yaw * 180.0f / PI, 0, 1, 0);
    glRotatef(pitch * 180.0f / PI, 1, 0, 0);
    glRotatef(roll * 180.0f / PI, 0, 0, 1);
    glScalef(shipScale, shipScale, shipScale);
    drawShipModel(tsec, boostK);
    glPopMatrix();
}

void drawKamikazeModel(const Enemy& e, float tsec) {
    bool hit = e.flash > 0.0f;
    float pulse = 0.5f + 0.5f * sinf(tsec * 14.0f + e.phase);
    gfxLight(true);
    setMat(0.5f, 30.0f);
    if (hit) glColor3f(1.0f, 1.0f, 1.0f); else glColor3f(0.60f, 0.22f + 0.10f * pulse, 0.10f);
    glPushMatrix();
    glScalef(0.9f, 0.9f, 1.5f);
    glutSolidSphere(1.0, 16, 12);
    glPopMatrix();

    glColor3f(0.9f, 0.85f, 0.8f);
    glPushMatrix();
    glTranslatef(0, 0, -1.5f);
    glRotatef(180.0f, 0, 1, 0);
    glutSolidCone(0.5, 1.3, 12, 4);
    glPopMatrix();

    glColor3f(0.25f, 0.1f, 0.08f);
    for (int k = 0; k < 4; ++k) {
        glPushMatrix();
        glRotatef(k * 90.0f + 45.0f, 0, 0, 1);
        glTranslatef(1.1f, 0.0f, 0.7f);
        glScalef(1.3f, 0.12f, 0.9f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    gfxLight(false);
    glColor3f(1.0f, 0.4f + 0.5f * pulse, 0.1f);
    glutSolidSphere(0.5, 10, 8);
    glPushMatrix();
    glTranslatef(0, 0, 1.6f);
    glutSolidCone(0.4, 1.0f + 0.5f * pulse, 10, 3);
    glPopMatrix();
    gfxLight(true);
    setMat(0.0f, 0.0f);
}

void drawEnemyModel(const Enemy& e, float tsec) {
    bool hit = e.flash > 0.0f;
    float br = 0.32f, bg = 0.12f, bb = 0.14f;      // hull colour
    float er = 1.0f,  eg = 0.15f, eb = 0.10f;      // eye colour
    if (e.type == ET_GUNSHIP) { br = 0.14f; bg = 0.18f; bb = 0.34f; er = 0.2f; eg = 0.55f; eb = 1.0f; }
    if (e.type == ET_BOSS)    { br = 0.28f; bg = 0.08f; bb = 0.32f; er = 0.9f; eg = 0.25f; eb = 1.0f; }
    gfxLight(true);
    setMat(0.6f, 40.0f);

    if (hit) glColor3f(1.0f, 0.9f, 0.8f); else glColor3f(br, bg, bb);
    glPushMatrix();
    glScalef(0.7f, 0.42f, 1.8f);
    glutSolidSphere(1.0, 20, 14);
    glPopMatrix();

    if (hit) glColor3f(1.0f, 1.0f, 1.0f); else glColor3f(std::min(1.0f, br * 1.6f), std::min(1.0f, bg * 1.6f), std::min(1.0f, bb * 1.6f));
    glPushMatrix();
    glTranslatef(0, 0, -1.6f);
    glRotatef(180.0f, 0, 1, 0);
    glutSolidCone(0.38, 1.3, 16, 8);
    glPopMatrix();

    if (hit) glColor3f(1.0f, 0.9f, 0.8f); else glColor3f(br * 0.7f, bg * 0.7f, bb * 0.7f);
    glBegin(GL_TRIANGLES);
    glNormal3f(0, 1, 0);
    glVertex3f( 0.3f, 0.0f, -0.6f); glVertex3f( 2.3f, 0.0f, 0.9f); glVertex3f( 0.5f, 0.0f, 1.0f);
    glVertex3f(-0.3f, 0.0f, -0.6f); glVertex3f(-0.5f, 0.0f, 1.0f); glVertex3f(-2.3f, 0.0f, 0.9f);
    glEnd();

    glBegin(GL_TRIANGLES);
    glNormal3f(1, 0, 0);
    glVertex3f(0.0f, 0.3f, 0.4f); glVertex3f(0.0f, 1.1f, 1.5f); glVertex3f(0.0f, 0.3f, 1.5f);
    glEnd();

    glColor3f(0.15f, 0.15f, 0.18f);
    for (int sd = -1; sd <= 1; sd += 2) {
        glPushMatrix();
        glTranslatef(1.7f * sd, -0.05f, -1.0f);
        glutSolidCylinder(0.09, 1.6, 8, 1);
        glPopMatrix();
    }

    gfxLight(false);
    float pulse = 0.6f + 0.4f * sinf(tsec * 8.0f + e.phase);
    glColor3f(er, std::min(1.0f, eg + 0.2f * pulse), eb);
    glPushMatrix(); glTranslatef(0.0f, 0.32f, -0.9f); glutSolidSphere(0.22, 12, 8); glPopMatrix();
    for (int sd = -1; sd <= 1; sd += 2) {
        glPushMatrix(); glTranslatef(1.7f * sd, -0.05f, -1.05f); glutSolidSphere(0.14, 8, 6); glPopMatrix();
    }
    glColor3f(1.0f, 0.45f + 0.3f * pulse, 0.1f);
    glPushMatrix();
    glTranslatef(0, 0, 1.7f);
    glutSolidCone(0.28, 0.9f + 0.4f * pulse, 12, 4);
    glPopMatrix();
    gfxLight(true);
    setMat(0.0f, 0.0f);
}

void drawEnemies(float tsec) {
    for (const auto& e : enemies) {
        if (!e.active || behindCam(e.pos, e.radius * 5.0f)) continue;
        float yawE = atan2f(-e.fwd.x, -e.fwd.z);
        float pitchE = asinf(clampf(e.fwd.y, -1.0f, 1.0f));
        float bank = sinf(tsec * 1.5f + e.phase) * 0.25f;
        float sc = 1.3f;
        if (e.type == ET_BOSS) sc = 4.2f;
        else if (e.type == ET_GUNSHIP) sc = 2.0f;
        else if (e.type == ET_KAMIKAZE) sc = 1.0f;
        glPushMatrix();
        glTranslatef(e.pos.x, e.pos.y, e.pos.z);
        glRotatef(yawE * 180.0f / PI, 0, 1, 0);
        glRotatef(pitchE * 180.0f / PI, 1, 0, 0);
        glRotatef(bank * 180.0f / PI, 0, 0, 1);
        glScalef(sc, sc, sc);
        if (e.type == ET_KAMIKAZE) drawKamikazeModel(e, tsec);
        else                       drawEnemyModel(e, tsec);
        glPopMatrix();
    }
}

// v1.3: the wingman drone (solid body)
void drawDrone3D(float tsec) {
    if (droneTimer <= 0.0f || !shipVisible) return;
    gfxLight(true);
    setMat(0.9f, 80.0f);
    glPushMatrix();
    glTranslatef(dronePos.x, dronePos.y, dronePos.z);
    glRotatef(tsec * 160.0f, 0, 1, 0);
    glRotatef(tsec * 90.0f, 1, 0, 0);
    glColor3f(0.30f, 0.95f, 0.80f);
    glScalef(0.6f, 0.6f, 0.6f);
    glutSolidOctahedron();
    glPopMatrix();
    setMat(0.0f, 0.0f);
}

float bridgeActivity() {
    if (!bridgeOpen) return 0.0f;
    float d = length(bridge.pos - ship);
    return clampf(1.0f - d / GRAV_RADIUS, 0.0f, 1.0f);
}

void drawEffects(float tsec) {
    const Theme& th = curTheme();
    beginAdditive();

    for (const auto& c : coins) {
        if (c.taken || behindCam(c.pos, 4.0f)) continue;
        float bob = sinf(tsec * 2.0f + c.phase) * 0.3f;
        Vec3 cp = {c.pos.x, c.pos.y + bob, c.pos.z};
        glowDisc(cp, 2.4f, 1.0f, 0.85f, 0.2f, 0.40f);
        glowDisc(cp, 1.2f, 1.0f, 0.95f, 0.6f, 0.55f);
        float tw = 0.5f + 0.5f * sinf(tsec * 4.0f + c.phase);
        if (tw > 0.3f)
            drawStar(cp, 3.2f * tw, tsec * 30.0f + c.phase * 20.0f,
                     1.0f, 0.95f, 0.6f, 0.6f * tw);
    }

    for (const auto& h : hearts) {
        if (h.taken || behindCam(h.pos, 5.0f)) continue;
        float bob = sinf(tsec * 2.6f + h.phase) * 0.45f;
        Vec3 hp = {h.pos.x, h.pos.y + bob, h.pos.z};
        glowDisc(hp, 3.2f, 1.0f, 0.2f, 0.45f, 0.45f);
        drawStar(hp, 3.0f, tsec * 45.0f, 1.0f, 0.5f, 0.7f, 0.55f);
        drawBeam(h.pos, 32.0f, 0.3f, 1.0f, 0.2f, 0.45f, 0.30f);
    }

    // v1.2: power-up glows
    for (const auto& p : powerups) {
        if (p.taken || behindCam(p.pos, 5.0f)) continue;
        const float* c = POW_COL[p.type];
        float bob = sinf(tsec * 2.2f + p.phase) * 0.5f;
        Vec3 pp = {p.pos.x, p.pos.y + bob, p.pos.z};
        float pulse = 0.7f + 0.3f * sinf(tsec * 5.0f + p.phase);
        glowDisc(pp, 4.2f, c[0], c[1], c[2], 0.40f * pulse);
        glowDisc(pp, 1.8f, 1.0f, 1.0f, 1.0f, 0.30f);
        drawStar(pp, 3.6f, -tsec * 40.0f, c[0], c[1], c[2], 0.55f);
        drawBeam(p.pos, 22.0f, 0.25f, c[0], c[1], c[2], 0.30f);
    }

    for (const auto& b : beacons) {
        float act = b.active ? 1.0f : 0.0f;
        float cr = lerpf(1.0f, 0.2f, act), cg = lerpf(0.55f, 1.0f, act), cb = lerpf(0.10f, 0.5f, act);
        float pulse = 0.7f + 0.3f * sinf(tsec * 4.0f + b.anim);
        glowDisc(b.pos, 7.5f, cr, cg, cb, 0.35f * pulse);
        drawBeam(b.pos, 90.0f, 0.5f, cr, cg, cb, act ? 0.25f : 0.5f);
        glLineWidth(2.0f);
        for (int k = 0; k < 3; ++k) {
            float ph = fmodf(tsec * 0.5f + k / 3.0f + b.anim * 0.1f, 1.0f);
            float yy = ph * 60.0f;
            glColor4f(cr, cg, cb, (1.0f - ph) * 0.6f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 40; ++i) {
                float a = 2.0f * PI * i / 40.0f;
                glVertex3f(b.pos.x + 2.6f * cosf(a), b.pos.y + yy, b.pos.z + 2.6f * sinf(a));
            }
            glEnd();
        }
        glLineWidth(1.0f);
    }

    for (const auto& o : obstacles) {
        if (!o.active || behindCam(o.pos, o.orbitR + o.radius * 3.0f + 6.0f)) continue;
        if (o.type == OB_MINE) {
            float pulse = 0.5f + 0.5f * sinf(tsec * 6.0f + o.pos.x);
            glowDisc(o.pos, o.radius * 2.6f, 1.0f, 0.15f, 0.1f, 0.25f + 0.25f * pulse);
        } else if (o.type == OB_ORBITER) {
            glowDisc(o.pos, o.radius * 3.0f, 1.0f, 0.4f, 0.15f, 0.40f);
            glLineWidth(1.0f);
            glColor4f(1.0f, 0.4f, 0.2f, 0.16f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 64; ++i) {
                float a = 2.0f * PI * i / 64.0f;
                float c = cosf(a) * o.orbitR, s = sinf(a) * o.orbitR;
                Vec3 p;
                if (o.plane == 0)      p = o.center + Vec3{c, 0, s};
                else if (o.plane == 1) p = o.center + Vec3{c, s, 0};
                else                   p = o.center + Vec3{0, c, s};
                glVertex3f(p.x, p.y, p.z);
            }
            glEnd();
        }
    }

    float act = bridgeActivity();
    {
        float cr = lerpf(1.0f, th.accent[0], openAmt);
        float cg = lerpf(0.25f, th.accent[1], openAmt);
        float cb = lerpf(0.10f, th.accent[2], openAmt);
        glowDisc(bridge.pos, MOUTH_RADIUS * 2.8f, cr, cg, cb, 0.25f + 0.25f * openAmt + 0.3f * act);
        glowDisc(bridge.pos, MOUTH_RADIUS * 1.5f, cr, cg, cb, 0.25f);
        drawBeam(bridge.pos, 140.0f, 0.9f, cr, cg, cb, 0.25f + 0.3f * openAmt);
    }
    glowDisc(entry.pos, MOUTH_RADIUS * 2.2f, th.accent[0], th.accent[1], th.accent[2], 0.25f);
    drawPortalFx(bridge, tsec, openAmt, act);
    drawPortalFx(entry, tsec, 1.0f, 0.0f);

    {
        bool bo = boosting && state == S_FLYING;
        float wd = bo ? 0.38f : 0.22f;
        float cr = bo ? 0.85f : 0.30f;
        float cg = bo ? 0.95f : 0.70f;
        float cb = 1.0f;
        drawRibbon(trailL, wd, cr, cg, cb);
        drawRibbon(trailR, wd, cr, cg, cb);
        // Second layer (inner core glow)
        float wd2 = wd * 0.45f;
        drawRibbon(trailL, wd2, 0.85f, 0.95f, 1.0f);
        drawRibbon(trailR, wd2, 0.85f, 0.95f, 1.0f);
    }

    for (const auto& sh : shocks) {
        float pr = clampf(sh.t / sh.dur, 0.0f, 1.0f);
        float rad = sh.maxR * (1.0f - (1.0f - pr) * (1.0f - pr));
        float al = 1.0f - pr;
        glowDisc(sh.pos, rad * 0.8f, sh.r, sh.g, sh.b, 0.35f * al);
        beginBillboard(sh.pos);
        glLineWidth(1.0f + 3.5f * al);
        glColor4f(sh.r, sh.g, sh.b, 0.9f * al);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 64; ++i) {
            float an = 2.0f * PI * i / 64.0f;
            glVertex3f(rad * cosf(an), rad * sinf(an), 0.0f);
        }
        glEnd();
        glColor4f(sh.r, sh.g, sh.b, 0.5f * al);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 64; ++i) {
            float an = 2.0f * PI * i / 64.0f;
            glVertex3f(rad * 0.75f * cosf(an), rad * 0.75f * sinf(an), 0.0f);
        }
        glEnd();
        endBillboard();
    }
    glLineWidth(1.0f);

    for (const auto& e : enemies) {
        if (!e.active || behindCam(e.pos, e.radius * 6.0f)) continue;
        float pulse = 0.6f + 0.4f * sinf(tsec * 8.0f + e.phase);
        float gs = e.radius / 2.2f;
        float gr = 1.0f, gg = 0.2f, gb = 0.12f;
        if (e.type == ET_GUNSHIP) { gr = 0.3f; gg = 0.5f; gb = 1.0f; }
        if (e.type == ET_BOSS)    { gr = 0.8f; gg = 0.2f; gb = 1.0f; }
        glowDisc(e.pos, 5.0f * gs, gr, gg, gb, 0.20f + 0.15f * pulse);
        glowDisc(e.pos - e.fwd * (2.4f * gs), 2.6f * gs, 1.0f, 0.5f, 0.15f, 0.45f);
        if (e.type == ET_BOSS) glowDisc(e.pos, e.radius * 3.4f, 0.8f, 0.2f, 1.0f, 0.10f + 0.05f * e.stage);
    }

    // v1.3: fly-through rings
    for (const auto& r : rings) {
        if (r.taken) continue;
        float pulse = 0.7f + 0.3f * sinf(tsec * 4.0f + r.phase);
        glPushMatrix();
        glTranslatef(r.pos.x, r.pos.y, r.pos.z);
        alignZ(r.dir);
        glLineWidth(3.5f);
        glColor4f(0.3f, 1.0f, 1.0f, 0.85f * pulse);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 64; ++i) {
            float a = 2.0f * PI * i / 64.0f;
            glVertex3f(RING_RADIUS * cosf(a), RING_RADIUS * sinf(a), 0.0f);
        }
        glEnd();
        glLineWidth(1.5f);
        glColor4f(0.5f, 1.0f, 1.0f, 0.40f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 64; ++i) {
            float a = 2.0f * PI * i / 64.0f;
            glVertex3f(RING_RADIUS * 1.12f * cosf(a), RING_RADIUS * 1.12f * sinf(a), 0.0f);
        }
        glEnd();
        glBegin(GL_LINES);
        for (int k = 0; k < 12; ++k) {
            float a = k * PI / 6.0f + tsec * 1.2f;
            glVertex3f(RING_RADIUS * 0.90f * cosf(a), RING_RADIUS * 0.90f * sinf(a), 0.0f);
            glVertex3f(RING_RADIUS * 1.04f * cosf(a), RING_RADIUS * 1.04f * sinf(a), 0.0f);
        }
        glEnd();
        glPopMatrix();
        glowDisc(r.pos, RING_RADIUS * 1.6f, 0.2f, 0.9f, 1.0f, 0.08f * pulse);
    }
    glLineWidth(1.0f);

    // v1.3: ion storms
    {
        int slice = (int)(tsec * 7.0f);
        int ci = 0;
        for (const auto& c : clouds) {
            float pulse = 0.8f + 0.2f * sinf(tsec * 1.5f + c.phase);
            glowDisc(c.pos, c.radius * 1.15f, 0.45f, 0.25f, 1.0f, 0.16f * pulse);
            glowDisc(c.pos, c.radius * 0.75f, 0.30f, 0.60f, 1.0f, 0.14f);
            glowDisc(c.pos, c.radius * 0.35f, 0.80f, 0.90f, 1.0f, 0.12f);
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            for (int k = 0; k < 6; ++k) {
                unsigned int sd = (unsigned int)(slice * 131 + k * 17 + ci * 977);
                if (hash01(sd + 99u) < 0.45f) continue;
                Vec3 p0 = c.pos + Vec3{(hash01(sd) - 0.5f), (hash01(sd + 1u) - 0.5f), (hash01(sd + 2u) - 0.5f)} * (c.radius * 1.4f);
                Vec3 p1 = c.pos + Vec3{(hash01(sd + 3u) - 0.5f), (hash01(sd + 4u) - 0.5f), (hash01(sd + 5u) - 0.5f)} * (c.radius * 1.4f);
                glColor4f(0.7f, 0.9f, 1.0f, 0.8f);
                glVertex3f(p0.x, p0.y, p0.z);
                glColor4f(0.5f, 0.4f, 1.0f, 0.2f);
                glVertex3f(p1.x, p1.y, p1.z);
            }
            glEnd();
            glLineWidth(1.0f);
            ++ci;
        }
    }

    // v1.3: drone glow
    if (droneTimer > 0.0f && shipVisible) {
        glowDisc(dronePos, 2.4f, 0.3f, 1.0f, 0.8f, 0.55f);
        glowDisc(dronePos, 1.0f, 1.0f, 1.0f, 1.0f, 0.80f);
        drawStar(dronePos, 2.4f, tsec * 90.0f, 0.3f, 1.0f, 0.8f, 0.7f);
    }
    // v1.3: dash afterimage
    if (dashTimer > 0.0f && shipVisible) {
        float a = clampf(dashTimer / DASH_TIME, 0.0f, 1.0f);
        glowDisc(ship, 4.5f, 0.6f, 0.9f, 1.0f, 0.35f * a);
    }

    glLineWidth(4.0f);
    glBegin(GL_LINES);
    for (const auto& b : bolts) {
        float r = b.enemy ? 1.0f : 0.3f, g = b.enemy ? 0.3f : 0.9f, bl = b.enemy ? 0.15f : 1.0f;
        Vec3 tail = b.pos - normalize(b.vel) * (b.enemy ? 3.5f : 6.0f);
        glColor4f(r, g, bl, 0.0f);
        glVertex3f(tail.x, tail.y, tail.z);
        glColor4f(r, g, bl, 1.0f);
        glVertex3f(b.pos.x, b.pos.y, b.pos.z);
    }
    glEnd();
    // Inner bright core line
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (const auto& b : bolts) {
        float r = b.enemy ? 1.0f : 0.7f, g = b.enemy ? 0.7f : 0.95f, bl = b.enemy ? 0.5f : 1.0f;
        Vec3 tail = b.pos - normalize(b.vel) * (b.enemy ? 2.0f : 3.5f);
        glColor4f(r, g, bl, 0.0f);
        glVertex3f(tail.x, tail.y, tail.z);
        glColor4f(r, g, bl, 0.9f);
        glVertex3f(b.pos.x, b.pos.y, b.pos.z);
    }
    glEnd();
    glLineWidth(1.0f);
    for (const auto& b : bolts) {
        if (b.enemy) glowDisc(b.pos, 2.2f, 1.0f, 0.3f, 0.15f, 0.75f);
        else         glowDisc(b.pos, 1.8f, 0.3f, 0.9f, 1.0f, 0.75f);
    }

    // v1.2: homing missiles (fiery streak + hot core)
    glLineWidth(5.0f);
    glBegin(GL_LINES);
    for (const auto& m : missiles) {
        Vec3 tail = m.pos - normalize(m.vel) * 9.0f;
        glColor4f(1.0f, 0.45f, 0.1f, 0.0f);
        glVertex3f(tail.x, tail.y, tail.z);
        glColor4f(1.0f, 0.75f, 0.35f, 1.0f);
        glVertex3f(m.pos.x, m.pos.y, m.pos.z);
    }
    glEnd();
    glLineWidth(1.0f);
    for (const auto& m : missiles) {
        glowDisc(m.pos, 3.2f, 1.0f, 0.5f, 0.15f, 0.7f);
        glowDisc(m.pos, 1.3f, 1.0f, 1.0f, 0.85f, 0.95f);
    }

    if (shipVisible) {
        Vec3 f = forwardVec();
        float k = (boosting && state == S_FLYING) ? 2.0f : 1.0f;
        float spK = clampf(fabsf(speed) * 2.0f, 0.3f, 1.0f);
        glowDisc(ship - f * (1.9f * shipScale), 2.2f * shipScale * k * spK, 0.15f, 0.55f, 1.0f, 0.55f);
        glowDisc(ship - f * (1.9f * shipScale), 1.0f * shipScale * k * spK, 0.7f, 0.9f, 1.0f, 0.40f);
        // Wing tip glows
        Vec3 lwing = shipLocalToWorld({-2.0f, 0.05f, 0.85f});
        Vec3 rwing = shipLocalToWorld({ 2.0f, 0.05f, 0.85f});
        float pulse = 0.6f + 0.4f * sinf(tsec * 4.0f);
        glowDisc(lwing, 0.7f * shipScale, 1.0f, 0.15f, 0.12f, 0.4f * pulse);
        glowDisc(rwing, 0.7f * shipScale, 0.1f, 0.8f, 0.3f, 0.4f * pulse);
    }

    if (shipVisible && invuln > 0.0f) {
        float fade = clampf(invuln, 0.0f, 1.0f);
        fade *= clampf((length(camPos - ship) - 3.0f) / 8.0f, 0.25f, 1.0f);
        float pulse = 0.6f + 0.4f * sinf(tsec * 14.0f);
        glPushMatrix();
        glTranslatef(ship.x, ship.y, ship.z);
        glColor4f(0.3f, 0.8f, 1.0f, (0.07f + 0.07f * pulse) * fade);
        glutSolidSphere(2.7, 28, 18);
        glColor4f(0.5f, 0.95f, 1.0f, 0.35f * fade);
        glutWireSphere(2.72, 14, 10);
        glPopMatrix();
    }

    // v1.2: shield power-up bubble (flickers when about to expire)
    if (shipVisible && shieldTimer > 0.0f) {
        float flick = (shieldTimer < 4.0f) ? (0.5f + 0.5f * sinf(tsec * 22.0f)) : 1.0f;
        float fade = clampf((length(camPos - ship) - 3.0f) / 8.0f, 0.25f, 1.0f) * flick;
        float pulse = 0.6f + 0.4f * sinf(tsec * 5.0f);
        glPushMatrix();
        glTranslatef(ship.x, ship.y, ship.z);
        glColor4f(0.35f, 0.9f, 1.0f, (0.06f + 0.05f * pulse) * fade);
        glutSolidSphere(3.1, 28, 18);
        glColor4f(0.6f, 1.0f, 1.0f, 0.45f * fade);
        glutWireSphere(3.12, 16, 12);
        glPopMatrix();
    }

    // Particle glow halos (larger, fainter)
    gfxPoints(true);
    glPointSize(11.0f);
    glBegin(GL_POINTS);
    for (const auto& p : particles) {
        float b = clampf(p.life, 0.0f, 1.0f);
        if (b < 0.2f) continue;
        glColor4f(p.r, p.g, p.b, b * 0.18f);
        glVertex3f(p.pos.x, p.pos.y, p.pos.z);
    }
    glEnd();

    // Core particle points
    glPointSize(5.5f);
    glBegin(GL_POINTS);
    for (const auto& p : particles) {
        float b = clampf(p.life, 0.0f, 1.0f);
        glColor4f(p.r, p.g, p.b, b);
        glVertex3f(p.pos.x, p.pos.y, p.pos.z);
    }
    glEnd();
    gfxPoints(false);

    endAdditive();
}

void setupCamera(float tsec) {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    Vec3 cam, look;
    Vec3 up = {0.0f, 1.0f, 0.0f};
    if (state == S_MENU || state == S_GAMEOVER || state == S_VICTORY) {
        Vec3 focus = ship;
        float rad = 20.0f;
        if (state == S_GAMEOVER) { focus = deathPos; rad = 26.0f; }
        if (state == S_VICTORY)  { focus = bridge.pos; rad = 45.0f; }
        float a = tsec * 0.25f + camYawOff;
        cam = focus + Vec3{cosf(a) * rad, 6.0f + 2.0f * sinf(tsec * 0.3f) + camPitchOff * 8.0f, sinf(a) * rad};
        look = focus;
    } else {
        cam = camCur;
        float sh = (camShake * camShake * 0.8f + nearIntensity * nearIntensity * 0.25f) * cfgShake;
        cam += Vec3{(frand() - 0.5f) * sh, (frand() - 0.5f) * sh, (frand() - 0.5f) * sh};
        Vec3 f = forwardVec();
        look = ship + f * (aiming ? 14.0f : 6.0f);

        if (state == S_FLYING) {
            Vec3 rightV = normalize(cross(f, Vec3{0.0f, 1.0f, 0.0f}));
            Vec3 upC = cross(rightV, f);
            float a = sinf(roll) * 0.35f;
            up = upC * cosf(a) - rightV * sinf(a);
        }
    }
    camPos = cam;
    gluLookAt(cam.x, cam.y, cam.z, look.x, look.y, look.z, up.x, up.y, up.z);
    captureView();
}

void drawWorld(float tsec) {
    const Theme& th = curTheme();
    setProjection();
    setupCamera(tsec);
    setLight();

    glGetDoublev(GL_MODELVIEW_MATRIX, hudMV);
    glGetDoublev(GL_PROJECTION_MATRIX, hudProj);
    hudVP[0] = 0; hudVP[1] = 0; hudVP[2] = curWinW; hudVP[3] = curWinH;   // HUD projects to the full window even when the scene is rendered at a lower scale

    GLfloat fogCol[] = {th.nebA[0] * 0.03f, th.nebA[1] * 0.03f, th.nebA[2] * 0.05f, 1.0f};
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogCol);
    glFogf(GL_FOG_START, 180.0f);
    glFogf(GL_FOG_END, 650.0f);

    if (gfxOK) drawSkyShader(tsec); else drawSky(tsec);
    drawCenterpiece(tsec);
    drawPlanets(tsec);

    glEnable(GL_FOG);
    drawDust();
    drawBeacons(tsec);
    matBegin(1.0f, 0.22f, 0);  drawCoins(tsec);      matEnd();
    matBegin(0.0f, 0.10f, 0);  drawHearts3D(tsec);   matEnd();
    matBegin(0.35f, 0.15f, 0); drawPowerUps3D(tsec); matEnd();
    drawObstacles(tsec);
    matBegin(0.55f, 0.40f, 3); drawEnemies(tsec);    matEnd();
    drawPortalBody(entry, tsec, 0.0f, 1.0f);
    drawPortalBody(bridge, tsec, bridgeActivity(), openAmt);
    matBegin(0.90f, 0.30f, 3); drawShipWorld(tsec); matEnd();
    matBegin(0.80f, 0.25f, 0); drawDrone3D(tsec);   matEnd();
    drawEffects(tsec);
    glDisable(GL_FOG);
}

// ----------------------------------------------------------------------------
//  Inside the bridge
// ----------------------------------------------------------------------------
static float ringRadius(int g, float tsec) { return 4.5f + 0.9f * sinf(g * 0.45f + tsec * 1.5f); }
static float ringTwist(int g, float tsec)  { return g * 0.18f + tsec * 0.4f; }

void drawTransit(float tsec) {
    const Theme& th = curTheme();
    const float* ac = th.accent;

    setProjection();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0, 0, 0, 0, -1, 0, 1, 0);
    captureView();
    GLfloat lpos[] = {0.0f, 0.6f, 1.0f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);

    float scroll = 50.0f * stateTimer + 6.0f * stateTimer * stateTimer;
    const float SP = 3.0f;
    const int RINGS = 45, SEG = 48, LINES = 16;
    int k0 = (int)floorf(scroll / SP);
    float frac = scroll - k0 * SP;

    camPos = {0, 0, 0};
    beginAdditive();
    glowDisc({0.0f, 0.0f, -120.0f}, 75.0f, ac[0], ac[1], ac[2], 0.55f);
    glowDisc({0.0f, 0.0f, -120.0f}, 30.0f, 1.0f, 1.0f, 1.0f, 0.75f);
    for (int i = 0; i < RINGS - 1; ++i) {
        int g0 = i + k0, g1 = g0 + 1;
        float z0 = -i * SP + frac, z1 = z0 - SP;
        float r0 = ringRadius(g0, tsec), r1 = ringRadius(g1, tsec);
        float t0 = ringTwist(g0, tsec), t1 = ringTwist(g1, tsec);
        float f0 = 1.0f - (float)i / RINGS, f1 = 1.0f - (float)(i + 1) / RINGS;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= SEG; ++j) {
            float band = 0.5f + 0.5f * sinf(2.0f * PI * j * 4.0f / SEG + g0 * 0.8f + tsec * 2.0f);
            float a0 = 2.0f * PI * j / SEG + t0, a1 = 2.0f * PI * j / SEG + t1;
            glColor4f(ac[0], ac[1], ac[2], 0.14f * f0 * (0.25f + band));
            glVertex3f(r0 * cosf(a0), r0 * sinf(a0), z0);
            glColor4f(ac[0], ac[1], ac[2], 0.14f * f1 * (0.25f + band));
            glVertex3f(r1 * cosf(a1), r1 * sinf(a1), z1);
        }
        glEnd();
    }
    endAdditive();

    gfxLight(false);

    glLineWidth(1.5f);
    for (int i = 0; i < RINGS; ++i) {
        int g = i + k0;
        float z = -i * SP + frac;
        float fade = 1.0f - (float)i / RINGS;
        float r = ringRadius(g, tsec);
        float tw = ringTwist(g, tsec);
        float hue = 0.5f + 0.5f * sinf(g * 0.3f);
        glColor3f(ac[0] * (0.4f + 0.6f * hue) * fade, ac[1] * 0.7f * fade, ac[2] * (1.0f - 0.4f * hue) * fade);
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j < SEG; ++j) {
            float a = 2.0f * PI * j / SEG + tw;
            glVertex3f(r * cosf(a), r * sinf(a), z);
        }
        glEnd();
    }

    for (int j = 0; j < LINES; ++j) {
        float a0 = 2.0f * PI * j / LINES;
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < RINGS; ++i) {
            int g = i + k0;
            float z = -i * SP + frac;
            float fade = 1.0f - (float)i / RINGS;
            float r = ringRadius(g, tsec);
            float a = a0 + ringTwist(g, tsec);
            glColor3f(ac[0] * 0.5f * fade, ac[1] * 0.7f * fade, ac[2] * 0.9f * fade);
            glVertex3f(r * cosf(a), r * sinf(a), z);
        }
        glEnd();
    }

    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int s2 = 0; s2 < NUM_STREAKS; ++s2) {
        float a = streak[s2][0] * 2.0f * PI;
        float rr = 0.6f + streak[s2][1] * 2.8f;
        float zz = -150.0f + fmodf(streak[s2][2] * 150.0f + scroll * 1.8f, 150.0f);
        float b = clampf(1.0f - fabsf(zz) / 150.0f, 0.0f, 1.0f);
        glColor3f(0.5f * b + ac[0] * 0.3f * b, 0.6f * b, 1.0f * b);
        glVertex3f(rr * cosf(a), rr * sinf(a), zz);
        glVertex3f(rr * cosf(a), rr * sinf(a), zz - 3.0f);
    }
    glEnd();

    gfxLight(true);
    glPushMatrix();
    glTranslatef(0.25f * sinf(tsec * 1.3f), -0.9f + 0.15f * sinf(tsec * 2.1f), -7.0f);
    glRotatef(sinf(tsec * 0.9f) * 12.0f, 0, 0, 1);
    glScalef(0.9f, 0.9f, 0.9f);
    matBegin(0.9f, 0.30f, 3);
    drawShipModel(tsec, 1.8f);
    matEnd();
    glPopMatrix();

    beginAdditive();
    glowDisc({0.0f, -0.9f, -8.8f}, 2.0f, 0.3f, 0.7f, 1.0f, 0.5f);
    endAdditive();
}

// ----------------------------------------------------------------------------
//  HUD
// ----------------------------------------------------------------------------
void beginHUD(int w, int h) {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT | GL_POINT_BIT | GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    gfxLight(false);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void endHUD() {
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

void drawText(float x, float y, const char* s, void* font, float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
    glRasterPos2f(x, y);
    for (const char* p = s; *p; ++p) glutBitmapCharacter(font, *p);
}

float textWidth(void* font, const char* s) {
    return (float)glutBitmapLength(font, (const unsigned char*)s);
}

void drawTextC(float cx, float y, const char* s, void* font, float r, float g, float b, float a = 1.0f) {
    drawText(cx - textWidth(font, s) * 0.5f, y, s, font, r, g, b, a);
}

void drawStrokeC(float cx, float cy, const char* s, float maxW, float maxScale,
                 float r, float g, float b, float a, float lw) {
    float len = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)s);
    float sc = std::min(maxScale, maxW / std::max(1.0f, len));
    glPushMatrix();
    glTranslatef(cx - len * sc * 0.5f, cy, 0.0f);
    glScalef(sc, sc, 1.0f);
    glLineWidth(lw);
    glEnable(GL_LINE_SMOOTH);
    glColor4f(r, g, b, a);
    for (const char* p = s; *p; ++p) glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
    glPopMatrix();
}

void drawPanel(float x0, float y0, float x1, float y1, float a, const float* ac) {
    glBegin(GL_QUADS);
    glColor4f(0.03f, 0.07f, 0.13f, a * 1.2f);
    glVertex2f(x0, y1); glVertex2f(x1, y1);
    glColor4f(0.00f, 0.01f, 0.04f, a * 0.7f);
    glVertex2f(x1, y0); glVertex2f(x0, y0);
    glEnd();

    glLineWidth(1.0f);
    glColor4f(ac[0], ac[1], ac[2], 0.25f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x0, y0); glVertex2f(x1, y0); glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();

    glLineWidth(2.5f);
    glColor4f(ac[0], ac[1], ac[2], 0.95f);
    float L = 14.0f;
    glBegin(GL_LINES);
    glVertex2f(x0, y1); glVertex2f(x1, y1);
    glVertex2f(x0, y0); glVertex2f(x0 + L, y0);
    glVertex2f(x0, y0); glVertex2f(x0, y0 + L);
    glVertex2f(x1, y0); glVertex2f(x1 - L, y0);
    glVertex2f(x1, y0); glVertex2f(x1, y0 + L);
    glEnd();
    glLineWidth(1.0f);
}

void drawGlowBar(float cx, float y, float half, float r, float g, float b, float a) {
    glBegin(GL_QUADS);
    glColor4f(r, g, b, 0.0f); glVertex2f(cx - half, y - 1.5f); glVertex2f(cx - half, y + 1.5f);
    glColor4f(r, g, b, a);    glVertex2f(cx, y + 1.5f);        glVertex2f(cx, y - 1.5f);
    glColor4f(r, g, b, a);    glVertex2f(cx, y - 1.5f);        glVertex2f(cx, y + 1.5f);
    glColor4f(r, g, b, 0.0f); glVertex2f(cx + half, y + 1.5f); glVertex2f(cx + half, y - 1.5f);
    glEnd();
}

void drawVignette(int w, int h, float tsec) {
    if (gfxOK && postOn) {
        // the HDR post pass already does vignette + grain; keep only the low-hull warning pulse
        if (state == S_FLYING && health == 1) {
            float m = std::min(w, h) * 0.30f, W = (float)w, H = (float)h;
            float pa = 0.22f + 0.16f * sinf(tsec * 5.0f);
            glBegin(GL_QUADS);
            glColor4f(0.9f, 0.05f, 0.05f, pa); glVertex2f(0, 0); glVertex2f(0, H);
            glColor4f(0.9f, 0.05f, 0.05f, 0.0f); glVertex2f(m, H); glVertex2f(m, 0);
            glColor4f(0.9f, 0.05f, 0.05f, pa); glVertex2f(W, 0); glVertex2f(W, H);
            glColor4f(0.9f, 0.05f, 0.05f, 0.0f); glVertex2f(W - m, H); glVertex2f(W - m, 0);
            glColor4f(0.9f, 0.05f, 0.05f, pa); glVertex2f(0, 0); glVertex2f(W, 0);
            glColor4f(0.9f, 0.05f, 0.05f, 0.0f); glVertex2f(W, m); glVertex2f(0, m);
            glColor4f(0.9f, 0.05f, 0.05f, pa); glVertex2f(0, H); glVertex2f(W, H);
            glColor4f(0.9f, 0.05f, 0.05f, 0.0f); glVertex2f(W, H - m); glVertex2f(0, H - m);
            glEnd();
        }
        return;
    }
    const Theme& th = curTheme();
    float m = std::min(w, h) * 0.30f;
    float a = 0.60f;
    float W = (float)w, H = (float)h;

    // Dark vignette
    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, a); glVertex2f(0, 0); glVertex2f(0, H);
    glColor4f(0, 0, 0, 0); glVertex2f(m, H); glVertex2f(m, 0);
    glColor4f(0, 0, 0, a); glVertex2f(W, 0); glVertex2f(W, H);
    glColor4f(0, 0, 0, 0); glVertex2f(W - m, H); glVertex2f(W - m, 0);
    glColor4f(0, 0, 0, a); glVertex2f(0, 0); glVertex2f(W, 0);
    glColor4f(0, 0, 0, 0); glVertex2f(W, m); glVertex2f(0, m);
    glColor4f(0, 0, 0, a); glVertex2f(0, H); glVertex2f(W, H);
    glColor4f(0, 0, 0, 0); glVertex2f(W, H - m); glVertex2f(0, H - m);
    glEnd();

    // Subtle themed color vignette (adds atmosphere)
    float cm = m * 0.7f;
    float ta = 0.12f;
    glBegin(GL_QUADS);
    glColor4f(th.accent[0], th.accent[1], th.accent[2], ta);
    glVertex2f(0, 0); glVertex2f(0, H);
    glColor4f(th.accent[0], th.accent[1], th.accent[2], 0);
    glVertex2f(cm, H); glVertex2f(cm, 0);

    glColor4f(th.accent[0], th.accent[1], th.accent[2], ta);
    glVertex2f(W, 0); glVertex2f(W, H);
    glColor4f(th.accent[0], th.accent[1], th.accent[2], 0);
    glVertex2f(W - cm, H); glVertex2f(W - cm, 0);
    glEnd();

    // Low-health red pulse
    if (state == S_FLYING && health == 1) {
        float pa = 0.22f + 0.16f * sinf(tsec * 5.0f);
        glBegin(GL_QUADS);
        glColor4f(0.9f, 0.05f, 0.05f, pa); glVertex2f(0, 0); glVertex2f(0, H);
        glColor4f(0.9f, 0.05f, 0.05f, 0.0f); glVertex2f(m, H); glVertex2f(m, 0);
        glColor4f(0.9f, 0.05f, 0.05f, pa); glVertex2f(W, 0); glVertex2f(W, H);
        glColor4f(0.9f, 0.05f, 0.05f, 0.0f); glVertex2f(W - m, H); glVertex2f(W - m, 0);
        glColor4f(0.9f, 0.05f, 0.05f, pa); glVertex2f(0, 0); glVertex2f(W, 0);
        glColor4f(0.9f, 0.05f, 0.05f, 0.0f); glVertex2f(W, m); glVertex2f(0, m);
        glColor4f(0.9f, 0.05f, 0.05f, pa); glVertex2f(0, H); glVertex2f(W, H);
        glColor4f(0.9f, 0.05f, 0.05f, 0.0f); glVertex2f(W, H - m); glVertex2f(0, H - m);
        glEnd();
    }

    // Subtle film grain overlay (cinematic look)
    glPointSize(1.0f);
    glBegin(GL_POINTS);
    unsigned int grainSeed = (unsigned int)(tsec * 60.0f);
    for (int i = 0; i < 800; ++i) {
        grainSeed = grainSeed * 1103515245u + 12345u;
        float gx = (float)(grainSeed % (unsigned int)w);
        grainSeed = grainSeed * 1103515245u + 12345u;
        float gy = (float)(grainSeed % (unsigned int)h);
        grainSeed = grainSeed * 1103515245u + 12345u;
        float ga = (float)(grainSeed & 0xFF) / 255.0f;
        glColor4f(0.85f, 0.88f, 0.92f, ga * 0.035f);
        glVertex2f(gx, gy);
    }
    glEnd();

    // Subtle horizontal scanlines (sci-fi CRT feel)
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int sy = 0; sy < h; sy += 4) {
        glColor4f(0.0f, 0.0f, 0.0f, 0.04f);
        glVertex2f(0.0f, (float)sy);
        glVertex2f(W, (float)sy);
    }
    glEnd();
}

void drawHeart(float cx, float cy, float size, bool filled, float r, float g, float b, float a) {
    float s = size / 17.0f;
    const int N = 48;
    if (filled) {
        glColor4f(r, g, b, a);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= N; ++i) {
            float t = 2.0f * PI * i / N;
            float sn = sinf(t);
            glVertex2f(cx + 16.0f * sn * sn * sn * s,
                       cy + (13.0f * cosf(t) - 5.0f * cosf(2 * t) - 2.0f * cosf(3 * t) - cosf(4 * t)) * s);
        }
        glEnd();
    }
    glLineWidth(2.0f);
    glColor4f(1.0f, 0.85f, 0.85f, a);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < N; ++i) {
        float t = 2.0f * PI * i / N;
        float sn = sinf(t);
        glVertex2f(cx + 16.0f * sn * sn * sn * s,
                   cy + (13.0f * cosf(t) - 5.0f * cosf(2 * t) - 2.0f * cosf(3 * t) - cosf(4 * t)) * s);
    }
    glEnd();
}

void radarBlip(float cx, float cy, float R, float range, Vec3 p, bool clampEdge,
               float size, float r, float g, float b, float a) {
    Vec3 fwdH = {-sinf(yaw), 0.0f, -cosf(yaw)};
    Vec3 right = {cosf(yaw), 0.0f, -sinf(yaw)};
    Vec3 rel = p - ship;
    float x = dot(rel, right), y = dot(rel, fwdH);
    float k = R / range;
    float px = x * k, py = y * k;
    float dr = sqrtf(px * px + py * py);
    if (dr > R - 4.0f) {
        if (!clampEdge) return;
        px *= (R - 4.0f) / dr;
        py *= (R - 4.0f) / dr;
    }
    glPointSize(size);
    glColor4f(r, g, b, a);
    glBegin(GL_POINTS);
    glVertex2f(cx + px, cy + py);
    glEnd();
}

void drawRadar(int w, int /*h*/, float tsec) {
    const Theme& th = curTheme();
    float R = 78.0f, cx = w - R - 22.0f, cy = R + 22.0f;
    const float range = 150.0f;

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.0f, 0.04f, 0.08f, 0.55f);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 48; ++i) {
        float a = 2.0f * PI * i / 48.0f;
        glVertex2f(cx + R * cosf(a), cy + R * sinf(a));
    }
    glEnd();

    glLineWidth(1.5f);
    glColor4f(th.accent[0], th.accent[1], th.accent[2], 0.7f);
    for (int ring = 1; ring <= 2; ++ring) {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 48; ++i) {
            float a = 2.0f * PI * i / 48.0f;
            glVertex2f(cx + R * ring * 0.5f * cosf(a), cy + R * ring * 0.5f * sinf(a));
        }
        glEnd();
    }
    glBegin(GL_LINES);
    glColor4f(th.accent[0], th.accent[1], th.accent[2], 0.25f);
    glVertex2f(cx - R, cy); glVertex2f(cx + R, cy);
    glVertex2f(cx, cy - R); glVertex2f(cx, cy + R);
    float sw = tsec * 2.0f;
    glColor4f(th.accent[0], th.accent[1], th.accent[2], 0.6f);
    glVertex2f(cx, cy); glVertex2f(cx + R * cosf(sw), cy + R * sinf(sw));
    glEnd();

    // v1.3: ion storms jam the radar (blips flicker, static appears)
    bool jam = inIon;
    auto show = [&]() -> bool { return !jam || frand() < 0.15f; };
    for (const auto& c : clouds)
        radarBlip(cx, cy, R, range, c.pos, false, c.radius * 2.0f * R / range, 0.5f, 0.3f, 1.0f, 0.22f);
    for (const auto& r : rings)
        if (!r.taken && show()) radarBlip(cx, cy, R, range, r.pos, false, 4.0f, 0.3f, 1.0f, 1.0f, 0.9f);
    for (const auto& o : obstacles)
        if (o.active && show()) {
            if (o.type == OB_MINE) radarBlip(cx, cy, R, range, o.pos, false, 4.0f, 1.0f, 0.1f, 0.1f, 0.95f);
            else                   radarBlip(cx, cy, R, range, o.pos, false, 3.0f, 1.0f, 0.25f, 0.2f, 0.9f);
        }
    for (const auto& e : enemies)
        if (e.active && show()) radarBlip(cx, cy, R, range, e.pos, true, e.type == ET_BOSS ? 12.0f : 7.0f, 1.0f, 0.1f, 0.6f, 1.0f);
    for (const auto& c : coins)
        if (!c.taken && show()) radarBlip(cx, cy, R, range, c.pos, false, 3.0f, 1.0f, 0.85f, 0.2f, 1.0f);
    for (const auto& h : hearts)
        if (!h.taken && show()) radarBlip(cx, cy, R, range, h.pos, true, 7.0f, 1.0f, 0.25f, 0.55f, 1.0f);
    for (const auto& p : powerups)
        if (!p.taken && show()) radarBlip(cx, cy, R, range, p.pos, true, 6.0f, POW_COL[p.type][0], POW_COL[p.type][1], POW_COL[p.type][2], 1.0f);
    for (const auto& b : beacons)
        if (!b.active && show()) radarBlip(cx, cy, R, range, b.pos, true, 8.0f, 1.0f, 0.6f, 0.1f, 1.0f);
    float pulse = 0.6f + 0.4f * sinf(tsec * 6.0f);
    if (bridgeOpen) radarBlip(cx, cy, R, range, bridge.pos, true, 11.0f, 0.5f, 0.9f, 1.0f, pulse);
    else            radarBlip(cx, cy, R, range, bridge.pos, true, 8.0f, 1.0f, 0.3f, 0.2f, 0.6f);
    if (jam) {
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        for (int i = 0; i < 70; ++i) {
            float a = frand() * 2.0f * PI, rr = R * sqrtf(frand());
            glColor4f(0.7f, 0.7f, 1.0f, 0.5f);
            glVertex2f(cx + rr * cosf(a), cy + rr * sinf(a));
        }
        glEnd();
    }

    glColor4f(1, 1, 1, 1);
    glBegin(GL_TRIANGLES);
    glVertex2f(cx, cy + 7); glVertex2f(cx - 5, cy - 5); glVertex2f(cx + 5, cy - 5);
    glEnd();

    drawTextC(cx, cy + R + 8, "RADAR", GLUT_BITMAP_9_BY_15, th.accent[0], th.accent[1], th.accent[2]);
}

// v1.2: energy / missiles / power-up status panel (bottom-left)
void drawSystemsPanel(float tsec) {
    const Theme& th = curTheme();
    const float* ac = th.accent;
    void* F = GLUT_BITMAP_9_BY_15;
    char buf[64];

    float px0 = 8.0f, py0 = 36.0f, px1 = 316.0f, py1 = 36.0f + 205.0f;
    drawPanel(px0, py0, px1, py1, 0.45f, ac);
    float ty = py1 - 26.0f;

    // energy bar
    drawText(18, ty, "ENERGY", F, 0.8f, 0.9f, 1.0f);
    float bx0 = 100.0f, bx1 = 304.0f;
    glColor4f(0.05f, 0.1f, 0.15f, 0.8f);
    glRectf(bx0, ty - 2.0f, bx1, ty + 10.0f);
    float ef = clampf(energy / maxEnergyVal(), 0.0f, 1.0f);
    if (energyLocked) {
        float bl = 0.5f + 0.5f * sinf(tsec * 12.0f);
        glColor4f(1.0f, 0.25f, 0.2f, 0.5f + 0.5f * bl);
    } else if (inIon) {
        glColor4f(0.7f, 0.4f, 1.0f, 0.95f);
    } else if (boosting) {
        glColor4f(1.0f, 0.65f, 0.2f, 0.95f);
    } else {
        glColor4f(ac[0], ac[1], ac[2], 0.95f);
    }
    glRectf(bx0, ty - 2.0f, bx0 + (bx1 - bx0) * ef, ty + 10.0f);
    if (energyLocked) drawText(bx0 + 4.0f, ty - 1.0f, "RECHARGING", F, 1.0f, 0.9f, 0.9f);
    else if (dashTimer > 0.0f) drawText(bx0 + 4.0f, ty - 1.0f, "EVADE!", F, 1.0f, 1.0f, 1.0f);

    // missile icons
    ty -= 26.0f;
    drawText(18, ty, "MISSILES", F, 0.8f, 0.9f, 1.0f);
    int mm = maxMissilesVal();
    float msp = std::min(32.0f, 196.0f / (float)mm);
    for (int i = 0; i < mm; ++i) {
        float mx = 104.0f + i * msp;
        bool have = i < missileAmmo;
        if (have) glColor4f(1.0f, 0.55f, 0.15f, 0.95f);
        else      glColor4f(0.3f, 0.3f, 0.35f, 0.45f);
        glBegin(GL_TRIANGLES);
        glVertex2f(mx, ty + 14.0f); glVertex2f(mx - 6.0f, ty - 3.0f); glVertex2f(mx + 6.0f, ty - 3.0f);
        glEnd();
        glRectf(mx - 3.0f, ty - 7.0f, mx + 3.0f, ty - 3.0f);
    }

    // pulse bomb icons
    ty -= 26.0f;
    drawText(18, ty, "BOMBS [V]", F, 0.8f, 0.9f, 1.0f);
    int mb = maxBombsVal();
    for (int i = 0; i < mb; ++i) {
        float cx = 120.0f + i * 28.0f;
        bool have = i < bombAmmo;
        if (have) glColor4f(1.0f, 0.3f, 0.6f, 0.95f);
        else      glColor4f(0.3f, 0.3f, 0.35f, 0.45f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, ty + 4.0f);
        for (int k = 0; k <= 16; ++k) {
            float a = 2.0f * PI * k / 16.0f;
            glVertex2f(cx + 7.0f * cosf(a), ty + 4.0f + 7.0f * sinf(a));
        }
        glEnd();
    }

    // active power-up timers
    ty -= 26.0f;
    struct Row { int type; float t; };
    Row rows[5] = { {PW_SHIELD, shieldTimer}, {PW_RAPID, rapidTimer}, {PW_MAGNET, magnetTimer},
                    {PW_DOUBLE, doubleTimer}, {PW_DRONE, droneTimer} };
    for (int i = 0; i < 5; ++i) {
        if (rows[i].t <= 0.0f) continue;
        const float* c = POW_COL[rows[i].type];
        float a = (rows[i].t < 3.0f) ? (0.55f + 0.45f * sinf(tsec * 14.0f)) : 1.0f;
        if (rows[i].type == PW_SHIELD) snprintf(buf, sizeof(buf), "%-10s %4.1fs (1 hit)", POW_NAMES[rows[i].type], rows[i].t);
        else                           snprintf(buf, sizeof(buf), "%-10s %4.1fs", POW_NAMES[rows[i].type], rows[i].t);
        drawText(18, ty, buf, F, c[0], c[1], c[2], a);
        ty -= 18.0f;
    }
}

void drawPlayHUD(int w, int h, float tsec) {
    const Theme& th = curTheme();
    const float* ac = th.accent;
    char buf[160];
    void* F = GLUT_BITMAP_9_BY_15;
    void* FL = GLUT_BITMAP_HELVETICA_18;

    if (state == S_TRANSIT) {
        snprintf(buf, sizeof(buf), "TRAVERSING EINSTEIN-ROSEN BRIDGE   %3.0f%%",
                 clampf(stateTimer / TRANSIT_TIME, 0.0f, 1.0f) * 100.0f);
        drawTextC(w * 0.5f, h - 90, buf, FL, 0.75f, 0.9f, 1.0f);
        if (level < MAX_LEVEL) snprintf(buf, sizeof(buf), "Destination: SECTOR %d - %s", level + 1, themes[level].name);
        else snprintf(buf, sizeof(buf), "Destination: THE OTHER SIDE");
        drawTextC(w * 0.5f, h - 118, buf, F, 0.6f, 0.8f, 1.0f);
        return;
    }

    // ---- objectives panel ----
    drawPanel(8, h - 236, 372, h - 8, 0.45f, ac);
    float y = h - 30;
    snprintf(buf, sizeof(buf), "SECTOR %d/%d  -  %s", level, MAX_LEVEL, th.name);
    drawText(18, y, buf, FL, ac[0], ac[1], ac[2]);
    y -= 26;
    snprintf(buf, sizeof(buf), "SCORE  %07d", (int)(dispScore + 0.5f));
    drawText(18, y, buf, FL, 1.0f, 0.92f, 0.5f);
    y -= 26;
    drawText(18, y, "OBJECTIVES", F, 0.7f, 0.8f, 0.9f);
    y -= 22;

    bool coinsDone = coinsGot >= coinsReq;
    snprintf(buf, sizeof(buf), "%s Collect coins      %2d / %2d", coinsDone ? "[X]" : "[ ]", coinsGot, coinsReq);
    drawText(18, y, buf, F, coinsDone ? 0.4f : 1.0f, coinsDone ? 1.0f : 1.0f, coinsDone ? 0.5f : 1.0f);
    y -= 20;
    bool beaconsDone = beaconsGot >= (int)beacons.size();
    snprintf(buf, sizeof(buf), "%s Activate beacons   %2d / %2d", beaconsDone ? "[X]" : "[ ]", beaconsGot, (int)beacons.size());
    drawText(18, y, buf, F, beaconsDone ? 0.4f : 1.0f, beaconsDone ? 1.0f : 1.0f, beaconsDone ? 0.5f : 1.0f);
    y -= 20;
    bool enemiesDone = enemiesKilled >= enemiesReq;
    snprintf(buf, sizeof(buf), "%s Destroy enemy ships %2d / %2d", enemiesDone ? "[X]" : "[ ]", enemiesKilled, enemiesReq);
    drawText(18, y, buf, F, enemiesDone ? 0.4f : 1.0f, 1.0f, enemiesDone ? 0.5f : 1.0f);
    y -= 20;
    if (bridgeOpen) {
        float bl = 0.6f + 0.4f * sinf(tsec * 6.0f);
        drawText(18, y, "[>] ENTER THE BRIDGE!", F, 0.5f * bl + 0.3f, 0.9f * bl, 1.0f);
    } else {
        drawText(18, y, "[ ] Enter the bridge (SEALED)", F, 0.9f, 0.4f, 0.3f);
    }
    y -= 22;

    // next target distance
    {
        float best = 1e9f;
        bool found = false;
        int bi = 0;
        for (size_t i = 0; i < beacons.size(); ++i) {
            if (beacons[i].active) continue;
            float d = length(beacons[i].pos - ship);
            if (d < best) { best = d; bi = (int)i + 1; found = true; }
        }
        if (found) { snprintf(buf, sizeof(buf), "Next beacon %d: %.0f m", bi, best); }
        else {
            float bestE = 1e9f;
            int alive = 0;
            for (const auto& e : enemies)
                if (e.active) { alive++; bestE = std::min(bestE, length(e.pos - ship)); }
            if (alive > 0) snprintf(buf, sizeof(buf), "Nearest enemy: %.0f m (%d left)", bestE, alive);
            else if (bridgeOpen) snprintf(buf, sizeof(buf), "Bridge: %.0f m", length(bridge.pos - ship));
            else snprintf(buf, sizeof(buf), "Collect remaining coins!");
        }
        drawText(18, y, buf, F, 0.6f, 0.85f, 1.0f);
    }
    y -= 20;
    snprintf(buf, sizeof(buf), "Rings: %d / %d%s", ringsGot, ringsTotal, (ringsTotal > 0 && ringsGot >= ringsTotal) ? "  COMPLETE!" : "");
    drawText(18, y, buf, F, 0.4f, 1.0f, 1.0f);

    // ---- health (supports up to MAX_HEALTH = 5 hearts) ----
    int slotsToShow = std::max(diffStartHealth(), health);
    float panelW = 55.0f + slotsToShow * 44.0f;
    drawPanel(w - panelW - 8.0f, h - 112, w - 8, h - 8, 0.45f, ac);
    drawText(w - panelW + 4.0f, h - 28, padConnected ? "HULL [XBOX 360]" : "HULL", F, 0.8f, 0.9f, 1.0f);
    for (int i = 0; i < slotsToShow; ++i) {
        float cx = (w - panelW + 32.0f) + i * 44.0f, cy = h - 56;
        bool alive = i < health;
        float a = 1.0f;
        if (invuln > 0.0f && alive && fmodf(tsec, 0.3f) < 0.15f) a = 0.45f;
        bool overcharged = (i >= diffStartHealth());
        if (alive) drawHeart(cx, cy, 19.0f, true, 1.0f, overcharged ? 0.45f : 0.15f, overcharged ? 0.75f : 0.25f, a);
        else       drawHeart(cx, cy, 19.0f, false, 0.2f, 0.2f, 0.2f, 0.5f);
    }
    if (invuln > 0.0f) {
        float frac = invuln / cfgInvuln;
        float bx0 = w - panelW + 4.0f, bx1 = w - 18.0f;
        glColor4f(0.1f, 0.2f, 0.3f, 0.8f);
        glRectf(bx0, h - 98, bx1, h - 88);
        glColor4f(0.3f, 0.85f, 1.0f, 0.95f);
        glRectf(bx0, h - 98, bx0 + (bx1 - bx0) * frac, h - 88);
        snprintf(buf, sizeof(buf), "INVINCIBLE %.1fs", invuln);
        drawText(bx0, h - 108, buf, F, 0.5f, 0.9f, 1.0f);
    }

    // ---- combo chain (top centre) ----
    if (state == S_FLYING && combo >= 2 && comboTimer > 0.0f) {
        int m = comboMult();
        float pulse = 0.75f + 0.25f * sinf(tsec * 10.0f);
        snprintf(buf, sizeof(buf), "CHAIN x%d   (%d in a row)", m, combo);
        drawTextC(w * 0.5f, h - 48, buf, FL, 1.0f, 0.75f + 0.25f * (m / 5.0f), 0.25f, pulse);
        float cw = 160.0f, cx0 = w * 0.5f - cw * 0.5f;
        float cf = clampf(comboTimer / cfgComboWin, 0.0f, 1.0f);
        glColor4f(0.1f, 0.1f, 0.1f, 0.7f);
        glRectf(cx0, h - 62, cx0 + cw, h - 56);
        glColor4f(1.0f, 0.8f, 0.25f, 0.95f);
        glRectf(cx0, h - 62, cx0 + cw * cf, h - 56);
    }

    // ---- v1.7: live sector / run timer ----
    if (state == S_FLYING || state == S_EMERGING || state == S_SUCKING) {
        char ta[16], tr[16], tc[16], tb2[96];
        fmtTime(levelTime, ta, sizeof(ta));
        fmtTime(runTime + levelTime, tr, sizeof(tr));
        if (level >= 1 && level <= MAX_LEVEL && bestSector[level - 1] > 0.0f) {
            fmtTime(bestSector[level - 1], tc, sizeof(tc));
            snprintf(tb2, sizeof(tb2), "SECTOR %s   RUN %s   BEST %s", ta, tr, tc);
        } else {
            snprintf(tb2, sizeof(tb2), "SECTOR %s   RUN %s", ta, tr);
        }
        drawTextC(w * 0.5f, h - 18.0f, tb2, F, 0.85f, 0.95f, 1.0f, 0.92f);
    }

    // ---- v1.3: boss health bar ----
    for (const auto& e : enemies) {
        if (!e.active || e.type != ET_BOSS || length(e.pos - ship) > 320.0f) continue;
        float bw = 360.0f, bx0 = w * 0.5f - bw * 0.5f, by = h - 100.0f;
        float frac = clampf(e.hp / e.maxHp, 0.0f, 1.0f);
        drawTextC(w * 0.5f, by + 14.0f, e.stage >= 2 ? "THE GUARDIAN - ENRAGED" : "THE GUARDIAN", F, 1.0f, 0.4f, 0.9f);
        glColor4f(0.1f, 0.0f, 0.1f, 0.8f);
        glRectf(bx0 - 2.0f, by - 2.0f, bx0 + bw + 2.0f, by + 10.0f);
        glColor4f(0.9f, 0.2f + 0.3f * frac, 1.0f, 0.95f);
        glRectf(bx0, by, bx0 + bw * frac, by + 8.0f);
        glColor4f(1.0f, 1.0f, 1.0f, 0.6f);
        glRectf(bx0 + bw * 0.33f, by, bx0 + bw * 0.33f + 2.0f, by + 8.0f);
        glRectf(bx0 + bw * 0.66f, by, bx0 + bw * 0.66f + 2.0f, by + 8.0f);
        break;
    }

    // ---- v1.2/1.3: energy / missiles / bombs / power-ups panel ----
    drawSystemsPanel(tsec);

    // ---- speed bar ----
    {
        float maxS = 1.0f;
        float frac = clampf(fabsf(speed) / maxS, 0.0f, 1.0f);
        float x0 = w * 0.5f - 110, y0 = 26;
        glColor4f(0.05f, 0.1f, 0.15f, 0.7f);
        glRectf(x0, y0, x0 + 220, y0 + 12);
        if (boosting) glColor4f(1.0f, 0.55f, 0.15f, 0.95f);
        else          glColor4f(ac[0], ac[1], ac[2], 0.95f);
        glRectf(x0, y0, x0 + 220 * frac, y0 + 12);
        drawTextC(w * 0.5f, y0 + 20, boosting ? "BOOST" : "SPEED", F, 0.8f, 0.9f, 1.0f);
    }

    if (cfgHints && padConnected) {
        drawText(12, 14, "Xbox360: L-Stick Steer | R-Stick Cam | RT Aim | LT Fire | A Boost | LB/RB Roll (x2 Dash) | R3 Missile | D-Left Bomb", F, 0.5f, 0.75f, 0.9f, 0.85f);
    } else if (cfgHints) {
        drawText(12, 14, "Mouse: Steer | LMB: Shoot | RMB: Aim | W/S: Thrust | Q/E: Roll (x2 Dash) | SHIFT: Boost | SPACE: Missile | V: Bomb", F, 0.5f, 0.6f, 0.7f, 0.85f);
    }

    // ---- weapon & aim status (Swapped: RT = Aim, LT = Fire) ----
    if (state == S_FLYING || state == S_EMERGING || state == S_SUCKING) {
        bool firingNow = mouseLeftDown || (padLT > 0.15f);
        if (aiming && firingNow) {
            drawTextC(w * 0.5f, 64, "PRECISION AIM + FIRING", FL, 1.0f, 0.75f, 0.3f, 1.0f);
        } else if (aiming) {
            drawTextC(w * 0.5f, 64, "PRECISION AIM LOCK ACTIVE", FL, 0.35f, 0.95f, 1.0f, 1.0f);
        } else if (firingNow) {
            drawTextC(w * 0.5f, 64, "WEAPONS FIRING", FL, 0.3f, 1.0f, 0.6f, 1.0f);
        } else {
            drawTextC(w * 0.5f, 64, padConnected ? "[RT] Aim   [LT] Fire" : "[RMB] Aim   [LMB] Fire", F, 0.6f, 0.68f, 0.75f);
        }
    }

    if (cfgRadar) drawRadar(w, h, tsec);

    // ---- warnings ----
    float blink = fmodf(tsec, 0.6f) < 0.35f ? 1.0f : 0.0f;
    if (state == S_FLYING) {
        if (nearIntensity > 0.05f && blink > 0.0f)
            drawTextC(w * 0.5f, 90, "WARNING: WORMHOLE GRAVITY FIELD DETECTED", FL, 1.0f, 0.65f, 0.2f);
        if (nearIntensity > 0.6f)
            drawTextC(w * 0.5f, 112, "POINT OF NO RETURN", FL, 1.0f, 0.3f, 0.3f);
        if (outOfBounds && blink > 0.0f)
            drawTextC(w * 0.5f, h * 0.5f - 120, "LEAVING SECTOR - TURN BACK", FL, 1.0f, 0.3f, 0.2f);
        if (inIon && blink > 0.0f)
            drawTextC(w * 0.5f, h * 0.5f - 150, "ION STORM - ENERGY DRAINING / RADAR JAMMED", FL, 0.7f, 0.5f, 1.0f);
    }
    if (state == S_SUCKING)
        drawTextC(w * 0.5f, h * 0.5f + 80, "CAPTURED - ENTERING THE THROAT", FL, 1.0f, 0.85f, 0.35f);

    // ---- banner ----
    if (bannerTimer > 0.0f) {
        float a = clampf(bannerTimer / 0.7f, 0.0f, 1.0f);
        float by = h * 0.70f;
        drawGlowBar(w * 0.5f, by + 34.0f, w * 0.30f, bannerR, bannerG, bannerB, 0.8f * a);
        drawGlowBar(w * 0.5f, by - 44.0f, w * 0.30f, bannerR, bannerG, bannerB, 0.8f * a);
        drawTextC(w * 0.5f + 2.0f, by - 2.0f, bannerText, GLUT_BITMAP_TIMES_ROMAN_24, 0.0f, 0.0f, 0.0f, 0.8f * a);
        drawTextC(w * 0.5f, by, bannerText, GLUT_BITMAP_TIMES_ROMAN_24, bannerR, bannerG, bannerB, a);
        drawTextC(w * 0.5f + 1.0f, by - 29.0f, bannerSub, F, 0.0f, 0.0f, 0.0f, 0.8f * a);
        drawTextC(w * 0.5f, by - 28, bannerSub, F, 0.85f, 0.92f, 1.0f, a);
    }

    // ---- enemy health bars, aiming reticle, popups ----
    if (state != S_TRANSIT) {
        for (const auto& e : enemies) {
            if (!e.active) continue;
            if (length(e.pos - ship) > 180.0f) continue;
            GLdouble ex, ey, ez;
            if (gluProject(e.pos.x, e.pos.y + 3.8f, e.pos.z, hudMV, hudProj, hudVP, &ex, &ey, &ez) == GL_TRUE
                && ez > 0.0 && ez < 1.0) {
                float bw = (e.type == ET_BOSS) ? 120.0f : 46.0f;
                float frac = clampf(e.hp / e.maxHp, 0.0f, 1.0f);
                float bx = (float)ex, by = (float)ey;
                glColor4f(0.1f, 0.0f, 0.0f, 0.75f);
                glRectf(bx - bw * 0.5f - 1.0f, by - 1.0f, bx + bw * 0.5f + 1.0f, by + 7.0f);
                glColor4f(1.0f, 0.25f + 0.5f * frac, 0.2f, 0.95f);
                glRectf(bx - bw * 0.5f, by, bx - bw * 0.5f + bw * frac, by + 6.0f);
                drawTextC(bx, by + 10.0f, (&e == lockedEnemy) ? "LOCKED" : enemyLabel(e.type), F, 1.0f, (&e == lockedEnemy) ? 0.85f : 0.4f, 0.3f, 0.9f);

                GLdouble cx2, cy2, cz2;
                float dist2 = length(e.pos - ship);
                if (gluProject(e.pos.x, e.pos.y, e.pos.z, hudMV, hudProj, hudVP, &cx2, &cy2, &cz2) == GL_TRUE) {
                    float sz2 = clampf(1500.0f / std::max(dist2, 20.0f), 16.0f, 54.0f);
                    float cxf = (float)cx2, cyf = (float)cy2, cl = 0.45f * sz2;
                    bool isLockedTarget = (&e == lockedEnemy);
                    glLineWidth(isLockedTarget ? 2.8f : 2.0f);
                    if (isLockedTarget) glColor4f(1.0f, 0.85f, 0.2f, 0.95f);
                    else                glColor4f(1.0f, 0.3f, 0.2f, 0.9f);
                    glBegin(GL_LINES);
                    for (int sx2 = -1; sx2 <= 1; sx2 += 2) {
                        for (int sy2 = -1; sy2 <= 1; sy2 += 2) {
                            glVertex2f(cxf + sx2 * sz2, cyf + sy2 * sz2);
                            glVertex2f(cxf + sx2 * (sz2 - cl), cyf + sy2 * sz2);
                            glVertex2f(cxf + sx2 * sz2, cyf + sy2 * sz2);
                            glVertex2f(cxf + sx2 * sz2, cyf + sy2 * (sz2 - cl));
                        }
                    }
                    glEnd();
                    char dbuf[24];
                    snprintf(dbuf, sizeof(dbuf), "%.0fm", dist2);
                    drawTextC(cxf, cyf - sz2 - 14.0f, dbuf, F, 1.0f, 0.5f, 0.4f, 0.9f);
                }
            }
        }

        if (state == S_FLYING) {
            Vec3 f = forwardVec();
            Vec3 aim = hasAimLock ? aimAssistPoint : (ship + f * (aiming ? 75.0f : 55.0f));
            GLdouble rx, ry, rz;
            if (gluProject(aim.x, aim.y, aim.z, hudMV, hudProj, hudVP, &rx, &ry, &rz) == GL_TRUE
                && rz > 0.0 && rz < 1.0) {
                float cx = (float)rx, cy = (float)ry;
                float cr = hasAimLock ? 1.0f : (aiming ? 1.0f : 0.4f);
                float cg = hasAimLock ? 0.25f : (aiming ? 0.85f : 0.95f);
                float cb = hasAimLock ? 0.2f : (aiming ? 0.25f : 1.0f);
                float rad = aiming ? 10.0f : 14.0f;
                glLineWidth(aiming ? 2.5f : 2.0f);
                glColor4f(cr, cg, cb, 0.9f);
                glBegin(GL_LINE_LOOP);
                for (int i = 0; i < 28; ++i) {
                    float a = 2.0f * PI * i / 28.0f;
                    glVertex2f(cx + rad * cosf(a), cy + rad * sinf(a));
                }
                glEnd();
                if (aiming || hasAimLock) {
                    glBegin(GL_LINE_LOOP);
                    for (int i = 0; i < 28; ++i) {
                        float a = 2.0f * PI * i / 28.0f;
                        glVertex2f(cx + 22.0f * cosf(a), cy + 22.0f * sinf(a));
                    }
                    glEnd();
                }
                glBegin(GL_LINES);
                glVertex2f(cx - 22, cy); glVertex2f(cx - 9, cy);
                glVertex2f(cx + 9, cy);  glVertex2f(cx + 22, cy);
                glVertex2f(cx, cy - 22); glVertex2f(cx, cy - 9);
                glVertex2f(cx, cy + 9);  glVertex2f(cx, cy + 22);
                glEnd();
                if (hasAimLock) drawTextC(cx, cy - 38.0f, aiming ? "ASSIST LOCK" : "LOCK", F, 1.0f, 0.35f, 0.2f);
            }
        }

        for (const auto& p : popups) {
            GLdouble sx, sy, sz;
            if (gluProject(p.pos.x, p.pos.y, p.pos.z, hudMV, hudProj, hudVP, &sx, &sy, &sz) == GL_TRUE
                && sz > 0.0 && sz < 1.0) {
                float a = clampf(p.life / 0.6f, 0.0f, 1.0f);
                drawTextC((float)sx, (float)sy, p.text, FL, p.r, p.g, p.b, a);
            }
        }
    }
}

// ============================================================================
//  v1.7 PROGRESS: scoreboard + per-sector timing, save/continue, main & pause menus
// ============================================================================
static const char* SAVE_FILE   = "EinsteinRosen_save.dat";
static const char* SCORES_FILE = "EinsteinRosen_scores.dat";
static const char* DIFF_NAMES[4] = {"EASY", "NORMAL", "HARD", "INSANE"};

struct SaveData {
    int level, score, health, missiles, bombs, diff;
    int up[UP_COUNT];
    float runTime;
    float secTimes[MAX_LEVEL];
};
struct ScoreEntry { int score, level, diff; bool won; float time; char date[16]; };

static bool g_hasSave = false;
static SaveData g_save;
static std::vector<ScoreEntry> g_scores;

// ---- save file (checkpoint at the start of every sector) ----
static bool readSave(SaveData& d) {
    FILE* f = fopen(SAVE_FILE, "r");
    if (!f) return false;
    memset(&d, 0, sizeof(d));
    char line[160];
    bool gotLevel = false;
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        float v = (float)atof(eq + 1);
        if (!strcmp(line, "level"))        { d.level = (int)v; gotLevel = true; }
        else if (!strcmp(line, "score"))   d.score = (int)v;
        else if (!strcmp(line, "health"))  d.health = (int)v;
        else if (!strcmp(line, "missiles")) d.missiles = (int)v;
        else if (!strcmp(line, "bombs"))   d.bombs = (int)v;
        else if (!strcmp(line, "diff"))    d.diff = (int)v;
        else if (!strcmp(line, "runtime")) d.runTime = v;
        else if (line[0] == 'u' && line[1] == 'p') { int i = atoi(line + 2); if (i >= 0 && i < UP_COUNT) d.up[i] = (int)v; }
        else if (line[0] == 's' && line[1] == 'e' && line[2] == 'c') { int i = atoi(line + 3); if (i >= 0 && i < MAX_LEVEL) d.secTimes[i] = v; }
    }
    fclose(f);
    if (!gotLevel || d.level < 1 || d.level > MAX_LEVEL) return false;
    d.diff = std::max(0, std::min(3, d.diff));
    d.health = std::max(1, d.health);
    for (int i = 0; i < UP_COUNT; ++i) d.up[i] = std::max(0, std::min(MAX_UPGRADE_LEVEL, d.up[i]));
    return true;
}
void saveCheckpoint() {
    if (shotPath) return;                       // never touch the player's files during automated tests
    FILE* f = fopen(SAVE_FILE, "w");
    if (!f) return;
    fprintf(f, "level=%d\nscore=%d\nhealth=%d\nmissiles=%d\nbombs=%d\ndiff=%d\nruntime=%.3f\n",
            level, score, health, missileAmmo, bombAmmo, runDiff, runTime);
    for (int i = 0; i < UP_COUNT; ++i) fprintf(f, "up%d=%d\n", i, upg[i]);
    for (int i = 0; i < MAX_LEVEL; ++i) fprintf(f, "sec%d=%.3f\n", i, sectorTimes[i]);
    fclose(f);
    g_hasSave = readSave(g_save);
}
static void deleteSave() {
    if (!shotPath) remove(SAVE_FILE);
    g_hasSave = false;
}

// ---- scoreboard + personal-best sector times ----
static void saveScores() {
    if (shotPath) return;
    FILE* f = fopen(SCORES_FILE, "w");
    if (!f) return;
    for (const auto& e : g_scores)
        fprintf(f, "S %d %d %d %d %.3f %s\n", e.score, e.level, e.diff, e.won ? 1 : 0, e.time, e.date);
    for (int i = 0; i < MAX_LEVEL; ++i) fprintf(f, "B %d %.3f\n", i, bestSector[i]);
    fclose(f);
}
static void loadScores() {
    g_scores.clear();
    for (int i = 0; i < MAX_LEVEL; ++i) bestSector[i] = 0.0f;
    FILE* f = fopen(SCORES_FILE, "r");
    if (!f) return;
    char line[200];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'S') {
            ScoreEntry e{};
            int won = 0;
            char date[32] = "";
            if (sscanf(line, "S %d %d %d %d %f %15s", &e.score, &e.level, &e.diff, &won, &e.time, date) >= 5) {
                e.won = (won != 0);
                e.diff = std::max(0, std::min(3, e.diff));
                snprintf(e.date, sizeof(e.date), "%s", date);
                if (g_scores.size() < 10) g_scores.push_back(e);
            }
        } else if (line[0] == 'B') {
            int i; float t;
            if (sscanf(line, "B %d %f", &i, &t) == 2 && i >= 0 && i < MAX_LEVEL) bestSector[i] = t;
        }
    }
    fclose(f);
}
static int addScoreEntry(const ScoreEntry& e) {
    size_t pos = 0;
    while (pos < g_scores.size() && (g_scores[pos].score > e.score || (g_scores[pos].score == e.score && g_scores[pos].time <= e.time))) ++pos;
    if (pos >= 10) return -1;
    g_scores.insert(g_scores.begin() + pos, e);
    if (g_scores.size() > 10) g_scores.resize(10);
    return (int)pos;
}
// returns true when this time is a new personal best for the sector
static bool recordBestSector(int sector, float t) {
    if (sector < 0 || sector >= MAX_LEVEL || t <= 0.0f) return false;
    if (bestSector[sector] <= 0.0f || t < bestSector[sector]) {
        bool first = (bestSector[sector] <= 0.0f);
        bestSector[sector] = t;
        saveScores();
        return !first;     // the very first time is not announced as a "new best"
    }
    return false;
}
static void clearScores() {
    g_scores.clear();
    for (int i = 0; i < MAX_LEVEL; ++i) bestSector[i] = 0.0f;
    lastRunRank = -1; lastRunValid = false;
    saveScores();
}
// called when a run ends (death or victory): records it and removes the checkpoint
void endRun(bool finished) {
    float total = runTime + (finished ? 0.0f : levelTime);
    ScoreEntry e{};
    e.score = score; e.level = level; e.diff = runDiff; e.won = finished; e.time = total;
    time_t tt = time(nullptr);
    struct tm* lt = localtime(&tt);
    if (lt) strftime(e.date, sizeof(e.date), "%Y-%m-%d", lt); else snprintf(e.date, sizeof(e.date), "----------");
    lastRunRank = addScoreEntry(e);
    lastRunTotal = total; lastRunLevel = level; lastRunWon = finished; lastRunValid = true;
    for (int i = 0; i < MAX_LEVEL; ++i) lastRunTimes[i] = sectorTimes[i];
    saveScores();
    deleteSave();
}

// ---- starting / continuing / leaving a run ----
static void continueGame() {
    SaveData d;
    if (!readSave(d)) { g_hasSave = false; return; }
    paused = false;
    runDiff = d.diff;
    for (int i = 0; i < UP_COUNT; ++i) upg[i] = d.up[i];
    level = d.level;
    score = d.score;
    health = std::min(d.health, maxHealthVal());
    missileAmmo = std::min(d.missiles, maxMissilesVal());
    bombAmmo = std::min(d.bombs, maxBombsVal());
    runTime = d.runTime;
    for (int i = 0; i < MAX_LEVEL; ++i) sectorTimes[i] = d.secTimes[i];
    fireTimer = 0.0f;
    dispScore = (float)score;
    droneTimer = 0.0f; dashTimer = 0.0f;
    shieldTimer = rapidTimer = magnetTimer = doubleTimer = 0.0f;
    combo = 0; comboTimer = 0.0f;
    buildLevel(level);
    resuming = true;                 // the checkpoint already contains this sector's resupply
    beginLevel();
    resuming = false;
}
static void goMainMenu() {
    paused = false;
    level = 1;
    buildLevel(1);
    state = S_MENU;
    stateTimer = 0.0f;
    shipVisible = true;
    shipScale = 1.0f;
    boosting = false;
    flashAlpha = 0.0f; hitFlash = 0.0f; camShake = 0.0f; bannerTimer = 0.0f; invuln = 0.0f;
    menuSel = 0; menuPage = 0; confirmId = 0;
    g_hasSave = readSave(g_save);
}
static void quitGame() {
    exit(0);     // settings are saved when the settings screen closes; progress is saved at each sector start
}

// ---- menu model ----
enum { MI_CONTINUE = 1, MI_NEW, MI_SCORES, MI_HELP, MI_SETTINGS, MI_QUIT, MI_RESUME, MI_MAINMENU };
static int menuIds[8];
static int menuCount = 0;
static int menuBaseMode() { return state == S_MENU ? 1 : (paused ? 2 : 0); }
static bool menuActive()  { return !settingsOpen && !confirmId && !menuPage && menuBaseMode() != 0; }
static void buildMenuItems() {
    menuCount = 0;
    int m = menuBaseMode();
    if (m == 1) {
        if (g_hasSave) menuIds[menuCount++] = MI_CONTINUE;
        menuIds[menuCount++] = MI_NEW;
        menuIds[menuCount++] = MI_SCORES;
        menuIds[menuCount++] = MI_HELP;
        menuIds[menuCount++] = MI_SETTINGS;
        menuIds[menuCount++] = MI_QUIT;
    } else if (m == 2) {
        menuIds[menuCount++] = MI_RESUME;
        menuIds[menuCount++] = MI_SETTINGS;
        menuIds[menuCount++] = MI_MAINMENU;
        menuIds[menuCount++] = MI_QUIT;
    }
    if (menuSel >= menuCount) menuSel = 0;
    if (menuSel < 0) menuSel = 0;
}
static const char* menuLabel(int id) {
    switch (id) {
        case MI_CONTINUE: return "CONTINUE";
        case MI_NEW:      return "NEW GAME";
        case MI_SCORES:   return "SCOREBOARD";
        case MI_HELP:     return "HOW TO PLAY";
        case MI_SETTINGS: return "SETTINGS";
        case MI_QUIT:     return "QUIT GAME";
        case MI_RESUME:   return "RESUME";
        case MI_MAINMENU: return "MAIN MENU";
    }
    return "";
}
static void menuBtnRect(int mode, int i, int w, int h, float& x0, float& y0, float& x1, float& y1) {
    float top = (mode == 1) ? h * 0.57f : h * 0.56f;
    float bw = std::min(460.0f, (float)w - 60.0f);
    x0 = w * 0.5f - bw * 0.5f; x1 = x0 + bw;
    y1 = top - i * 40.0f; y0 = y1 - 34.0f;
}
static void menuActivate(int id) {
    switch (id) {
        case MI_CONTINUE: continueGame(); break;
        case MI_NEW:
            if (g_hasSave) { confirmId = 1; confirmSel = 0; }
            else { paused = false; startGame(); }
            break;
        case MI_SCORES:   menuPage = 1; break;
        case MI_HELP:     menuPage = 2; break;
        case MI_SETTINGS: openSettings(); break;
        case MI_QUIT:     quitGame(); break;
        case MI_RESUME:   paused = false; break;
        case MI_MAINMENU: goMainMenu(); break;
    }
    playSfx(SFX_BEACON);
}
static void menuMove(int d) {
    buildMenuItems();
    if (menuCount <= 0) return;
    menuSel = (menuSel + d + menuCount) % menuCount;
    playSfx(SFX_COIN);
}
static void menuKey(unsigned char key) {
    int mode = menuBaseMode();
    buildMenuItems();
    if (key == 27)  { if (mode == 1) quitGame(); else paused = false; return; }
    if (key == 'p' && mode == 2) { paused = false; return; }
    if (key == 'w') { menuMove(-1); return; }
    if (key == 's') { menuMove(1); return; }
    if ((key == 13 || key == ' ') && menuCount > 0) menuActivate(menuIds[menuSel]);
}
static void menuSpecial(int key) {
    if (key == GLUT_KEY_UP) menuMove(-1);
    else if (key == GLUT_KEY_DOWN) menuMove(1);
}
static void menuHover(int mx, int my) {
    int mode = menuBaseMode();
    buildMenuItems();
    float hy = (float)curWinH - (float)my;
    for (int i = 0; i < menuCount; ++i) {
        float x0, y0, x1, y1;
        menuBtnRect(mode, i, curWinW, curWinH, x0, y0, x1, y1);
        if (mx >= x0 && mx <= x1 && hy >= y0 && hy <= y1) { menuSel = i; return; }
    }
}
static void menuClick(int mx, int my) {
    int mode = menuBaseMode();
    buildMenuItems();
    float hy = (float)curWinH - (float)my;
    for (int i = 0; i < menuCount; ++i) {
        float x0, y0, x1, y1;
        menuBtnRect(mode, i, curWinW, curWinH, x0, y0, x1, y1);
        if (mx >= x0 && mx <= x1 && hy >= y0 && hy <= y1) { menuSel = i; menuActivate(menuIds[i]); return; }
    }
}

// ---- confirmation dialog ----
static void resolveConfirm(bool yes) {
    int id = confirmId;
    confirmId = 0;
    if (!yes) return;
    if (id == 1) { paused = false; startGame(); }
    else if (id == 2) clearScores();
}
static void confirmKey(unsigned char key) {
    if (key == 'y') resolveConfirm(true);
    else if (key == 'n' || key == 27) resolveConfirm(false);
    else if (key == 13 || key == ' ') resolveConfirm(confirmSel == 1);
}
static void confirmSpecial(int key) {
    if (key == GLUT_KEY_LEFT || key == GLUT_KEY_RIGHT) confirmSel = 1 - confirmSel;
}
static void confirmRects(int w, int h, float* nx0, float* ny0, float* nx1, float* ny1, float* yx0, float* yy0, float* yx1, float* yy1) {
    float bw = std::min(560.0f, (float)w - 40.0f), bh = 190.0f;
    float x0 = (w - bw) * 0.5f, y0 = (h - bh) * 0.5f;
    *nx0 = x0 + bw * 0.5f - 170.0f; *nx1 = *nx0 + 150.0f; *ny0 = y0 + 22.0f; *ny1 = *ny0 + 36.0f;
    *yx0 = x0 + bw * 0.5f + 20.0f;  *yx1 = *yx0 + 150.0f; *yy0 = *ny0;       *yy1 = *ny1;
}
static void confirmHover(int mx, int my) {
    float a, b, c, d, e, f, g, hh;
    confirmRects(curWinW, curWinH, &a, &b, &c, &d, &e, &f, &g, &hh);
    float hy = (float)curWinH - (float)my;
    if (mx >= a && mx <= c && hy >= b && hy <= d) confirmSel = 0;
    else if (mx >= e && mx <= g && hy >= f && hy <= hh) confirmSel = 1;
}
static void confirmClick(int mx, int my) {
    float a, b, c, d, e, f, g, hh;
    confirmRects(curWinW, curWinH, &a, &b, &c, &d, &e, &f, &g, &hh);
    float hy = (float)curWinH - (float)my;
    if (mx >= a && mx <= c && hy >= b && hy <= d) resolveConfirm(false);
    else if (mx >= e && mx <= g && hy >= f && hy <= hh) resolveConfirm(true);
}

// ---- scoreboard / how-to-play pages ----
static void pageBtnRects(int w, int h, float* cx0, float* cy0, float* cx1, float* cy1, float* kx0, float* ky0, float* kx1, float* ky1) {
    float pw = std::min(960.0f, (float)w - 30.0f), ph = std::min(620.0f, (float)h - 30.0f);
    float px0 = ((float)w - pw) * 0.5f, py0 = ((float)h - ph) * 0.5f, px1 = px0 + pw;
    *cx0 = px0 + 24.0f; *cx1 = px0 + 220.0f; *cy0 = py0 + 14.0f; *cy1 = py0 + 46.0f;     // CLEAR
    *kx0 = px1 - 220.0f; *kx1 = px1 - 24.0f; *ky0 = py0 + 14.0f; *ky1 = py0 + 46.0f;      // CLOSE
}
static void pageKey(unsigned char key) {
    if (key == 27 || key == 13 || key == ' ') { menuPage = 0; return; }
    if (key == 'c' && menuPage == 1) { confirmId = 2; confirmSel = 0; }
}
static void pageClick(int mx, int my) {
    float a, b, c, d, e, f, g, hh;
    pageBtnRects(curWinW, curWinH, &a, &b, &c, &d, &e, &f, &g, &hh);
    float hy = (float)curWinH - (float)my;
    if (menuPage == 1 && mx >= a && mx <= c && hy >= b && hy <= d) { confirmId = 2; confirmSel = 0; return; }
    if (mx >= e && mx <= g && hy >= f && hy <= hh) menuPage = 0;
}

// ---- controller ----
static void menuPadUpdate(float dt) {
    static float repT = 0.0f;
    int mode = menuBaseMode();
    buildMenuItems();
    int dy = 0;
    if (uiUp) dy = -1;
    if (uiDown) dy = 1;
    repT -= dt;
    if (repT <= 0.0f && fabsf(padLY) > 0.6f) { dy = (padLY > 0.0f) ? -1 : 1; repT = 0.2f; }
    if (dy) menuMove(dy);
    if (uiConfirm && menuCount > 0) menuActivate(menuIds[menuSel]);
    else if (uiBack && mode == 2) paused = false;
}
static void pagePadUpdate() {
    if (uiConfirm || uiBack) menuPage = 0;
    else if (uiReset && menuPage == 1) { confirmId = 2; confirmSel = 0; }
}
static void confirmPadUpdate() {
    if (uiLeft || uiRight) confirmSel = 1 - confirmSel;
    if (uiConfirm) resolveConfirm(confirmSel == 1);
    else if (uiBack) resolveConfirm(false);
}

// ---- drawing ----
static void drawMenuButtons(int mode, int w, int h, float tsec) {
    const Theme& th = curTheme();
    const float* ac = th.accent;
    void* FL = GLUT_BITMAP_HELVETICA_18;
    void* F = GLUT_BITMAP_9_BY_15;
    buildMenuItems();
    for (int i = 0; i < menuCount; ++i) {
        float x0, y0, x1, y1;
        menuBtnRect(mode, i, w, h, x0, y0, x1, y1);
        bool sel = (i == menuSel);
        float pulse = 0.5f + 0.5f * sinf(tsec * 5.0f);
        glColor4f(ac[0] * (sel ? 0.45f : 0.12f), ac[1] * (sel ? 0.45f : 0.12f), ac[2] * (sel ? 0.45f : 0.12f), sel ? 0.88f : 0.55f);
        glRectf(x0, y0, x1, y1);
        if (sel) {
            glColor4f(ac[0], ac[1], ac[2], 0.7f + 0.3f * pulse);
            glRectf(x0, y0, x0 + 5.0f, y1);
            glRectf(x1 - 5.0f, y0, x1, y1);
        }
        int id = menuIds[i];
        float lr = sel ? 1.0f : 0.8f, lg = sel ? 1.0f : 0.9f;
        drawText(x0 + 26.0f, y0 + 10.0f, menuLabel(id), FL, lr, lg, 1.0f);
        char sub[96] = "";
        if (id == MI_CONTINUE && g_hasSave) {
            char tb[16];
            snprintf(sub, sizeof(sub), "SECTOR %d   %s   %d pts", g_save.level, fmtTime(g_save.runTime, tb, sizeof(tb)), g_save.score);
        } else if (id == MI_MAINMENU) {
            snprintf(sub, sizeof(sub), "progress is saved each sector");
        }
        if (sub[0]) drawText(x1 - 20.0f - textWidth(F, sub), y0 + 11.0f, sub, F, 0.75f, 0.9f, 1.0f);
    }
}

static void drawBackdrop(int w, int h, float* px0, float* py0, float* px1, float* py1, float pwMax, float phMax, const float* ac) {
    float pw = std::min(pwMax, (float)w - 30.0f), ph = std::min(phMax, (float)h - 30.0f);
    *px0 = ((float)w - pw) * 0.5f; *px1 = *px0 + pw;
    *py0 = ((float)h - ph) * 0.5f; *py1 = *py0 + ph;
    glColor4f(0.0f, 0.0f, 0.02f, 0.74f);
    glRectf(0, 0, (float)w, (float)h);
    glColor4f(0.0f, 0.01f, 0.03f, 0.88f);
    glRectf(*px0, *py0, *px1, *py1);
    drawPanel(*px0, *py0, *px1, *py1, 0.88f, ac);
}

static void drawMenuBtn(float x0, float y0, float x1, float y1, const char* label, float r, float g, float b) {
    glColor4f(r * 0.35f, g * 0.35f, b * 0.35f, 0.95f);
    glRectf(x0, y0, x1, y1);
    drawTextC((x0 + x1) * 0.5f, y0 + 11.0f, label, GLUT_BITMAP_9_BY_15, 1.0f, 1.0f, 1.0f);
}

void drawScoreboard(int w, int h, float tsec) {
    const Theme& th = curTheme();
    const float* ac = th.accent;
    void* F = GLUT_BITMAP_9_BY_15;
    void* FL = GLUT_BITMAP_HELVETICA_18;
    float px0, py0, px1, py1;
    drawBackdrop(w, h, &px0, &py0, &px1, &py1, 960.0f, 620.0f, ac);
    drawStrokeC((px0 + px1) * 0.5f, py1 - 52.0f, "SCOREBOARD", (px1 - px0) * 0.5f, 0.22f, ac[0], ac[1], ac[2], 1.0f, 2.5f);
    char buf[160], tb[16];

    // ---- high scores ----
    float lx = px0 + 30.0f, y = py1 - 106.0f;
    drawText(lx, y, "HIGH SCORES", FL, ac[0], ac[1], ac[2]);
    y -= 28.0f;
    snprintf(buf, sizeof(buf), "%-4s%-10s%-6s%-10s%-8s%s", "#", "SCORE", "SECT", "TIME", "MODE", "DATE");
    drawText(lx, y, buf, F, 0.6f, 0.75f, 0.85f);
    glColor4f(ac[0], ac[1], ac[2], 0.5f);
    glRectf(lx, y - 6.0f, lx + 440.0f, y - 5.0f);
    y -= 26.0f;
    for (int i = 0; i < 10; ++i) {
        if (i < (int)g_scores.size()) {
            const ScoreEntry& e = g_scores[i];
            snprintf(buf, sizeof(buf), "%2d  %07d   S%d%c   %-7s   %-6s  %s",
                     i + 1, e.score, e.level, e.won ? '*' : ' ', fmtTime(e.time, tb, sizeof(tb)), DIFF_NAMES[e.diff], e.date);
            bool hl = (i == lastRunRank);
            float pulse = 0.5f + 0.5f * sinf(tsec * 5.0f);
            if (hl) { glColor4f(1.0f, 0.85f, 0.3f, 0.12f + 0.08f * pulse); glRectf(lx - 6.0f, y - 5.0f, lx + 450.0f, y + 15.0f); }
            drawText(lx, y, buf, F, hl ? 1.0f : 0.85f, hl ? 0.9f : 0.92f, hl ? 0.4f : 1.0f);
        } else {
            snprintf(buf, sizeof(buf), "%2d  -------   --   --:--.-   ------  ----------", i + 1);
            drawText(lx, y, buf, F, 0.38f, 0.42f, 0.48f);
        }
        y -= 24.0f;
    }
    drawText(lx, y - 4.0f, "* = finished all 5 sectors", F, 0.55f, 0.65f, 0.7f);

    // ---- best sector times + last run ----
    float rx = px0 + (px1 - px0) * 0.62f;
    y = py1 - 106.0f;
    drawText(rx, y, "BEST SECTOR TIMES", FL, ac[0], ac[1], ac[2]);
    y -= 28.0f;
    for (int i = 0; i < MAX_LEVEL; ++i) {
        if (bestSector[i] > 0.0f) snprintf(buf, sizeof(buf), "SECTOR %d    %s", i + 1, fmtTime(bestSector[i], tb, sizeof(tb)));
        else                      snprintf(buf, sizeof(buf), "SECTOR %d    --:--.-", i + 1);
        drawText(rx, y, buf, F, bestSector[i] > 0.0f ? 0.7f : 0.4f, bestSector[i] > 0.0f ? 1.0f : 0.45f, bestSector[i] > 0.0f ? 0.8f : 0.5f);
        y -= 22.0f;
    }
    y -= 20.0f;
    drawText(rx, y, "LAST RUN", FL, ac[0], ac[1], ac[2]);
    y -= 28.0f;
    if (lastRunValid) {
        for (int i = 0; i < MAX_LEVEL; ++i) {
            if (lastRunTimes[i] > 0.0f) snprintf(buf, sizeof(buf), "SECTOR %d    %s", i + 1, fmtTime(lastRunTimes[i], tb, sizeof(tb)));
            else                        snprintf(buf, sizeof(buf), "SECTOR %d    --:--.-", i + 1);
            drawText(rx, y, buf, F, 0.85f, 0.92f, 1.0f);
            y -= 22.0f;
        }
        snprintf(buf, sizeof(buf), "TOTAL       %s%s", fmtTime(lastRunTotal, tb, sizeof(tb)), lastRunWon ? "  (WIN)" : "");
        drawText(rx, y - 4.0f, buf, F, 1.0f, 0.92f, 0.5f);
    } else {
        drawText(rx, y, "No run finished yet.", F, 0.5f, 0.55f, 0.6f);
    }

    float a, b, c, d, e, f, g, hh;
    pageBtnRects(w, h, &a, &b, &c, &d, &e, &f, &g, &hh);
    drawMenuBtn(a, b, c, d, "CLEAR [C]", 1.0f, 0.5f, 0.4f);
    drawMenuBtn(e, f, g, hh, "CLOSE [ENTER]", ac[0], ac[1], ac[2]);
}

void drawHelp(int w, int h, float tsec) {
    (void)tsec;
    const Theme& th = curTheme();
    const float* ac = th.accent;
    void* F = GLUT_BITMAP_9_BY_15;
    void* FL = GLUT_BITMAP_HELVETICA_18;
    float px0, py0, px1, py1;
    drawBackdrop(w, h, &px0, &py0, &px1, &py1, 960.0f, 620.0f, ac);
    drawStrokeC((px0 + px1) * 0.5f, py1 - 52.0f, "HOW TO PLAY", (px1 - px0) * 0.5f, 0.22f, ac[0], ac[1], ac[2], 1.0f, 2.5f);
    float x = px0 + 34.0f, y = py1 - 104.0f;
    auto H = [&](const char* s) { drawText(x, y, s, FL, ac[0], ac[1], ac[2]); y -= 24.0f; };
    auto L = [&](const char* s, float r, float g, float b) { drawText(x + 12.0f, y, s, F, r, g, b); y -= 19.0f; };
    H("GOAL");
    L("Collect coins, light every beacon and destroy the enemy fleet - then the bridge opens. Fly in.", 0.85f, 0.92f, 1.0f);
    L("Finish each sector as fast as you can: your time is recorded and a faster run scores a pace bonus.", 0.85f, 0.92f, 1.0f);
    L("Progress is saved automatically at the start of every sector: use CONTINUE next time you play.", 0.5f, 1.0f, 0.7f);
    y -= 6.0f;
    H("MOUSE & KEYBOARD");
    L("Mouse = steer | LMB = shoot | RMB = precision aim | W/S = thrust | Q/E = roll | SHIFT = boost", 0.7f, 0.85f, 1.0f);
    L("SPACE = homing missile | V = pulse bomb | double-tap Q/E = dash (brief invincibility)", 0.7f, 0.85f, 1.0f);
    L("P / Esc = pause menu | O = settings | M = mouse lock | N = mute | B = bloom | G = HDR renderer", 0.7f, 0.85f, 1.0f);
    y -= 6.0f;
    H("XBOX 360 CONTROLLER");
    L("L-Stick = steer | R-Stick = camera / fine aim | RT = aim | LT = shoot | A = boost | B = brake", 0.55f, 0.95f, 0.75f);
    L("LB/RB = roll (double-tap to dash) | R-Stick click = missile | D-Pad Left = bomb | Start = pause | Back = settings", 0.55f, 0.95f, 0.75f);
    y -= 6.0f;
    H("PICKUPS & HAZARDS");
    L("Hearts repair your hull (up to 5+) | power-ups: shield, rapid fire, magnet, score x2, missiles, drone, bomb", 1.0f, 0.6f, 0.8f);
    L("Fly through rings for bonus score and energy | ion storms drain energy and jam radar | mines chain-explode", 1.0f, 0.75f, 0.4f);
    L("Chain kills and pickups for a combo multiplier - a hull hit breaks it. Defeat the GUARDIAN in sector 5.", 1.0f, 0.75f, 0.4f);
    float a, b, c, d, e, f, g, hh;
    pageBtnRects(w, h, &a, &b, &c, &d, &e, &f, &g, &hh);
    drawMenuBtn(e, f, g, hh, "CLOSE [ENTER]", ac[0], ac[1], ac[2]);
}

void drawConfirm(int w, int h, float tsec) {
    (void)tsec;
    const Theme& th = curTheme();
    const float* ac = th.accent;
    void* F = GLUT_BITMAP_9_BY_15;
    void* FL = GLUT_BITMAP_HELVETICA_18;
    float bw = std::min(560.0f, (float)w - 40.0f), bh = 190.0f;
    float x0 = ((float)w - bw) * 0.5f, y0 = ((float)h - bh) * 0.5f;
    glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
    glRectf(0, 0, (float)w, (float)h);
    glColor4f(0.0f, 0.01f, 0.03f, 0.95f);
    glRectf(x0, y0, x0 + bw, y0 + bh);
    drawPanel(x0, y0, x0 + bw, y0 + bh, 0.9f, ac);
    char buf[128], tb[16];
    if (confirmId == 1) {
        drawTextC(x0 + bw * 0.5f, y0 + bh - 42.0f, "START A NEW GAME?", FL, 1.0f, 0.9f, 0.5f);
        snprintf(buf, sizeof(buf), "Your saved run (Sector %d, %d pts, %s) will be overwritten.", g_save.level, g_save.score, fmtTime(g_save.runTime, tb, sizeof(tb)));
        drawTextC(x0 + bw * 0.5f, y0 + bh - 76.0f, buf, F, 0.85f, 0.92f, 1.0f);
    } else {
        drawTextC(x0 + bw * 0.5f, y0 + bh - 42.0f, "CLEAR THE SCOREBOARD?", FL, 1.0f, 0.9f, 0.5f);
        drawTextC(x0 + bw * 0.5f, y0 + bh - 76.0f, "All high scores and best sector times will be deleted.", F, 0.85f, 0.92f, 1.0f);
    }
    float a, b, c, d, e, f, g, hh;
    confirmRects(w, h, &a, &b, &c, &d, &e, &f, &g, &hh);
    glColor4f(0.15f, 0.2f, 0.25f + (confirmSel == 0 ? 0.25f : 0.0f), 0.95f);
    glRectf(a, b, c, d);
    glColor4f(0.45f + (confirmSel == 1 ? 0.35f : 0.0f), 0.12f, 0.1f, 0.95f);
    glRectf(e, f, g, hh);
    drawTextC((a + c) * 0.5f, b + 11.0f, "NO [N]", F, 1.0f, 1.0f, 1.0f);
    drawTextC((e + g) * 0.5f, f + 11.0f, "YES [Y]", F, 1.0f, 1.0f, 1.0f);
    glLineWidth(2.5f);
    glColor4f(1.0f, 0.9f, 0.4f, 0.95f);
    glBegin(GL_LINE_LOOP);
    if (confirmSel == 0) { glVertex2f(a, b); glVertex2f(c, b); glVertex2f(c, d); glVertex2f(a, d); }
    else                 { glVertex2f(e, f); glVertex2f(g, f); glVertex2f(g, hh); glVertex2f(e, hh); }
    glEnd();
    glLineWidth(1.0f);
}

static void drawRunSummary(int w, int h, float baseY, bool won) {
    char buf[200], tb[16];
    void* F = GLUT_BITMAP_9_BY_15;
    snprintf(buf, sizeof(buf), "RUN TIME %s     %s", fmtTime(lastRunTotal, tb, sizeof(tb)), DIFF_NAMES[runDiff]);
    drawTextC(w * 0.5f, baseY, buf, GLUT_BITMAP_HELVETICA_18, 0.85f, 0.95f, 1.0f);
    buf[0] = 0;
    for (int i = 0; i < MAX_LEVEL; ++i) {
        char one[40];
        if (lastRunTimes[i] > 0.0f) snprintf(one, sizeof(one), "S%d %s   ", i + 1, fmtTime(lastRunTimes[i], tb, sizeof(tb)));
        else                        snprintf(one, sizeof(one), "S%d --:--.-   ", i + 1);
        strncat(buf, one, sizeof(buf) - strlen(buf) - 1);
    }
    drawTextC(w * 0.5f, baseY - 26.0f, buf, F, 0.65f, 0.8f, 0.9f);
    if (lastRunRank >= 0) {
        snprintf(buf, sizeof(buf), "NEW HIGH SCORE - RANK #%d !", lastRunRank + 1);
        drawTextC(w * 0.5f, baseY - 56.0f, buf, GLUT_BITMAP_HELVETICA_18, 1.0f, 0.85f, 0.3f);
    } else {
        drawTextC(w * 0.5f, baseY - 56.0f, won ? "Not on the top 10 this time." : "Not on the top 10 - try again!", F, 0.6f, 0.65f, 0.7f);
    }
}

void drawMenu(int w, int h, float tsec) {
    glColor4f(0.0f, 0.0f, 0.03f, 0.45f);
    glRectf(0, 0, (float)w, (float)h);

    for (int pass = 0; pass < 3; ++pass) {
        float lw = (pass == 0) ? 14.0f : (pass == 1 ? 8.0f : 3.0f);
        float al = (pass == 0) ? 0.10f : (pass == 1 ? 0.25f : 1.0f);
        drawStrokeC(w * 0.5f, h * 0.83f, "EINSTEIN-ROSEN", w * 0.8f, 0.55f, 0.4f, 0.8f, 1.0f, al, lw);
        drawStrokeC(w * 0.5f, h * 0.83f - 80, "RUNNER", w * 0.5f, 0.55f, 1.0f, 0.75f, 0.3f, al, lw);
    }
    void* F = GLUT_BITMAP_9_BY_15;
    drawTextC(w * 0.5f, h * 0.83f - 118.0f, "5 sectors  -  collect, light, destroy, dive into the bridge", F, 0.7f, 0.85f, 1.0f);
    drawMenuButtons(1, w, h, tsec);
    (void)tsec;
    drawTextC(w * 0.5f, h * 0.07f, padConnected ? "D-PAD / STICK + [A]      [BACK] = SETTINGS" : "UP/DOWN + ENTER   |   MOUSE   |   O = SETTINGS", F, 0.55f, 0.7f, 0.8f);
}

void drawGameOver(int w, int h, float tsec) {
    glColor4f(0.25f, 0.0f, 0.0f, 0.55f);
    glRectf(0, 0, (float)w, (float)h);
    char buf[128];
    drawStrokeC(w * 0.5f, h * 0.68f, "SHIP DESTROYED", w * 0.7f, 0.5f, 1.0f, 0.25f, 0.2f, 1.0f, 3.0f);
    snprintf(buf, sizeof(buf), "FINAL SCORE: %d      REACHED SECTOR %d", score, level);
    drawTextC(w * 0.5f, h * 0.55f, buf, GLUT_BITMAP_TIMES_ROMAN_24, 1.0f, 0.92f, 0.6f);
    if (lastRunValid) drawRunSummary(w, h, h * 0.46f, false);
    if (fmodf(tsec, 1.0f) < 0.65f)
        drawTextC(w * 0.5f, h * 0.22f, padConnected ? "[A] / [START] = TRY AGAIN      [B] = MAIN MENU" : "ENTER = TRY AGAIN      ESC = MAIN MENU", GLUT_BITMAP_HELVETICA_18, 1.0f, 1.0f, 1.0f);
}

void drawVictory(int w, int h, float tsec) {
    glColor4f(0.0f, 0.0f, 0.04f, 0.35f);
    glRectf(0, 0, (float)w, (float)h);
    char buf[128];
    drawStrokeC(w * 0.5f, h * 0.70f, "YOU CROSSED THE FINAL BRIDGE", w * 0.85f, 0.45f, 0.5f, 1.0f, 0.7f, 1.0f, 3.0f);
    snprintf(buf, sizeof(buf), "FINAL SCORE: %d   (hull bonus included)", score);
    drawTextC(w * 0.5f, h * 0.57f, buf, GLUT_BITMAP_TIMES_ROMAN_24, 1.0f, 0.92f, 0.5f);
    if (lastRunValid) drawRunSummary(w, h, h * 0.48f, true);
    if (fmodf(tsec, 1.0f) < 0.65f)
        drawTextC(w * 0.5f, h * 0.22f, padConnected ? "[A] / [START] = PLAY AGAIN      [B] = MAIN MENU" : "ENTER = PLAY AGAIN      ESC = MAIN MENU", GLUT_BITMAP_HELVETICA_18, 1.0f, 1.0f, 1.0f);
}

// ============================================================================
//  v1.6 SETTINGS SYSTEM  (open with O, or [Back] on a controller; Esc/O/[B] closes)
// ============================================================================
enum { SET_TOGGLE = 0, SET_SLIDER, SET_CHOICE };
enum { TAB_DISPLAY = 0, TAB_GRAPHICS, TAB_AUDIO, TAB_CONTROLS, TAB_GAMEPLAY, TAB_COUNT };
static const char* TAB_NAMES[TAB_COUNT] = {"DISPLAY", "GRAPHICS", "AUDIO", "CONTROLS", "GAMEPLAY"};
static const char* NAMES_DIFF[4] = {"EASY", "NORMAL", "HARD", "INSANE"};
static const char* NAMES_FPS[8]  = {"OFF", "30", "60", "90", "120", "144", "165", "240"};

struct SettingDef {
    const char* key; const char* label; const char* tip;
    int tab, type;
    float* f; bool* b; int* i;
    float mn, mx, step;
    const char* const* names; int nNames;
    int fmt;                       // 0 number, 1 percent, 2 multiplier, 3 degrees, 4 seconds
    void (*onChange)();
    float defF; bool defB; int defI;
};

static std::vector<SettingDef> settingsTbl;
static std::vector<int> tabRows[TAB_COUNT];
static bool settingsDirty = false;
static int  setTab = 0, setSel = 0, setScroll = 0, setDragRow = -1;
static const char* CFG_FILE = "EinsteinRosen_settings.cfg";

// ---- applying settings to the running game ----
static void applyVsync() {
#ifdef _WIN32
    typedef BOOL (WINAPI *SwapIntervalFn)(int);
    static SwapIntervalFn sw = nullptr;
    static bool tried = false;
    if (!tried) { sw = (SwapIntervalFn)glutGetProcAddress("wglSwapIntervalEXT"); tried = true; }
    if (sw) sw(cfgVsync ? 1 : 0);
#endif
}
static int savedW = 1280, savedH = 720;
static void applyFullscreen() {
    if (cfgFullscreen) { savedW = curWinW; savedH = curWinH; glutFullScreen(); }
    else { glutReshapeWindow(savedW, savedH); glutPositionWindow(100, 50); }
}
static void cbVsync()      { applyVsync(); }
static void cbFullscreen() { applyFullscreen(); }
static void cbMsaa()       { noMSAA = !cfgMSAA; targetsDirty = true; }

// ---- registry ----
static void pushDef(SettingDef& d) { tabRows[d.tab].push_back((int)settingsTbl.size()); settingsTbl.push_back(d); }
static void addToggle(int tab, const char* key, const char* label, const char* tip, bool* b, void (*cb)() = nullptr) {
    SettingDef d{}; d.key = key; d.label = label; d.tip = tip; d.tab = tab; d.type = SET_TOGGLE;
    d.b = b; d.onChange = cb; d.defB = *b; pushDef(d);
}
static void addSlider(int tab, const char* key, const char* label, const char* tip, float* f, float mn, float mx, float st, int fmt, void (*cb)() = nullptr) {
    SettingDef d{}; d.key = key; d.label = label; d.tip = tip; d.tab = tab; d.type = SET_SLIDER;
    d.f = f; d.mn = mn; d.mx = mx; d.step = st; d.fmt = fmt; d.onChange = cb; d.defF = *f; pushDef(d);
}
static void addChoice(int tab, const char* key, const char* label, const char* tip, int* i, const char* const* names, int n, void (*cb)() = nullptr) {
    SettingDef d{}; d.key = key; d.label = label; d.tip = tip; d.tab = tab; d.type = SET_CHOICE;
    d.i = i; d.names = names; d.nNames = n; d.onChange = cb; d.defI = *i; pushDef(d);
}

void initSettings() {
    settingsTbl.clear();
    for (int t = 0; t < TAB_COUNT; ++t) tabRows[t].clear();
    // ---- DISPLAY ----
    addToggle(TAB_DISPLAY, "fullscreen", "Fullscreen", "Switch between windowed mode and fullscreen.", &cfgFullscreen, cbFullscreen);
    addToggle(TAB_DISPLAY, "vsync", "VSync", "Sync to the monitor refresh: smoother motion, no tearing.", &cfgVsync, cbVsync);
    addChoice(TAB_DISPLAY, "fpscap", "Frame rate limit", "Caps FPS (saves power/heat). Only matters if VSync is off or the cap is lower.", &cfgFpsIdx, NAMES_FPS, 8);
    addSlider(TAB_DISPLAY, "renderscale", "Render resolution", "Internal resolution (HDR renderer). Lower = faster; the image is smoothly upscaled.", &cfgRenderScale, 0.5f, 1.0f, 0.05f, 1);
    addSlider(TAB_DISPLAY, "fov", "Field of view", "Base camera field of view. Boosting and aiming still zoom relative to this.", &cfgFov, 50.0f, 95.0f, 1.0f, 3);
    addToggle(TAB_DISPLAY, "showfps", "Show FPS counter", "Shows the current frames per second at the top of the screen.", &cfgShowFps);
    addToggle(TAB_DISPLAY, "radar", "Show radar", "The circular radar in the bottom-right corner.", &cfgRadar);
    addToggle(TAB_DISPLAY, "hints", "Show control hints", "The line of key/button hints along the bottom of the screen.", &cfgHints);
    addToggle(TAB_DISPLAY, "popups", "Show floating popups", "Score numbers, pickups and warnings that float over the action.", &cfgPopups);
    // ---- GRAPHICS ----
    addToggle(TAB_GRAPHICS, "hdr", "HDR cinematic renderer", "Bloom, tone mapping, procedural sky and planets. Off = classic renderer (fastest).", &postOn);
    addToggle(TAB_GRAPHICS, "msaa", "Anti-aliasing (MSAA 4x)", "Smooths jagged edges. Costs GPU performance on weaker cards.", &cfgMSAA, cbMsaa);
    addToggle(TAB_GRAPHICS, "bloom", "Bloom", "Soft glow around bright lights, engines, explosions and the sun.", &bloomOn);
    addSlider(TAB_GRAPHICS, "bloomstr", "Bloom strength", "How strong the glow is.", &bloomStrength, 0.0f, 2.5f, 0.05f, 2);
    addSlider(TAB_GRAPHICS, "exposure", "Exposure / brightness", "Overall brightness of the image before tone mapping.", &cfgExposure, 0.5f, 1.6f, 0.02f, 2);
    addSlider(TAB_GRAPHICS, "vignette", "Vignette", "Darkening toward the screen corners.", &cfgVignette, 0.0f, 1.0f, 0.05f, 1);
    addSlider(TAB_GRAPHICS, "chroma", "Chromatic aberration", "Colour fringing at the screen edges (stronger when shaking or boosting).", &cfgCA, 0.0f, 3.0f, 0.1f, 2);
    addSlider(TAB_GRAPHICS, "grain", "Film grain", "Fine cinematic noise. Also hides colour banding.", &cfgGrain, 0.0f, 3.0f, 0.1f, 2);
    addSlider(TAB_GRAPHICS, "speedblur", "Speed blur", "Radial motion blur while flying fast or boosting.", &cfgBlur, 0.0f, 2.0f, 0.1f, 2);
    addToggle(TAB_GRAPHICS, "godrays", "God rays", "Light shafts streaming from the sun.", &cfgGodRays);
    addSlider(TAB_GRAPHICS, "glow", "Glow / particle brightness", "Brightness of additive glows, sparks and engine trails.", &glowGain, 0.6f, 3.0f, 0.1f, 2);
    // ---- AUDIO ----
    addSlider(TAB_AUDIO, "volmaster", "Master volume", "Overall loudness of everything.", &volMaster, 0.0f, 1.0f, 0.05f, 1);
    addSlider(TAB_AUDIO, "volmusic", "Music volume", "Background music.", &volMusic, 0.0f, 1.0f, 0.05f, 1);
    addSlider(TAB_AUDIO, "volsfx", "Effects volume", "Weapons, explosions, pickups and engine sounds.", &volSfx, 0.0f, 1.0f, 0.05f, 1);
    addToggle(TAB_AUDIO, "mute", "Mute all audio", "Silences everything (same as pressing N).", &audioMuted);
    // ---- CONTROLS ----
    addSlider(TAB_CONTROLS, "mousesens", "Mouse sensitivity", "How fast the ship turns with the mouse.", &cfgMouseSens, 0.2f, 3.0f, 0.1f, 2);
    addToggle(TAB_CONTROLS, "inverty", "Invert Y axis", "Flips up/down for both mouse and controller stick.", &cfgInvertY);
    addToggle(TAB_CONTROLS, "mouselock", "Lock mouse to window", "Captures the cursor while flying (same as pressing M).", &mouseLocked);
    addSlider(TAB_CONTROLS, "padsens", "Gamepad sensitivity", "How fast the ship turns with the left stick.", &cfgPadSens, 0.5f, 2.0f, 0.1f, 2);
    addSlider(TAB_CONTROLS, "paddead", "Gamepad deadzone", "Ignores tiny stick movements. Raise this if the ship drifts on its own.", &cfgPadDead, 0.05f, 0.35f, 0.01f, 1);
    addSlider(TAB_CONTROLS, "rumble", "Gamepad vibration", "Strength of controller rumble. 0% turns it off.", &cfgRumble, 0.0f, 1.0f, 0.1f, 1);
    addToggle(TAB_CONTROLS, "autocruise", "Gamepad auto-cruise", "Keeps a gentle forward speed when you are not using thrust or brake.", &cfgAutoCruise);
    addSlider(TAB_CONTROLS, "aimassist", "Aim assist strength", "Steers bolts and nudges your nose toward enemies. 0 = pure manual aim.", &cfgAimAssist, 0.0f, 2.0f, 0.1f, 2);
    // ---- GAMEPLAY ----
    addChoice(TAB_GAMEPLAY, "difficulty", "Difficulty", "Enemy toughness, fire rate, starting hull and score. Fully applies from a new run.", &cfgDifficulty, NAMES_DIFF, 4);
    addSlider(TAB_GAMEPLAY, "invuln", "Invincibility after a hit", "Seconds of protection after taking hull damage.", &cfgInvuln, 1.0f, 6.0f, 0.5f, 4);
    addSlider(TAB_GAMEPLAY, "combowin", "Combo chain window", "Seconds you have to keep a combo chain alive.", &cfgComboWin, 1.5f, 6.0f, 0.25f, 4);
    addSlider(TAB_GAMEPLAY, "shake", "Camera shake", "Strength of screen shake from hits and explosions.", &cfgShake, 0.0f, 1.5f, 0.1f, 1);
    addSlider(TAB_GAMEPLAY, "flash", "Screen flash intensity", "Brightness of full-screen flashes (damage, bridge opening, bombs).", &cfgFlash, 0.0f, 1.5f, 0.1f, 1);
}

// ---- value formatting / editing ----
static void fmtValue(const SettingDef& d, char* out, size_t n) {
    if (d.type == SET_TOGGLE)      snprintf(out, n, "%s", *d.b ? "ON" : "OFF");
    else if (d.type == SET_CHOICE) snprintf(out, n, "%s", d.names[*d.i]);
    else {
        float v = *d.f;
        switch (d.fmt) {
            case 1:  snprintf(out, n, "%d%%", (int)(v * 100.0f + 0.5f)); break;
            case 2:  snprintf(out, n, "x%.2f", v); break;
            case 3:  snprintf(out, n, "%d deg", (int)(v + 0.5f)); break;
            case 4:  snprintf(out, n, "%.1f s", v); break;
            default: snprintf(out, n, "%.2f", v); break;
        }
    }
}
static void settingChanged(SettingDef& d) {
    if (d.onChange) d.onChange();
    settingsDirty = true;
    if (d.tab == TAB_AUDIO) playSfx(SFX_COIN);     // audible preview when tuning volume
}
static void setSliderValue(SettingDef& d, float v) {
    v = clampf(v, d.mn, d.mx);
    v = d.mn + floorf((v - d.mn) / d.step + 0.5f) * d.step;
    *d.f = clampf(v, d.mn, d.mx);
    settingChanged(d);
}
static void adjustSetting(int idx, int dir, bool flip) {
    SettingDef& d = settingsTbl[idx];
    if (d.type == SET_TOGGLE)      { *d.b = flip ? !*d.b : (dir > 0); settingChanged(d); }
    else if (d.type == SET_CHOICE) { *d.i = (*d.i + dir + d.nNames) % d.nNames; settingChanged(d); }
    else                           { setSliderValue(d, *d.f + dir * d.step); }
}
static void resetSetting(int idx) {
    SettingDef& d = settingsTbl[idx];
    if (d.type == SET_TOGGLE) *d.b = d.defB; else if (d.type == SET_CHOICE) *d.i = d.defI; else *d.f = d.defF;
    settingChanged(d);
}
static void resetAllSettings() {
    for (size_t i = 0; i < settingsTbl.size(); ++i) {
        SettingDef& d = settingsTbl[i];
        if (d.type == SET_TOGGLE) *d.b = d.defB; else if (d.type == SET_CHOICE) *d.i = d.defI; else *d.f = d.defF;
        if (d.onChange) d.onChange();
    }
    settingsDirty = true;
}

// ---- persistence ----
static void saveSettings() {
    FILE* f = fopen(CFG_FILE, "w");
    if (!f) return;
    for (const auto& d : settingsTbl) {
        if (d.type == SET_TOGGLE)      fprintf(f, "%s=%d\n", d.key, *d.b ? 1 : 0);
        else if (d.type == SET_CHOICE) fprintf(f, "%s=%d\n", d.key, *d.i);
        else                           fprintf(f, "%s=%.4f\n", d.key, *d.f);
    }
    fclose(f);
    settingsDirty = false;
}
static void loadSettings() {
    FILE* f = fopen(CFG_FILE, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        float v = (float)atof(eq + 1);
        for (auto& d : settingsTbl) {
            if (strcmp(d.key, line) != 0) continue;
            if (d.type == SET_TOGGLE)      *d.b = (v != 0.0f);
            else if (d.type == SET_CHOICE) *d.i = std::max(0, std::min(d.nNames - 1, (int)(v + 0.5f)));
            else                           *d.f = clampf(v, d.mn, d.mx);
        }
    }
    fclose(f);
}

// ---- layout shared by drawing and mouse hit-testing ----
struct SettingsLayout { float px0, py0, px1, py1, tabY0, tabH, rowsTop, rowH, ctrlX0, ctrlX1, trackX1; int vis; };
static SettingsLayout setLayout(int w, int h) {
    SettingsLayout L;
    float pw = std::min(920.0f, (float)w - 40.0f), ph = std::min(660.0f, (float)h - 30.0f);
    L.px0 = ((float)w - pw) * 0.5f; L.px1 = L.px0 + pw;
    L.py0 = ((float)h - ph) * 0.5f; L.py1 = L.py0 + ph;
    L.tabH = 34.0f;
    L.tabY0 = L.py1 - 76.0f - L.tabH;
    L.rowsTop = L.tabY0 - 14.0f;
    L.rowH = 36.0f;
    L.vis = std::max(3, (int)((L.rowsTop - (L.py0 + 112.0f)) / L.rowH));
    L.ctrlX0 = L.px0 + pw * 0.47f;
    L.ctrlX1 = L.px1 - 34.0f;
    L.trackX1 = L.ctrlX1 - 92.0f;
    return L;
}
static void ensureSelVisible() {
    SettingsLayout L = setLayout(curWinW, curWinH);
    int n = (int)tabRows[setTab].size();
    if (setSel < 0) setSel = 0;
    if (setSel >= n) setSel = n - 1;
    if (setSel < setScroll) setScroll = setSel;
    if (setSel >= setScroll + L.vis) setScroll = setSel - L.vis + 1;
    if (setScroll < 0) setScroll = 0;
}
static void changeTab(int dir) {
    setTab = (setTab + dir + TAB_COUNT) % TAB_COUNT;
    setSel = 0; setScroll = 0; setDragRow = -1;
    playSfx(SFX_BEACON);
}

void openSettings() {
    if (settingsOpen) return;
    settingsOpen = true;
    setDragRow = -1;
    mouseLeftDown = mouseRightDown = false;
    mouseDx = mouseDy = 0.0f;
    ensureSelVisible();
}
static void closeSettings() {
    settingsOpen = false;
    setDragRow = -1;
    mouseLeftDown = mouseRightDown = false;
    mouseDx = mouseDy = 0.0f;
    if (settingsDirty) saveSettings();
}

// ---- input ----
static void settingsMoveSel(int d) {
    int n = (int)tabRows[setTab].size();
    setSel = (setSel + d + n) % n;
    ensureSelVisible();
}
static int  curRowIdx() { return tabRows[setTab][std::max(0, std::min((int)tabRows[setTab].size() - 1, setSel))]; }

static void settingsKey(unsigned char key) {
    if (key == 27) { closeSettings(); return; }
    if (key == 13 || key == ' ') { adjustSetting(curRowIdx(), 1, true); return; }
    if (key == 9 || key == 'e') { changeTab(1); return; }
    if (key == 'q')             { changeTab(-1); return; }
    if (key >= '1' && key <= '5') { int t = key - '1'; if (t != setTab) { setTab = t; setSel = 0; setScroll = 0; } return; }
    if (key == 'r') { resetAllSettings(); return; }
    if (key == 8 || key == 127) { resetSetting(curRowIdx()); return; }
}
static void settingsSpecial(int key) {
    if (key == GLUT_KEY_UP)         settingsMoveSel(-1);
    else if (key == GLUT_KEY_DOWN)  settingsMoveSel(1);
    else if (key == GLUT_KEY_LEFT)  adjustSetting(curRowIdx(), -1, false);
    else if (key == GLUT_KEY_RIGHT) adjustSetting(curRowIdx(), 1, false);
    else if (key == GLUT_KEY_PAGE_UP)   changeTab(-1);
    else if (key == GLUT_KEY_PAGE_DOWN) changeTab(1);
    else if (key == GLUT_KEY_HOME)  { setSel = 0; ensureSelVisible(); }
    else if (key == GLUT_KEY_END)   { setSel = (int)tabRows[setTab].size() - 1; ensureSelVisible(); }
}
static void sliderFromX(int idx, float mx) {
    SettingsLayout L = setLayout(curWinW, curWinH);
    SettingDef& d = settingsTbl[idx];
    float t = clampf((mx - L.ctrlX0) / (L.trackX1 - L.ctrlX0), 0.0f, 1.0f);
    setSliderValue(d, d.mn + t * (d.mx - d.mn));
}
static int rowUnderMouse(const SettingsLayout& L, float hy) {
    if (hy > L.rowsTop) return -1;
    int k = (int)floorf((L.rowsTop - hy) / L.rowH);
    if (k < 0 || k >= L.vis) return -1;
    int idx = setScroll + k;
    return (idx < (int)tabRows[setTab].size()) ? idx : -1;
}
static void settingsHover(int mx, int my) {
    SettingsLayout L = setLayout(curWinW, curWinH);
    float hy = (float)curWinH - (float)my;
    if (setDragRow >= 0) { sliderFromX(setDragRow, (float)mx); return; }
    int r = rowUnderMouse(L, hy);
    if (r >= 0) setSel = r;
}
static void settingsClick(int mx, int my, bool down) {
    if (!down) { setDragRow = -1; return; }
    SettingsLayout L = setLayout(curWinW, curWinH);
    float hy = (float)curWinH - (float)my, fx = (float)mx;
    float tabW = (L.px1 - L.px0 - 40.0f) / TAB_COUNT;
    for (int t = 0; t < TAB_COUNT; ++t) {
        float x0 = L.px0 + 20.0f + t * tabW;
        if (fx >= x0 && fx <= x0 + tabW - 4.0f && hy >= L.tabY0 && hy <= L.tabY0 + L.tabH) {
            if (t != setTab) { setTab = t; setSel = 0; setScroll = 0; playSfx(SFX_BEACON); }
            return;
        }
    }
    if (hy >= L.py0 + 14.0f && hy <= L.py0 + 46.0f) {
        if (fx >= L.px0 + 24.0f && fx <= L.px0 + 200.0f) { resetAllSettings(); return; }
        if (fx >= L.px1 - 200.0f && fx <= L.px1 - 24.0f) { closeSettings(); return; }
    }
    int r = rowUnderMouse(L, hy);
    if (r < 0) return;
    setSel = r;
    int idx = tabRows[setTab][r];
    SettingDef& d = settingsTbl[idx];
    if (fx < L.ctrlX0 - 10.0f) return;
    if (d.type == SET_TOGGLE) { if (fx <= L.ctrlX0 + 110.0f) adjustSetting(idx, 1, true); }
    else if (d.type == SET_CHOICE) { if (fx <= L.ctrlX0 + 230.0f) adjustSetting(idx, (fx < L.ctrlX0 + 115.0f) ? -1 : 1, false); }
    else { setDragRow = idx; sliderFromX(idx, fx); }
}

// controller navigation (D-pad / stick, LB/RB tabs, A = toggle, X = reset, B/Start = close)
void settingsPadUpdate(float dt) {
    static float repT = 0.0f;
    int dy = 0, dx = 0;
    if (uiUp) dy = -1;
    if (uiDown) dy = 1;
    if (uiLeft) dx = -1;
    if (uiRight) dx = 1;
    repT -= dt;
    if (repT <= 0.0f && fabsf(padLY) > 0.6f) { dy = (padLY > 0.0f) ? -1 : 1; repT = 0.18f; }
    if (repT <= 0.0f && fabsf(padLX) > 0.6f) { dx = (padLX > 0.0f) ? 1 : -1; repT = 0.09f; }
    if (dy) settingsMoveSel(dy);
    if (dx) adjustSetting(curRowIdx(), dx, false);
    if (uiConfirm)  adjustSetting(curRowIdx(), 1, true);
    if (uiTabPrev)  changeTab(-1);
    if (uiTabNext)  changeTab(1);
    if (uiReset)    resetSetting(curRowIdx());
    if (uiBack)     closeSettings();
}

// ---- drawing ----
void drawSettings(int w, int h, float tsec) {
    const Theme& th = curTheme();
    const float* ac = th.accent;
    void* F = GLUT_BITMAP_9_BY_15;
    void* FL = GLUT_BITMAP_HELVETICA_18;
    SettingsLayout L = setLayout(w, h);
    char buf[96];

    glColor4f(0.0f, 0.0f, 0.02f, 0.74f);
    glRectf(0, 0, (float)w, (float)h);
    glColor4f(0.0f, 0.01f, 0.03f, 0.86f);
    glRectf(L.px0, L.py0, L.px1, L.py1);
    drawPanel(L.px0, L.py0, L.px1, L.py1, 0.88f, ac);
    drawStrokeC((L.px0 + L.px1) * 0.5f, L.py1 - 52.0f, "SETTINGS", (L.px1 - L.px0) * 0.5f, 0.22f, ac[0], ac[1], ac[2], 1.0f, 2.5f);

    // tabs
    float tabW = (L.px1 - L.px0 - 40.0f) / TAB_COUNT;
    for (int t = 0; t < TAB_COUNT; ++t) {
        float x0 = L.px0 + 20.0f + t * tabW, x1 = x0 + tabW - 4.0f;
        bool act = (t == setTab);
        glColor4f(ac[0] * (act ? 0.45f : 0.12f), ac[1] * (act ? 0.45f : 0.12f), ac[2] * (act ? 0.45f : 0.12f), act ? 0.85f : 0.55f);
        glRectf(x0, L.tabY0, x1, L.tabY0 + L.tabH);
        if (act) { glColor4f(ac[0], ac[1], ac[2], 1.0f); glRectf(x0, L.tabY0, x1, L.tabY0 + 3.0f); }
        snprintf(buf, sizeof(buf), "%d %s", t + 1, TAB_NAMES[t]);
        drawTextC((x0 + x1) * 0.5f, L.tabY0 + 11.0f, buf, F, act ? 1.0f : 0.65f, act ? 1.0f : 0.78f, act ? 1.0f : 0.85f);
    }

    // rows
    const std::vector<int>& rows = tabRows[setTab];
    int n = (int)rows.size();
    for (int k = 0; k < L.vis; ++k) {
        int ri = setScroll + k;
        if (ri >= n) break;
        const SettingDef& d = settingsTbl[rows[ri]];
        float yc = L.rowsTop - (k + 0.5f) * L.rowH;
        bool sel = (ri == setSel);
        if (sel) {
            float pulse = 0.5f + 0.5f * sinf(tsec * 5.0f);
            glColor4f(ac[0], ac[1], ac[2], 0.16f + 0.05f * pulse);
            glRectf(L.px0 + 14.0f, yc - L.rowH * 0.5f + 2.0f, L.px1 - 14.0f, yc + L.rowH * 0.5f - 2.0f);
            glColor4f(ac[0], ac[1], ac[2], 0.95f);
            glRectf(L.px0 + 14.0f, yc - L.rowH * 0.5f + 2.0f, L.px0 + 18.0f, yc + L.rowH * 0.5f - 2.0f);
        }
        drawText(L.px0 + 34.0f, yc - 6.0f, d.label, FL, sel ? 1.0f : 0.82f, sel ? 1.0f : 0.9f, 1.0f);
        fmtValue(d, buf, sizeof(buf));
        if (d.type == SET_TOGGLE) {
            bool on = *d.b;
            glColor4f(on ? ac[0] * 0.85f : 0.2f, on ? ac[1] * 0.85f : 0.22f, on ? ac[2] * 0.85f : 0.26f, 0.95f);
            glRectf(L.ctrlX0, yc - 12.0f, L.ctrlX0 + 90.0f, yc + 12.0f);
            drawTextC(L.ctrlX0 + 45.0f, yc - 5.0f, buf, F, on ? 0.0f : 0.75f, on ? 0.05f : 0.78f, on ? 0.08f : 0.82f);
        } else if (d.type == SET_CHOICE) {
            glColor4f(0.2f, 0.22f, 0.26f, 0.95f);
            glRectf(L.ctrlX0, yc - 12.0f, L.ctrlX0 + 220.0f, yc + 12.0f);
            drawText(L.ctrlX0 + 10.0f, yc - 5.0f, "<", F, ac[0], ac[1], ac[2]);
            drawText(L.ctrlX0 + 203.0f, yc - 5.0f, ">", F, ac[0], ac[1], ac[2]);
            drawTextC(L.ctrlX0 + 110.0f, yc - 5.0f, buf, F, 1.0f, 1.0f, 1.0f);
        } else {
            float t = clampf((*d.f - d.mn) / (d.mx - d.mn), 0.0f, 1.0f);
            float tx = L.ctrlX0 + t * (L.trackX1 - L.ctrlX0);
            glColor4f(0.14f, 0.18f, 0.22f, 0.95f);
            glRectf(L.ctrlX0, yc - 3.0f, L.trackX1, yc + 3.0f);
            glColor4f(ac[0], ac[1], ac[2], 0.95f);
            glRectf(L.ctrlX0, yc - 3.0f, tx, yc + 3.0f);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glRectf(tx - 5.0f, yc - 10.0f, tx + 5.0f, yc + 10.0f);
            drawText(L.trackX1 + 16.0f, yc - 5.0f, buf, F, 1.0f, 0.95f, 0.7f);
        }
    }
    if (setScroll > 0)       drawTextC(L.px1 - 24.0f, L.rowsTop - 8.0f, "^", FL, ac[0], ac[1], ac[2]);
    if (setScroll + L.vis < n) drawTextC(L.px1 - 24.0f, L.py0 + 118.0f, "v", FL, ac[0], ac[1], ac[2]);

    // footer: tip, hints, buttons
    const SettingDef& cur = settingsTbl[rows[std::max(0, std::min(n - 1, setSel))]];
    drawText(L.px0 + 30.0f, L.py0 + 82.0f, cur.tip, F, 0.75f, 0.9f, 1.0f);
    if (padConnected) drawText(L.px0 + 30.0f, L.py0 + 62.0f, "D-PAD move/change | LB/RB tabs | A toggle | X reset | B close", F, 0.55f, 0.95f, 0.75f);
    else              drawText(L.px0 + 30.0f, L.py0 + 62.0f, "UP/DOWN select | LEFT/RIGHT change | TAB / Q E tabs | ENTER toggle | BACKSPACE reset", F, 0.55f, 0.7f, 0.85f);
    glColor4f(0.2f, 0.12f, 0.12f, 0.9f);
    glRectf(L.px0 + 24.0f, L.py0 + 14.0f, L.px0 + 200.0f, L.py0 + 46.0f);
    drawTextC(L.px0 + 112.0f, L.py0 + 25.0f, "RESET ALL [R]", F, 1.0f, 0.7f, 0.6f);
    glColor4f(ac[0] * 0.4f, ac[1] * 0.4f, ac[2] * 0.4f, 0.95f);
    glRectf(L.px1 - 200.0f, L.py0 + 14.0f, L.px1 - 24.0f, L.py0 + 46.0f);
    drawTextC(L.px1 - 112.0f, L.py0 + 25.0f, "CLOSE [O / ESC]", F, 1.0f, 1.0f, 1.0f);
    drawTextC((L.px0 + L.px1) * 0.5f, L.py0 + 25.0f, "Settings save automatically", F, 0.5f, 0.6f, 0.65f);
}

void drawUpgrade(int w, int h, float tsec) {
    const Theme& th = curTheme();
    const float* ac = th.accent;
    void* F = GLUT_BITMAP_9_BY_15;
    void* FL = GLUT_BITMAP_HELVETICA_18;
    char buf[96];
    glColor4f(0.0f, 0.0f, 0.03f, 0.35f);
    glRectf(0, 0, (float)w, (float)h);
    snprintf(buf, sizeof(buf), "SECTOR %d CLEARED", level);
    drawStrokeC(w * 0.5f, h * 0.80f, buf, w * 0.7f, 0.35f, ac[0], ac[1], ac[2], 1.0f, 3.0f);
    drawTextC(w * 0.5f, h * 0.72f, "CHOOSE A PERMANENT UPGRADE", FL, 0.85f, 0.92f, 1.0f);
    {
        char tt[16];
        snprintf(buf, sizeof(buf), "SCORE %07d      SECTOR TIME %s%s", score,
                 fmtTime((level >= 1 && level <= MAX_LEVEL) ? sectorTimes[level - 1] : 0.0f, tt, sizeof(tt)),
                 lastSectorPB ? "  NEW BEST!" : "");
    }
    drawTextC(w * 0.5f, h * 0.68f, buf, F, 1.0f, 0.92f, 0.5f);

    for (int i = 0; i < 3; ++i) {
        float x0, y0, x1, y1;
        upgradeCardRect(i, w, h, x0, y0, x1, y1);
        int u = upChoice[i];
        const float* c = UP_COL[u];
        bool sel = (i == upSelected);
        float pulse = 0.7f + 0.3f * sinf(tsec * 6.0f);
        drawPanel(x0, y0, x1, y1, sel ? 0.80f : 0.45f, c);
        if (sel) {
            glLineWidth(3.0f);
            glColor4f(1.0f, 0.9f, 0.4f, pulse);
            glBegin(GL_LINE_LOOP);
            glVertex2f(x0 - 3, y0 - 3); glVertex2f(x1 + 3, y0 - 3); glVertex2f(x1 + 3, y1 + 3); glVertex2f(x0 - 3, y1 + 3);
            glEnd();
            glLineWidth(1.0f);
        }
        float cx = (x0 + x1) * 0.5f;
        snprintf(buf, sizeof(buf), "[%d]", i + 1);
        drawText(x0 + 14.0f, y1 - 28.0f, buf, FL, 1.0f, 0.9f, 0.5f);
        drawTextC(cx, y1 - 70.0f, UP_NAMES[u], FL, c[0], c[1], c[2]);
        drawTextC(cx, y1 - 112.0f, UP_DESC_A[u], F, 0.85f, 0.92f, 1.0f);
        drawTextC(cx, y1 - 132.0f, UP_DESC_B[u], F, 0.85f, 0.92f, 1.0f);
        snprintf(buf, sizeof(buf), "LEVEL %d -> %d", upg[u], upg[u] + 1);
        drawTextC(cx, y0 + 60.0f, buf, F, 0.7f, 1.0f, 0.7f);
        for (int k = 0; k < MAX_UPGRADE_LEVEL; ++k) {
            float px = cx - 36.0f + k * 36.0f;
            if (k < upg[u])       glColor4f(c[0], c[1], c[2], 0.95f);
            else if (k == upg[u]) glColor4f(1.0f, 1.0f, 1.0f, 0.4f + 0.4f * pulse);
            else                  glColor4f(0.3f, 0.3f, 0.35f, 0.5f);
            glRectf(px - 10.0f, y0 + 24.0f, px + 10.0f, y0 + 40.0f);
        }
    }
    drawTextC(w * 0.5f, h * 0.20f, "[1] [2] [3] or CLICK to take   |   ARROWS / D-PAD + ENTER / [A] to confirm", F, 0.7f, 0.85f, 1.0f);
}

void drawHUD(float tsec) {
    int w = curWinW;
    int h = curWinH;
    beginHUD(w, h);

    if (flashAlpha > 0.0f) {
        glColor4f(flashR, flashG, flashB, clampf(flashAlpha, 0.0f, 1.0f) * cfgFlash);
        glRectf(0, 0, (float)w, (float)h);
    }
    if (hitFlash > 0.0f) {
        glColor4f(1.0f, 0.05f, 0.05f, clampf(hitFlash, 0.0f, 1.0f) * 0.38f * cfgFlash);
        glRectf(0, 0, (float)w, (float)h);
    }

    drawVignette(w, h, tsec);

    if (state == S_MENU)          drawMenu(w, h, tsec);
    else if (state == S_GAMEOVER) drawGameOver(w, h, tsec);
    else if (state == S_VICTORY)  drawVictory(w, h, tsec);
    else if (state == S_UPGRADE)  drawUpgrade(w, h, tsec);
    else                          drawPlayHUD(w, h, tsec);

    if (paused) {
        glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
        glRectf(0, 0, (float)w, (float)h);
        drawStrokeC(w * 0.5f, h * 0.68f, "PAUSED", w * 0.4f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 3.0f);
        drawMenuButtons(2, w, h, tsec);
    }
    if (menuPage == 1)      drawScoreboard(w, h, tsec);
    else if (menuPage == 2) drawHelp(w, h, tsec);
    if (confirmId)          drawConfirm(w, h, tsec);

    if (cfgShowFps) {
        char fpsBuf[24];
        snprintf(fpsBuf, sizeof(fpsBuf), "FPS %d", (int)(fpsSmooth + 0.5f));
        drawText(w * 0.5f + 190.0f, h - 18.0f, fpsBuf, GLUT_BITMAP_9_BY_15, 0.7f, 1.0f, 0.7f);
    }
    if (settingsOpen) drawSettings(w, h, tsec);

    endHUD();
}

// ----------------------------------------------------------------------------
//  Display & Bloom
// ----------------------------------------------------------------------------
void initBloomTextures() {
    GLuint ids[3];
    glGenTextures(3, ids);
    texFull = ids[0];
    texSmall[0] = ids[1];
    texSmall[1] = ids[2];
    std::vector<unsigned char> zeros((size_t)FULL_TEX * FULL_TEX * 3, 0);
    for (int i = 0; i < 3; ++i) {
        int sz = (i == 0) ? FULL_TEX : SMALL_TEX;
        glBindTexture(GL_TEXTURE_2D, ids[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sz, sz, 0, GL_RGB, GL_UNSIGNED_BYTE, zeros.data());
    }
}

static void texQuad(float u0, float v0, float u1, float v1) {
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0); glVertex2f(0, 0);
    glTexCoord2f(u1, v0); glVertex2f(1, 0);
    glTexCoord2f(u1, v1); glVertex2f(1, 1);
    glTexCoord2f(u0, v1); glVertex2f(0, 1);
    glEnd();
}

void applyBloom(int w, int h) {
    if (!bloomOn || texFull == 0) return;
    if (w > FULL_TEX || h > FULL_TEX || w < 16 || h < 16) return;

    int bw = std::max(16, std::min(SMALL_TEX, w / 4));
    int bh = std::max(16, std::min(SMALL_TEX, h / 4));
    float fu = (float)w / FULL_TEX, fv = (float)h / FULL_TEX;
    float su = (float)bw / SMALL_TEX, sv = (float)bh / SMALL_TEX;

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT | GL_SCISSOR_BIT |
                 GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    gfxLight(false);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glBindTexture(GL_TEXTURE_2D, texFull);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);

    glViewport(0, 0, bw, bh);
    glDisable(GL_BLEND);
    glColor3f(1.0f, 1.0f, 1.0f);
    texQuad(0, 0, fu, fv);
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_ZERO);
    texQuad(0, 0, fu, fv);
    texQuad(0, 0, fu, fv);
    glBindTexture(GL_TEXTURE_2D, texSmall[0]);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bw, bh);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, bw, bh);
    const float texel = 1.0f / SMALL_TEX;
    const float wts[5] = {0.12f, 0.23f, 0.30f, 0.23f, 0.12f};
    for (int pass = 0; pass < 4; ++pass) {
        int src = pass % 2, dst = 1 - src;
        bool horiz = (pass % 2 == 0);
        glBindTexture(GL_TEXTURE_2D, texSmall[src]);
        glDisable(GL_BLEND);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        for (int k = -2; k <= 2; ++k) {
            float off = k * texel * 1.6f;
            float ox = horiz ? off : 0.0f, oy = horiz ? 0.0f : off;
            glColor3f(wts[k + 2], wts[k + 2], wts[k + 2]);
            texQuad(ox, oy, su + ox, sv + oy);
        }
        glBindTexture(GL_TEXTURE_2D, texSmall[dst]);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, bw, bh);
    }

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, w, h);
    glBindTexture(GL_TEXTURE_2D, texFull);
    glDisable(GL_BLEND);
    glColor3f(1.0f, 1.0f, 1.0f);
    texQuad(0, 0, fu, fv);

    glBindTexture(GL_TEXTURE_2D, texSmall[0]);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glColor3f(bloomStrength, bloomStrength, bloomStrength);
    texQuad(0, 0, su, sv);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

static void saveShot(int w, int h) {
    std::vector<unsigned char> px((size_t)w * h * 3), flipped((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h; ++y) memcpy(&flipped[(size_t)y * w * 3], &px[(size_t)(h - 1 - y) * w * 3], (size_t)w * 3);
    FILE* f = fopen(shotPath, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(flipped.data(), 1, flipped.size(), f);
    fclose(f);
}

void display() {
    float tsec = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    const Theme& th = curTheme();
    int w = curWinW, h = curWinH;

    {   // smoothed FPS for the optional counter
        static double lastD = 0.0;
        double tD = nowSec();
        if (lastD > 0.0) {
            float inst = (float)(1.0 / std::max(1e-4, tD - lastD));
            fpsSmooth = (fpsSmooth <= 0.0f) ? inst : fpsSmooth * 0.92f + inst * 0.08f;
        }
        lastD = tD;
    }
    bool pipe = gfxOK && postOn && w > 16 && h > 16;
    int sw = w, sh = h;
    if (pipe) {   // render-resolution scale: the HDR scene can be rendered smaller and smoothly upscaled
        sw = std::max(16, (int)(w * cfgRenderScale + 0.5f));
        sh = std::max(16, (int)(h * cfgRenderScale + 0.5f));
    }
    if (pipe && !ensureTargets(sw, sh)) { postOn = false; pipe = false; }
    if (pipe) ensureSkyBaked(w, h);
    if (pipe) {
        glBindFramebuffer(GL_FRAMEBUFFER, msOK ? msFbo : sceneFbo);
        glViewport(0, 0, sw, sh);
    }

    glClearColor(th.nebA[0] * 0.03f, th.nebA[1] * 0.03f, th.nebA[2] * 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (state == S_TRANSIT || state == S_UPGRADE) drawTransit(tsec);
    else                                          drawWorld(tsec);

    if (pipe) {
        useProg(0);
        if (msOK) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFbo);
            glBlitFramebuffer(0, 0, sw, sh, 0, 0, sw, sh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        runPost(w, h, tsec);
    } else {
        applyBloom(w, h);
    }
    drawHUD(tsec);

    if (benchN > 0) {
        static double t0 = 0.0;
        static int cnt = 0;
        glFinish();
        double tn = nowSec();
        ++cnt;
        if (cnt == 40) t0 = tn;
        if (cnt == 40 + benchN) {
            fprintf(stderr, "[bench] %.3f ms/frame over %d frames\n", (tn - t0) * 1000.0 / benchN, benchN);
            exit(0);
        }
    }
    if (shotPath && ++shotFrame >= shotAt) {
        saveShot(w, h);
        exit(0);
    }
    glutSwapBuffers();
}

// ----------------------------------------------------------------------------
//  Simulation
// ----------------------------------------------------------------------------
void beginSuck() {
    state = S_SUCKING;
    stateTimer = 0.0f;
    suckRel = ship - bridge.pos;
    speed = 0.0f;
    drift = {0, 0, 0};

    // v1.2: sector clear bonuses (not affected by combo / x2)
    float par = 75.0f + 30.0f * (float)level;
    int timeBonus = std::max(0, (int)((par - levelTime) * 10.0f));
    int flawless = tookHit ? 0 : 500;
    int base = 1000 * level;
    score += base + timeBonus + flawless;

    // v1.7: record this sector's time
    sectorTimes[level - 1] = levelTime;
    runTime += levelTime;
    lastSectorPB = recordBestSector(level - 1, levelTime);
    char tbuf[16];
    fmtTime(levelTime, tbuf, sizeof(tbuf));

    char sub[128];
    snprintf(sub, sizeof(sub), "Time %s%s | Sector +%d | Pace +%d | Flawless +%d | Chain %d",
             tbuf, lastSectorPB ? " NEW BEST!" : "", base, timeBonus, flawless, comboBest);
    setBanner("SECTOR COMPLETE!", sub, 3.5f, 0.5f, 1.0f, 0.7f);

    spawnBurst(bridge.pos, 80, 0.3f, 0.6f, 1.0f);
    spawnShock(bridge.pos, 40.0f, 0.5f, 0.8f, 1.0f, 1.0f);
    playSfx(SFX_WORMHOLE);
    triggerRumble(0.5f, 0.8f, 1.2f);
}

void takeDamage(Vec3 away) {
    health--;
    invuln = cfgInvuln;
    hitFlash = 1.0f;
    camShake = 1.0f;
    speed *= 0.3f;
    drift = away * 0.6f;

    // v1.2: a hull hit breaks the chain and the flawless bonus
    tookHit = true;
    combo = 0;
    comboTimer = 0.0f;

    spawnBurst(ship, 45, 1.0f, 0.55f, 0.2f, 0.15f, 0.9f);
    spawnShock(ship, 11.0f, 1.0f, 0.45f, 0.2f, 0.5f);
    addPopup(ship + Vec3{0, 2, 0}, "-1 HULL", 1.0f, 0.3f, 0.3f);
    playSfx(SFX_HIT);
    triggerRumble(0.9f, 0.9f, 0.45f);

    if (health <= 0) {
        state = S_GAMEOVER;
        stateTimer = 0.0f;
        shipVisible = false;
        endRun(false);          // v1.7: record the run, clear the checkpoint
        deathPos = ship;
        spawnBurst(ship, 260, 1.0f, 0.5f, 0.15f, 0.2f, 1.2f);
        spawnBurst(ship, 120, 1.0f, 0.9f, 0.5f, 0.1f, 0.6f);
        flashAlpha = 0.8f; flashR = 1.0f; flashG = 0.6f; flashB = 0.3f;
        bannerTimer = 0.0f;
        playSfx(SFX_EXPLOSION);
        triggerRumble(1.0f, 1.0f, 0.9f);
    } else {
        setBanner("HULL BREACH!", "Shields online - brief invincibility", 2.0f, 1.0f, 0.4f, 0.3f);
    }
}

// v1.2: every source of player damage goes through here so the Shield power-up can absorb it
void hurtPlayer(Vec3 away) {
    if (invuln > 0.0f || dashTimer > 0.0f) return;
    if (shieldTimer > 0.0f) {
        shieldTimer = 0.0f;
        invuln = 1.5f;
        hitFlash = 0.35f;
        camShake = std::max(camShake, 0.6f);
        speed *= 0.6f;
        drift = away * 0.4f;
        spawnBurst(ship, 60, 0.4f, 0.9f, 1.0f, 0.15f, 0.8f);
        spawnShock(ship, 14.0f, 0.4f, 0.9f, 1.0f, 0.5f);
        addPopup(ship + Vec3{0, 2, 0}, "SHIELD BROKEN", 0.5f, 0.95f, 1.0f);
        playSfx(SFX_HIT);
        triggerRumble(0.5f, 0.5f, 0.25f);
        return;
    }
    takeDamage(away);
}

void damageEnemy(Enemy& e, Vec3 hitPos, float dmg = 1.0f) {
    if (!e.active) return;
    e.hp -= dmg;
    e.flash = 0.15f;
    spawnBurst(hitPos, 10, 1.0f, 0.8f, 0.4f, 0.05f, 0.3f);
    if (e.hp > 0.0f) return;

    e.active = false;
    if (e.counts) enemiesKilled++;
    bool boss = (e.type == ET_BOSS);
    int base = boss ? 3000 : (e.type == ET_GUNSHIP ? 400 : (e.type == ET_KAMIKAZE ? 150 : 250));
    int gained = addScore(base);
    spawnBurst(e.pos, boss ? 500 : 160, 1.0f, 0.45f, 0.15f, 0.1f, boss ? 1.6f : 1.0f);
    spawnBurst(e.pos, boss ? 200 : 60, 1.0f, 0.9f, 0.5f, 0.05f, 0.5f);
    char pt[24];
    snprintf(pt, sizeof(pt), boss ? "GUARDIAN DOWN +%d" : "ENEMY DOWN +%d", gained);
    addPopup(e.pos + Vec3{0, 3, 0}, pt, 1.0f, 0.5f, 0.3f);
    camShake = std::max(camShake, boss ? 1.0f : 0.35f);
    spawnShock(e.pos, boss ? 70.0f : 24.0f, 1.0f, 0.5f, 0.2f, 0.7f);
    playSfx(SFX_EXPLOSION);
    triggerRumble(0.6f, 0.75f, boss ? 0.8f : 0.3f);

    // enemies sometimes drop a power-up (bosses drop three)
    if (boss) {
        for (int k = 0; k < 3; ++k)
            spawnPowerUp(e.pos + Vec3{rnd(-8.0f, 8.0f), rnd(-4.0f, 4.0f), rnd(-8.0f, 8.0f)}, rand() % PW_COUNT);
    } else if (frand() < (e.type == ET_KAMIKAZE ? 0.15f : 0.40f)) {
        spawnPowerUp(e.pos, rand() % PW_COUNT);
    }

    if (e.counts && enemiesKilled >= enemiesReq)
        setBanner("ALL ENEMY SHIPS DESTROYED", "", 2.0f, 1.0f, 0.6f, 0.3f);
    if (boss)
        setBanner("THE GUARDIAN IS DESTROYED!", "The final bridge awaits", 4.0f, 1.0f, 0.5f, 0.9f);
}

// ---- v1.2: area damage, mine chain reactions, destructible rocks ----
void blast(Vec3 pos, float radius, float enemyDmg, float obstDmg, bool hurtsPlayer);
void detonateMine(Obstacle& o, bool hurtsPlayer);

void damageObstacle(Obstacle& o, float dmg, Vec3 hitPos, bool hurtsPlayer) {
    if (!o.active) return;
    if (o.type == OB_MINE) { detonateMine(o, hurtsPlayer); return; }

    o.hp -= dmg;
    spawnBurst(hitPos, 6, 0.8f, 0.8f, 0.9f, 0.03f, 0.2f);
    if (o.hp > 0.0f) return;

    o.active = false;
    bool orb = (o.type == OB_ORBITER);
    int gained = addScore(orb ? 75 : 30);
    spawnBurst(o.pos, orb ? 90 : 60, orb ? 1.0f : 0.7f, orb ? 0.45f : 0.65f, orb ? 0.2f : 0.6f, 0.08f, 0.6f);
    spawnShock(o.pos, o.radius * 3.0f + 4.0f, 0.9f, 0.7f, 0.4f, 0.5f);
    char pt[24];
    snprintf(pt, sizeof(pt), "+%d", gained);
    addPopup(o.pos + Vec3{0, 2, 0}, pt, 0.95f, 0.9f, 0.7f);
    playSfx(SFX_EXPLOSION);
    if (frand() < (orb ? 0.35f : 0.14f)) spawnPowerUp(o.pos, rand() % PW_COUNT);
}

void detonateMine(Obstacle& o, bool hurtsPlayer) {
    if (!o.active) return;
    o.active = false;
    int gained = addScore(50);
    char pt[24];
    snprintf(pt, sizeof(pt), "MINE +%d", gained);
    addPopup(o.pos + Vec3{0, 2, 0}, pt, 1.0f, 0.7f, 0.3f);
    blast(o.pos, MINE_BLAST, 2.0f, 3.0f, hurtsPlayer);
}

void blast(Vec3 pos, float radius, float enemyDmg, float obstDmg, bool hurtsPlayer) {
    spawnBurst(pos, 140, 1.0f, 0.55f, 0.2f, 0.1f, 0.9f);
    spawnBurst(pos, 50, 1.0f, 0.9f, 0.6f, 0.05f, 0.4f);
    spawnShock(pos, radius * 1.6f, 1.0f, 0.6f, 0.25f, 0.7f);
    camShake = std::max(camShake, 0.3f);
    playSfx(SFX_EXPLOSION);

    if (enemyDmg > 0.0f) {
        for (auto& e : enemies) {
            if (!e.active) continue;
            if (length(e.pos - pos) < radius + e.radius) damageEnemy(e, e.pos, enemyDmg);
        }
    }
    for (auto& o : obstacles) {
        if (!o.active) continue;
        if (length(o.pos - pos) < radius + o.radius) damageObstacle(o, obstDmg, o.pos, hurtsPlayer);
    }
    if (hurtsPlayer && state == S_FLYING && shipVisible) {
        float d = length(ship - pos);
        if (d < radius * 0.75f) hurtPlayer(normalize(ship - pos));
    }
}

void pulseBomb() {
    bombCooldown = 1.5f;
    flashAlpha = 0.6f; flashR = 1.0f; flashG = 0.4f; flashB = 0.7f;
    spawnShock(ship, BOMB_RADIUS, 1.0f, 0.4f, 0.7f, 0.9f);
    spawnShock(ship, BOMB_RADIUS * 0.6f, 1.0f, 0.8f, 0.9f, 0.6f);
    for (auto& b : bolts) {
        if (b.enemy && length(b.pos - ship) < BOMB_RADIUS * 1.4f) {
            spawnBurst(b.pos, 4, 1.0f, 0.5f, 0.8f, 0.02f, 0.15f);
            b.life = 0.0f;
        }
    }
    blast(ship, BOMB_RADIUS, 4.0f, 8.0f, false);
    addPopup(ship + Vec3{0, 3, 0}, "PULSE BOMB!", 1.0f, 0.4f, 0.7f);
    playSfx(SFX_WORMHOLE);
    camShake = std::max(camShake, 0.7f);
    triggerRumble(0.8f, 0.8f, 0.5f);
}

void updateEnemies(float dt, float tk) {
    bool combat = (state == S_FLYING);
    for (auto& e : enemies) {
        if (!e.active) continue;
        e.phase += dt * 0.6f;
        if (e.flash > 0.0f) e.flash = std::max(0.0f, e.flash - dt);

        Vec3 toShip = ship - e.pos;
        float dist = length(toShip);

        // ---- boss phase changes ----
        if (e.type == ET_BOSS) {
            float ratio = e.hp / e.maxHp;
            int ns = (ratio < 0.34f) ? 2 : (ratio < 0.67f ? 1 : 0);
            if (ns > e.stage) {
                e.stage = ns;
                e.fireTimer = 1.0f; e.special = 1.5f; e.aux = 3.0f;
                spawnShock(e.pos, 55.0f, 0.9f, 0.3f, 1.0f, 0.9f);
                camShake = std::max(camShake, 0.8f);
                playSfx(SFX_BRIDGE_OPEN);
                setBanner(ns == 1 ? "THE GUARDIAN IS ANGRY" : "THE GUARDIAN IS ENRAGED!",
                          ns == 1 ? "Bullet rings incoming" : "Bullet rings + drone swarms", 3.0f, 0.9f, 0.3f, 1.0f);
            }
        }

        float engageR = (e.type == ET_BOSS) ? 170.0f : (e.type == ET_KAMIKAZE ? 150.0f : 110.0f);
        if (combat) {
            if (!e.hunting && dist < engageR) e.hunting = true;
            else if (e.hunting && dist > engageR * 2.1f) { e.hunting = false; e.anchor = e.pos; }
        } else if (e.hunting && e.type != ET_BOSS) {
            e.hunting = false;
            e.anchor = e.pos;
        }

        Vec3 target;
        float maxSp;
        if (e.type == ET_BOSS) {
            target = e.anchor + Vec3{cosf(e.phase) * 22.0f, sinf(e.phase * 0.7f) * 8.0f, sinf(e.phase) * 22.0f};
            maxSp = e.maxSpeed * (1.0f + 0.3f * e.stage);
        } else if (e.hunting && e.type == ET_KAMIKAZE) {
            target = ship;
            maxSp = e.maxSpeed;
        } else if (e.hunting) {
            float orb = (e.type == ET_GUNSHIP) ? 48.0f : 38.0f;
            target = ship + Vec3{cosf(e.phase * 1.3f) * orb, 7.0f * sinf(e.phase), sinf(e.phase * 1.3f) * orb};
            maxSp = e.maxSpeed;
        } else {
            target = e.anchor + Vec3{cosf(e.phase) * 14.0f, sinf(e.phase * 0.7f) * 5.0f, sinf(e.phase) * 14.0f};
            maxSp = 0.2f;
        }
        Vec3 to = target - e.pos;
        float spd = (e.hunting && e.type == ET_KAMIKAZE) ? maxSp : std::min(maxSp, length(to) * 0.08f);
        Vec3 desiredVel = normalize(to) * spd;
        e.vel += (desiredVel - e.vel) * (1.0f - powf(0.9f, tk));
        e.pos += e.vel * tk;

        e.pos.x = clampf(e.pos.x, boundMin.x, boundMax.x);
        e.pos.y = clampf(e.pos.y, boundMin.y, boundMax.y);
        e.pos.z = clampf(e.pos.z, boundMin.z, boundMax.z);

        Vec3 ft = e.fwd;
        if (e.hunting) ft = normalize(toShip);
        else if (length(e.vel) > 0.03f) ft = normalize(e.vel);
        e.fwd = normalize(e.fwd + (ft - e.fwd) * (1.0f - powf(0.9f, tk)));
        if (length(e.fwd) < 0.01f) e.fwd = {0, 0, 1};

        // ---- kamikaze: explode on contact ----
        if (combat && e.type == ET_KAMIKAZE && e.hunting && dist < e.radius + SHIP_RADIUS + 2.5f) {
            e.active = false;
            if (e.counts) enemiesKilled++;
            blast(e.pos, 9.0f, 0.0f, 3.0f, true);
            continue;
        }

        // ---- fighters & gunships ----
        if (combat && e.hunting && e.type != ET_BOSS && e.type != ET_KAMIKAZE && dist < 95.0f) {
            e.fireTimer -= dt;
            if (e.fireTimer <= 0.0f) {
                Vec3 dirTo = normalize(toShip);
                if (dot(e.fwd, dirTo) > 0.85f && bolts.size() < 500) {
                    float ticks = dist / ENEMY_BOLT_SPEED;
                    Vec3 shipVel = forwardVec() * speed + drift;
                    Vec3 aim = ship + shipVel * (ticks * 0.5f)
                             + Vec3{rnd(-2.5f, 2.5f), rnd(-2.5f, 2.5f), rnd(-2.5f, 2.5f)};
                    Vec3 muzzle = e.pos + e.fwd * (e.radius + 0.6f);
                    Vec3 d0 = normalize(aim - muzzle);
                    Vec3 rgt = normalize(cross(d0, Vec3{0.0f, 1.0f, 0.0f}));
                    int n = (e.type == ET_GUNSHIP) ? 3 : 1;
                    for (int k = 0; k < n; ++k) {
                        Vec3 dk = normalize(d0 + rgt * ((k - (n - 1) * 0.5f) * 0.08f));
                        bolts.push_back(makeBolt(muzzle, dk * ENEMY_BOLT_SPEED, 3.5f, true));
                    }
                    spawnBurst(muzzle, 4, 1.0f, 0.3f, 0.15f, 0.02f, 0.1f);
                    playSfx(SFX_ENEMY_LASER);
                    e.fireTimer = e.fireInterval * rnd(0.8f, 1.2f) * diffFire();
                } else {
                    e.fireTimer = 0.0f;
                }
            }
        }

        // ---- the Guardian ----
        if (e.type == ET_BOSS && combat && e.hunting && bolts.size() < 450) {
            Vec3 muzzle = e.pos + e.fwd * (e.radius + 1.0f);
            e.fireTimer -= dt;
            if (e.fireTimer <= 0.0f) {
                float bs = ENEMY_BOLT_SPEED * 1.05f;
                Vec3 aim = ship + (forwardVec() * speed + drift) * (dist / bs * 0.4f);
                Vec3 da = normalize(aim - muzzle);
                Vec3 rgt = normalize(cross(da, Vec3{0.0f, 1.0f, 0.0f}));
                for (int k = -2; k <= 2; ++k)
                    bolts.push_back(makeBolt(muzzle, normalize(da + rgt * (k * 0.09f)) * bs, 4.0f, true));
                playSfx(SFX_ENEMY_LASER);
                e.fireTimer = ((e.stage == 2) ? 1.1f : (e.stage == 1 ? 1.4f : 1.8f)) * diffFire();
            }
            if (e.stage >= 1) {
                e.special -= dt;
                if (e.special <= 0.0f) {
                    const int N = 18;
                    float off = e.phase * 2.0f;
                    for (int i = 0; i < N; ++i) {
                        float a = 2.0f * PI * i / N + off;
                        Vec3 dr = normalize(Vec3{cosf(a), 0.12f * sinf(off * 3.0f + i), sinf(a)});
                        bolts.push_back(makeBolt(e.pos + dr * (e.radius + 0.5f), dr * (ENEMY_BOLT_SPEED * 0.8f), 4.5f, true));
                    }
                    spawnShock(e.pos, e.radius * 2.0f, 1.0f, 0.4f, 0.9f, 0.5f);
                    playSfx(SFX_ENEMY_LASER);
                    e.special = (e.stage == 2) ? 3.0f : 4.0f;
                }
            }
            if (e.stage >= 2) {
                e.aux -= dt;
                if (e.aux <= 0.0f) {
                    summonQueue.push_back(e.pos + Vec3{ 9.0f, 0.0f, 0.0f});
                    summonQueue.push_back(e.pos + Vec3{-9.0f, 0.0f, 0.0f});
                    addPopup(e.pos + Vec3{0, 10, 0}, "DRONES INBOUND", 1.0f, 0.5f, 0.3f);
                    e.aux = 9.0f;
                }
            }
        }
    }

    // spawn queued drones after the loop (the vector must not grow mid-iteration)
    if (!summonQueue.empty()) {
        for (const auto& sp : summonQueue) {
            Enemy k = makeEnemy(ET_KAMIKAZE, sp, level, false);
            k.hunting = true;
            enemies.push_back(k);
        }
        summonQueue.clear();
        lockedEnemy = nullptr;
        hasAimLock = false;
    }
}

void updateBolts(float dt, float tk) {
    float assistCone = aiming ? 0.82f : 0.90f;
    float assistTurn = (aiming ? 0.22f : 0.12f) * cfgAimAssist;

    for (auto& b : bolts) {
        if (!b.enemy) {
            Vec3 vd = normalize(b.vel);
            const Enemy* best = nullptr;
            float bestDot = assistCone;
            for (const auto& e : enemies) {
                if (!e.active) continue;
                Vec3 d = (e.pos + e.vel * 4.0f) - b.pos;
                float dl = length(d);
                if (dl > 110.0f || dl < 0.5f) continue;
                float cosA = dot(vd, d * (1.0f / dl));
                if (cosA > bestDot) { bestDot = cosA; best = &e; }
            }
            if (best) {
                Vec3 leadPos = best->pos + best->vel * (length(best->pos - b.pos) / PLAYER_BOLT_SPEED * 0.65f);
                Vec3 d = normalize(leadPos - b.pos);
                b.vel = normalize(vd + (d - vd) * (assistTurn * tk)) * PLAYER_BOLT_SPEED;
            }
        }

        b.pos += b.vel * tk;
        b.life -= dt;
        if (b.life <= 0.0f) continue;

        if (!b.enemy) {
            for (auto& e : enemies) {
                if (!e.active) continue;
                if (length(b.pos - e.pos) < e.radius + 0.7f) {
                    damageEnemy(e, b.pos, boltDamageVal());
                    b.life = 0.0f;
                    break;
                }
            }
            if (b.life <= 0.0f) continue;
            for (auto& o : obstacles) {
                if (!o.active) continue;
                if (length(b.pos - o.pos) < o.radius * 0.9f + 0.4f) {
                    damageObstacle(o, boltDamageVal(), b.pos, true);
                    b.life = 0.0f;
                    break;
                }
            }
        } else if (state == S_FLYING && shipVisible) {
            float dS = length(b.pos - ship);
            if (dS < b.minD) {
                b.minD = dS;
            } else if (!b.graze && b.minD < GRAZE_DIST && b.minD > SHIP_RADIUS + 0.6f && dS > b.minD + 0.05f) {
                // v1.3: GRAZE - slipped past a bolt
                b.graze = true;
                int g = addScore(25);
                char pt[24];
                snprintf(pt, sizeof(pt), "GRAZE +%d", g);
                addPopup(ship + Vec3{0, 2.5f, 0}, pt, 0.6f, 0.9f, 1.0f);
                energy = std::min(maxEnergyVal(), energy + 4.0f);
            }
            if (dS < SHIP_RADIUS + 0.6f && dashTimer <= 0.0f) {
                b.life = 0.0f;
                if (invuln > 0.0f) {
                    spawnBurst(b.pos, 12, 0.4f, 0.9f, 1.0f, 0.05f, 0.3f);
                } else {
                    hurtPlayer(normalize(b.vel));
                }
            }
        }
    }
    bolts.erase(
        std::remove_if(bolts.begin(), bolts.end(),
                       [](const Bolt& b) { return b.life <= 0.0f; }),
        bolts.end());
}

// v1.2: homing missiles
void updateMissiles(float dt, float tk) {
    for (auto& m : missiles) {
        if (m.life <= 0.0f) continue;

        // (re)acquire target
        Vec3 vd = normalize(m.vel);
        if (m.target < 0 || m.target >= (int)enemies.size() || !enemies[m.target].active) {
            m.target = -1;
            float bestD = 1e9f;
            for (int i = 0; i < (int)enemies.size(); ++i) {
                if (!enemies[i].active) continue;
                Vec3 d = enemies[i].pos - m.pos;
                float dl = length(d);
                if (dl < 1.0f || dl > 180.0f) continue;
                float c = dot(vd, d * (1.0f / dl));
                if (c > 0.35f && dl < bestD) { bestD = dl; m.target = i; }
            }
        }

        float spd = std::min(MISSILE_SPEED, length(m.vel) + 0.05f * tk);
        if (m.target >= 0) {
            const Enemy& te = enemies[m.target];
            Vec3 d = normalize((te.pos + te.vel * 3.0f) - m.pos);
            float turn = std::min(1.0f, 0.16f * tk);
            vd = normalize(vd + (d - vd) * turn);
        }
        m.vel = vd * spd;
        m.pos += m.vel * tk;
        m.life -= dt;
        spawnBurst(m.pos, 1, 1.0f, 0.65f, 0.25f, 0.0f, 0.03f);

        bool boom = (m.life <= 0.0f);
        if (!boom) {
            for (const auto& e : enemies) {
                if (!e.active) continue;
                if (length(m.pos - e.pos) < e.radius + 1.2f) { boom = true; break; }
            }
        }
        if (!boom) {
            for (const auto& o : obstacles) {
                if (!o.active) continue;
                if (length(m.pos - o.pos) < o.radius * 0.9f + 0.8f) { boom = true; break; }
            }
        }
        if (boom) {
            m.life = 0.0f;
            blast(m.pos, MISSILE_BLAST, 3.0f, 4.0f, false);
        }
    }
    missiles.erase(
        std::remove_if(missiles.begin(), missiles.end(),
                       [](const Missile& m) { return m.life <= 0.0f; }),
        missiles.end());
}

void updateFlying(float dt, float tk) {
    levelTime += dt;

    // v1.3: ion storm check
    inIon = false;
    for (const auto& c : clouds)
        if (length(ship - c.pos) < c.radius) { inIon = true; break; }

    bool keyLeft  = keys['a'];
    bool keyRight = keys['d'];
    bool keyUp    = keys['r'];
    bool keyDown  = keys['f'];

    // Left Shift for boost (polls both GLUT special key and Win32 VK_LSHIFT for multi-key reliability)
    bool lShiftDown = specialKeys[GLUT_KEY_SHIFT_L];
#ifdef _WIN32
    if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) lShiftDown = true;
#endif

    // Swapped Triggers: RT = Aim, LT = Shoot
    aiming   = mouseRightDown || (padRT > 0.15f);

    // ---- v1.2: boost now costs energy ----
    bool wantBoost = lShiftDown || padA || padX;
    if (energyLocked && energy >= ENERGY_UNLOCK) energyLocked = false;
    boosting = wantBoost && !energyLocked && energy > 0.0f;
    if (boosting) {
        energy -= BOOST_DRAIN * dt;
        energyDelay = ENERGY_DELAY;
        if (energy <= 0.0f) {
            energy = 0.0f;
            energyLocked = true;
            boosting = false;
            addPopup(ship + Vec3{0, 2, 0}, "ENERGY DEPLETED", 1.0f, 0.5f, 0.2f);
        }
    } else {
        energyDelay -= dt;
        if (energyDelay <= 0.0f) energy = std::min(maxEnergyVal(), energy + energyRegenVal() * dt);
    }
    if (inIon) {
        energy = std::max(0.0f, energy - ION_DRAIN * dt);
        energyDelay = ENERGY_DELAY;
        if (energy <= 0.0f) { energyLocked = true; boosting = false; }
    }

    float acceleration = boosting ? 0.055f : 0.025f;
    if (keys['w'] || lShiftDown || padA || padUp) speed += acceleration * tk;
    if (keys['s'] || padB || padDown)             speed -= acceleration * tk;

    // Gentle auto-cruise on Xbox controller when neither brake nor thrust is held
    if (cfgAutoCruise && padConnected && !padA && !padB && !padUp && !padDown && !keys['w'] && !keys['s'] && !lShiftDown) {
        float cruise = 0.42f;
        speed += (cruise - speed) * (1.0f - powf(0.95f, tk));
    } else {
        speed *= powf(0.985f, tk);
    }
    speed = clampf(speed, -0.35f, boosting ? 1.0f : 0.6f);

    // ---- Target Acquisition & Controller Aim Assist ----
    Vec3 f = forwardVec();
    lockedEnemy = nullptr;
    hasAimLock = false;
    float bestCone = aiming ? 0.86f : 0.92f; // wider lock cone when holding RT / RMB
    float maxLockDist = aiming ? 145.0f : 115.0f;

    for (const auto& e : enemies) {
        if (!e.active) continue;
        Vec3 toE = e.pos - ship;
        float dist = length(toE);
        if (dist > maxLockDist || dist < 1.5f) continue;
        float c = dot(f, toE * (1.0f / dist));
        if (c > bestCone) {
            bestCone = c;
            lockedEnemy = &e;
        }
    }

    // Calculate predicted lead point and angular error for aim assist
    float assistSlowdown = 1.0f;
    float dyawTarget = 0.0f, dpitchTarget = 0.0f;
    if (lockedEnemy) {
        hasAimLock = true;
        float dist = length(lockedEnemy->pos - ship);
        float leadTicks = (dist / PLAYER_BOLT_SPEED) * 0.7f;
        aimAssistPoint = lockedEnemy->pos + lockedEnemy->vel * leadTicks;

        Vec3 toLead = aimAssistPoint - ship;
        float leadDist = std::max(0.01f, length(toLead));
        float targetYaw = atan2f(-toLead.x, -toLead.z);
        float targetPitch = asinf(clampf(toLead.y / leadDist, -1.0f, 1.0f));

        dyawTarget = wrapAngle(targetYaw - yaw);
        dpitchTarget = clampf(targetPitch - pitch, -1.0f, 1.0f);

        // Slow down thumbstick sensitivity when crosshair is over the target (friction)
        assistSlowdown = 1.0f - (1.0f - (aiming ? 0.55f : 0.72f)) * clampf(cfgAimAssist, 0.0f, 1.0f);
    }

    // Base steering sensitivity
    float aimSens = (aiming ? 0.60f : 1.0f) * assistSlowdown;

    // Combine Left Stick + (when Aiming) Right Stick for fine micro-aiming
    float stickYaw   = padLX + (aiming ? padRX * 0.45f : 0.0f);
    float stickPitch = padLY + (aiming ? padRY * 0.45f : 0.0f);
    float stickMag   = sqrtf(stickYaw * stickYaw + stickPitch * stickPitch);

    float yawInput = 0.0f, pitchInput = 0.0f;
    if (keyLeft)  yawInput += 1.4f;
    if (keyRight) yawInput -= 1.4f;
    if (keyUp)    pitchInput += 1.0f;
    if (keyDown)  pitchInput -= 1.0f;

    yawInput   -= stickYaw   * 1.95f * aimSens * cfgPadSens;
    pitchInput += stickPitch * 1.45f * aimSens * cfgPadSens * (cfgInvertY ? -1.0f : 1.0f);

    yaw   += yawInput * dt - mouseDx * 0.0026f * aimSens * cfgMouseSens;
    pitch += pitchInput * dt - mouseDy * 0.0024f * aimSens * cfgMouseSens * (cfgInvertY ? -1.0f : 1.0f);
    mouseDx = 0.0f;
    mouseDy = 0.0f;

    // Rotational Magnetism: gently pull nose toward locked target when steering or holding Aim
    if (hasAimLock && (aiming || stickMag > 0.05f || mouseLeftDown || padLT > 0.15f)) {
        float pullStrength = (aiming ? 4.2f : 2.1f) * (padConnected ? 1.25f : 0.85f) * cfgAimAssist;
        float pull = 1.0f - expf(-pullStrength * dt);
        yaw   += dyawTarget * pull;
        pitch += dpitchTarget * pull;
    }

    pitch = clampf(pitch, -1.05f, 1.05f);

    // Xbox Right Thumbstick camera orbit control (when not in RT Precision Aim)
    if (!aiming && (fabsf(padRX) > 0.01f || fabsf(padRY) > 0.01f)) {
        camYawOff   = clampf(camYawOff + padRX * 2.2f * dt, -1.2f, 1.2f);
        camPitchOff = clampf(camPitchOff + padRY * 1.5f * dt, -0.7f, 0.8f);
    } else {
        camYawOff   *= powf(0.84f, tk);
        camPitchOff *= powf(0.84f, tk);
    }

    // ---- Fixed Q/E & LB/RB Rolling Animation ----
    bool rollLeft  = keys['q'] || padLB;
    bool rollRight = keys['e'] || padRB;
    {   // v1.3: double-tap a roll key to DASH
        bool tapL = rollLeft && !prevRollL, tapR = rollRight && !prevRollR;
        prevRollL = rollLeft; prevRollR = rollRight;
        for (int sd = 0; sd < 2; ++sd) {
            bool tap = (sd == 0) ? tapL : tapR;
            if (!tap) continue;
            float dir = (sd == 0) ? 1.0f : -1.0f;
            if (gameTime - lastRollTap[sd] < DASH_TAP_WINDOW && dashTimer <= 0.0f && energy >= DASH_COST) {
                energy -= DASH_COST;
                energyDelay = ENERGY_DELAY;
                dashTimer = DASH_TIME;
                dashDir = dir;
                Vec3 rightH = {cosf(yaw), 0.0f, -sinf(yaw)};
                drift += rightH * (-dir * 0.40f);
                spawnBurst(ship, 30, 0.6f, 0.9f, 1.0f, 0.1f, 0.5f);
                spawnShock(ship, 9.0f, 0.6f, 0.9f, 1.0f, 0.4f);
                playSfx(SFX_DASH);
                lastRollTap[sd] = -10.0f;
            } else {
                lastRollTap[sd] = gameTime;
            }
        }
    }
    if (rollLeft || rollRight) {
        float dir = rollLeft ? 1.0f : -1.0f;
        rollVel = lerpf(rollVel, dir * 4.8f, 1.0f - powf(0.80f, tk));
        roll = wrapAngle(roll + rollVel * dt);
        Vec3 rightH = {cosf(yaw), 0.0f, -sinf(yaw)};
        drift += rightH * (-dir * 0.012f * tk);
    } else {
        rollVel *= powf(0.82f, tk);
        float turnBank = clampf(yawInput * 0.32f, -0.55f, 0.55f);
        float diff = wrapAngle(turnBank - roll);
        roll = wrapAngle(roll + diff * (1.0f - powf(0.88f, tk)) + rollVel * dt);
    }
    if (dashTimer > 0.0f) {
        roll = wrapAngle(roll + dashDir * 14.0f * dt);
        spawnBurst(ship, 2, 0.6f, 0.9f, 1.0f, 0.03f, 0.2f);
    }

    // ---- bridge gravity (only when open) / repulsion (when sealed) ----
    nearIntensity = 0.0f;
    {
        Vec3 d = bridge.pos - ship;
        float dist = length(d);
        if (bridgeOpen) {
            if (dist < GRAV_RADIUS && dist > 0.001f) {
                float k = 1.0f - dist / GRAV_RADIUS;
                float pull = 0.045f * k * k + 0.004f * k;
                drift += d * (pull / dist * tk);
                nearIntensity = k;
            }
            if (dist < LOCK_DIST) { beginSuck(); return; }
        } else if (dist < MOUTH_RADIUS + 6.0f && dist > 0.001f) {
            Vec3 n = d * (-1.0f / dist);
            drift += n * (0.05f * tk);
            if (dist < MOUTH_RADIUS + 1.5f) {
                ship = bridge.pos + n * (MOUTH_RADIUS + 1.5f);
                speed *= 0.5f;
            }
            if (sealCooldown <= 0.0f) {
                setBanner("BRIDGE SEALED", "Collect the coins, activate beacons, and clear enemies first", 2.5f, 1.0f, 0.4f, 0.25f);
                sealCooldown = 4.0f;
            }
        }
    }
    drift = drift * powf(0.97f, tk);

    // ---- move ----
    ship += (forwardVec() * speed + drift) * tk;

    // ---- soft sector boundary ----
    outOfBounds = false;
    Vec3 push = {0, 0, 0};
    if (ship.x < boundMin.x) push.x = boundMin.x - ship.x; else if (ship.x > boundMax.x) push.x = boundMax.x - ship.x;
    if (ship.y < boundMin.y) push.y = boundMin.y - ship.y; else if (ship.y > boundMax.y) push.y = boundMax.y - ship.y;
    if (ship.z < boundMin.z) push.z = boundMin.z - ship.z; else if (ship.z > boundMax.z) push.z = boundMax.z - ship.z;
    if (push.x != 0.0f || push.y != 0.0f || push.z != 0.0f) {
        outOfBounds = true;
        Vec3 acc = push * 0.006f;
        float al = length(acc);
        if (al > 0.08f) acc = acc * (0.08f / al);
        drift += acc * tk;
    }

    // ---- engine trail ----
    if (fabsf(speed) > 0.2f) spawnTrail(boosting ? 2.0f : 1.0f);

    // ---- weapons (LMB or Xbox LT - swapped as requested) ----
    fireTimer = std::max(0.0f, fireTimer - dt);
    bool firing = mouseLeftDown || (padLT > 0.15f);
    if (firing && fireTimer <= 0.0f && bolts.size() < 500) {
        fireTimer = FIRE_INTERVAL * (rapidTimer > 0.0f ? 0.5f : 1.0f);
        Vec3 fNow = forwardVec();
        fireSide = -fireSide;
        Vec3 muzzle = shipLocalToWorld({0.85f * fireSide, -0.04f, -0.85f});
        // Auto-converge cannons onto aimAssistPoint when locked on
        Vec3 convergePt = hasAimLock ? aimAssistPoint : (ship + fNow * 65.0f);
        Vec3 dir = normalize(convergePt - muzzle);
        bolts.push_back(makeBolt(muzzle, dir * PLAYER_BOLT_SPEED, 1.8f, false));
        spawnBurst(muzzle, 2, 0.4f, 0.9f, 1.0f, 0.02f, 0.08f);
        playSfx(SFX_LASER);
        triggerRumble(0.0f, 0.18f, 0.06f);
    }

    // ---- v1.2: homing missile launch (SPACE / R-Stick click) ----
    if (missileRequest) {
        missileRequest = false;
        if (missileAmmo > 0 && missileCooldown <= 0.0f && missiles.size() < 32) {
            missileAmmo--;
            missileCooldown = MISSILE_COOLDOWN;
            Vec3 fNow = forwardVec();
            Missile m;
            m.pos = shipLocalToWorld({0.0f, -0.30f, -1.2f});
            m.vel = fNow * (MISSILE_SPEED * 0.5f);
            m.life = 5.0f;
            m.target = lockedEnemy ? (int)(lockedEnemy - enemies.data()) : -1;
            missiles.push_back(m);
            spawnBurst(m.pos, 14, 1.0f, 0.7f, 0.3f, 0.03f, 0.2f);
            camShake = std::max(camShake, 0.15f);
            playSfx(SFX_MISSILE);
            triggerRumble(0.35f, 0.5f, 0.15f);
        } else if (missileAmmo <= 0) {
            addPopup(ship + Vec3{0, 2, 0}, "NO MISSILES", 1.0f, 0.4f, 0.3f);
        }
    }

    // ---- v1.3: pulse bomb ----
    if (bombRequest) {
        bombRequest = false;
        if (bombAmmo > 0 && bombCooldown <= 0.0f) {
            bombAmmo--;
            pulseBomb();
        } else if (bombAmmo <= 0) {
            addPopup(ship + Vec3{0, 2, 0}, "NO BOMBS", 1.0f, 0.4f, 0.3f);
        }
    }

    // ---- v1.3: wingman drone ----
    if (droneTimer > 0.0f) {
        droneAngle += 2.4f * dt;
        dronePos = ship + Vec3{cosf(droneAngle) * 5.0f, 2.0f + 0.8f * sinf(droneAngle * 2.0f), sinf(droneAngle) * 5.0f};
        droneFireTimer -= dt;
        if (droneFireTimer <= 0.0f) {
            int bi = -1;
            float bd = 100.0f;
            for (int i = 0; i < (int)enemies.size(); ++i) {
                if (!enemies[i].active) continue;
                float d = length(enemies[i].pos - dronePos);
                if (d < bd) { bd = d; bi = i; }
            }
            if (bi >= 0 && bolts.size() < 500) {
                Vec3 lead = enemies[bi].pos + enemies[bi].vel * (bd / PLAYER_BOLT_SPEED * 0.5f);
                Vec3 dir = normalize(lead - dronePos);
                bolts.push_back(makeBolt(dronePos, dir * PLAYER_BOLT_SPEED, 1.8f, false));
                spawnBurst(dronePos, 2, 0.4f, 1.0f, 0.8f, 0.02f, 0.08f);
                droneFireTimer = 0.45f;
            } else {
                droneFireTimer = 0.2f;
            }
        }
    }

    // ---- coins ----
    bool magnet = magnetTimer > 0.0f;
    float magR = magnet ? 30.0f : baseMagnetR();
    float magK = magnet ? 0.86f : 0.72f;
    for (auto& c : coins) {
        if (c.taken) continue;
        Vec3 d = ship - c.pos;
        float dist = length(d);
        if (dist < magR) {
            // Snap coin directly to ship position so it can't trail behind
            float pull = 1.0f - powf(magK, tk);
            c.pos += d * pull;
            d = ship - c.pos;
            dist = length(d);
        }
        if (dist < 3.5f) {
            c.taken = true;
            coinsGot++;
            int gained = addScore(100);
            spawnBurst(c.pos, 16, 1.0f, 0.85f, 0.25f, 0.05f, 0.3f);
            spawnShock(c.pos, 3.5f, 1.0f, 0.85f, 0.3f, 0.35f);
            char pt[24];
            snprintf(pt, sizeof(pt), "+%d", gained);
            addPopup(c.pos + Vec3{0, 1, 0}, pt, 1.0f, 0.9f, 0.3f);
            playSfx(SFX_COIN);
        }
    }

    // ---- collectible hearts ----
    float heartR = magnet ? 30.0f : (9.5f + (baseMagnetR() - 8.0f));
    float heartK = magnet ? 0.86f : 0.70f;
    for (auto& h : hearts) {
        if (h.taken) continue;
        Vec3 d = ship - h.pos;
        float dist = length(d);
        if (dist < heartR) {
            float pull = 1.0f - powf(heartK, tk);
            h.pos += d * pull;
            d = ship - h.pos;
            dist = length(d);
        }
        if (dist < 3.8f) {
            h.taken = true;
            spawnBurst(h.pos, 45, 1.0f, 0.25f, 0.5f, 0.08f, 0.45f);
            spawnShock(h.pos, 8.0f, 1.0f, 0.25f, 0.55f, 0.5f);
            playSfx(SFX_HEART);
            triggerRumble(0.25f, 0.4f, 0.2f);
            char pt[24];
            if (health < maxHealthVal()) {
                health++;
                int gained = addScore(150, false);
                snprintf(pt, sizeof(pt), "+1 HULL +%d", gained);
                addPopup(h.pos + Vec3{0, 2, 0}, pt, 1.0f, 0.35f, 0.6f);
            } else {
                int gained = addScore(300, false);
                snprintf(pt, sizeof(pt), "MAX HULL +%d", gained);
                addPopup(h.pos + Vec3{0, 2, 0}, pt, 1.0f, 0.5f, 0.75f);
            }
        }
    }

    // ---- v1.2: power-ups ----
    for (auto& p : powerups) {
        if (p.taken) continue;
        Vec3 d = ship - p.pos;
        float dist = length(d);
        if (dist < heartR) {
            float pull = 1.0f - powf(heartK, tk);
            p.pos += d * pull;
            d = ship - p.pos;
            dist = length(d);
        }
        if (dist < 4.0f) {
            p.taken = true;
            applyPowerUp(p.type, p.pos);
        }
    }

    // ---- v1.3: rings ----
    for (auto& r : rings) {
        float side = dot(ship - r.pos, r.dir);
        if (r.lastSide > 1e8f) { r.lastSide = side; continue; }
        if (!r.taken && r.lastSide * side < 0.0f) {
            Vec3 rel = (ship - r.pos) - r.dir * side;
            if (length(rel) < RING_RADIUS) {
                r.taken = true;
                ringsGot++;
                int g = addScore(200);
                energy = std::min(maxEnergyVal(), energy + 20.0f);
                char pt[24];
                snprintf(pt, sizeof(pt), "RING +%d", g);
                addPopup(r.pos + Vec3{0, 2, 0}, pt, 0.3f, 1.0f, 1.0f);
                spawnBurst(r.pos, 40, 0.3f, 1.0f, 1.0f, 0.1f, 0.5f);
                spawnShock(r.pos, RING_RADIUS * 1.6f, 0.3f, 1.0f, 1.0f, 0.5f);
                playSfx(SFX_BEACON);
            }
        }
        r.lastSide = side;
    }
    if (!ringBonusGiven && ringsTotal > 0 && ringsGot >= ringsTotal) {
        ringBonusGiven = true;
        int g = addScore(1500, false);
        char pt[24];
        snprintf(pt, sizeof(pt), "RING RUN +%d", g);
        addPopup(ship + Vec3{0, 3, 0}, pt, 0.3f, 1.0f, 1.0f);
        setBanner("RING RUN COMPLETE!", "Every ring cleared", 2.5f, 0.3f, 1.0f, 1.0f);
        playSfx(SFX_HEART);
    }

    // ---- beacons ----
    for (size_t i = 0; i < beacons.size(); ++i) {
        Beacon& b = beacons[i];
        if (b.active) continue;
        if (length(ship - b.pos) < 6.5f) {
            b.active = true;
            beaconsGot++;
            int gained = addScore(500);
            spawnBurst(b.pos, 90, 0.3f, 1.0f, 0.55f, 0.1f, 0.6f);
            spawnShock(b.pos, 18.0f, 0.3f, 1.0f, 0.55f, 0.8f);
            char pt[24];
            snprintf(pt, sizeof(pt), "BEACON +%d", gained);
            addPopup(b.pos + Vec3{0, 3, 0}, pt, 0.4f, 1.0f, 0.6f);
            char t[96];
            snprintf(t, sizeof(t), "BEACON %d/%d ONLINE", beaconsGot, (int)beacons.size());
            setBanner(t, "", 1.8f, 0.4f, 1.0f, 0.6f);
            playSfx(SFX_BEACON);
            triggerRumble(0.4f, 0.5f, 0.25f);
        }
    }

    // ---- goals complete -> open the bridge ----
    if (!bridgeOpen && coinsGot >= coinsReq && beaconsGot >= (int)beacons.size()
        && enemiesKilled >= enemiesReq) {
        bridgeOpen = true;
        flashAlpha = 0.7f; flashR = 0.7f; flashG = 0.85f; flashB = 1.0f;
        spawnBurst(bridge.pos, 200, 0.5f, 0.8f, 1.0f, 0.2f, 1.0f);
        spawnShock(bridge.pos, 70.0f, 0.6f, 0.9f, 1.0f, 1.4f);
        setBanner("THE BRIDGE IS OPEN!", "Fly into the gateway to reach the next sector", 5.0f, 0.5f, 0.95f, 1.0f);
        playSfx(SFX_BRIDGE_OPEN);
        triggerRumble(0.7f, 0.7f, 0.6f);
    }

    // ---- obstacle collisions ----
    for (auto& o : obstacles) {
        if (!o.active) continue;
        Vec3 d = ship - o.pos;
        float dist = length(d);
        float rr = o.radius * 0.9f + SHIP_RADIUS;
        if (dist >= rr) continue;
        if (invuln > 0.0f || dashTimer > 0.0f) continue;

        Vec3 n = (dist > 0.01f) ? d * (1.0f / dist) : forwardVec() * -1.0f;
        ship = o.pos + n * (rr + 0.6f);
        hurtPlayer(n);
        if (o.type == OB_MINE) {
            // chain reaction: the mine you hit blows up neighbours too (no extra damage to you)
            detonateMine(o, false);
        }
        break;
    }

    // ---- ramming an enemy ship ----
    if (invuln <= 0.0f && dashTimer <= 0.0f) {
        for (auto& e : enemies) {
            if (!e.active) continue;
            Vec3 d = ship - e.pos;
            float dist = length(d);
            float rr = e.radius + SHIP_RADIUS * 0.8f;
            if (dist >= rr) continue;
            Vec3 n = (dist > 0.01f) ? d * (1.0f / dist) : forwardVec() * -1.0f;
            ship = e.pos + n * (rr + 0.6f);
            e.vel += n * -0.4f;
            hurtPlayer(n);
            break;
        }
    }
}

void updateSucking(float dt) {
    float p = clampf(stateTimer / SUCK_TIME, 0.0f, 1.0f);
    Vec3 n = bridge.normal;
    Vec3 u = {n.z, 0.0f, -n.x};

    float ru = dot(suckRel, u);
    float rv = suckRel.y;
    float rn = dot(suckRel, n);

    float ang = 3.0f * PI * p;
    float f = powf(1.0f - p, 1.6f);
    float ru2 = (ru * cosf(ang) - rv * sinf(ang)) * f;
    float rv2 = (ru * sinf(ang) + rv * cosf(ang)) * f;
    float rn2 = rn * f;

    ship = bridge.pos + u * ru2 + Vec3{0.0f, rv2, 0.0f} + n * rn2;

    Vec3 dir = bridge.pos - ship;
    float dist = length(dir);
    if (dist > 0.05f) {
        yaw = atan2f(-dir.x, -dir.z);
        pitch = asinf(clampf(dir.y / dist, -1.0f, 1.0f));
    }
    roll = wrapAngle(roll + 7.5f * dt);
    shipScale = 1.0f - 0.9f * p * p;

    spawnTrail(1.5f);

    if (p >= 1.0f) {
        state = S_TRANSIT;
        stateTimer = 0.0f;
        flashAlpha = 1.0f; flashR = 0.6f; flashG = 0.8f; flashB = 1.0f;
        shipScale = 1.0f;
        roll = 0.0f;
        spawnBurst(bridge.pos, 100, 0.3f, 0.5f, 1.0f);
        playSfx(SFX_WORMHOLE);
    }
}

void updateUpgrade() {
    int w = curWinW, h = curWinH;
    float hy = (float)h - (float)mouseY;
    static int lmx = -1, lmy = -1;
    if (mouseX != lmx || mouseY != lmy) {
        lmx = mouseX; lmy = mouseY;
        for (int i = 0; i < 3; ++i) {
            float x0, y0, x1, y1;
            upgradeCardRect(i, w, h, x0, y0, x1, y1);
            if (mouseX >= x0 && mouseX <= x1 && hy >= y0 && hy <= y1) upSelected = i;
        }
    }
    if (uiLeft)  upSelected = (upSelected + 2) % 3;
    if (uiRight) upSelected = (upSelected + 1) % 3;
    int pick = -1;
    if (uiPick >= 0 && uiPick < 3) pick = uiPick;
    if (uiConfirm) pick = upSelected;
    if (uiClick) {
        for (int i = 0; i < 3; ++i) {
            float x0, y0, x1, y1;
            upgradeCardRect(i, w, h, x0, y0, x1, y1);
            if (mouseX >= x0 && mouseX <= x1 && hy >= y0 && hy <= y1) pick = i;
        }
    }
    if (pick >= 0 && stateTimer > 0.35f) {
        applyUpgrade(upChoice[pick]);
        playSfx(SFX_POWERUP);
        level++;
        buildLevel(level);
        beginLevel();
    }
}

void updateTransit() {
    if (stateTimer < TRANSIT_TIME) return;

    if (level >= MAX_LEVEL) {
        score += health * 500;
        endRun(true);           // v1.7: record the winning run
        state = S_VICTORY;
        stateTimer = 0.0f;
        shipVisible = false;
        flashAlpha = 1.0f; flashR = 1.0f; flashG = 0.95f; flashB = 0.8f;
        spawnBurst(bridge.pos, 200, 1.0f, 0.85f, 0.4f, 0.2f, 1.2f);
        playSfx(SFX_BRIDGE_OPEN);
    } else {
        state = S_UPGRADE;
        stateTimer = 0.0f;
        generateUpgradeChoices();
        flashAlpha = 0.5f; flashR = 0.6f; flashG = 0.8f; flashB = 1.0f;
    }
}

void updateEmerging() {
    float p = clampf(stateTimer / EMERGE_TIME, 0.0f, 1.0f);
    ship = entry.pos + entry.normal * (0.5f + p * 11.5f);
    shipScale = 0.2f + 0.8f * (p * p * (3.0f - 2.0f * p));
    spawnTrail(1.0f);

    if (p >= 1.0f) {
        state = S_FLYING;
        stateTimer = 0.0f;
        shipScale = 1.0f;
        speed = 0.4f;
    }
}

void updateCamera(float tk) {
    float cy = yaw + camYawOff;
    float cp = clampf(pitch + camPitchOff, -1.2f, 1.2f);
    Vec3 orbitDir = {-sinf(cy) * cosf(cp), sinf(cp), -cosf(cy) * cosf(cp)};
    float camDist = aiming ? 7.2f : 10.0f;
    Vec3 target = ship - orbitDir * camDist + Vec3{0.0f, aiming ? 2.1f : 3.0f, 0.0f};
    if (camSnap || state != S_FLYING) {
        camCur = target;
        camSnap = false;
    } else {
        camCur += (target - camCur) * (1.0f - powf(0.82f, tk));
    }
}

void updateTrails() {
    bool emit = shipVisible && (state == S_FLYING || state == S_EMERGING ||
                                state == S_SUCKING || state == S_MENU);
    if (emit) {
        trailL.push_back(shipLocalToWorld({-0.7f, -0.05f, 1.75f}));
        trailR.push_back(shipLocalToWorld({ 0.7f, -0.05f, 1.75f}));
    } else if (!trailL.empty()) {
        trailL.erase(trailL.begin());
        trailR.erase(trailR.begin());
    }
    while ((int)trailL.size() > TRAIL_LEN) {
        trailL.erase(trailL.begin());
        trailR.erase(trailR.begin());
    }
    while (trailL.size() > 2 && length(trailL.front() - trailL.back()) > 7.0f) {
        trailL.erase(trailL.begin());
        trailR.erase(trailR.begin());
    }
}

void stepGame(float dt, float tk) {
    gameTime += dt;
    stateTimer += dt;

    if (flashAlpha > 0.0f) flashAlpha = std::max(0.0f, flashAlpha - dt * 1.4f);
    if (hitFlash > 0.0f)   hitFlash = std::max(0.0f, hitFlash - dt * 1.8f);
    if (camShake > 0.0f)   camShake = std::max(0.0f, camShake - dt * 1.5f);
    if (bannerTimer > 0.0f) bannerTimer -= dt;
    if (sealCooldown > 0.0f) sealCooldown -= dt;
    if (invuln > 0.0f)     invuln = std::max(0.0f, invuln - dt);

    // ---- v1.2 timers ----
    if (shieldTimer > 0.0f)  shieldTimer  = std::max(0.0f, shieldTimer  - dt);
    if (rapidTimer > 0.0f)   rapidTimer   = std::max(0.0f, rapidTimer   - dt);
    if (magnetTimer > 0.0f)  magnetTimer  = std::max(0.0f, magnetTimer  - dt);
    if (doubleTimer > 0.0f)  doubleTimer  = std::max(0.0f, doubleTimer  - dt);
    if (missileCooldown > 0.0f) missileCooldown = std::max(0.0f, missileCooldown - dt);
    if (bombCooldown > 0.0f)    bombCooldown    = std::max(0.0f, bombCooldown    - dt);
    if (droneTimer > 0.0f)      droneTimer      = std::max(0.0f, droneTimer      - dt);
    if (dashTimer > 0.0f)       dashTimer       = std::max(0.0f, dashTimer       - dt);
    if (comboTimer > 0.0f) {
        comboTimer -= dt;
        if (comboTimer <= 0.0f) { comboTimer = 0.0f; combo = 0; }
    }

    openAmt += ((bridgeOpen ? 1.0f : 0.0f) - openAmt) * std::min(1.0f, dt * 1.2f);

    float fovTarget = cfgFov;
    if (state == S_FLYING) {
        if (aiming) fovTarget = cfgFov * 0.68f;
        else if (boosting && speed > 0.5f) fovTarget = cfgFov + 23.0f;
        else fovTarget = cfgFov + speed * 12.0f; // subtle dynamic FOV
    }
    currentFov += (fovTarget - currentFov) * std::min(1.0f, dt * 6.0f);

    updateObstacles(dt);
    updateEnemies(dt, tk);
    updateBolts(dt, tk);
    updateMissiles(dt, tk);
    updatePopups(dt);

    nearestDist = length(bridge.pos - ship);
    if (state != S_FLYING) {
        nearIntensity = 0.0f;
        aiming = false;
        hasAimLock = false;
        lockedEnemy = nullptr;
        missileRequest = false;
        bombRequest = false;
    }

    switch (state) {
        case S_MENU:
            ship = {0.0f, sinf(gameTime * 1.2f) * 0.4f, 4.0f};
            yaw = 0.0f; pitch = 0.0f;
            roll = sinf(gameTime * 0.8f) * 0.2f;
            shipScale = 1.0f;
            shipVisible = true;
            boosting = false;
            camYawOff += padRX * 1.5f * dt;
            camPitchOff = clampf(camPitchOff + padRY * 1.2f * dt, -0.5f, 0.5f);
            break;
        case S_FLYING:   updateFlying(dt, tk); break;
        case S_SUCKING:  updateSucking(dt);    break;
        case S_TRANSIT:  updateTransit();      break;
        case S_UPGRADE:  updateUpgrade();      break;
        case S_EMERGING: updateEmerging();     break;
        case S_GAMEOVER:
        case S_VICTORY:
            camYawOff += padRX * 1.5f * dt;
            if (state == S_VICTORY && frand() < dt * 2.5f) {
                Vec3 p = bridge.pos + Vec3{rnd(-30, 30), rnd(-5, 30), rnd(-30, 30)};
                spawnBurst(p, 90, frand(), frand(), frand() * 0.5f + 0.5f, 0.1f, 0.8f);
            }
            break;
    }

    for (auto& sh : shocks) sh.t += dt;
    shocks.erase(
        std::remove_if(shocks.begin(), shocks.end(),
                       [](const Shock& sh) { return sh.t >= sh.dur; }),
        shocks.end());

    dispScore += ((float)score - dispScore) * std::min(1.0f, dt * 6.0f);
    if (fabsf((float)score - dispScore) < 1.0f) dispScore = (float)score;

    updateTrails();
    updateParticles(tk);
    updateCamera(tk);
}

void update(int /*value*/) {
    static double lastT = nowSec();
    double tNow = nowSec();
    float dt = (float)(tNow - lastT);
    lastT = tNow;
    dt = clampf(dt, 0.0005f, 0.05f);

    pollGamepad(dt);

    // test/debug: --level N jumps straight into a sector
    static bool jumped = false;
    if (startLevel > 0 && !jumped && state == S_MENU) {
        jumped = true;
        paused = false;
        startGame();
        if (startLevel > 1) { level = startLevel; buildLevel(level); beginLevel(); }
    }

    // test/debug hooks (used by the automated screenshots only)
    static bool pausedOnce = false, endedOnce = false;
    if (startPause && state == S_FLYING && !pausedOnce) { pausedOnce = true; paused = true; menuSel = 1; }
    if (testEnd && state == S_FLYING && !endedOnce) {
        endedOnce = true;
        if (testEnd == 1) {   // fake defeat in sector 3
            sectorTimes[0] = 95.3f; sectorTimes[1] = 130.2f; runTime = 225.5f; levelTime = 41.7f; level = 3; score = 12345;
            endRun(false); state = S_GAMEOVER; shipVisible = false;
        } else {              // fake victory
            const float t[5] = {95.3f, 130.2f, 148.9f, 171.0f, 203.4f};
            runTime = 0.0f;
            for (int i = 0; i < 5; ++i) { sectorTimes[i] = t[i]; runTime += t[i]; }
            level = 5; score = 98765;
            endRun(true); state = S_VICTORY; shipVisible = false;
        }
    }

    bool lockNow = mouseLocked && !paused && !settingsOpen && (state == S_FLYING || state == S_EMERGING);
    static int lastCursor = -1;
    int wantCursor = lockNow ? GLUT_CURSOR_NONE : GLUT_CURSOR_INHERIT;
    if (wantCursor != lastCursor) { glutSetCursor(wantCursor); lastCursor = wantCursor; }

    if (!paused && !settingsOpen) stepGame(dt, dt / TICK);
    if (settingsOpen)       settingsPadUpdate(dt);
    else if (confirmId)     confirmPadUpdate();
    else if (menuPage)      pagePadUpdate();
    else if (menuActive())  menuPadUpdate(dt);
    else if ((state == S_GAMEOVER || state == S_VICTORY) && uiBack) goMainMenu();
    uiLeft = uiRight = uiConfirm = uiClick = false;
    uiUp = uiDown = uiBack = uiTabPrev = uiTabNext = uiReset = false;
    uiPick = -1;

    glutPostRedisplay();
}

// idle-driven main loop: one simulation step per displayed frame (vsync paces it)
static void idleFunc() {
    static double lastIdle = 0.0;
    double t = nowSec();
    double minFrame = (FPS_VALUES[cfgFpsIdx] > 0) ? 1.0 / FPS_VALUES[cfgFpsIdx] : 0.0028;   // user cap, else ~350 fps safety
    if (lastIdle > 0.0 && t - lastIdle < minFrame) {
        if (minFrame - (t - lastIdle) > 0.002) sleepMs(1);
        return;
    }
    lastIdle = t;
    update(0);
}

// ----------------------------------------------------------------------------
//  Input Callbacks (Keyboard + Mouse)
// ----------------------------------------------------------------------------
void keyboardDown(unsigned char key, int /*x*/, int /*y*/) {
    if (key >= 'A' && key <= 'Z') key = (unsigned char)(key - 'A' + 'a');
    if (key == 'o') { if (settingsOpen) closeSettings(); else openSettings(); return; }   // v1.6: settings screen
    if (settingsOpen) { settingsKey(key); return; }
    if (confirmId)  { confirmKey(key); return; }
    if (menuPage)   { pageKey(key); return; }
    if (menuActive()) { menuKey(key); return; }
    keys[key] = true;

    if (key == 27) {                                     // Esc: pause menu in a run, main menu from the end screens
        if (state == S_GAMEOVER || state == S_VICTORY) goMainMenu();
        else if (state != S_MENU) { paused = true; menuSel = 0; }
        return;
    }
    if (key == 'p' && state != S_MENU) { paused = !paused; menuSel = 0; }
    if (key == 'b') bloomOn = !bloomOn;
    if (key == 'g') postOn = !postOn;
    if (key == 'm') mouseLocked = !mouseLocked;
    if (key == 'n') audioMuted = !audioMuted;
    if (key == ' ' && state == S_FLYING && !paused) missileRequest = true;   // v1.2: fire missile
    if (key == 'v' && state == S_FLYING && !paused) bombRequest = true;      // v1.3: pulse bomb
    if (state == S_UPGRADE && key >= '1' && key <= '3') uiPick = key - '1';
    if (state == S_UPGRADE && (key == 13 || key == '\n')) uiConfirm = true;
    if (key == 13 || key == '\n') {
        if (state == S_GAMEOVER || state == S_VICTORY) {
            paused = false;
            startGame();
        }
    }
}

void keyboardUp(unsigned char key, int /*x*/, int /*y*/) {
    if (key >= 'A' && key <= 'Z') key = (unsigned char)(key - 'A' + 'a');
    keys[key] = false;
}

void specialDown(int key, int /*x*/, int /*y*/) {
    if (settingsOpen) { settingsSpecial(key); return; }
    if (confirmId)    { confirmSpecial(key); return; }
    if (menuPage)     return;
    if (menuActive()) { menuSpecial(key); return; }
    if (key >= 0 && key < 256) specialKeys[key] = true;
    if (state == S_UPGRADE) {
        if (key == GLUT_KEY_LEFT)  uiLeft = true;
        if (key == GLUT_KEY_RIGHT) uiRight = true;
    }
}
void specialUp(int key, int /*x*/, int /*y*/)   { if (key >= 0 && key < 256) specialKeys[key] = false; }

void mouseButton(int button, int btnState, int x, int y) {
    if (settingsOpen) { if (button == GLUT_LEFT_BUTTON) { mouseX = x; mouseY = y; settingsClick(x, y, btnState == GLUT_DOWN); } return; }
    if (confirmId)    { if (button == GLUT_LEFT_BUTTON && btnState == GLUT_DOWN) confirmClick(x, y); return; }
    if (menuPage)     { if (button == GLUT_LEFT_BUTTON && btnState == GLUT_DOWN) pageClick(x, y); return; }
    if (menuActive()) { if (button == GLUT_LEFT_BUTTON && btnState == GLUT_DOWN) menuClick(x, y); return; }
    if (button == GLUT_LEFT_BUTTON) {
        mouseLeftDown = (btnState == GLUT_DOWN);
        if (btnState == GLUT_DOWN && state == S_UPGRADE) { uiClick = true; mouseX = x; mouseY = y; }
        if (btnState == GLUT_DOWN && (state == S_GAMEOVER || state == S_VICTORY)) {
            paused = false;
            startGame();
        }
    } else if (button == GLUT_RIGHT_BUTTON) {
        mouseRightDown = (btnState == GLUT_DOWN);
    }
}

void handleMouseMove(int x, int y) {
    mouseX = x; mouseY = y;
    if (settingsOpen) { settingsHover(x, y); return; }
    if (confirmId)    { confirmHover(x, y); return; }
    if (menuPage)     return;
    if (menuActive()) { menuHover(x, y); return; }
    if (!mouseLocked || paused || state != S_FLYING) return;
    if (warpingPointer) {
        warpingPointer = false;
        return;
    }
    int cx = curWinW / 2;
    int cy = curWinH / 2;
    int dx = x - cx;
    int dy = y - cy;
    if (dx == 0 && dy == 0) return;

    mouseDx += (float)dx;
    mouseDy += (float)dy;
    warpingPointer = true;
    glutWarpPointer(cx, cy);
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    curWinW = w; curWinH = h;
    glViewport(0, 0, w, h);
}

// ----------------------------------------------------------------------------
//  Setup
// ----------------------------------------------------------------------------
void init() {
    srand(static_cast<unsigned>(time(nullptr)));

    glEnable(GL_DEPTH_TEST);
    gfxLight(true);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    GLfloat ambientGlobal[] = {0.16f, 0.18f, 0.26f, 1.0f};
    GLfloat ambientLight[]  = {0.0f, 0.0f, 0.0f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientGlobal);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHT1);
    initBloomTextures();
    initXInput();
    initAudio();
    buildLists();
    gfxInit();
    applyVsync();   // honours the VSync setting (and --novsync)
    particles.reserve(8192); bolts.reserve(640); popups.reserve(64); shocks.reserve(64);
    missiles.reserve(40); powerups.reserve(48); coins.reserve(256); obstacles.reserve(256);

    for (int i = 0; i < NUM_STARS; ++i) {
        float z = frand() * 2.0f - 1.0f;
        float a = frand() * 2.0f * PI;
        float s = sqrtf(1.0f - z * z);
        starPos[i] = Vec3{s * cosf(a), s * sinf(a), z} * 300.0f;
        float b = frand();
        starBright[i] = 0.3f + 0.7f * b * b;
        starHue[i] = frand();  // 0=blue, 0.5=white, 1.0=warm
    }
    for (int i = 0; i < NUM_PUFFS; ++i) {
        float z = frand() * 1.6f - 0.8f;
        float a = frand() * 2.0f * PI;
        float s = sqrtf(1.0f - z * z);
        puffs[i].pos = Vec3{s * cosf(a), s * sinf(a), z} * 280.0f;
        puffs[i].size = rnd(110.0f, 230.0f);
        puffs[i].t = frand();
        puffs[i].a = rnd(0.12f, 0.28f);
    }
    for (int i = 0; i < NUM_DUST; ++i)
        dustBase[i] = {frand() * 1000.0f, frand() * 1000.0f, frand() * 1000.0f};
    for (int i = 0; i < NUM_STREAKS; ++i) {
        streak[i][0] = frand();
        streak[i][1] = frand();
        streak[i][2] = frand();
    }

    level = 1;
    buildLevel(1);
    ship = {0.0f, 0.0f, 4.0f};
    state = S_MENU;
    lastTime = glutGet(GLUT_ELAPSED_TIME);
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc)   shotPath = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) shotAt = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--level") && i + 1 < argc)  startLevel = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc)   sscanf(argv[++i], "%dx%d", &winW, &winH);
        else if (!strcmp(argv[i], "--nomsaa")) noMSAA = true;
        else if (!strcmp(argv[i], "--bench") && i + 1 < argc)  benchN = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--novsync")) noVsync = true;
        else if (!strcmp(argv[i], "--settings")) startSettings = true;
        else if (!strcmp(argv[i], "--page") && i + 1 < argc) startPage = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--confirm") && i + 1 < argc) startConfirm = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pause")) startPause = true;
        else if (!strcmp(argv[i], "--fakedie")) testEnd = 1;
        else if (!strcmp(argv[i], "--fakewin")) testEnd = 2;
        else if (!strcmp(argv[i], "--tab") && i + 1 < argc) startTab = atoi(argv[++i]);
    }
    initSettings();
    loadSettings();
    loadScores();
    g_hasSave = readSave(g_save);
    if (noMSAA) cfgMSAA = false; else noMSAA = !cfgMSAA;
    if (noVsync) cfgVsync = false;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | (shotPath ? 0 : GLUT_MULTISAMPLE));
    glutInitWindowSize(winW, winH);
    glutInitWindowPosition(100, 50);
    glutCreateWindow("A7iT's Einstein-Rosen Runner v1.7");
    glutIgnoreKeyRepeat(1);

    init();
    if (cfgFullscreen) glutFullScreen();
    if (startPage) menuPage = startPage;
    if (startConfirm) { confirmId = startConfirm; confirmSel = 0; }
    if (startSettings) { setTab = std::max(0, std::min(TAB_COUNT - 1, startTab)); openSettings(); }

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouseButton);
    glutMotionFunc(handleMouseMove);
    glutPassiveMotionFunc(handleMouseMove);

    glutIdleFunc(idleFunc);
    glutMainLoop();
    return 0;
}

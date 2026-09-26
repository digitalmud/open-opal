// crop_bench — measures the follow-mode crop pipelines (T-002).
//
// For each crop mode: open the camera with that pipeline, let it settle for
// 5 s (short runs overstate throughput while the warm-up backlog drains), then
// measure a STATIC run with the window still and a MOVING run with the window
// moved 30 times a second along a smooth path. fps and latency come from this
// program's own frame callback over the whole run, not the bridge's rolling
// telemetry. Two small grey thumbnails per mode (PGM, Y plane) show the window
// actually moved. Nothing here writes to the camera's flash: opening boots our
// pipeline into RAM, closing returns the camera to its stock webcam firmware.
//
// Build (from the repo root, against a CMake build of the bridge in $B):
//   clang -ISources/OpalBridge/include Sources/OpalBridge/test/crop_bench.c \
//         -L"$B" -lOpalBridge -Wl,-rpath,"$B" -Wl,-rpath,"$PWD/vendor/install/lib" -o crop_bench
// Run:
//   ./crop_bench [--modes 0,1,2,3,4] [--secs 60] [--rate 30] [--mxid <serial>] [--out build/crop-bench]
// --rate is how many times a second the moving run moves the window (default: every frame).
//   ./crop_bench --modes 0 --isp 2/3   (mode NONE with a different ISP scale: the whole frame at 2560x1440)
//   ./crop_bench --delay-probe
// measures how long the camera takes to apply a window move (mode WINDOW_1440): it jumps the
// window between a left and a right position 60 times and times, by each frame's capture time
// (arrival minus latency), when the new position first shows up. Frames carry no crop metadata
// in depthai v2.30, so the app matches frames to windows with this delay (T-003).

#include "OpalBridge.h"

#include <math.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_SAMPLES 8192
#define THUMB_W 320

static const char* kModeNames[] = {"NONE", "MANIP_4K", "MANIP_1440", "WINDOW_1080", "WINDOW_1440"};

// --- frame callback state (written on the bridge's capture thread) ----------
static atomic_long g_frames;
static atomic_int  g_nsamples;
static double      g_lat[MAX_SAMPLES];
static atomic_int  g_w, g_h;
static atomic_int  g_snapWant;          // 1 = grab the next frame's Y plane
static atomic_int  g_snapReady;
static unsigned char g_thumb[THUMB_W * THUMB_W];  // big enough for 16:9 at 320 wide
static int         g_thumbH;

// --- delay probe: a column-luma profile + capture time per frame ------------
#define PROF_N 64
#define MAX_REC 4096
static atomic_int  g_recOn;
static atomic_int  g_nrec;
static double      g_recT[MAX_REC];               // capture time, bench clock (s)
static float       g_recP[MAX_REC][PROF_N];       // normalised column profile

// Ctrl-C / SIGTERM: finish the current step, close the camera, then exit.
static volatile sig_atomic_t g_stop;
static void onSignal(int sig) { (void)sig; g_stop = 1; }

static double now(void) {  // (declared above for the callback)
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void onLog(const char* line, void* ctx) {
    (void)ctx;
    printf("  BOOT %s\n", line);
    fflush(stdout);
}

static double now(void);

// Column profile: mean luma of PROF_N vertical bands (rows sampled every 8th),
// then zero-mean / unit-norm so exposure drift doesn't matter.
static void profileOf(const uint8_t* y, size_t yStride, int w, int h, float* out) {
    double mean = 0;
    for(int b = 0; b < PROF_N; b++) {
        int c0 = b * w / PROF_N, c1 = (b + 1) * w / PROF_N;
        double sum = 0; long n = 0;
        for(int r = 0; r < h; r += 8)
            for(int c = c0; c < c1; c += 4) { sum += y[(size_t)r * yStride + (size_t)c]; n++; }
        out[b] = (float)(sum / (n ? n : 1));
        mean += out[b];
    }
    mean /= PROF_N;
    double norm = 0;
    for(int b = 0; b < PROF_N; b++) { out[b] -= (float)mean; norm += (double)out[b] * out[b]; }
    norm = sqrt(norm) + 1e-9;
    for(int b = 0; b < PROF_N; b++) out[b] = (float)(out[b] / norm);
}

static void onFrame(const uint8_t* y, size_t yStride, const uint8_t* uv, size_t uvStride,
                    int w, int h, int64_t t, double latencyMs, void* ctx) {
    (void)uv; (void)uvStride; (void)t; (void)ctx;
    if(atomic_load(&g_recOn)) {
        int k = atomic_load(&g_nrec);
        if(k < MAX_REC) {
            g_recT[k] = now() - latencyMs / 1000.0;
            profileOf(y, yStride, w, h, g_recP[k]);
            atomic_store(&g_nrec, k + 1);
        }
    }
    atomic_fetch_add(&g_frames, 1);
    int i = atomic_fetch_add(&g_nsamples, 1);
    if(i < MAX_SAMPLES) g_lat[i] = latencyMs;
    atomic_store(&g_w, w);
    atomic_store(&g_h, h);
    if(atomic_load(&g_snapWant) && !atomic_load(&g_snapReady)) {
        int th = h * THUMB_W / w;
        for(int r = 0; r < th; r++)
            for(int c = 0; c < THUMB_W; c++)
                g_thumb[r * THUMB_W + c] = y[(size_t)(r * h / th) * yStride + (size_t)(c * w / THUMB_W)];
        g_thumbH = th;
        atomic_store(&g_snapWant, 0);
        atomic_store(&g_snapReady, 1);
    }
}

static void resetCounters(void) {
    atomic_store(&g_frames, 0);
    atomic_store(&g_nsamples, 0);
}

static int cmpDouble(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y);
}

static void report(const char* mode, const char* run, double secs) {
    long frames = atomic_load(&g_frames);
    int n = atomic_load(&g_nsamples);
    if(n > MAX_SAMPLES) {
        printf("  WARNING: latency from the first %d of %d frames only (buffer full)\n", MAX_SAMPLES, n);
        n = MAX_SAMPLES;
    }
    double p50 = 0, p95 = 0;
    if(n > 0) {
        qsort(g_lat, (size_t)n, sizeof(double), cmpDouble);
        p50 = g_lat[n / 2];
        p95 = g_lat[(n * 95) / 100 < n ? (n * 95) / 100 : n - 1];
    }
    printf("RESULT mode=%s run=%s frames=%ld secs=%.1f fps=%.2f p50_ms=%.1f p95_ms=%.1f out=%dx%d\n",
           mode, run, frames, secs, secs > 0 ? frames / secs : 0, p50, p95,
           atomic_load(&g_w), atomic_load(&g_h));
    fflush(stdout);
}

static void snapshot(const char* dir, const char* mode, const char* tag) {
    atomic_store(&g_snapReady, 0);
    atomic_store(&g_snapWant, 1);
    double t0 = now();
    while(!atomic_load(&g_snapReady) && now() - t0 < 3) usleep(10000);
    if(!atomic_load(&g_snapReady)) { printf("  thumbnail %s/%s: no frame\n", mode, tag); return; }
    char path[512];
    snprintf(path, sizeof path, "%s/%s-%s.pgm", dir, mode, tag);
    FILE* f = fopen(path, "wb");
    if(!f) { printf("  thumbnail %s: can't write\n", path); return; }
    fprintf(f, "P5\n%d %d\n255\n", THUMB_W, g_thumbH);
    fwrite(g_thumb, 1, (size_t)(THUMB_W * g_thumbH), f);
    fclose(f);
    printf("  thumbnail %s\n", path);
}

// The moving path: a pan across whatever the mode allows plus, where the mode
// can zoom, a zoom oscillation. The bridge clamps to the mode's real range.
static void pathAt(double t, float* x, float* y, float* w) {
    float zw = 0.75f + 0.25f * (float)sin(2 * M_PI * t / 6.0);   // 0.5 .. 1.0
    float cx = 0.5f + 0.3f * (float)sin(2 * M_PI * t / 4.0);
    float cy = 0.5f + 0.2f * (float)cos(2 * M_PI * t / 5.0);
    *w = zw;
    *x = cx - zw / 2;
    *y = cy - zw / 2;
}

static int waitUsable(const char* want, char* mxidOut, double timeout) {
    double t0 = now();
    while(now() - t0 < timeout) {
        OpalDeviceInfo d[8];
        int n = opal_list_devices(d, 8);
        for(int i = 0; i < n; i++) {
            if(!d[i].usable) continue;
            if(want && want[0] && strcmp(d[i].mxid, want) != 0) continue;
            snprintf(mxidOut, OPAL_MXID_LEN, "%s", d[i].mxid);
            return 1;
        }
        usleep(100000);
    }
    return 0;
}

static float corr(const float* a, const float* b) {
    double s = 0;
    for(int i = 0; i < PROF_N; i++) s += (double)a[i] * b[i];
    return (float)s;
}

static int cmpD(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y);
}

// Mean of the recorded profiles [from, to), normalised: a reference for one position.
static void refFrom(int from, int to, float* out) {
    for(int b = 0; b < PROF_N; b++) out[b] = 0;
    for(int k = from; k < to; k++) for(int b = 0; b < PROF_N; b++) out[b] += g_recP[k][b];
    double norm = 0;
    for(int b = 0; b < PROF_N; b++) norm += (double)out[b] * out[b];
    norm = sqrt(norm) + 1e-9;
    for(int b = 0; b < PROF_N; b++) out[b] = (float)(out[b] / norm);
}

static int delayProbe(const char* wantMxid) {
    char mxid[OPAL_MXID_LEN];
    if(!waitUsable(wantMxid, mxid, 60)) { printf("DELAY error=\"no usable camera\"\n"); return 1; }
    OpalPipelineConfig cfg = {.ispNum = 1, .ispDen = 2, .fps = 30, .keep4K = false,
                              .orientation = OPAL_ORIENT_ROTATE_180, .cropMode = OPAL_CROP_WINDOW_1440};
    OpalDeviceHandle* h = opal_open(mxid, cfg, onFrame, NULL);
    if(!h) { printf("DELAY error=\"%s\"\n", opal_last_error()); return 1; }
    const float L = 0.f, R = 1.f / 3.f, Y = 1.f / 6.f, W = 2.f / 3.f;  // window positions
    for(int i = 0; i < 50 && !g_stop; i++) usleep(100000);             // drain

    // References: 1 s parked at each position.
    float refL[PROF_N], refR[PROF_N];
    atomic_store(&g_nrec, 0);
    opal_set_crop(h, L, Y, W, W); usleep(800000);
    int a0 = atomic_load(&g_nrec); atomic_store(&g_recOn, 1); usleep(1000000); atomic_store(&g_recOn, 0);
    refFrom(a0, atomic_load(&g_nrec), refL);
    opal_set_crop(h, R, Y, W, W); usleep(800000);
    int b0 = atomic_load(&g_nrec); atomic_store(&g_recOn, 1); usleep(1000000); atomic_store(&g_recOn, 0);
    refFrom(b0, atomic_load(&g_nrec), refR);
    printf("  reference similarity L-R: %.3f (lower = easier to tell apart)\n", corr(refL, refR));

    // 60 jumps, ~20 frames apart, alternating R->L->R..., recording every frame.
    enum { JUMPS = 60 };
    double sendT[JUMPS]; int target[JUMPS];
    int base = atomic_load(&g_nrec);
    atomic_store(&g_recOn, 1);
    for(int j = 0; j < JUMPS && !g_stop; j++) {
        target[j] = (j % 2 == 0) ? 0 : 1;   // 0 = L, 1 = R (we start parked at R)
        sendT[j] = now();
        opal_set_crop(h, target[j] ? R : L, Y, W, W);
        usleep(667000);
    }
    atomic_store(&g_recOn, 0);
    int end = atomic_load(&g_nrec);
    opal_close(h);

    // Classify every recorded frame; per jump, time the first frame at the new position.
    double delays[JUMPS], lastOld[JUMPS]; int nd = 0, mixed = 0; double minMargin = 1e9;
    for(int j = 0; j < JUMPS; j++) {
        double t0 = sendT[j], t1 = (j + 1 < JUMPS) ? sendT[j + 1] : 1e18;
        double first = -1, lastOldT = t0; int flippedBack = 0;
        for(int k = base; k < end; k++) {
            if(g_recT[k] < t0 - 0.2 || g_recT[k] >= t1) continue;
            float cl = corr(g_recP[k], refL), cr = corr(g_recP[k], refR);
            int cls = cr > cl ? 1 : 0;
            double m = fabs((double)cl - cr);
            if(m < minMargin) minMargin = m;
            if(cls == target[j]) { if(first < 0 && g_recT[k] >= t0 - 0.2) first = g_recT[k]; }
            else if(first >= 0) flippedBack = 1;
            else lastOldT = g_recT[k];
        }
        if(first >= 0) { delays[nd] = (first - t0) * 1000; lastOld[nd] = (lastOldT - t0) * 1000; nd++; }
        mixed += flippedBack;
    }
    if(nd == 0) { printf("DELAY error=\"no jump detected\"\n"); return 1; }
    double sorted[JUMPS]; memcpy(sorted, delays, sizeof(double) * (size_t)nd);
    qsort(sorted, (size_t)nd, sizeof(double), cmpD);
    double maxOld = -1e9;
    for(int i = 0; i < nd; i++) if(lastOld[i] > maxOld) maxOld = lastOld[i];
    printf("DELAY jumps=%d detected=%d first_new_ms min=%.1f median=%.1f max=%.1f "
           "last_old_ms_max=%.1f mixed=%d min_margin=%.3f frames=%d\n",
           JUMPS, nd, sorted[0], sorted[nd / 2], sorted[nd - 1], maxOld, mixed, minMargin, end - base);
    fflush(stdout);
    return 0;
}

int main(int argc, char** argv) {
    int modes[8] = {0, 1, 2, 3, 4}, nmodes = 5;
    double secs = 60, rate = 30;
    const char* wantMxid = "";
    const char* outDir = "build/crop-bench";
    int probe = 0;
    int ispNum = 1, ispDen = 2;   // mode NONE's ISP scale; --isp 2/3 sends the whole frame at 2560x1440
    for(int i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "--modes") && i + 1 < argc) {
            nmodes = 0;
            for(char* tok = strtok(argv[++i], ","); tok && nmodes < 8; tok = strtok(NULL, ","))
                modes[nmodes++] = atoi(tok);
        } else if(!strcmp(argv[i], "--secs") && i + 1 < argc) {
            secs = atof(argv[++i]);
        } else if(!strcmp(argv[i], "--rate") && i + 1 < argc) {
            rate = atof(argv[++i]);
            if(rate <= 0) rate = 30;
        } else if(!strcmp(argv[i], "--mxid") && i + 1 < argc) {
            wantMxid = argv[++i];
        } else if(!strcmp(argv[i], "--isp") && i + 1 < argc) {
            if(sscanf(argv[++i], "%d/%d", &ispNum, &ispDen) != 2 || ispNum <= 0 || ispDen <= 0) { ispNum = 1; ispDen = 2; }
        } else if(!strcmp(argv[i], "--delay-probe")) {
            probe = 1;
        } else if(!strcmp(argv[i], "--out") && i + 1 < argc) {
            outDir = argv[++i];
        }
    }
    mkdir("build", 0755);
    mkdir(outDir, 0755);
    opal_set_boot_logger(onLog, NULL);
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    if(probe) return delayProbe(wantMxid);

    for(int m = 0; m < nmodes && !g_stop; m++) {
        int mode = modes[m];
        if(mode < 0 || mode > 4) continue;
        const char* name = kModeNames[mode];
        char mxid[OPAL_MXID_LEN];
        if(!waitUsable(wantMxid, mxid, 60)) {
            printf("RESULT mode=%s run=open error=\"no usable camera within 60 s\"\n", name);
            continue;
        }
        printf("\n=== mode %s on %s\n", name, mxid);
        fflush(stdout);

        OpalPipelineConfig cfg = {.ispNum = ispNum, .ispDen = ispDen, .fps = 30, .keep4K = false,
                                  .orientation = OPAL_ORIENT_ROTATE_180,
                                  .cropMode = (OpalCropMode)mode};
        double t0 = now();
        OpalDeviceHandle* h = opal_open(mxid, cfg, onFrame, NULL);
        if(!h) {
            printf("RESULT mode=%s run=open error=\"%s\"\n", name, opal_last_error());
            fflush(stdout);
            continue;
        }
        printf("  opened in %.1f s\n", now() - t0);

        for(int i = 0; i < 50 && !g_stop; i++) usleep(100000);  // 5 s: drain the warm-up backlog

        // Two thumbnails at opposite corners of the mode's range.
        if(mode != 0) {
            opal_set_crop(h, 0.f, 0.f, 0.5f, 0.5f);
            usleep(700000);
            snapshot(outDir, name, "topleft");
            opal_set_crop(h, 0.5f, 0.5f, 0.5f, 0.5f);
            usleep(700000);
            snapshot(outDir, name, "bottomright");
            opal_set_crop(h, 0.25f, 0.25f, 0.5f, 0.5f);  // centred for the static run
            usleep(300000);
        } else {
            snapshot(outDir, name, "full");
        }

        resetCounters();
        double s0 = now();
        while(now() - s0 < secs && !g_stop) usleep(100000);
        report(name, "static", now() - s0);

        if(mode != 0 && !g_stop) {
            resetCounters();
            double m0 = now(), next = m0;
            while(now() - m0 < secs && !g_stop) {
                float x, y, w;
                pathAt(now() - m0, &x, &y, &w);
                opal_set_crop(h, x, y, w, w);
                next += 1.0 / rate;
                double d = next - now();
                if(d > 0) usleep((useconds_t)(d * 1e6));
            }
            char run[32];
            snprintf(run, sizeof run, rate == 30 ? "moving" : "moving@%gHz", rate);
            report(name, run, now() - m0);
        }

        opal_close(h);
        printf("  closed%s\n", g_stop ? " (interrupted)" : "");
        fflush(stdout);
    }
    return 0;
}

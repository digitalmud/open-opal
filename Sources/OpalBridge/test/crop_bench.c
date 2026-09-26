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

// Ctrl-C / SIGTERM: finish the current step, close the camera, then exit.
static volatile sig_atomic_t g_stop;
static void onSignal(int sig) { (void)sig; g_stop = 1; }

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void onLog(const char* line, void* ctx) {
    (void)ctx;
    printf("  BOOT %s\n", line);
    fflush(stdout);
}

static void onFrame(const uint8_t* y, size_t yStride, const uint8_t* uv, size_t uvStride,
                    int w, int h, int64_t t, double latencyMs, void* ctx) {
    (void)uv; (void)uvStride; (void)t; (void)ctx;
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

int main(int argc, char** argv) {
    int modes[8] = {0, 1, 2, 3, 4}, nmodes = 5;
    double secs = 60, rate = 30;
    const char* wantMxid = "";
    const char* outDir = "build/crop-bench";
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
        } else if(!strcmp(argv[i], "--out") && i + 1 < argc) {
            outDir = argv[++i];
        }
    }
    mkdir("build", 0755);
    mkdir(outDir, 0755);
    opal_set_boot_logger(onLog, NULL);
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);

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

        OpalPipelineConfig cfg = {.ispNum = 1, .ispDen = 2, .fps = 30, .keep4K = false,
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

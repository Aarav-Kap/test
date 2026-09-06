
#include <os.h>
#include <libndls.h>
#include <ngc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INPUT_MAX 420
#define MSG_MAX 2200
#define WRAP_COLS 44
#define MAX_RENDER_LINES 11

typedef struct {
    char input[INPUT_MAX];
    int input_len;
    char last_user[INPUT_MAX];
    char last_ai[MSG_MAX];
    char status[96];
    unsigned long seq;
    int waiting;
    int scroll;
    int shift;
} App;

typedef struct { const t_key *key; char normal; char shifted; int old; } KeyMap;

static KeyMap keys[] = {
    {&KEY_NSPIRE_A,'a','A',0},{&KEY_NSPIRE_B,'b','B',0},{&KEY_NSPIRE_C,'c','C',0},
    {&KEY_NSPIRE_D,'d','D',0},{&KEY_NSPIRE_E,'e','E',0},{&KEY_NSPIRE_F,'f','F',0},
    {&KEY_NSPIRE_G,'g','G',0},{&KEY_NSPIRE_H,'h','H',0},{&KEY_NSPIRE_I,'i','I',0},
    {&KEY_NSPIRE_J,'j','J',0},{&KEY_NSPIRE_K,'k','K',0},{&KEY_NSPIRE_L,'l','L',0},
    {&KEY_NSPIRE_M,'m','M',0},{&KEY_NSPIRE_N,'n','N',0},{&KEY_NSPIRE_O,'o','O',0},
    {&KEY_NSPIRE_P,'p','P',0},{&KEY_NSPIRE_Q,'q','Q',0},{&KEY_NSPIRE_R,'r','R',0},
    {&KEY_NSPIRE_S,'s','S',0},{&KEY_NSPIRE_T,'t','T',0},{&KEY_NSPIRE_U,'u','U',0},
    {&KEY_NSPIRE_V,'v','V',0},{&KEY_NSPIRE_W,'w','W',0},{&KEY_NSPIRE_X,'x','X',0},
    {&KEY_NSPIRE_Y,'y','Y',0},{&KEY_NSPIRE_Z,'z','Z',0},
    {&KEY_NSPIRE_0,'0','0',0},{&KEY_NSPIRE_1,'1','1',0},{&KEY_NSPIRE_2,'2','2',0},
    {&KEY_NSPIRE_3,'3','3',0},{&KEY_NSPIRE_4,'4','4',0},{&KEY_NSPIRE_5,'5','5',0},
    {&KEY_NSPIRE_6,'6','6',0},{&KEY_NSPIRE_7,'7','7',0},{&KEY_NSPIRE_8,'8','8',0},
    {&KEY_NSPIRE_9,'9','9',0},
    {&KEY_NSPIRE_SPACE,' ',' ',0},{&KEY_NSPIRE_PERIOD,'.','?',0},{&KEY_NSPIRE_COMMA,',','!',0},
    {&KEY_NSPIRE_PLUS,'+','+',0},{&KEY_NSPIRE_MINUS,'-','-',0},{&KEY_NSPIRE_MULTIPLY,'*','*',0},
    {&KEY_NSPIRE_DIVIDE,'/','/',0},{&KEY_NSPIRE_LP,'(','(',0},{&KEY_NSPIRE_RP,')',')',0},
    {&KEY_NSPIRE_EQU,'=','=',0}
};

static int del_old=0, enter_old=0, esc_old=0, up_old=0, down_old=0, shift_old=0;

static int edge(const t_key *key, int *old) {
    int now = isKeyPressed(*key), hit = now && !*old; *old = now; return hit;
}
static void append_char(App *a, char c) {
    if (a->input_len < INPUT_MAX - 1) {
        a->input[a->input_len++] = c;
        a->input[a->input_len] = 0;
    }
}
static unsigned long make_seq(const char *s) {
    unsigned long h = (unsigned long) clock() + 1;
    while (*s) { h = (h * 16777619UL) ^ (unsigned char) *s++; }
    return h ? h : 1UL;
}
static void trim_eol(char *s) { s[strcspn(s, "\r\n")] = 0; }

static int write_uplink(App *a) {
    FILE *f = fopen("uplink.tns", "wb");
    if (!f) return 0;
    fprintf(f, "NSPIREAI_UPLINK_V1\nseq=%lu\n%s\n", a->seq, a->last_user);
    fclose(f);
    return 1;
}

/* downlink format:
NSPIREAI_DOWNLINK_V1
seq=123
state=thinking|streaming|done|error
<message text...>
*/
static int read_downlink(App *a) {
    FILE *f = fopen("downlink.tns", "rb");
    char header[64], seqline[64], stateline[64];
    unsigned long seq = 0;
    size_t n;
    if (!f) return 0;
    if (!fgets(header, sizeof header, f)) { fclose(f); return 0; }
    if (!fgets(seqline, sizeof seqline, f)) { fclose(f); return 0; }
    if (!fgets(stateline, sizeof stateline, f)) { fclose(f); return 0; }
    trim_eol(header); trim_eol(seqline); trim_eol(stateline);
    if (strcmp(header, "NSPIREAI_DOWNLINK_V1") != 0) { fclose(f); return 0; }
    if (sscanf(seqline, "seq=%lu", &seq) != 1) { fclose(f); return 0; }
    if (seq != a->seq) { fclose(f); return 0; }
    strncpy(a->status, stateline + 6, sizeof(a->status)-1);
    a->status[sizeof(a->status)-1] = 0;
    n = fread(a->last_ai, 1, sizeof(a->last_ai)-1, f);
    a->last_ai[n] = 0;
    fclose(f);
    if (strcmp(a->status, "done") == 0 || strcmp(a->status, "error") == 0) a->waiting = 0;
    return 1;
}

static size_t ascii_to_u16(const char *s, uint16_t *out, size_t cap) {
    size_t n = 0;
    while (*s && n + 1 < cap) out[n++] = (unsigned char) *s++;
    out[n] = 0;
    return n;
}
static void draw_text(Gc gc, const char *s, int x, int y, int bold) {
    uint16_t w[220];
    ascii_to_u16(s, w, 220);
    gui_gc_setFont(gc, bold ? Bold10 : Regular10);
    gui_gc_drawString(gc, (char*) w, x, y, GC_SM_TOP);
}
static void next_wrapped_line(const char **ps, char *out, int cols) {
    const char *s = *ps;
    int i = 0, last_sp = -1;
    while (*s && *s != '\n' && i < cols) {
        out[i] = *s;
        if (*s == ' ') last_sp = i;
        i++; s++;
    }
    if (*s && *s != '\n' && i >= cols && last_sp > 8) {
        int back = i - last_sp - 1;
        s -= back;
        i = last_sp;
    }
    out[i] = 0;
    while (i > 0 && (out[i-1] == ' ' || out[i-1] == '\r')) out[--i] = 0;
    if (*s == '\n') s++;
    while (*s == ' ') s++;
    *ps = s;
}
static int total_wrapped_lines(const char *s, int cols) {
    int count = 0; char line[96];
    while (*s) { next_wrapped_line(&s, line, cols); count++; }
    return count ? count : 1;
}
static void draw_wrapped_window(Gc gc, const char *s, int x, int y, int cols, int max_lines, int skip) {
    char line[96];
    int line_no = 0, shown = 0;
    if (!s || !*s) return;
    while (*s && shown < max_lines) {
        next_wrapped_line(&s, line, cols);
        if (line_no++ >= skip) {
            draw_text(gc, line, x, y + shown * 14, 0);
            shown++;
        }
    }
}
static void render(App *a) {
    Gc gc = gui_gc_global_GC();
    char header[80], status_line[110], prompt_label[24];
    int ai_total, max_scroll;

    gui_gc_begin(gc);
    gui_gc_setRegion(gc, 0, 0, 320, 240, 0, 0, 320, 240);

    /* background */
    gui_gc_setColorRGB(gc, 245, 247, 250);
    gui_gc_fillRect(gc, 0, 0, 320, 240);

    /* title bar */
    gui_gc_setColorRGB(gc, 34, 45, 67);
    gui_gc_fillRect(gc, 0, 0, 320, 24);
    gui_gc_setColorRGB(gc, 255, 255, 255);
    draw_text(gc, "NspireAI", 10, 6, 1);
    draw_text(gc, a->waiting ? "LIVE" : "READY", 255, 6, 1);

    /* chat window */
    gui_gc_setColorRGB(gc, 226, 232, 240);
    gui_gc_fillRect(gc, 8, 30, 304, 138);
    gui_gc_setColorRGB(gc, 35, 40, 48);

    snprintf(prompt_label, sizeof(prompt_label), "You:");
    draw_text(gc, prompt_label, 14, 38, 1);
    if (a->last_user[0]) draw_wrapped_window(gc, a->last_user, 54, 38, 35, 2, 0);
    else draw_text(gc, "Ask anything...", 54, 38, 0);

    draw_text(gc, "Gemini:", 14, 74, 1);
    if (a->waiting && !a->last_ai[0]) {
        draw_text(gc, "Thinking...", 72, 74, 0);
    } else if (a->last_ai[0]) {
        ai_total = total_wrapped_lines(a->last_ai, WRAP_COLS-2);
        max_scroll = ai_total > 6 ? ai_total - 6 : 0;
        if (a->scroll > max_scroll) a->scroll = max_scroll;
        draw_wrapped_window(gc, a->last_ai, 72, 74, WRAP_COLS-2, 6, a->scroll);
    } else {
        draw_text(gc, "No response yet.", 72, 74, 0);
    }

    /* status pill */
    gui_gc_setColorRGB(gc, 255, 255, 255);
    gui_gc_fillRect(gc, 12, 148, 296, 16);
    gui_gc_setColorRGB(gc, 80, 90, 105);
    snprintf(status_line, sizeof(status_line), "Status: %s", a->status[0] ? a->status : "idle");
    draw_text(gc, status_line, 18, 150, 0);

    /* input box */
    gui_gc_setColorRGB(gc, 255, 255, 255);
    gui_gc_fillRect(gc, 8, 175, 304, 40);
    gui_gc_setColorRGB(gc, 160, 170, 185);
    gui_gc_drawRect(gc, 8, 175, 304, 40);

    gui_gc_setColorRGB(gc, 25, 30, 38);
    if (a->input[0]) draw_wrapped_window(gc, a->input, 15, 182, WRAP_COLS, 2, 0);
    else draw_text(gc, "Type a message...", 15, 182, 0);

    /* footer */
    gui_gc_setColorRGB(gc, 80, 90, 105);
    if (a->shift) strcpy(header, "Shift ON | Enter send | Del erase | Esc exit");
    else strcpy(header, "Enter send | Shift caps | Up/Down scroll | Esc exit");
    draw_text(gc, header, 8, 222, 0);

    gui_gc_finish(gc);
    gui_gc_blit_to_screen(gc);
}

int main(int argc, char **argv) {
    App a;
    int running = 1;
    size_t i;
    (void) argc;
    memset(&a, 0, sizeof a);
    strcpy(a.status, "idle");
    enable_relative_paths(argv);

    while (running) {
        for (i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
            int now = isKeyPressed(*keys[i].key);
            if (now && !keys[i].old && !a.waiting) append_char(&a, a.shift ? keys[i].shifted : keys[i].normal);
            keys[i].old = now;
        }

        if (edge(&KEY_NSPIRE_SHIFT, &shift_old) && !a.waiting) a.shift = !a.shift;
        if (edge(&KEY_NSPIRE_DEL, &del_old) && !a.waiting && a.input_len > 0) {
            a.input[--a.input_len] = 0;
        }
        if (edge(&KEY_NSPIRE_UP, &up_old) && a.scroll > 0) a.scroll--;
        if (edge(&KEY_NSPIRE_DOWN, &down_old)) a.scroll++;

        if (edge(&KEY_NSPIRE_ENTER, &enter_old) && !a.waiting && a.input_len > 0) {
            strncpy(a.last_user, a.input, sizeof(a.last_user)-1);
            a.last_user[sizeof(a.last_user)-1] = 0;
            a.seq = make_seq(a.last_user);
            a.last_ai[0] = 0;
            a.input_len = 0;
            a.input[0] = 0;
            a.scroll = 0;
            strcpy(a.status, "sending");
            if (write_uplink(&a)) {
                a.waiting = 1;
                strcpy(a.status, "thinking");
            } else {
                strcpy(a.status, "error");
                strcpy(a.last_ai, "Could not create uplink.tns.");
            }
        }

        if (a.waiting) read_downlink(&a);
        if (edge(&KEY_NSPIRE_ESC, &esc_old)) running = 0;
        render(&a);
        msleep(80);
    }
    return 0;
}

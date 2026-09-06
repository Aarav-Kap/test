
#include <os.h>
#include <libndls.h>
#include <ngc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INPUT_MAX 420
#define RESP_MAX 2200

struct app {
    char input[INPUT_MAX];
    int input_len;
    char response[RESP_MAX];
    unsigned long request_id;
    int waiting;
    int scroll;
    int shift;
};

struct keymap { const t_key *key; char normal; char shifted; int old; };

static struct keymap keys[] = {
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
static void append_char(struct app *a, char c) {
    if (a->input_len < INPUT_MAX-1) {
        a->input[a->input_len++] = c;
        a->input[a->input_len] = 0;
    }
}
static unsigned long make_id(const char *s) {
    unsigned long h=(unsigned long)clock()+1; while(*s){h=(h*16777619UL)^(unsigned char)*s++;} return h?h:1;
}
static int write_request(struct app *a) {
    FILE *f=fopen("request.tns","wb"); if(!f) return 0;
    fprintf(f,"NSPIREAI_REQUEST_V2\nid=%lu\n%s\n",a->request_id,a->input);
    fclose(f); return 1;
}
static int read_response(struct app *a) {
    FILE *f=fopen("response.tns","rb"); char head[64], idl[64]; unsigned long id=0; size_t n;
    if(!f) return 0;
    if(!fgets(head,sizeof head,f) || !fgets(idl,sizeof idl,f)){fclose(f);return 0;}
    head[strcspn(head,"\r\n")]=0;
    if(strcmp(head,"NSPIREAI_RESPONSE_V2") || sscanf(idl,"id=%lu",&id)!=1 || id!=a->request_id){fclose(f);return 0;}
    n=fread(a->response,1,sizeof(a->response)-1,f); a->response[n]=0; fclose(f); return 1;
}
static size_t utf8_to_utf16(const char *s, uint16_t *out, size_t cap) {
    size_t n=0; while(*s && n+1<cap) out[n++]=(unsigned char)*s++; out[n]=0; return n;
}
static void draw_line(Gc gc,const char *s,int x,int y,int bold){
    uint16_t w[180]; utf8_to_utf16(s,w,180); gui_gc_setFont(gc,bold?Bold10:Regular10); gui_gc_drawString(gc,(char*)w,x,y,GC_SM_TOP);
}
static void draw_wrapped(Gc gc,const char *s,int x,int y,int width_chars,int max_lines,int skip){
    char line[96]; int line_no=0, shown=0;
    while(*s && shown<max_lines){
        int i=0,lastsp=-1;
        while(*s && *s!='\n' && i<width_chars){ line[i]=*s; if(*s==' ') lastsp=i; i++; s++; }
        if(*s && *s!='\n' && i>=width_chars && lastsp>8){ int back=i-lastsp-1; s-=back; i=lastsp; }
        if(*s=='\n') s++;
        line[i]=0;
        if(line_no++>=skip){ draw_line(gc,line,x,y+shown*16,0); shown++; }
    }
}
static void render(struct app *a){
    Gc gc=gui_gc_global_GC();
    gui_gc_begin(gc); gui_gc_setRegion(gc,0,0,320,240,0,0,320,240);
    gui_gc_setColorRGB(gc,247,248,250); gui_gc_fillRect(gc,0,0,320,240);
    gui_gc_setColorRGB(gc,34,45,67); gui_gc_fillRect(gc,0,0,320,28);
    gui_gc_setColorRGB(gc,255,255,255); draw_line(gc,"NspireAI",10,6,1);

    gui_gc_setColorRGB(gc,225,231,240); gui_gc_fillRect(gc,8,38,304,118);
    gui_gc_setColorRGB(gc,35,40,48);
    if(a->waiting) draw_wrapped(gc,"Gemini is thinking...",18,50,42,6,0);
    else if(a->response[0]) draw_wrapped(gc,a->response,18,50,42,6,a->scroll);
    else draw_wrapped(gc,"Ask anything. Type below and press Enter.",18,50,42,6,0);

    gui_gc_setColorRGB(gc,255,255,255); gui_gc_fillRect(gc,8,166,304,48);
    gui_gc_setColorRGB(gc,165,175,190); gui_gc_drawRect(gc,8,166,304,48);
    gui_gc_setColorRGB(gc,25,30,38);
    draw_wrapped(gc,a->input[0]?a->input:"Type a message...",16,174,44,2,0);

    gui_gc_setColorRGB(gc,80,90,105);
    draw_line(gc,a->shift?"SHIFT  Enter send  Del erase":"Enter send  Shift caps  Esc exit",10,220,0);
    gui_gc_finish(gc); gui_gc_blit_to_screen(gc);
}
int main(int argc,char **argv){
    struct app a; int running=1; (void)argc; enable_relative_paths(argv); memset(&a,0,sizeof a);
    while(running){
        size_t i;
        for(i=0;i<sizeof(keys)/sizeof(keys[0]);i++){
            int now=isKeyPressed(*keys[i].key);
            if(now && !keys[i].old && !a.waiting) append_char(&a,a.shift?keys[i].shifted:keys[i].normal);
            keys[i].old=now;
        }
        if(edge(&KEY_NSPIRE_SHIFT,&shift_old) && !a.waiting) a.shift=!a.shift;
        if(edge(&KEY_NSPIRE_DEL,&del_old) && !a.waiting && a.input_len){a.input[--a.input_len]=0;}
        if(edge(&KEY_NSPIRE_ENTER,&enter_old) && !a.waiting && a.input_len){
            a.request_id=make_id(a.input); a.response[0]=0; a.scroll=0;
            if(write_request(&a)) a.waiting=1;
        }
        if(a.waiting && read_response(&a)){a.waiting=0; a.input_len=0; a.input[0]=0;}
        if(edge(&KEY_NSPIRE_UP,&up_old) && a.response[0] && a.scroll>0) a.scroll--;
        if(edge(&KEY_NSPIRE_DOWN,&down_old) && a.response[0]) a.scroll++;
        if(edge(&KEY_NSPIRE_ESC,&esc_old)) running=0;
        render(&a); msleep(60);
    }
    return 0;
}

/* ZZTemperature 1.0: read-only ZZ9000 Zynq temperature monitor. */
#include <exec/types.h>
#include <exec/ports.h>
#include <devices/timer.h>
#include <libraries/configvars.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/wb.h>
#include <stdio.h>
#include <string.h>

struct ExpansionBase *ExpansionBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *WorkbenchBase;
static const char version[] __attribute__((used)) =
    "\0$VER: ZZTemperature 1.0 (08.10.2026)";
#define PORT_NAME "ZZTemperature.1"
#define TEMP_REG 0xe0
#define FW_REG 0xc0
static volatile UBYTE *registers;
static struct Window *window;
static struct TextFont *font;
static int current = -1, minimum = -1, maximum = -1;
static unsigned long samples;

/* MNT ZZTop documents 16-bit temperature at 0xe0 in tenths Celsius. */
static int sample(void)
{
    unsigned int value = *(volatile UWORD *)(registers + TEMP_REG);
    /* Zero/all-ones and implausible readings are not a usable sensor. */
    current = (value > 0 && value <= 1500) ? (int)value : -1;
    if (current >= 0) {
        if (minimum < 0 || current < minimum) minimum = current;
        if (maximum < 0 || current > maximum) maximum = current;
        ++samples;
    }
    return current;
}

static void line(int row, const char *text)
{
    struct RastPort *rp = window->RPort;
    Move(rp, window->BorderLeft + 12,
         window->BorderTop + 16 + row * 18);
    Text(rp, (const void *)text, strlen(text));
}

static void draw(void)
{
    char text[64];
    struct RastPort *rp;
    if (!window) return;
    rp = window->RPort;
    SetFont(rp, font);
    SetAPen(rp, 0);
    RectFill(rp, window->BorderLeft, window->BorderTop,
             window->Width - window->BorderRight - 1,
             window->Height - window->BorderBottom - 1);
    SetAPen(rp, 1);
    SetBPen(rp, 0);
    SetDrMd(rp, JAM1);
    line(0, "ZZ9000 / Zynq chip temperature");
    if (current >= 0)
        sprintf(text, "Current: %ld.%ld C", (long)current / 10, (long)current % 10);
    else strcpy(text, "Current: sensor unavailable");
    line(2, text);
    if (minimum >= 0)
        sprintf(text, "Session min: %ld.%ld C   max: %ld.%ld C",
                (long)minimum / 10, (long)minimum % 10,
                (long)maximum / 10, (long)maximum % 10);
    else strcpy(text, "Session min/max: no valid readings");
    line(3, text);
    sprintf(text, "Refresh: 1 second   Samples: %lu", samples);
    line(4, text);
    line(6, "Close / Esc: hide     R: reset min/max");
    line(7, "Reopen from Workbench Tools menu.");
}

static void show(void)
{
    if (window) {
        WindowToFront(window);
        ActivateWindow(window);
        return;
    }
    window = OpenWindowTags(NULL,
        WA_Title, (ULONG)"ZZ9000 Temperature",
        WA_PubScreenName, (ULONG)"Workbench",
        WA_Left, 40, WA_Top, 50,
        WA_InnerWidth, 352, WA_InnerHeight, 158,
        WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET |
                  WFLG_SMART_REFRESH | WFLG_ACTIVATE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY,
        TAG_DONE);
    draw();
}

static void arm_timer(struct timerequest *io)
{
    io->tr_node.io_Command = TR_ADDREQUEST;
    io->tr_time.tv_secs = 1;
    io->tr_time.tv_micro = 0;
    SendIO((struct IORequest *)io);
}

int main(int argc, char **argv)
{
    struct ConfigDev *card;
    struct MsgPort *port = NULL, *timer_port = NULL, *existing;
    struct AppMenuItem *menu = NULL;
    struct timerequest *io = NULL;
    struct Message *message;
    struct TextAttr ta = {(STRPTR)"topaz.font", 8, 0, FPF_ROMFONT};
    ULONG signals, mask;
    int once = argc > 1 && !strcmp(argv[1], "ONCE");
    int stop = argc > 1 && !strcmp(argv[1], "STOP");
    int quiet = argc == 0 || (argc > 1 && !strcmp(argv[1], "MENU"));
    int timer_open = 0, pending = 0, running = 1, rc = 20;
    if (argc > 1 && !once && !stop && !quiet && strcmp(argv[1], "SHOW")) {
        puts("Usage: ZZTemperature [SHOW|MENU|ONCE|STOP]");
        return 10;
    }
    /* Signal while forbidden: an existing instance cannot exit in between. */
    if (!once) {
        Forbid();
        existing = FindPort((CONST_STRPTR)PORT_NAME);
        if (existing)
            Signal(existing->mp_SigTask,
                   stop ? SIGBREAKF_CTRL_C : SIGBREAKF_CTRL_F);
        Permit();
        if (existing || stop) return 0;
    }
    ExpansionBase = (void *)OpenLibrary((CONST_STRPTR)"expansion.library", 37);
    if (!ExpansionBase) goto done;
    card = FindConfigDev(NULL, 0x6d6e, 4);
    if (!card) card = FindConfigDev(NULL, 0x6d6e, 3);
    if (!card || card->cd_BoardSize <= TEMP_REG + 1) goto done;
    registers = (volatile UBYTE *)card->cd_BoardAddr;
    if (once) {
        unsigned int fw = *(volatile UWORD *)(registers + FW_REG);
        sample();
        if (current >= 0) {
            printf("ZZ9000 Zynq: %d.%d C (raw=%d, firmware=0x%04x)\n",
                   current / 10, current % 10, current, fw);
            rc = 0;
        } else puts("ZZ9000 temperature sensor unavailable");
        goto done;
    }
    IntuitionBase = (void *)OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    GfxBase = (void *)OpenLibrary((CONST_STRPTR)"graphics.library", 37);
    WorkbenchBase = OpenLibrary((CONST_STRPTR)"workbench.library", 37);
    if (!IntuitionBase || !GfxBase || !WorkbenchBase) goto done;
    font = OpenFont(&ta);
    port = CreateMsgPort();
    timer_port = CreateMsgPort();
    if (!font || !port || !timer_port) goto done;
    io = (struct timerequest *)CreateIORequest(timer_port, sizeof(*io));
    if (!io || OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_VBLANK,
                          (struct IORequest *)io, 0)) goto done;
    timer_open = 1;
    port->mp_Node.ln_Name = (char *)PORT_NAME;
    /* Recheck after allocating resources to prevent simultaneous duplicates. */
    Forbid();
    existing = FindPort((CONST_STRPTR)PORT_NAME);
    if (existing) Signal(existing->mp_SigTask, SIGBREAKF_CTRL_F);
    else AddPort(port);
    Permit();
    if (existing) { rc = 0; goto done; }
    menu = AddAppMenuItemA(1, 0, (CONST_STRPTR)"ZZ9000 Temperature...", port, NULL);
    if (!menu) goto done;
    sample();
    if (!quiet) show();
    arm_timer(io);
    pending = 1;
    while (running) {
        mask = (1UL << port->mp_SigBit) | (1UL << timer_port->mp_SigBit) |
               SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_F;
        if (window) mask |= 1UL << window->UserPort->mp_SigBit;
        signals = Wait(mask);
        if (signals & SIGBREAKF_CTRL_C) running = 0;
        if (running && (signals & SIGBREAKF_CTRL_F)) show();
        while ((message = GetMsg(port))) {
            ReplyMsg(message);
            if (running) show();
        }
        if (signals & (1UL << timer_port->mp_SigBit)) {
            WaitIO((struct IORequest *)io);
            pending = 0;
            sample();
            draw();
            if (running) { arm_timer(io); pending = 1; }
        }
        if (window) {
            int hide = 0;
            struct IntuiMessage *im;
            while ((im = (struct IntuiMessage *)GetMsg(window->UserPort))) {
                ULONG cls = im->Class;
                UWORD code = im->Code;
                ReplyMsg((struct Message *)im);
                if (cls == IDCMP_CLOSEWINDOW ||
                    (cls == IDCMP_VANILLAKEY && code == 27)) hide = 1;
                if (cls == IDCMP_VANILLAKEY && (code == 'r' || code == 'R')) {
                    minimum = maximum = -1; samples = 0; sample(); draw();
                }
                if (cls == IDCMP_REFRESHWINDOW) {
                    BeginRefresh(window); draw(); EndRefresh(window, TRUE);
                }
            }
            if (hide) { CloseWindow(window); window = NULL; }
        }
    }
    rc = 0;
done:
    if (menu) RemoveAppMenuItem(menu);
    if (port) {
        Forbid();
        if (FindPort((CONST_STRPTR)PORT_NAME) == port) RemPort(port);
        Permit();
        while ((message = GetMsg(port))) ReplyMsg(message);
    }
    if (window) CloseWindow(window);
    if (pending) {
        AbortIO((struct IORequest *)io);
        WaitIO((struct IORequest *)io);
    }
    if (timer_open) CloseDevice((struct IORequest *)io);
    if (io) DeleteIORequest((struct IORequest *)io);
    if (timer_port) DeleteMsgPort(timer_port);
    if (port) DeleteMsgPort(port);
    if (font) CloseFont(font);
    if (WorkbenchBase) CloseLibrary(WorkbenchBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    if (ExpansionBase) CloseLibrary((struct Library *)ExpansionBase);
    if (rc && argc) puts("ZZTemperature: requires ZZ9000 and AmigaOS 2.04+ resources");
    return rc;
}

/* Live acceptance helper; never installed in WBStartup. Sends only the
 * monitor's close event or the Workbench menu item identified by its label. */
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>
struct IntuitionBase *IntuitionBase;
int main(int argc, char **argv)
{
    struct Screen *screen;
    struct Window *win, *target = NULL;
    struct Menu *menu;
    struct MenuItem *item;
    struct IntuiMessage msg;
    struct MsgPort *reply;
    ULONG lock;
    UWORD code = 0, mn, it;
    int close_window = argc > 1 && !strcmp(argv[1], "CLOSE");
    int invoke = argc > 1 && !strcmp(argv[1], "INVOKE");
    IntuitionBase = (void *)OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (!IntuitionBase) return 20;
    reply = CreateMsgPort();
    if (!reply) { CloseLibrary((struct Library *)IntuitionBase); return 20; }
    memset(&msg, 0, sizeof(msg));
    msg.ExecMessage.mn_Node.ln_Type = NT_MESSAGE;
    msg.ExecMessage.mn_ReplyPort = reply;
    msg.ExecMessage.mn_Length = sizeof(msg);
    screen = LockPubScreen((CONST_STRPTR)"Workbench");
    lock = LockIBase(0);
    for (win = screen ? screen->FirstWindow : NULL; win && !target; win = win->NextWindow) {
        if (close_window) {
            if (win->Title && !strcmp((const char *)win->Title, "ZZ9000 Temperature"))
                target = win;
        } else {
            for (menu = win->MenuStrip, mn = 0; menu && !target; menu = menu->NextMenu, ++mn)
                for (item = menu->FirstItem, it = 0; item; item = item->NextItem, ++it)
                    if ((item->Flags & ITEMTEXT) && item->ItemFill) {
                        struct IntuiText *text = (struct IntuiText *)item->ItemFill;
                        if (text->IText && !strcmp((const char *)text->IText, "ZZ9000 Temperature...")) {
                            target = win;
                            code = FULLMENUNUM(mn, it, NOSUB);
                            break;
                        }
                    }
        }
    }
    if (target && (close_window || invoke)) {
        msg.Class = close_window ? IDCMP_CLOSEWINDOW : IDCMP_MENUPICK;
        msg.Code = code;
        msg.IDCMPWindow = target;
        PutMsg(target->UserPort, (struct Message *)&msg);
    }
    UnlockIBase(lock);
    if (screen) UnlockPubScreen(NULL, screen);
    if (target && (close_window || invoke)) {
        WaitPort(reply);
        GetMsg(reply);
    }
    printf("%s: %s (menu code=0x%04x)\n",
           close_window ? "Close monitor" : invoke ? "Invoke Workbench menu" : "Find Workbench menu",
           target ? "PASS" : "NOT FOUND", code);
    DeleteMsgPort(reply);
    CloseLibrary((struct Library *)IntuitionBase);
    return target ? 0 : 20;
}

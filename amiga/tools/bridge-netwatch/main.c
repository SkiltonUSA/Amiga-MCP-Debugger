/* A4000TX network-triggered bridge startup. No network connections are made.
 * Build with the pinned Amiga toolchain, -m68020 -O2 -Wall -Wextra -Werror.
 */
#include <exec/execbase.h>
#include <exec/ports.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <sys/types.h>
#include <proto/bsdsocket.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

#define WATCH_PORT "SIXIES.BRIDGE.NETWATCH"
#define BRIDGE_PORT "AMIGABRIDGE"
#define START_COMMAND "Execute SD032G:amiga-bridge/Start-Amiga-Bridge"
struct Library *SocketBase;

static int port_exists(const char *name) {
    int found;
    Forbid(); found = FindPort((CONST_STRPTR)name) != NULL; Permit();
    return found;
}

/* Do not cause Roadshow to start while it is offline. Never keep a socket
 * library open between polls, which would prevent a normal NetShutdown. */
static int network_ready(const char *interface) {
    struct ifreq req;
    struct sockaddr_in *addr;
    ULONG ip;
    LONG fd;
    int ready = 0, present;
    Forbid();
    present = FindName(&SysBase->LibList, (CONST_STRPTR)"bsdsocket.library") != NULL;
    Permit();
    if (!present || strlen(interface) >= sizeof(req.ifr_name)) return 0;
    SocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
    if (!SocketBase) return 0;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0) {
        memset(&req, 0, sizeof(req));
        strcpy((char *)req.ifr_name, interface);
        if (IoctlSocket(fd, SIOCGIFFLAGS, (char *)&req) == 0 &&
            (req.ifr_flags & IFF_UP) && !(req.ifr_flags & IFF_LOOPBACK)) {
            if (IoctlSocket(fd, SIOCGIFADDR, (char *)&req) == 0) {
                addr = (struct sockaddr_in *)&req.ifr_addr;
                ip = addr->sin_addr.s_addr; /* native 68k and network are big-endian */
                ready = addr->sin_family == AF_INET && ip != 0 &&
                        ip != 0xffffffffUL && (ip >> 24) != 127;
            }
        }
        CloseSocket(fd);
    }
    CloseLibrary(SocketBase); SocketBase = NULL;
    return ready;
}

/* An absent SD card must never open an Insert Volume requester at boot. */
static int files_available(void) {
    struct Process *process = (struct Process *)FindTask(NULL);
    APTR oldwindow = process->pr_WindowPtr;
    BPTR script, bridge;
    process->pr_WindowPtr = (APTR)-1;
    script = Lock((CONST_STRPTR)"SD032G:amiga-bridge/Start-Amiga-Bridge", ACCESS_READ);
    bridge = Lock((CONST_STRPTR)"SD032G:amiga-bridge/amiga-bridge", ACCESS_READ);
    process->pr_WindowPtr = oldwindow;
    if (script) UnLock(script);
    if (bridge) UnLock(bridge);
    return script && bridge;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "WATCH";
    const char *interface = argc > 2 ? argv[2] : "x-surf-100";
    struct MsgPort *port, *other;
    struct Task *me;
    char *oldname;
    LONG oldpri;
    int tick;
    if (!strcmp(mode, "CHECK")) {
        int ready = network_ready(interface);
        printf("Interface %s: %s; bridge: %s\n", interface,
               ready ? "UP with IPv4 address" : "not ready",
               port_exists(BRIDGE_PORT) ? "already running" : "not running");
        return ready ? 0 : 5;
    }
    if (!strcmp(mode, "READY")) {
        /* Nonzero means the launch script must not start another instance. */
        if (port_exists(BRIDGE_PORT)) return 5;
        return network_ready(interface) ? 0 : 5;
    }
    if (!strcmp(mode, "STOP")) {
        Forbid(); other = FindPort((CONST_STRPTR)WATCH_PORT);
        if (other) Signal(other->mp_SigTask, SIGBREAKF_CTRL_C);
        Permit(); return 0;
    }
    if (strcmp(mode, "WATCH")) {
        puts("Usage: bridge-netwatch [WATCH|CHECK|READY|STOP] [interface]");
        return 20;
    }
    port = CreateMsgPort();
    if (!port) return 20;
    port->mp_Node.ln_Name = (char *)WATCH_PORT;
    Forbid();
    if (FindPort((CONST_STRPTR)WATCH_PORT)) {
        Permit(); DeleteMsgPort(port); return 0;
    }
    AddPort(port); Permit();
    me = FindTask(NULL); oldname = me->tc_Node.ln_Name;
    me->tc_Node.ln_Name = (char *)"AmigaBridge NetWatch";
    oldpri = SetTaskPri(me, -5);
    for (;;) {
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) break;
        if (!port_exists(BRIDGE_PORT) && network_ready(interface) && files_available()) {
            BPTR input = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
            BPTR output = Open((CONST_STRPTR)"RAM:AmigaBridge-Autostart.log", MODE_NEWFILE);
            if (input && output) {
                /* The bridge must inherit normal priority, not the watcher's. */
                SetTaskPri(me, 0);
                SystemTags((CONST_STRPTR)START_COMMAND,
                           SYS_Input, input, SYS_Output, output, TAG_DONE);
                SetTaskPri(me, -5);
            }
            if (input) Close(input);
            if (output) Close(output);
            /* Avoid a rapid retry loop if the executable fails to start. */
            for (tick = 0; tick < 30; tick++) {
                Delay(50);
                if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) goto done;
                if (port_exists(BRIDGE_PORT)) break;
            }
        }
        /* 5 seconds between polls; STOP responds within one second. */
        for (tick = 0; tick < 5; tick++) {
            Delay(50);
            if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) goto done;
        }
    }
 done:
    Forbid(); RemPort(port); Permit();
    DeleteMsgPort(port);
    SetTaskPri(me, oldpri); me->tc_Node.ln_Name = oldname;
    return 0;
}

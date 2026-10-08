/* Minimal post-install handshake, based on XX19c main.c's 0x64 handler.
 * Reserved 0x0340 returns status 3 without a DDR payload or Core1 launch.
 * Requires idle status, then resets the idle command state afterwards.
 */
#include <exec/types.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/dos.h>
#include <stdio.h>
struct ExpansionBase *ExpansionBase;
static UWORD wait_status(volatile UWORD *reg, UWORD expected) {
 unsigned i; UWORD status;
 for(i=0;i<25;i++) { status=*reg; if(status==expected) return status; Delay(1); }
 return *reg;
}
int main(void) {
 struct ConfigDev *cd; ULONG addr; UWORD version,before,reply,after;
 volatile UWORD *cmd; int rc=10;
 ExpansionBase=(struct ExpansionBase *)OpenLibrary("expansion.library",0);
 if(!ExpansionBase) return 20;
 cd=FindConfigDev(NULL,0x6d6e,4);
 if(!cd) { puts("FAIL: Zorro III ZZ9000 not found"); goto done; }
 addr=(ULONG)cd->cd_BoardAddr;
 version=*(volatile UWORD *)(addr+0xc0);
 printf("Firmware register: 0x%04x\n",(unsigned)version);
 if(version!=0x0113) { puts("FAIL: unexpected firmware; no commands sent"); goto done; }
 cmd=(volatile UWORD *)(addr+0x64); before=*cmd;
 printf("Initial XACP status: %u\n",(unsigned)before);
 if(before!=0) { puts("FAIL: XACP not idle; no commands sent"); goto done; }
 *cmd=0x0340; reply=wait_status(cmd,3);
 printf("Reserved opcode 0x0340 response: %u (expected 3 = unsupported)\n",(unsigned)reply);
 *cmd=0; after=wait_status(cmd,0);
 printf("Reset-to-idle status: %u (expected 0)\n",(unsigned)after);
 if(reply==3 && after==0) { puts("PASS: XACP command handshake 0 -> 3 -> 0"); rc=0; }
 else puts("FAIL: unexpected XACP response");
 done: CloseLibrary((struct Library *)ExpansionBase); return rc;
}

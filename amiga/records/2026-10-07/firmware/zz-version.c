#include <exec/types.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <stdio.h>
struct ExpansionBase *ExpansionBase;
int main(void) {
 struct ConfigDev *cd;
 ULONG addr;
 UWORD version;
 ExpansionBase=(struct ExpansionBase *)OpenLibrary("expansion.library",0);
 if(!ExpansionBase) return 20;
 cd=FindConfigDev(NULL,0x6d6e,4);
 if(!cd) cd=FindConfigDev(NULL,0x6d6e,3);
 if(!cd) { CloseLibrary((struct Library *)ExpansionBase); puts("ZZ9000 not found"); return 10; }
 addr=(ULONG)cd->cd_BoardAddr;
 version=*(volatile UWORD *)(addr+0xc0);
 printf("ZZ9000 board=0x%08lx firmware_register=0x%04x\n",(unsigned long)addr,(unsigned)version);
 CloseLibrary((struct Library *)ExpansionBase);
 return 0;
}

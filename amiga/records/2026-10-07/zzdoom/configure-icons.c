#include <exec/types.h>
#include <exec/memory.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <stdio.h>
#include <string.h>
struct Library *IconBase;
static int configure(char *path){
 struct DiskObject *d=GetDiskObject(path);STRPTR *old,*tt;int n=0,k=0,i;BOOL ok;
 if(!d){printf("Icon read failed: %s\n",path);return 0;}
 old=d->do_ToolTypes;if(old)while(old[n])n++;
 tt=AllocVec((n+3)*sizeof(STRPTR),MEMF_PUBLIC|MEMF_CLEAR);
 if(!tt){FreeDiskObject(d);return 0;}
 for(i=0;i<n;i++)if(strncmp((const char *)old[i],"WAD=",4)&&strcmp((const char *)old[i],"NOMUSIC"))tt[k++]=old[i];
 tt[k++]=(STRPTR)"WAD=DOOM1.WAD";tt[k++]=(STRPTR)"NOMUSIC";
 d->do_ToolTypes=tt;d->do_StackSize=65536;
 ok=PutDiskObject(path,d);d->do_ToolTypes=old;FreeVec(tt);FreeDiskObject(d);
 if(!ok){printf("Icon write failed: %s\n",path);return 0;}
 d=GetDiskObject(path);if(!d)return 0;
 printf("%s stack=%ld\n",path,(long)d->do_StackSize);
 for(i=0;d->do_ToolTypes&&d->do_ToolTypes[i];i++)if(!strncmp((const char *)d->do_ToolTypes[i],"WAD=",4)||!strcmp((const char *)d->do_ToolTypes[i],"NOMUSIC"))printf("%s\n",d->do_ToolTypes[i]);
 FreeDiskObject(d);return 1;
}
int main(void){struct DiskObject *d;int ok;
 IconBase=OpenLibrary("icon.library",39);if(!IconBase)return 20;
 ok=configure("HDD50Gig:Games/ZZDoom/ZZDoom320")&&configure("HDD50Gig:Games/ZZDoom/ZZDoom640");
 if(ok){d=GetDefDiskObject(WBDRAWER);if(d){ok=PutDiskObject("HDD50Gig:Games/ZZDoom",d);FreeDiskObject(d);}else ok=0;}
 CloseLibrary(IconBase);return ok?0:20;}

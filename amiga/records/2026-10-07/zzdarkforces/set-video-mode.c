#include <exec/types.h>
#include <exec/memory.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <stdio.h>
#include <string.h>
struct Library *IconBase;
int main(int argc,char **argv){
 struct DiskObject *d;STRPTR *tt,*old;int i,n=0,found=0;BOOL ok;
 if(argc!=2||strcmp(argv[1],"640x480"))return 20;
 IconBase=OpenLibrary("icon.library",39);if(!IconBase)return 20;
 d=GetDiskObject("HDD50Gig:Games/ZZDarkForcesNEXT/ZZDarkForces");
 if(!d){CloseLibrary(IconBase);return 20;}
 old=d->do_ToolTypes;
 while(old&&old[n])n++;
 tt=AllocVec((n+1)*sizeof(STRPTR),MEMF_PUBLIC|MEMF_CLEAR);
 if(!tt){FreeDiskObject(d);CloseLibrary(IconBase);return 20;}
 for(i=0;i<n;i++)tt[i]=old[i];
 for(i=0;tt&&tt[i];i++){
  if(!strcmp((char*)tt[i],"320x240")){tt[i]=(STRPTR)"(320x240)";found++;}
  else if(!strcmp((char*)tt[i],"(640x480)")){tt[i]=(STRPTR)"640x480";found++;}
 }
 if(found!=2){printf("Expected source tooltypes missing: %d\n",found);FreeVec(tt);FreeDiskObject(d);CloseLibrary(IconBase);return 20;}
 d->do_ToolTypes=tt;
 ok=PutDiskObject("HDD50Gig:Games/ZZDarkForcesNEXT/ZZDarkForces",d);
 d->do_ToolTypes=old;FreeVec(tt);
 FreeDiskObject(d);
 d=GetDiskObject("HDD50Gig:Games/ZZDarkForcesNEXT/ZZDarkForces");
 if(d){printf("Stack %ld\n",(long)d->do_StackSize);for(i=0;d->do_ToolTypes&&d->do_ToolTypes[i];i++)printf("%s\n",d->do_ToolTypes[i]);FreeDiskObject(d);}else ok=FALSE;
 CloseLibrary(IconBase);return ok?0:20;
}

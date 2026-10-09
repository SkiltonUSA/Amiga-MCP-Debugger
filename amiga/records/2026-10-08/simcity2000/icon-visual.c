/* Copy icon artwork while preserving the target's launch metadata. */
#include <exec/types.h>
#include <exec/memory.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <stdio.h>
#include <string.h>
struct Library *IconBase;
static int visual(const void *p) {
 const char *s=p;
 return !strncmp(s,"IM1=",4)||!strncmp(s,"IM2=",4)||!strcmp(s," ")||
 !strcmp(s,"*** DON'T EDIT THE FOLLOWING LINES!! ***")||!strncmp(s,"(icon by ",9);
}
static int same(const void *a,const void *b){return (!a&&!b)||(a&&b&&!strcmp(a,b));}
static int check(struct DiskObject *a,struct DiskObject *b) {
 unsigned i,j=0;
 if(a->do_Type!=b->do_Type||a->do_StackSize!=b->do_StackSize||
 a->do_CurrentX!=b->do_CurrentX||a->do_CurrentY!=b->do_CurrentY||
 !same(a->do_DefaultTool,b->do_DefaultTool)||!same(a->do_ToolWindow,b->do_ToolWindow))return 0;
 for(i=0;a->do_ToolTypes&&a->do_ToolTypes[i];i++)if(!visual(a->do_ToolTypes[i])){
  while(b->do_ToolTypes&&b->do_ToolTypes[j]&&visual(b->do_ToolTypes[j]))j++;
  if(!b->do_ToolTypes||!same(a->do_ToolTypes[i],b->do_ToolTypes[j]))return 0;
  j++;
 }
 while(b->do_ToolTypes&&b->do_ToolTypes[j])if(!visual(b->do_ToolTypes[j++]))return 0;
 return 1;
}
int main(int argc,char **argv){
 struct DiskObject *a=0,*b=0,*r=0,save;STRPTR *tt=0;unsigned i,n=0,k=0;int rc=20,ok;
 if(argc<2||argc>3)return 20;
 IconBase=OpenLibrary("icon.library",39);if(!IconBase)return 20;
 a=GetDiskObject(argv[1]);if(!a)goto done;
 printf("ICON %s type=%u stack=%ld tool=%s\n",argv[1],a->do_Type,(long)a->do_StackSize,a->do_DefaultTool?(char *)a->do_DefaultTool:"(none)");
 for(i=0;a->do_ToolTypes&&a->do_ToolTypes[i];i++)if(!visual(a->do_ToolTypes[i]))printf("  %s\n",a->do_ToolTypes[i]);
 if(argc==2){rc=0;goto done;}
 b=GetDiskObject(argv[2]);if(!b||a->do_Type!=b->do_Type)goto done;
 for(i=0;a->do_ToolTypes&&a->do_ToolTypes[i];i++)n++;
 for(i=0;b->do_ToolTypes&&b->do_ToolTypes[i];i++)n++;
 tt=AllocVec((n+1)*sizeof(STRPTR),MEMF_PUBLIC|MEMF_CLEAR);if(!tt)goto done;
 for(i=0;a->do_ToolTypes&&a->do_ToolTypes[i];i++)if(!visual(a->do_ToolTypes[i]))tt[k++]=a->do_ToolTypes[i];
 for(i=0;b->do_ToolTypes&&b->do_ToolTypes[i];i++)if(visual(b->do_ToolTypes[i]))tt[k++]=b->do_ToolTypes[i];
 save=*b;
 b->do_DefaultTool=a->do_DefaultTool;b->do_ToolTypes=tt;b->do_ToolWindow=a->do_ToolWindow;
 b->do_CurrentX=a->do_CurrentX;b->do_CurrentY=a->do_CurrentY;b->do_StackSize=a->do_StackSize;b->do_DrawerData=a->do_DrawerData;
 ok=PutDiskObject(argv[1],b);*b=save;if(!ok)goto done;
 r=GetDiskObject(argv[1]);if(!r||!check(a,r))goto done;
 puts("ARTWORK UPDATED; launch metadata preserved and read back");rc=0;
 done:if(r)FreeDiskObject(r);if(tt)FreeVec(tt);if(b)FreeDiskObject(b);if(a)FreeDiskObject(a);CloseLibrary(IconBase);return rc;
}

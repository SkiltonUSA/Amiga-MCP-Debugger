#include <exec/types.h>
#include <resources/filesysres.h>
#include <proto/exec.h>
#include <stdio.h>
int main(void){
 struct FileSysResource *r=(struct FileSysResource *)OpenResource("FileSystem.resource");
 struct FileSysEntry *e;
 if(!r)return 5;
 for(e=(struct FileSysEntry *)r->fsr_FileSysEntries.lh_Head;e->fse_Node.ln_Succ;e=(struct FileSysEntry *)e->fse_Node.ln_Succ)
 printf("%08lx\t%lu.%lu\t%ld\t%s\n",(unsigned long)e->fse_DosType,(unsigned long)(e->fse_Version>>16),(unsigned long)(e->fse_Version&65535),(long)e->fse_Node.ln_Pri,e->fse_Node.ln_Name?e->fse_Node.ln_Name:"");
 return 0;
}

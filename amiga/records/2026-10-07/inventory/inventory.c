/* Read-only A4000TX inventory. Writes stdout only; SCSI commands are
 * INQUIRY and READ CAPACITY, issued only to currently mounted disk units. */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <libraries/expansion.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <stdio.h>
#include <string.h>
struct ExpansionBase *ExpansionBase;
struct Entry {char name[96],id[256];ULONG addr,neg,pos,flags;UWORD version,revision,opens;};
static struct Entry entries[256];
static void clean(char *out,const char *in,int max){int i=0;if(in)while(in[i]&&i<max-1){out[i]=(in[i]=='\n'||in[i]=='\r'||in[i]=='\t')?' ':in[i];i++;}out[i]=0;}
static void listlibs(struct List *list,const char *type){
 struct Library *lib;int n=0,i;
 Forbid();
 for(lib=(struct Library *)list->lh_Head;lib->lib_Node.ln_Succ&&n<256;lib=(struct Library *)lib->lib_Node.ln_Succ){
  struct Entry *e=&entries[n++];clean(e->name,lib->lib_Node.ln_Name,sizeof(e->name));clean(e->id,lib->lib_IdString,sizeof(e->id));
  e->addr=(ULONG)lib;e->neg=lib->lib_NegSize;e->pos=lib->lib_PosSize;e->flags=lib->lib_Flags;e->version=lib->lib_Version;e->revision=lib->lib_Revision;e->opens=lib->lib_OpenCnt;
 }
 Permit();
 for(i=0;i<n;i++){struct Entry *e=&entries[i];printf("%s\t%s\t%u\t%u\t%u\t%08lx\t%lu\t%lu\t%02lx\t%s\n",type,e->name,e->version,e->revision,e->opens,(unsigned long)e->addr,(unsigned long)e->neg,(unsigned long)e->pos,(unsigned long)e->flags,e->id);}
}
struct Disk {char name[64],driver[128];ULONG unit,flags,dostype,sizeblock,surfaces,bpt,low,high,buf,maxtransfer,mask,bootpri;};
static struct Disk disks[32];
static void bstr(char *out,BPTR p,int max){UBYTE *s=(UBYTE *)BADDR(p);int len;if(!p){*out=0;return;}len=*s;if(len>=max)len=max-1;memcpy(out,s+1,len);out[len]=0;}
static int dosdisks(void){
 struct DosList *dl;int n=0;
 dl=LockDosList(LDF_DEVICES|LDF_READ);
 if(!dl)return 0;
 while((dl=NextDosEntry(dl,LDF_DEVICES))&&n<32){
  struct FileSysStartupMsg *fs;struct DosEnvec *de;struct Disk *d;
  if((ULONG)dl->dol_misc.dol_handler.dol_Startup<1024)continue;
  fs=(struct FileSysStartupMsg *)BADDR(dl->dol_misc.dol_handler.dol_Startup);
  if(!TypeOfMem(fs)||!fs->fssm_Device||!fs->fssm_Environ)continue;
  de=(struct DosEnvec *)BADDR(fs->fssm_Environ);
  if(!TypeOfMem(de)||de->de_TableSize<16||de->de_TableSize>64)continue;
  d=&disks[n++];bstr(d->name,dl->dol_Name,sizeof(d->name));bstr(d->driver,fs->fssm_Device,sizeof(d->driver));
  d->unit=fs->fssm_Unit;d->flags=fs->fssm_Flags;d->dostype=de->de_DosType;d->sizeblock=de->de_SizeBlock;d->surfaces=de->de_Surfaces;d->bpt=de->de_BlocksPerTrack;d->low=de->de_LowCyl;d->high=de->de_HighCyl;d->buf=de->de_NumBuffers;d->maxtransfer=de->de_MaxTransfer;d->mask=de->de_Mask;d->bootpri=de->de_BootPri;
 }
 UnLockDosList(LDF_DEVICES|LDF_READ);return n;
}
static int bounded_io(struct IOStdReq *io){int i;SendIO((struct IORequest *)io);for(i=0;i<150&&!CheckIO((struct IORequest *)io);i++)Delay(1);if(!CheckIO((struct IORequest *)io))AbortIO((struct IORequest *)io);WaitIO((struct IORequest *)io);return io->io_Error;}
static int scsi(struct IOStdReq *io,UBYTE *cmd,UWORD len,UBYTE *data,ULONG size,ULONG *actual){
 struct SCSICmd s;UBYTE sense[32];int err;
 memset(&s,0,sizeof(s));memset(data,0,size);memset(sense,0,sizeof(sense));
 s.scsi_Data=(UWORD *)data;s.scsi_Length=size;s.scsi_Command=cmd;s.scsi_CmdLength=len;s.scsi_Flags=SCSIF_READ|SCSIF_AUTOSENSE;s.scsi_SenseData=sense;s.scsi_SenseLength=sizeof(sense);
 io->io_Command=HD_SCSICMD;io->io_Data=&s;io->io_Length=sizeof(s);io->io_Flags=0;
 err=bounded_io(io);*actual=s.scsi_Actual;return err?err:s.scsi_Status;
}
static ULONG be32(const UBYTE *p){return ((ULONG)p[0]<<24)|((ULONG)p[1]<<16)|((ULONG)p[2]<<8)|p[3];}
static void inquiry(struct Disk *d){
 struct MsgPort *port;struct IOStdReq *io;UBYTE cmd[10],data[256];ULONG actual;int err;char vendor[9],product[17],revision[5],serial[253];
 /* No blind unit scans and no floppy motor operations. */
 if(!strstr(d->driver,"buddha")&&!strstr(d->driver,"ehide")&&!strstr(d->driver,"usbscsi"))return;
 port=CreateMsgPort();if(!port)return;io=(struct IOStdReq *)CreateIORequest(port,sizeof(*io));if(!io){DeleteMsgPort(port);return;}
 err=OpenDevice((CONST_STRPTR)d->driver,d->unit,(struct IORequest *)io,0);
 if(err){printf("DISK_ERROR\t%s\t%lu\tOpenDevice %d\n",d->driver,(unsigned long)d->unit,err);goto done;}
 memset(cmd,0,sizeof(cmd));cmd[0]=0x12;cmd[4]=96;err=scsi(io,cmd,6,data,96,&actual);
 if(!err&&actual>=36){memcpy(vendor,data+8,8);vendor[8]=0;memcpy(product,data+16,16);product[16]=0;memcpy(revision,data+32,4);revision[4]=0;printf("INQUIRY\t%s\t%lu\t%u\t%u\t%s\t%s\t%s\n",d->driver,(unsigned long)d->unit,data[0]&31,(data[1]&128)!=0,vendor,product,revision);}
 else printf("DISK_ERROR\t%s\t%lu\tINQUIRY %d actual %lu\n",d->driver,(unsigned long)d->unit,err,(unsigned long)actual);
 memset(cmd,0,sizeof(cmd));cmd[0]=0x25;err=scsi(io,cmd,10,data,8,&actual);
 if(!err&&actual>=8)printf("CAPACITY\t%s\t%lu\t%lu\t%lu\n",d->driver,(unsigned long)d->unit,(unsigned long)be32(data),(unsigned long)be32(data+4));
 else printf("DISK_ERROR\t%s\t%lu\tREAD_CAPACITY %d\n",d->driver,(unsigned long)d->unit,err);
 memset(cmd,0,sizeof(cmd));cmd[0]=0x12;cmd[1]=1;cmd[2]=0x80;cmd[4]=252;err=scsi(io,cmd,6,data,252,&actual);
 if(!err&&actual>=4&&data[1]==0x80){ULONG len=((ULONG)data[2]<<8)|data[3];if(len<=actual-4&&len<sizeof(serial)){memcpy(serial,data+4,len);serial[len]=0;printf("SERIAL\t%s\t%lu\t%s\n",d->driver,(unsigned long)d->unit,serial);}}
 CloseDevice((struct IORequest *)io);
 done:DeleteIORequest((struct IORequest *)io);DeleteMsgPort(port);
}
int main(void){
 struct Node *node;struct MemHeader *mh;struct ConfigDev *cd;int n,i,j;char name[96];
 printf("EXEC\t%u\t%u\t%04x\t%lu\t%u\t%u\n",SysBase->LibNode.lib_Version,SysBase->LibNode.lib_Revision,SysBase->AttnFlags,(unsigned long)SysBase->ex_EClockFrequency,SysBase->VBlankFrequency,SysBase->PowerSupplyFrequency);
 listlibs(&SysBase->LibList,"LIB");listlibs(&SysBase->DeviceList,"DEV");
 /* Names are stable OS resource entries; no driver resource internals read. */
 for(node=SysBase->ResourceList.lh_Head;node->ln_Succ;node=node->ln_Succ){clean(name,node->ln_Name,sizeof(name));printf("RESOURCE\t%s\t%08lx\n",name,(unsigned long)node);}
 for(mh=(struct MemHeader *)SysBase->MemList.lh_Head;mh->mh_Node.ln_Succ;mh=(struct MemHeader *)mh->mh_Node.ln_Succ){clean(name,mh->mh_Node.ln_Name,sizeof(name));printf("MEM\t%s\t%08lx\t%08lx\t%04x\t%lu\t%d\n",name,(unsigned long)mh->mh_Lower,(unsigned long)mh->mh_Upper,mh->mh_Attributes,(unsigned long)mh->mh_Free,mh->mh_Node.ln_Pri);}
 ExpansionBase=(struct ExpansionBase *)OpenLibrary("expansion.library",0);
 if(ExpansionBase){for(cd=FindConfigDev(NULL,-1,-1);cd;cd=FindConfigDev(cd,-1,-1))printf("BOARD\t%u\t%u\t%08lx\t%lu\t%02x\t%02x\t%lu\n",cd->cd_Rom.er_Manufacturer,cd->cd_Rom.er_Product,(unsigned long)cd->cd_BoardAddr,(unsigned long)cd->cd_BoardSize,cd->cd_Rom.er_Type,cd->cd_Flags,(unsigned long)cd->cd_Rom.er_SerialNumber);CloseLibrary((struct Library *)ExpansionBase);}
 n=dosdisks();for(i=0;i<n;i++){struct Disk *d=&disks[i];printf("DOSDEV\t%s\t%s\t%lu\t%08lx\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%08lx\t%08lx\t%ld\n",d->name,d->driver,(unsigned long)d->unit,(unsigned long)d->dostype,(unsigned long)d->sizeblock,(unsigned long)d->surfaces,(unsigned long)d->bpt,(unsigned long)d->low,(unsigned long)d->high,(unsigned long)d->buf,(unsigned long)d->maxtransfer,(unsigned long)d->mask,(long)d->bootpri);}
 for(i=0;i<n;i++){for(j=0;j<i;j++)if(disks[j].unit==disks[i].unit&&!strcmp(disks[j].driver,disks[i].driver))break;if(j==i)inquiry(&disks[i]);}
 return 0;
}

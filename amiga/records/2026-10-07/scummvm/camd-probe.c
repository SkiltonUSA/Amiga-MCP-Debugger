#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>
#include <stdio.h>
#include <string.h>
struct Library *CamdBase;
int main(int argc,char **argv) {
 struct MidiNode *node;struct MidiLink *link;struct MidiCluster *cluster;APTR lock;
 struct TagItem nt[]={{MIDI_Name,(ULONG)"ScummVM setup check"},{MIDI_MsgQueue,0},{MIDI_SysExSize,0},{TAG_DONE,0}};
 struct TagItem lt[]={{MLINK_Location,(ULONG)"out.0"},{MLINK_Name,(ULONG)"MT-32 setup check"},{TAG_DONE,0}};
 int found=0,rc=10;
 CamdBase=OpenLibrary("camd.library",37);
 if(!CamdBase){puts("Cannot open CAMD");return 20;}
 if(argc>1 && !strcmp(argv[1],"REFRESH")) printf("RethinkCAMD: %d\n",RethinkCAMD());
 node=CreateMidiA(nt);if(!node){puts("Cannot create MIDI node");goto done;}
 lock=LockCAMD(CD_Linkages);
 if(lock){
  for(cluster=NextCluster(NULL);cluster;cluster=NextCluster(cluster)){
   printf("Cluster: %s\n",cluster->mcl_Node.ln_Name);
   if(!strcmp(cluster->mcl_Node.ln_Name,"out.0"))found=1;
  }
  UnlockCAMD(lock);
 }
 if(!found){puts("No out.0 cluster");DeleteMidi(node);goto done;}
 link=AddMidiLinkA(node,MLTYPE_Sender,lt);
 if(link){
  printf("out.0 receiver connected: %s\n",MidiLinkConnected(link)?"yes":"no");
  rc=MidiLinkConnected(link)?0:10;
  if(!rc && argc>1 && !strcmp(argv[1],"NOTE")){
   /* Channel 2, middle C, moderate velocity; no program/patch changes. */
   PutMidi(link,0x913c4000UL);Delay(25);PutMidi(link,0x813c0000UL);Delay(5);
   puts("Sent a half-second middle C on MIDI channel 2, followed by note-off.");
  }
  RemoveMidiLink(link);
 }else puts("Cannot link to out.0");
 DeleteMidi(node);
 done:CloseLibrary(CamdBase);return rc;
}

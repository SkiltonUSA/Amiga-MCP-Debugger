/* Create classic Workbench icons using our own two-bitplane fractal artwork.
 * Build-time packaging helper only; not required or shipped with the app. */
#include <exec/types.h>
#include <workbench/workbench.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <stdio.h>
#include <string.h>
struct Library *IconBase;
static UWORD planes[2*3*32];
static struct Image image;
static struct DrawerData drawer;
static int make_icon(char *path,int type)
{
    struct DiskObject *d=NewDiskObject(type);APTR old;UWORD flags,width,height;BOOL ok;
    if(!d)return 0;
    if(type==WBDRAWER)d->do_DrawerData=&drawer;
    old=d->do_Gadget.GadgetRender;flags=d->do_Gadget.Flags;
    width=d->do_Gadget.Width;height=d->do_Gadget.Height;
    d->do_Gadget.GadgetRender=&image;d->do_Gadget.Flags=GFLG_GADGIMAGE|GFLG_GADGHCOMP;
    d->do_Gadget.Width=48;d->do_Gadget.Height=32;
    d->do_CurrentX=NO_ICON_POSITION;d->do_CurrentY=NO_ICON_POSITION;d->do_StackSize=65536;
    ok=PutDiskObject(path,d);
    d->do_Gadget.GadgetRender=old;d->do_Gadget.Flags=flags;
    d->do_Gadget.Width=width;d->do_Gadget.Height=height;
    if(type==WBDRAWER)d->do_DrawerData=0;
    FreeDiskObject(d);
    if(!ok)return 0;
    d=GetDiskObject(path);if(!d)return 0;
    ok=d->do_Type==type && d->do_StackSize==65536;
    printf("%s.info type=%d stack=%u image=%dx%d verified=%d\n",path,type,(unsigned)d->do_StackSize,d->do_Gadget.Width,d->do_Gadget.Height,ok);
    FreeDiskObject(d);return ok;
}
int main(int argc,char **argv)
{
    unsigned px,py,i,colour;char path[256];int ok;
    if(argc!=2||strlen(argv[1])>220)return 20;
    for(py=0;py<32;py++)for(px=0;px<48;px++) {
        long cr=-2304+(long)px*64,ci=-1024+(long)py*64,x=0,y=0,xx,yy;
        for(i=0;i<32;i++) {
            if(x>2048||x< -2048||y>2048||y< -2048)break;
            xx=x*x/1024;yy=y*y/1024;if(xx+yy>4096)break;
            y=2*(x*y/1024)+ci;x=xx-yy+cr;
        }
        colour=i==32?1:2+(i&1);
        if(colour&1)planes[py*3+px/16]|=1u<<(15-px%16);
        if(colour&2)planes[96+py*3+px/16]|=1u<<(15-px%16);
    }
    memset(&drawer,0,sizeof(drawer));
    drawer.dd_NewWindow.LeftEdge=80;drawer.dd_NewWindow.TopEdge=80;
    drawer.dd_NewWindow.Width=440;drawer.dd_NewWindow.Height=240;
    drawer.dd_NewWindow.DetailPen=0;drawer.dd_NewWindow.BlockPen=1;
    drawer.dd_NewWindow.Flags=WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_CLOSEGADGET|WFLG_SIZEGADGET;
    drawer.dd_NewWindow.MinWidth=100;drawer.dd_NewWindow.MinHeight=80;
    drawer.dd_NewWindow.MaxWidth=0xffff;drawer.dd_NewWindow.MaxHeight=0xffff;
    memset(&image,0,sizeof(image));image.Width=48;image.Height=32;image.Depth=2;
    image.ImageData=planes;image.PlanePick=3;
    IconBase=OpenLibrary("icon.library",44);if(!IconBase)return 20;
    ok=make_icon(argv[1],WBDRAWER);
    snprintf(path,sizeof(path),"%s/ZZFractal",argv[1]);ok=make_icon(path,WBTOOL)&&ok;
    CloseLibrary(IconBase);return ok?0:20;
}

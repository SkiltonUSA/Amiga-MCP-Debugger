/* Installation helper: create a normal tool icon for the resident app. */
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/icon.h>
struct Library *IconBase;
int main(int argc, char **argv)
{
    struct DiskObject *icon;
    STRPTR *original;
    STRPTR types[] = {(STRPTR)"DONOTWAIT", (STRPTR)"STARTPRI=-5", 0};
    int ok = 0;
    if (argc != 2) return 20;
    IconBase = OpenLibrary((CONST_STRPTR)"icon.library", 37);
    if (!IconBase) return 20;
    icon = GetDefDiskObject(WBTOOL);
    if (icon) {
        original = icon->do_ToolTypes;
        icon->do_ToolTypes = types;
        icon->do_StackSize = 16384;
        ok = PutDiskObject((CONST_STRPTR)argv[1], icon);
        icon->do_ToolTypes = original;
        FreeDiskObject(icon);
    }
    CloseLibrary(IconBase);
    return ok ? 0 : 20;
}

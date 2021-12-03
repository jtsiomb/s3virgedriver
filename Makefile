obj = main.obj pci.obj virge.obj vbe.obj mem.obj logger.obj mouse.obj
bin = test.exe

#opt = -otexan
warn = -w=3
dbg = -d3

CC = wcc386
LD = wlink
CFLAGS = $(warn) $(dbg) $(opt) $(def) -zq -bt=dos

$(bin): $(obj)
	%write objects.lnk $(obj)
	$(LD) debug all option map name $@ system dos4g file { @objects } $(LDFLAGS)

.c: src
.asm: src

.c.obj: .autodepend
	$(CC) -fo=$@ $(CFLAGS) $<

.asm.obj:
	nasm -f obj -o $@ $[*.asm

clean: .symbolic
	del *.obj
	del objects.lnk
	del $(bin)

```
.
├── Makefile
├── README.md
├── boot
│   ├── build.sh
│   ├── include
│   │   └── boot.inc
│   ├── loader.S
│   └── mbr.S
├── device
│   ├── console.c
│   ├── console.h
│   ├── keyboard.c
│   ├── keyboard.h
│   ├── timer.c
│   └── timer.h
├── kernel
│   ├── debug.c
│   ├── debug.h
│   ├── global.h
│   ├── init.c
│   ├── init.h
│   ├── interrupt.c
│   ├── interrupt.h
│   ├── kernel.S
│   ├── main.c
│   ├── memory.c
│   └── memory.h
├── lib
│   ├── kernel
│   │   ├── bitmap.c
│   │   ├── bitmap.h
│   │   ├── io.h
│   │   ├── ioqueue.c
│   │   ├── ioqueue.h
│   │   ├── list.c
│   │   ├── list.h
│   │   ├── print.S
│   │   └── print.h
│   ├── memfunc.c
│   ├── memfunc.h
│   ├── stdint.h
│   ├── stdtype.h
│   ├── string.c
│   ├── string.h
│   └── user
│       ├── syscall.c
│       └── syscall.h
├── link.script
├── thread
│   ├── switch.S
│   ├── sync.c
│   ├── sync.h
│   ├── thread.c
│   └── thread.h
├── userprog
│   ├── process.c
│   ├── process.h
│   ├── syscall-init.c
│   ├── syscall-init.h
│   ├── tss.c
│   └── tss.h
└── vdisk.img
```

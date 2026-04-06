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
├── kernel
│   ├── debug.c
│   ├── device
│   │   ├── console.c
│   │   ├── keyboard.c
│   │   └── timer.c
│   ├── include
│   │   ├── console.h
│   │   ├── debug.h
│   │   ├── global.h
│   │   ├── init.h
│   │   ├── interrupt.h
│   │   ├── keyboard.h
│   │   ├── memory.h
│   │   ├── process.h
│   │   ├── sync.h
│   │   ├── thread.h
│   │   ├── timer.h
│   │   └── tss.h
│   ├── init.c
│   ├── interrupt
│   │   └── interrupt.c
│   ├── kernel.S
│   ├── main.c
│   ├── memory
│   │   └── memory.c
│   ├── process
│   │   ├── process.c
│   │   └── tss.c
│   └── thread
│       ├── switch.S
│       ├── sync.c
│       └── thread.c
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
│   └── string.h
├── link.script
└── vdisk.img
```

target_sources(kernel PRIVATE

    main.c

    acpi/acpi.c
    blkdev/ahci.c
    blkdev/blkdev.c
    blkdev/blkpart.c
    blkdev/gpt.c
    cmdline.c
    conmgr.c
    console.c
    cpu.c
    devmgr.c
    elf.c
    fildes.c
    framebuf.c
    fs/devfs.c
    fs/ramfs.c
    heap.c
    init.c
    inputmgr.c
    kbd.c
    kerr.c
    keymap.c
    kprintf.c
    kstring.c
    ksyscall.c
    ldisc.c
    libshim.c
    log.c
    memfun.c
    panic.c
    pci.c
    pmm.c
    psf.c
    serial.c
    slab.c
    smp.c
    stack.c
    taskmgr.c
    textdisp.c
    tty.c
    usermem.c
    vfs/dir_tree.c
    vfs/file.c
    vfs/vnode.c
    vfs/vpath.c

    kshell/kbdlog.c
    kshell/ksharg.c
    kshell/kshcmd/ksh_clear.c
    kshell/kshcmd/kshcmd.c
    kshell/kshcmd/ksh_devmgr.c
    kshell/kshcmd/ksh_help.c
    kshell/kshcmd/ksh_lua.c
    kshell/kshcmd/ksh_taskmgr.c
    kshell/kshcmd/ksh_vasview.c
    kshell/kshcmd/ksh_vfs.c
    kshell/kshell.c
    kshell/kshinput.c
    kshell/kshscan.c
    kshell/vasview.c

    dynarr.c
    kmutex.c
    ksemaphore.c
    kspinlock.c
    list.c
    queue.c
    ringbuf.c

)

if(YTKERNEL_ENABLE_TESTS)
    target_sources(kernel PRIVATE

        test/ktest_smp.c
        test/ktest.c
        test/smp/smp_suite_mutex.c
        test/smp/smp_suite_vnode.c
        test/vmctl.c

    )
endif()

add_subdirectory(arch/${YTKERNEL_ARCH})

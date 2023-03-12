OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(bl31_entrypoint)
MEMORY {
    RAM (rwx): ORIGIN = 0x30000000, LENGTH = (0x30000000 + (0x40000) - 1) - 0x30000000
}
SECTIONS
{
    . = 0x30000000;
    ASSERT(. == ALIGN(((1) << (12))),
           "BL31_BASE address is not aligned on a page boundary.")
    .text . : {
        __TEXT_START__ = .;
        *bl31_entrypoint.o(.text*)
        *(.text*)
        *(.vectors)
        . = ALIGN(((1) << (12)));
        __TEXT_END__ = .;
    } >RAM
    .rodata . : {
        __RODATA_START__ = .;
        *(.rodata*)
        . = ALIGN(8);
        __RT_SVC_DESCS_START__ = .;
        KEEP(*(rt_svc_descs))
        __RT_SVC_DESCS_END__ = .;
        . = ALIGN(8);
        __CPU_OPS_START__ = .;
        KEEP(*(cpu_ops))
        __CPU_OPS_END__ = .;
        . = ALIGN(((1) << (12)));
        __RODATA_END__ = .;
    } >RAM
    __RT_SVC_DESCS_SIZE__ = __RT_SVC_DESCS_END__ - __RT_SVC_DESCS_START__;
    ASSERT(__CPU_OPS_END__ > __CPU_OPS_START__,
           "cpu_ops not defined for this platform.")
    __RW_START__ = . ;
   .data . : {
        __DATA_START__ = .;
        *(.data*)
        __DATA_END__ = .;
    } >RAM
    stacks (NOLOAD) : {
        __STACKS_START__ = .;
        *(tzfw_normal_stacks)
        __STACKS_END__ = .;
    } >RAM
    .bss (NOLOAD) : ALIGN(16) {
        __BSS_START__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(((1) << 6));
        __BAKERY_LOCK_START__ = .;
        *(bakery_lock)
        . = ALIGN(((1) << 6));
        __PERCPU_BAKERY_LOCK_SIZE__ = ABSOLUTE(. - __BAKERY_LOCK_START__);
        . = . + (__PERCPU_BAKERY_LOCK_SIZE__ * ((2 * 4) - 1));
        __BAKERY_LOCK_END__ = .;
        __BSS_END__ = .;
    } >RAM
    xlat_table (NOLOAD) : {
        *(xlat_table)
    } >RAM
    __RW_END__ = .;
    __BL31_END__ = .;
    __BSS_SIZE__ = SIZEOF(.bss);
    ASSERT(. <= (0x30000000 + (0x40000) - 1), "BL31 image has exceeded its limit.")
}

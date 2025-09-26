# qemu -kernel将内核加载到0x80000000地址
        # 并使每个hart（即CPU）跳转到那里。
        # kernel.ld使以下代码
        # 被放置在0x80000000处。
.section .text
.global _entry
_entry:
        # 为C语言设置栈。
        # stack0在start.c中声明，
        # 每个CPU有一个4096字节的栈。
        # sp = stack0 + ((hartid + 1) * 4096)
        la sp, stack0
        li a0, 1024*4
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        # 跳转到start.c中的start()函数
        call start
spin:
        j spin

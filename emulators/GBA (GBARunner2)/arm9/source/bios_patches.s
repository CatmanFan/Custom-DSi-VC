.section .itcm

.global bios_waitintr_fix
bios_waitintr_fix:
	mcr p15,0,r0,c7,c0,4

/*.global bios_cpufastset_sd_patch
bios_cpufastset_sd_patch:
	beq 0xC24

	movs r12, r2, lsr #25
	bcs 0xBD8

	ldr r12,= 0x083B0000
	cmp r0, r12
	blt 0xBD8
	cmp r0, #0x0D000000
	bge 0xBD8

	ldr r12,= 0x33333333
	mcr p15, 0, r12, c5, c0, 2

	//ensure block d is mapped to the arm7
	//ldr r3,= 0x4000243
	//mov r4, #0x8A
	//strb r4, [r3]
	ldr r3,= 0x4000242
	ldr r4,= 0x8080
	strh r4, [r3]

	ldr r3,= 0x04000188
	ldr r4,= 0xAA5500C8
	str r4, [r3]

	bic r4, r0, #0x0E000000
	str r4, [r3]	//address

	//ldrh r4, [r10, #-0x2]
	mov r4, r10, lsr #9
	//tst r11, #(1 << 10)
	//moveq r4, r4, lsl #1
	//movne r4, r4, lsl #2

	ldr r3,= 0x04000188
	str r4, [r3]	//size

	//wait for the arm7 sync command
bios_cpufastset_sd_patch_fifo_loop:
	ldr r3,= 0x04000184
	ldr r3, [r3]
	tst r3, #(1 << 8)
	bne bios_cpufastset_sd_patch_fifo_loop
	ldr r3,= 0x04100000
	ldr r3, [r3]	//read word from fifo
	ldr r4,= 0x55AAC8AC
	cmp r3, r4
	bne bios_cpufastset_sd_patch_fifo_loop

	//block d to arm9 lcdc
	ldr r3,= 0x4000243
	mov r4, #0x80
	strb r4, [r3]

	ldr r12,= 0x33660003
	mcr p15, 0, r12, c5, c0, 2

	ldr r0,= 0x06868000
	
	b 0xBD8*/
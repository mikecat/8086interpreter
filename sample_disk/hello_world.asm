bits 16
org 0x7c00

	jmp main
	nop

	db 'mkdosfs', 0
	dw 0x0200        ; BytesPerSector
	db 0x01          ; SectorPerCluster
	dw 0x0001        ; ReservedSectors
	db 0x02          ; TotalFATs
	dw 0x00E0        ; MaxRootEntries
	dw 0x0B40        ; TotalSectors
	db 0xF0          ; MediaDescriptor
	dw 0x0009        ; SectorsPerFat
	dw 0x0012        ; SectorsPerTrack
	dw 0x0002        ; NumHeads
	dd 0x00000000    ; HiddenSector
	dd 0x00000000    ; TotalSectorsBig
	db 0x00          ; DriveNumber
	db 0x00          ; Reserved
	dd 0x00000000    ; VolumeSerialnumber
	db '           ' ; VolumeLabel
	db 'FAT12   '    ; FileSystemType

main:
	xor ax, ax
	mov ax, ds
	mov si, message
print_loop:
	mov al, [si]
	test al, al
	jz loop_end
	mov ah, 0x0e
	mov bx, 0x000f
	int 0x10
	inc si
	jmp print_loop

loop_end:
	cli
hlt_loop:
	hlt
	jmp hlt_loop

message:
	db `hello, world\r\n\0`

	times 510-($-$$) db 0
	dw 0xaa55

	; FAT
	db 0xf0, 0xff, 0xff
	times 0x200 * 9 - 3 db 0
	db 0xf0, 0xff, 0xff
	times 0x200 * 9 - 3 db 0
	; RDE
	times 0x200 * 0x0E db 0
	; data
	times 1474560-($-$$) db 0

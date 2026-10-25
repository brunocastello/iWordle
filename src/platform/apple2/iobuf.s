;
; ProDOS 8 I/O buffer management for iWordle: one 1 KB buffer in our own BSS.
; It replaces cc65's default iobuf (posix_memalign on the heap), which costs
; ~1.4 KB of code and a heap we do not have room for. Modelled on cc65's
; libsrc/apple2/extra/iobuf-0800.s.
;
; The buffer must not live on the text page ($0400-$07FF): although every
; screen is hi-res, the "screen holes" there ($0478-$047F, $04F8-$04FF, ...)
; are scratch RAM for slot firmware (SmartPort on the IIc/IIgs, Floppy Emu,
; mouse, serial), so disk I/O could corrupt the buffer or the firmware.
; ProDOS needs the buffer page-aligned, so 1024+255 bytes are reserved and
; the first page boundary inside them is handed out.
;
; Only one file may be open at a time; a second open() fails with ENOMEM.
;

        .export         iobuf_alloc, iobuf_free
        .import         incsp2, popptr1

        .include        "zeropage.inc"
        .include        "errno.inc"

        .code

; open() pushes memptr and the alignment, and passes the size in AX.
iobuf_alloc:
        jsr     incsp2          ; Drop the alignment
        jsr     popptr1         ; ptr1 = memptr

        lda     used
        beq     :+
        lda     #ENOMEM
        rts

:       lda     #$FF
        sta     used

        ; *memptr = first page boundary at or after iobuf
        ldy     #$01
        lda     #<iobuf
        cmp     #$01            ; carry set if the low byte is not 0
        lda     #>iobuf
        adc     #$00
        sta     (ptr1),y
        dey
        tya                     ; A = 0 (success)
        sta     (ptr1),y
        rts

; close() passes the buffer address in AX.
iobuf_free:
        lda     #$00
        sta     used
        rts

        .data

used:   .byte   $00

        .bss

iobuf:  .res    1024+255

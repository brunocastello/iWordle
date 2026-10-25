;
; ProDOS 8 I/O buffer management for iWordle: one 1 KB buffer at $0400, the
; text page, which is unused because every screen is hi-res. It replaces
; cc65's default iobuf (posix_memalign on the heap), which costs ~1.4 KB of
; code and a heap we do not have room for. Modelled on cc65's
; libsrc/apple2/extra/iobuf-0800.s.
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

        ; *memptr = $0400
        ldy     #$01
        lda     #>$0400
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

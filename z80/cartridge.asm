; TuxDAN's cartridge for the CPC Dandanator mini: slot 0.
;
; The machine starts here, zone 0 showing slot 0 at &0000 and zone 1
; hidden. The menu lists the games of the directory (at &3000, filled by
; the plugin) and plays the one chosen:
; - a ROM: its slot is put at &0000 in place of the machine's ROM;
; - a snapshot: its memory is put back from the cartridge, then the
;   machine's chips and the Z80, and the cartridge hides itself as the game
;   goes on.
;
; Slot 1 holds the games' headers, one every 512 bytes: the snapshot's own
; header (&000-&0FF), the table of its blocks (&100), its registers in the
; order they are popped (&140), and the few instructions that end the
; loading (&160), which the plugin writes for each game with its values.
; The blocks of memory, 16K each and packed, follow from slot 2 on.
;
; While the memory is put back, the cartridge's zones are read in place of
; RAM, but writes go to the RAM beneath. A block is unpacked into a quarter
; of memory no zone covers, as it reads back what it has unpacked: the
; quarters 1 to 3 with zone 0 (this code) at &0000, then quarter 0 with
; zone 0 at &8000, running the same loader assembled there. Nothing of the
; loader is kept in RAM, and it has no stack: all of the RAM is the game's.
;
; Commands to the cartridge are LD (IY+0),r after three &FD prefixes; they
; write r to (IY), so IY points where a byte may be spoiled: in quarter 0
; until it is put back, then just below the game's stack.

DIR             equ #3000       ; the directory: "TXDN", version, count, autoboot
DIR_COUNT       equ DIR + 5
DIR_AUTOBOOT    equ DIR + 6
DIR_TITLE       equ DIR + 8     ; 32 characters, ended by 0
DIR_ENTRIES     equ DIR + #40   ; 64 bytes each: name (32), type, slot
ENTRY_SIZE      equ 64
ENTRY_TYPE      equ 32          ; 0 a snapshot, 1 a ROM
ENTRY_SLOT      equ 33
FONT            equ #3800       ; characters 32 to 127, 8 bytes each
HEADERS         equ #C000       ; slot 1 seen through zone 1 at &C000
HEADER_SIZE     equ 512
HEADER_TABLE    equ #100
HEADER_REGS     equ #140
HEADER_STUB     equ #160
SCRATCH         equ #3F00       ; quarter 0's RAM, put back last
VISIBLE         equ 21          ; lines of the list on the screen

; The menu's variables, in RAM it does not need once a game is chosen.
sel             equ #8000
top             equ #8001
count           equ #8002
idx             equ #8003

; A command to the cartridge: three &FD prefixes, the last being the
; instruction's own.
macro command reg
        db #fd,#fd
        ld (iy+0),reg
mend

; The next byte of a block, read through zone 1, whose window starts at
; I * 256: at its end the window goes back to its start, on the next slot
; (kept in C'). Uses A and AF'.
macro getbyte
        ld a,(hl)
        inc hl
        ex af,af'
        ld a,i
        add a,#40
        cp h
        jr nz,@same
        ld a,i
        ld h,a
        exx
        inc c
        command c
        exx
@same
        ex af,af'
mend

; The blocks of the table at IX, unpacked, until the entry whose target is
; `last`, where it goes on at `finish`. `z0` is zone 0's half: 0 with this
; code at &0000, 4 with it at &8000.
;
; An entry: RAM configuration (&C0 to &C7), target quarter (0 to 3), first
; slot, offset in that slot. A block: tokens, 0 to &7F for 1 to 128 bytes
; that follow as they are, &80 to &FE for 3 to 129 bytes found again
; further back in the quarter (the distance in the two bytes that follow),
; &FF at its end.
macro blocks z0, last, finish
@next
        ld a,(ix+1)
        cp last
        jp z,finish
        ld a,(ix+0)
        ld b,#7f
        out (c),a
        ld a,(ix+1)
        rrca
        rrca
        ld d,a
        ld e,0
        ld a,(ix+1)
        cp 3
        ld a,#c0
        ld b,#88 + z0
        jr nz,@window
        ld a,#40
        ld b,#80 + z0
@window
        ; All of the entry read while zone 1 still shows the table.
        ld i,a
        ld l,(ix+3)
        add a,(ix+4)
        ld h,a
        exx
        ld c,(ix+2)
        exx
        ld a,b
        command a
        exx
        command c
        exx
@token
        getbyte
        cp #ff
        jp z,@done
        bit 7,a
        jr nz,@match
        inc a
        ld b,a
@literal
        getbyte
        ld (de),a
        inc de
        djnz @literal
        jp @token
@match
        sub #7d
        ld c,a
        getbyte
        ld b,a
        getbyte
        ld sp,hl
        ld h,a
        ld l,b
        ld a,e
        sub l
        ld l,a
        ld a,d
        sbc a,h
        ld h,a
        ld b,0
        ldir
        ld hl,0
        add hl,sp
        jp @token
@done
        ld a,#88 + z0
        command a
        exx
        ld c,1
        command c
        exx
        ld bc,5
        add ix,bc
        jp @next
mend

; HL = the header's base (in IX) + offset.
macro hlbase offset
        ld a,ixl
        add a,offset
        ld l,a
        ld a,ixh
        adc a,0
        ld h,a
mend

        org #0000
start:
        di
        ld sp,#c000
        im 1
        ld bc,#7f8d             ; mode 1, both ROMs off
        out (c),c
        ld bc,#7fc0
        out (c),c
        ld hl,crtc_values
        ld bc,#bc00
init_crtc:
        out (c),c
        inc b
        ld a,(hl)
        out (c),a
        dec b
        inc hl
        inc c
        ld a,c
        cp 16
        jr nz,init_crtc
        ld bc,#f782
        out (c),c
        ld de,#073f             ; the sound chip quiet
        call psg_write
        ld de,#0800
        call psg_write
        ld de,#0900
        call psg_write
        ld de,#0a00
        call psg_write
        ld hl,palette
        ld bc,#7f00
init_pens:
        out (c),c
        ld a,(hl)
        out (c),a
        inc hl
        inc c
        ld a,c
        cp 4
        jr nz,init_pens
        ld c,#10
        out (c),c
        ld a,#54
        out (c),a
        ld hl,#c000
        ld de,#c001
        ld bc,#3fff
        ld (hl),0
        ldir
        ; A game that starts by itself, unless a key is held.
        ld a,(DIR_AUTOBOOT)
        cp #ff
        jr z,menu
        call any_key
        jr nz,menu
        ld a,(DIR_AUTOBOOT)
        jp play

menu:
        ld a,(DIR_COUNT)
        ld (count),a
        xor a
        ld (sel),a
        ld (top),a
        ld d,0
        call clear_row
        ld hl,DIR_TITLE
        ld de,#0001
        ld c,3
        call print_text
        ld d,24
        call clear_row
        ld hl,help
        ld de,#1801
        ld c,1
        call print_text
        ld a,(count)
        or a
        jr nz,menu_list
        ld hl,no_game
        ld de,#0203
        ld c,1
        call print_text
no_game_loop:
        jr no_game_loop
menu_list:
        call draw_list
release:
        call vsync
        call any_key
        jr nz,release
input:
        call vsync
        call read_keys
        or a
        jr z,input
        bit 2,a
        jr nz,play_selected
        ld hl,sel
        bit 0,a
        jr z,go_down
        ld a,(hl)
        or a
        jr z,input
        dec (hl)
        jr moved
go_down:
        ld a,(count)
        dec a
        cp (hl)
        jr z,input
        inc (hl)
moved:
        ld a,(sel)
        ld hl,top
        cp (hl)
        jr nc,moved_below
        ld (hl),a
        jr moved_shown
moved_below:
        sub (hl)
        cp VISIBLE
        jr c,moved_shown
        ld a,(sel)
        sub VISIBLE - 1
        ld (top),a
moved_shown:
        call draw_list
        ld b,8                  ; a key held repeats after 8 frames
repeat_wait:
        call vsync
        djnz repeat_wait
        jr input

play_selected:
        ld a,(sel)
play:
        ld (idx),a
        call entry_address
        ld bc,ENTRY_TYPE
        add hl,bc
        ld a,(hl)
        inc hl
        or a
        ld a,(idx)
        jp z,launch
        ; A ROM: a few bytes in RAM put its slot at &0000 and start it.
        ld a,(hl)
        ld hl,rom_start
        ld de,#4000
        ld bc,rom_start_end - rom_start
        ldir
        ld (#4001),a
        jp #4000
rom_start:
        ld b,0                  ; the ROM's slot, put here
        ld iy,#4100
        command b
        ld a,#a2                ; zone 1 hidden, no more commands
        command a
        rst 0
rom_start_end:

; ---- The list ----

draw_list:
        ld d,2
draw_line:
        push de
        call clear_row
        pop de
        ld a,(top)
        add a,d
        sub 2
        ld (idx),a
        ld hl,count
        cp (hl)
        jr nc,draw_next
        call entry_address
        ld c,1
        ld a,(idx)
        ld b,a
        ld a,(sel)
        cp b
        jr nz,draw_name
        ld c,2
        ld e,1
        ld a,'>'
        call put_char
draw_name:
        ld e,3
        call print_text
draw_next:
        inc d
        ld a,d
        cp 2 + VISIBLE
        jr c,draw_line
        ret

; HL = the directory's entry for game A.
entry_address:
        ld l,a
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        ld bc,DIR_ENTRIES
        add hl,bc
        ret

; ---- Text, in mode 1 ----

; The text at HL (ended by 0, 32 characters at most) at row D, column E,
; in pen C. D, C kept.
print_text:
        ld b,32
print_char:
        ld a,(hl)
        or a
        ret z
        ld a,e
        cp 40
        ret nc
        ld a,(hl)
        call put_char
        inc hl
        inc e
        djnz print_char
        ret

; Character A at row D, column E, in pen C. All registers kept but A.
put_char:
        push bc
        push de
        push hl
        push af
        call screen_address
        pop af
        sub 32
        cp 96
        jr c,put_known
        xor a
put_known:
        push hl
        ld l,a
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl
        ld de,FONT
        add hl,de
        ex de,hl
        pop hl
        ld b,8
put_row:
        ld a,(de)
        rrca
        rrca
        rrca
        rrca
        and #0f
        call pen_byte
        ld (hl),a
        inc hl
        ld a,(de)
        and #0f
        call pen_byte
        ld (hl),a
        dec hl
        inc de
        ld a,h
        add a,8
        ld h,a
        djnz put_row
        pop hl
        pop de
        pop bc
        ret

; Four pixels (the low bits of A, leftmost in bit 3) in pen C, as mode 1
; has them in a byte.
pen_byte:
        push hl
        ld l,a
        xor a
        bit 0,c
        jr z,pen_high
        ld a,l
        rlca
        rlca
        rlca
        rlca
pen_high:
        bit 1,c
        jr z,pen_done
        or l
pen_done:
        pop hl
        ret

; HL = where row D, column E starts on the screen. A changed.
screen_address:
        push de
        ld l,d
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        ld d,h
        ld e,l
        add hl,hl
        add hl,hl
        add hl,de
        pop de
        ld a,e
        add a,a
        add a,l
        ld l,a
        ld a,h
        adc a,#c0
        ld h,a
        ret

; Row D blanked. D lost.
clear_row:
        ld e,0
        call screen_address
        ld b,8
clear_line:
        push bc
        push hl
        ld (hl),0
        ld d,h
        ld e,l
        inc de
        ld bc,79
        ldir
        pop hl
        ld a,h
        add a,8
        ld h,a
        pop bc
        djnz clear_line
        ret

; ---- The keyboard and the joystick ----

; A: bit 0 up, bit 1 down, bit 2 fire (Space, Return, Enter, either fire
; button).
read_keys:
        ld e,0
        xor a
        call key_line
        cpl
        ld d,a
        and %00000001
        jr z,keys_1
        set 0,e
keys_1:
        ld a,d
        and %00000100
        jr z,keys_2
        set 1,e
keys_2:
        ld a,d
        and %01000000
        jr z,keys_3
        set 2,e
keys_3:
        ld a,2
        call key_line
        cpl
        and %00000100
        jr z,keys_4
        set 2,e
keys_4:
        ld a,5
        call key_line
        cpl
        and %10000000
        jr z,keys_5
        set 2,e
keys_5:
        ld a,9
        call key_line
        cpl
        ld d,a
        and %00000001
        jr z,keys_6
        set 0,e
keys_6:
        ld a,d
        and %00000010
        jr z,keys_7
        set 1,e
keys_7:
        ld a,d
        and %00110000
        jr z,keys_8
        set 2,e
keys_8:
        ld a,e
        ret

; Z if no key is held.
any_key:
        push de
        ld d,#ff
        ld e,0
any_line:
        ld a,e
        call key_line
        and d
        ld d,a
        inc e
        ld a,e
        cp 10
        jr nz,any_line
        ld a,d
        cp #ff
        pop de
        ret

; The keys of line A, a bit clear for each one held.
key_line:
        push bc
        ld bc,#f40e
        out (c),c
        ld bc,#f6c0
        out (c),c
        ld bc,#f600
        out (c),c
        ld bc,#f792
        out (c),c
        or #40
        ld b,#f6
        out (c),a
        ld b,#f4
        in a,(c)
        ld bc,#f782
        out (c),c
        ld bc,#f600
        out (c),c
        pop bc
        ret

; Register D of the sound chip set to E.
psg_write:
        push bc
        ld b,#f4
        out (c),d
        ld bc,#f6c0
        out (c),c
        ld bc,#f600
        out (c),c
        ld b,#f4
        out (c),e
        ld bc,#f680
        out (c),c
        ld bc,#f600
        out (c),c
        pop bc
        ret

; The start of the next frame's vertical sync. B kept.
vsync:
        push bc
        ld b,#f5
vsync_end:
        in a,(c)
        rra
        jr c,vsync_end
vsync_start:
        in a,(c)
        rra
        jr nc,vsync_start
        pop bc
        ret

crtc_values:
        db 63,40,46,#8e,38,0,25,30,0,7,0,0,#30,0,0,0
palette:
        db #54,#4b,#4a,#53      ; black, bright white, bright yellow, bright cyan
help:
        db "Cursors/joystick, Space or Fire",0
no_game:
        db "No game on this cartridge.",0

; ---- The loader, first part: quarters 1 to 3 ----

; Game A: its header is the A-th of slot 1.
launch:
        di
        ld h,a
        ld l,0
        add hl,hl
        ld de,HEADERS + HEADER_TABLE
        add hl,de
        push hl
        pop ix
        ld iy,SCRATCH
        ld bc,#7f8d             ; the lower ROM off: quarter 0 reads RAM
        out (c),c
        exx
        ld c,1
        command c               ; zone 1 on slot 1...
        exx
        ld a,#88
        command a               ; ...at &C000
        blocks 0, #fe, low_done

; The table's entry &FE (its second byte): where the commands may write
; from now on (bytes 2-3, just below the game's stack) and the game's RAM
; configuration (byte 4). Zone 0 goes to
; &8000; quarter 0 shows RAM, where a jump to the loader's other copy was
; put.
low_done:
        ld a,(ix+4)
        ld b,#7f
        out (c),a
        ld e,(ix+2)
        ld d,(ix+3)
        ld iy,0
        add iy,de
        ld bc,5
        add ix,bc
        ld hl,high_start
        ld (to_high + 1),hl
        ld a,#c3
        ld (to_high),a
        ld a,#8c
        command a
to_high:
        nop
        nop
        nop

; ---- The loader, second part: quarter 0, then the chips ----

        org #a000
high_start:
        blocks 4, #ff, chips

; The entry &FF (its second byte): the address of the game's last
; instructions (bytes 2-3), and of its header (bytes 4-5).
chips:
        ld e,(ix+2)
        ld d,(ix+3)
        exx
        ld l,(ix+4)
        ld h,(ix+5)
        ld e,l
        ld d,h
        ld ix,0
        add ix,de
        ; The sound chip's registers, through the PPI.
        ld bc,#f782
        out (c),c
        hlbase #5b
        ld e,0
chips_psg:
        ld b,#f4
        out (c),e
        ld bc,#f6c0
        out (c),c
        ld bc,#f600
        out (c),c
        ld a,(hl)
        inc hl
        ld b,#f4
        out (c),a
        ld bc,#f680
        out (c),c
        ld bc,#f600
        out (c),c
        inc e
        ld a,e
        cp 16
        jr nz,chips_psg
        ld a,(ix+#5a)
        ld b,#f4
        out (c),a
        ld bc,#f6c0
        out (c),c
        ld bc,#f600
        out (c),c
        ; The PPI.
        ld a,(ix+#59)
        ld b,#f7
        out (c),a
        ld a,(ix+#56)
        ld b,#f4
        out (c),a
        ld a,(ix+#58)
        ld b,#f6
        out (c),a
        ; The Gate Array: pens, mode and ROMs, RAM.
        hlbase #2f
        ld e,0
chips_pens:
        ld b,#7f
        out (c),e
        ld a,(hl)
        and #1f
        or #40
        out (c),a
        inc hl
        inc e
        ld a,e
        cp 17
        jr nz,chips_pens
        ld a,(ix+#2e)
        and #1f
        out (c),a
        ld a,(ix+#40)
        and #0f
        or #80
        out (c),a
        ld a,(ix+#41)
        and #3f
        or #c0
        out (c),a
        ld a,(ix+#55)
        ld bc,#df00
        out (c),a
        ; The CRTC.
        hlbase #43
        ld e,0
chips_crtc:
        ld b,#bc
        out (c),e
        ld a,(hl)
        inc b
        out (c),a
        inc hl
        inc e
        ld a,e
        cp 18
        jr nz,chips_crtc
        ld a,(ix+#42)
        ld b,#bc
        out (c),a
        ; The rest is the game's own: its registers, and the cartridge
        ; gone as it is entered.
        exx
        ex de,hl
        jp (hl)

LOADER_LOW_END  equ rom_start
LAUNCH_ADDRESS  equ launch
HIGH_ADDRESS    equ high_start

        org FONT
        read "font.asm"

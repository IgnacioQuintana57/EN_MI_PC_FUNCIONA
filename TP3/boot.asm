[bits 16]
org 0x7C00          ; La BIOS carga el bootloader en esta dirección

start:
    cli             ; 1. Deshabilitar interrupciones

    lgdt [gdt_descriptor] ; 2. Cargar la GDT

    mov eax, cr0
    or eax, 0x1     ; 3. Modificar el bit 0 (PE - Protection Enable) del registro CR0
    mov cr0, eax    ; Modo Protegido!!!

    ; 4. Far Jump (Salto Lejano) para limpiar el pipeline y cargar el registro CS
    jmp 0x08:modo_protegido 

[bits 32]
modo_protegido:
    ; 5. Cargar los registros de datos con nuestro nuevo Segmento de Datos (Selector 0x10)
    mov ax, 0x10    
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; --- PRUEBA DEL PUNTO 3 (Descomentar para probar) ---
    mov byte [0x00000000], 'A' ; Intentamos escribir en el segmento de datos
    
    jmp $           ; Loop infinito (hang)

; =====================================
; TABLA DE DESCRIPTORES GLOBALES (GDT) (ahora corregimos para que no se solape)
; =====================================
gdt_start:

gdt_null:           ; Descriptor nulo
    dq 0x0

gdt_code:           ; Descriptor de Código (Offset 0x08)
    dw 0xFFFF       ; Límite (0xFFFF = 64 KB de tamaño)
    dw 0x0000       ; Base (bits 0-15) -> 0x0000
    db 0x00         ; Base (bits 16-23) -> 0x00
    db 10011010b    ; Byte de Acceso (Código, Ejecutable/Lectura)
    db 01000000b    ; Banderas: Granularidad en 0 (Bytes) y bit D/B en 1 (32-bit)
    db 0x00         ; Base (bits 24-31) -> Base Total = 0x00000000

gdt_data:           ; Descriptor de Datos (Offset 0x10) -
    dw 0xFFFF       ; Límite (0xFFFF = 64 KB de tamaño)
    dw 0x0000       ; Base (bits 0-15) -> 0x0000
    db 0x01         ; Base (bits 16-23) -> 0x01
    db 10010010b    ; Byte de Acceso (Datos, Lectura/Escritura)
    db 01000000b    ; Banderas: Granularidad en 0 (Bytes) y bit D/B en 1 (32-bit)
    db 0x00         ; Base (bits 24-31) -> Base Total = 0x00010000

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; Tamaño de la GDT
    dd gdt_start               ; Dirección de memoria donde empieza la GDT

; Firma del Bootloader
times 510-($-$$) db 0
dw 0xAA55

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

; ==========================================
; TABLA DE DESCRIPTORES GLOBALES (GDT) RAW
; ==========================================
gdt_start:

gdt_null:           ; Descriptor nulo (Obligatorio, offset 0x00)
    dq 0x0

gdt_code:           ; Descriptor de Código (Offset 0x08) - Base: 0x00000000
    dw 0xFFFF       ; Límite (bits 0-15)
    dw 0x0000       ; Base (bits 0-15)
    db 0x00         ; Base (bits 16-23)
    db 10011010b    ; Byte de Acceso (Presente, Ring 0, Código, Ejecutable/Lectura)
    db 11001111b    ; Banderas (4KB granularidad, 32-bit) + Límite (bits 16-19)
    db 0x00         ; Base (bits 24-31)

gdt_data:           ; Descriptor de Datos (Offset 0x10) - Base: 0x00010000 (Diferenciado)
    dw 0xFFFF       ; Límite
    dw 0x0000       ; Base (bits 0-15)
    db 0x01         ; Base (bits 16-23) -> ¡Aquí le damos una base diferente! (0x010000)
    ;db 10010010b    ; Byte de Acceso (Presente, Ring 0, Datos, Lectura/Escritura)
    db 10010000b    ; Byte de Acceso (Cambiado a SOLO LECTURA)
    db 11001111b    ; Banderas + Límite
    db 0x00         ; Base

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; Tamaño de la GDT
    dd gdt_start               ; Dirección de memoria donde empieza la GDT

; Firma del Bootloader
times 510-($-$$) db 0
dw 0xAA55

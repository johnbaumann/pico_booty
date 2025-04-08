.cpu cortex-m0plus
.thumb

.data

.global payload_start_addr
.global payload_end_addr

payload_start_addr:
.incbin "hello.booty"
;.incbin "boot no_uartpassthru.bin"

payload_end_addr:


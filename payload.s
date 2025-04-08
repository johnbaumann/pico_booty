.cpu cortex-m0plus
.thumb

.data

.global payload_start_addr
.global payload_end_addr

payload_start_addr:
.incbin "unirom.booty"

payload_end_addr:


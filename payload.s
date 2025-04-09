.cpu cortex-m0plus
.thumb

.data

.global c_payloadStart
.global c_payloadEnd

c_payloadStart:
.incbin "unirom.booty"

c_payloadEnd:


# [DCN-2026-019] MC ACK response from receiver to sender.
#
# Issued by mc_protocol_node for every MCMessage received, regardless
# of acceptance. outcome enumerates the disposition; only OK results
# in /mc/command_validated republish.

builtin_interfaces/Time stamp
uint32 seq                  # echoes MCMessage.seq
string outcome              # "OK" | "INVALID_CHECKSUM" | "DUPLICATE" | "OUT_OF_ORDER"

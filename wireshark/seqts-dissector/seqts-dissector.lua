-- trivial protocol example
-- declare our protocol
seqts_proto = Proto("seqts","Sequence-Timestamp Protocol")
seq_field = ProtoField.uint32("seqts.seq", "Seq. Number", base.DEC)
ts_field = ProtoField.double("seqts.ts", "Timestamp")
data_field = ProtoField.bytes("seqts.data", "Data")
seqts_proto.fields = {seq_field, ts_field, data_field}

-- create a function to dissect it
function seqts_proto.dissector(buffer,pinfo,tree)
    pinfo.cols.protocol = "SEQ-TS"
    local len = buffer:len()
    local subtree = tree:add(seqts_proto,buffer(0,len),"Sequence-Timestamp Protocol Data")
    subtree:add(seq_field, buffer(0,4))
    local high = buffer(4,4):uint()
    local low = buffer(8,4):uint()
    local sec = (high * 0xFFFFFFFF + low) / 1000000000
    subtree:add(ts_field, buffer(4,8), sec):set_text(string.format("Timestamp: %.3f s", sec))
    subtree:add(data_field, buffer(12,len-12))
end
-- load the udp.port table
udp_table = DissectorTable.get("udp.port")
-- register our protocol to handle udp port 7777
udp_table:add(9,seqts_proto)

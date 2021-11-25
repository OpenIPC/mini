#include "nal.h"

char *nal_type_to_str(const enum NalUnitType nal_type) {
    switch (nal_type) {
    case NalUnitType_Unspecified:
        return "Unspecified";
    case NalUnitType_CodedSliceNonIdr:
        return "CodedSliceNonIdr";
    case NalUnitType_CodedSliceDataPartitionA:
        return "CodedSliceDataPartitionA";
    case NalUnitType_CodedSliceDataPartitionB:
        return "CodedSliceDataPartitionB";
    case NalUnitType_CodedSliceDataPartitionC:
        return "CodedSliceDataPartitionC";
    case NalUnitType_CodedSliceIdr:
        return "CodedSliceIdr";
    case NalUnitType_SEI:
        return "SEI";
    case NalUnitType_SPS:
        return "SPS";
    case NalUnitType_PPS:
        return "PPS";
    case NalUnitType_AUD:
        return "AUD";
    case NalUnitType_EndOfSequence:
        return "EndOfSequence";
    case NalUnitType_EndOfStream:
        return "EndOfStream";
    case NalUnitType_Filler:
        return "Filler";
    case NalUnitType_SpsExt:
        return "SpsExt";
    case NalUnitType_CodedSliceAux:
        return "CodedSliceAux";
    default:
        return "Unknown";
    }
}

void nal_parse_header(struct NAL *nal, const char first_byte) {
    nal->forbidden_zero_bit = ((first_byte & 0b10000000) >> 7) == 1;
    nal->ref_idc = (first_byte & 0b01100000) >> 5;
    nal->unit_type = (first_byte & 0b00011111) >> 0;
}

bool nal_chk4(const char *buf, const uint32_t offset) {
    if (buf[offset] == 0x00 && buf[offset + 1] == 0x00 &&
        buf[offset + 2] == 0x01) {
        return true;
    }
    if (buf[offset] == 0x00 && buf[offset + 1] == 0x00 &&
        buf[offset + 2] == 0x00 && buf[offset + 3] == 0x01) {
        return true;
    }
    return false;
}

bool nal_chk3(const char *buf, const uint32_t offset) {
    if (buf[offset] == 0x00 && buf[offset + 1] == 0x00 &&
        buf[offset + 2] == 0x01) {
        return true;
    }
    return false;
}

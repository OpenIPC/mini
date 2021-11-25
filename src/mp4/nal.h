#pragma once
#include <stdbool.h>
#include <stdint.h>

enum NalUnitType {                    //   Table 7-1 NAL unit type codes
    NalUnitType_Unspecified = 0,      // Unspecified
    NalUnitType_CodedSliceNonIdr = 1, // Coded slice of a non-IDR picture
    NalUnitType_CodedSliceDataPartitionA = 2, // Coded slice data partition A
    NalUnitType_CodedSliceDataPartitionB = 3, // Coded slice data partition B
    NalUnitType_CodedSliceDataPartitionC = 4, // Coded slice data partition C
    NalUnitType_CodedSliceIdr = 5,            // Coded slice of an IDR picture
    NalUnitType_SEI = 6, // Supplemental enhancement information (SEI)
    NalUnitType_SPS = 7, // Sequence parameter set
    NalUnitType_PPS = 8, // Picture parameter set
    NalUnitType_AUD = 9, // Access unit delimiter
    NalUnitType_EndOfSequence = 10, // End of sequence
    NalUnitType_EndOfStream = 11,   // End of stream
    NalUnitType_Filler = 12,        // Filler data
    NalUnitType_SpsExt = 13,        // Sequence parameter set extension
                                    // 14..18           // Reserved
    NalUnitType_CodedSliceAux =
        19, // Coded slice of an auxiliary coded picture without partitioning
    // 20..23           // Reserved
    // 24..31           // Unspecified
};

char *nal_type_to_str(const enum NalUnitType nal_type);

struct NAL {
    char *data;
    uint64_t data_size;
    uint32_t picture_order_count;

    // NAL header
    bool forbidden_zero_bit;
    uint8_t ref_idc;
    uint8_t unit_type_value;
    enum NalUnitType unit_type;
};

void nal_parse_header(struct NAL *nal, const char first_byte);
bool nal_chk4(const char *buf, const uint32_t offset);
bool nal_chk3(const char *buf, const uint32_t offset);

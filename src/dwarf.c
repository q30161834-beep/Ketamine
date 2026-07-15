#include "include/dwarf.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// DWARF DEBUG INFO GENERATOR
// ═══════════════════════════════════════════════════════════════════════════════

#define DWARF_INIT_CAP (64 * 1024)

// ─── DWARF constants ─────────────────────────────────────────────────────────
#define DW_TAG_compile_unit     0x11
#define DW_TAG_subprogram       0x2e
#define DW_TAG_variable         0x34
#define DW_TAG_formal_parameter 0x05
#define DW_TAG_base_type        0x24
#define DW_TAG_pointer_type     0x0f
#define DW_TAG_array_type       0x01
#define DW_TAG_structure_type   0x13
#define DW_TAG_enumeration_type 0x16

#define DW_AT_sibling           0x01
#define DW_AT_location          0x02
#define DW_AT_name              0x03
#define DW_AT_byte_size         0x0b
#define DW_AT_bit_size          0x07
#define DW_AT_stmt_list         0x10
#define DW_AT_low_pc            0x11
#define DW_AT_high_pc           0x12
#define DW_AT_language          0x13
#define DW_AT_comp_dir          0x1b
#define DW_AT_producer          0x25
#define DW_AT_prototyped        0x27
#define DW_AT_type              0x49
#define DW_AT_decl_line         0x3a
#define DW_AT_decl_file         0x3c
#define DW_AT_external          0x3f
#define DW_AT_encoding          0x3e
#define DW_AT_artificial        0x34
#define DW_AT_specification     0x47
#define DW_AT_inline            0x48
#define DW_AT_containing_type   0x3d
#define DW_AT_call_line         0x59
#define DW_AT_call_column       0x5a
#define DW_AT_call_file         0x58

#define DW_FORM_addr            0x01
#define DW_FORM_block2          0x03
#define DW_FORM_block4          0x04
#define DW_FORM_data2           0x05
#define DW_FORM_data4           0x06
#define DW_FORM_data8           0x07
#define DW_FORM_string          0x08
#define DW_FORM_block           0x09
#define DW_FORM_block1          0x0a
#define DW_FORM_data1           0x0b
#define DW_FORM_flag            0x0c
#define DW_FORM_sdata           0x0d
#define DW_FORM_strp            0x0e
#define DW_FORM_udata           0x0f
#define DW_FORM_ref_addr        0x10
#define DW_FORM_ref1            0x11
#define DW_FORM_ref2            0x12
#define DW_FORM_ref4            0x13
#define DW_FORM_ref8            0x14
#define DW_FORM_ref_udata       0x15
#define DW_FORM_indirect        0x16
#define DW_FORM_sec_offset      0x17
#define DW_FORM_exprloc         0x18
#define DW_FORM_flag_present    0x19
#define DW_FORM_strx            0x1a
#define DW_FORM_addrx           0x1b
#define DW_FORM_ref_sup4        0x1c
#define DW_FORM_ref_sup8        0x1d
#define DW_FORM_strx1           0x1e
#define DW_FORM_strx2           0x1f
#define DW_FORM_strx3           0x20
#define DW_FORM_strx4           0x21
#define DW_FORM_addrx1          0x22
#define DW_FORM_addrx2          0x23
#define DW_FORM_addrx3          0x24
#define DW_FORM_addrx4          0x25
#define DW_FORM_implicit_const  0x21

#define DW_LANG_C11             0x1d
#define DW_LNE_end_sequence     0x01
#define DW_LNE_set_address      0x02
#define DW_LNE_define_file      0x03
#define DW_LNS_copy             0x01
#define DW_LNS_advance_pc       0x02
#define DW_LNS_advance_line     0x03
#define DW_LNS_set_file         0x04
#define DW_LNS_set_column       0x05
#define DW_LNS_negate_stmt      0x06
#define DW_LNS_const_add_pc     0x08
#define DW_LNS_fixed_advance_pc 0x09
#define DW_LNS_set_prologue_end 0x0a
#define DW_LNS_set_epilogue_begin 0x0b
#define DW_LNS_set_isa          0x0c

#define DW_ATE_boolean          0x02
#define DW_ATE_signed           0x05
#define DW_ATE_unsigned         0x07
#define DW_ATE_signed_char      0x06
#define DW_ATE_unsigned_char    0x08
#define DW_ATE_float            0x01

// ─── Section buffer helpers ──────────────────────────────────────────────────
static void sect_write8(DwarfGen *dg, DwarfSection sect, uint8_t v) {
    int idx = dg->section_sizes[sect];
    if (idx + 1 > dg->section_caps[sect]) {
        dg->section_caps[sect] = dg->section_caps[sect] ? dg->section_caps[sect] * 2 : DWARF_INIT_CAP;
        dg->sections[sect] = (uint8_t*)realloc(dg->sections[sect], (size_t)dg->section_caps[sect]);
    }
    dg->sections[sect][idx] = v;
    dg->section_sizes[sect] = idx + 1;
}

static void sect_write16(DwarfGen *dg, DwarfSection sect, uint16_t v) {
    sect_write8(dg, sect, v & 0xff);
    sect_write8(dg, sect, (v >> 8) & 0xff);
}

static void sect_write32(DwarfGen *dg, DwarfSection sect, uint32_t v) {
    sect_write8(dg, sect, v & 0xff);
    sect_write8(dg, sect, (v >> 8) & 0xff);
    sect_write8(dg, sect, (v >> 16) & 0xff);
    sect_write8(dg, sect, (v >> 24) & 0xff);
}

static void sect_write64(DwarfGen *dg, DwarfSection sect, uint64_t v) {
    for (int i = 0; i < 8; i++) {
        sect_write8(dg, sect, v & 0xff);
        v >>= 8;
    }
}

static void sect_write_uleb128(DwarfGen *dg, DwarfSection sect, uint64_t v) {
    do {
        uint8_t byte = v & 0x7f;
        v >>= 7;
        if (v) byte |= 0x80;
        sect_write8(dg, sect, byte);
    } while (v);
}

static void sect_write_string(DwarfGen *dg, DwarfSection sect, const char *s) {
    while (*s) sect_write8(dg, sect, (uint8_t)*s++);
    sect_write8(dg, sect, 0);
}

static int dwarf_add_string(DwarfGen *dg, const char *s) {
    int offset = dg->strtab_offset;
    // Write to .debug_str section
    sect_write_string(dg, DWARF_SECT_STR, s);
    dg->strtab_offset = dg->section_sizes[DWARF_SECT_STR];
    return offset;
}

// ─── Generator lifecycle ──────────────────────────────────────────────────────
DwarfGen *dwarf_gen_new(const char *output_path, bool optimized) {
    DwarfGen *dg = (DwarfGen*)calloc(1, sizeof(DwarfGen));
    dg->output_path = output_path;
    dg->cu.version = 4;
    dg->cu.address_size = 8;
    dg->cu.producer = "Ketamine v1.0";
    dg->cu.is_optimized = optimized;
    dg->include_debug = true;
    return dg;
}

void dwarf_set_cu(DwarfGen *dg, const char *file, const char *dir) {
    dg->cu.name = file;
    dg->cu.comp_dir = dir;
}

void dwarf_add_line(DwarfGen *dg, const char *file, int line, int col,
                    int address, bool is_stmt) {
    if (dg->line_count >= dg->line_cap) {
        dg->line_cap = dg->line_cap ? dg->line_cap * 2 : 1024;
        dg->lines = (DwarfLineEntry*)realloc(dg->lines, (size_t)dg->line_cap * sizeof(DwarfLineEntry));
    }
    DwarfLineEntry *e = &dg->lines[dg->line_count++];
    e->file_name = file;
    e->file_index = 1;
    e->line = line;
    e->column = col;
    e->address = address;
    e->is_stmt = is_stmt;
    e->prologue_end = false;
    e->epilogue_begin = false;
}

void dwarf_add_function(DwarfGen *dg, const char *name, int low_pc, int high_pc,
                        int line, const char *file) {
    (void)file;
    // Simplified: we'd emit a DW_TAG_subprogram DIE
    // For now, just note it
    dwarf_add_line(dg, "?", line, 0, low_pc, true);
}

void dwarf_add_variable(DwarfGen *dg, const char *name, struct Type *type,
                        int line, int offset, bool is_param) {
    (void)type;
    (void)offset;
    (void)is_param;
    dwarf_add_line(dg, "?", line, 0, 0, false);
}

// ─── Section generation ──────────────────────────────────────────────────────
uint8_t *dwarf_gen_line_section(DwarfGen *dg, int *size) {
    int start = dg->section_sizes[DWARF_SECT_LINE];
    if (!start) return NULL;

    // .debug_line header
    // We'd encode the line number program here
    // For now, emit a minimal header
    sect_write32(dg, DWARF_SECT_LINE, 0); // will fill later
    sect_write16(dg, DWARF_SECT_LINE, 4); // version
    sect_write32(dg, DWARF_SECT_LINE, 0); // header length
    sect_write8(dg, DWARF_SECT_LINE, 1);  // min instruction length
    sect_write8(dg, DWARF_SECT_LINE, 1);  // default is stmt
    sect_write8(dg, DWARF_SECT_LINE, 0);  // line base
    sect_write8(dg, DWARF_SECT_LINE, 1);  // line range
    sect_write8(dg, DWARF_SECT_LINE, 0);  // opcode base
    sect_write8(dg, DWARF_SECT_LINE, 0);  // standard opcode lengths

    // Include directories
    sect_write8(dg, DWARF_SECT_LINE, 0);

    // File names
    sect_write8(dg, DWARF_SECT_LINE, 0);

    // Line number program
    for (int i = 0; i < dg->line_count; i++) {
        DwarfLineEntry *e = &dg->lines[i];
        // Set address
        sect_write8(dg, DWARF_SECT_LINE, 0); // extended op
        sect_write_uleb128(dg, DWARF_SECT_LINE, 9); // length
        sect_write8(dg, DWARF_SECT_LINE, DW_LNE_set_address);
        sect_write64(dg, DWARF_SECT_LINE, (uint64_t)e->address);

        // Set line
        sect_write8(dg, DWARF_SECT_LINE, DW_LNS_advance_line);
        sect_write_uleb128(dg, DWARF_SECT_LINE, (uint64_t)(e->line > 0 ? e->line : 1));

        // Copy
        sect_write8(dg, DWARF_SECT_LINE, DW_LNS_copy);
    }

    // End sequence
    sect_write8(dg, DWARF_SECT_LINE, 0);
    sect_write_uleb128(dg, DWARF_SECT_LINE, 0);
    sect_write8(dg, DWARF_SECT_LINE, DW_LNE_end_sequence);

    *size = dg->section_sizes[DWARF_SECT_LINE] - start;
    return dg->sections[DWARF_SECT_LINE] + start;
}

uint8_t *dwarf_gen_info_section(DwarfGen *dg, int *size) {
    int start = dg->section_sizes[DWARF_SECT_INFO];

    // Compilation unit header
    sect_write32(dg, DWARF_SECT_INFO, 0); // unit_length (placeholder)
    sect_write16(dg, DWARF_SECT_INFO, 4); // version
    sect_write32(dg, DWARF_SECT_INFO, 0); // debug_abbrev_offset
    sect_write8(dg, DWARF_SECT_INFO, 8);  // address_size

    // DW_TAG_compile_unit DIE
    sect_write_uleb128(dg, DWARF_SECT_INFO, 1); // abbrev code
    int name_off = dwarf_add_string(dg, dg->cu.name ? dg->cu.name : "unknown.kt");
    int dir_off = dwarf_add_string(dg, dg->cu.comp_dir ? dg->cu.comp_dir : ".");
    int prod_off = dwarf_add_string(dg, dg->cu.producer);

    // DW_AT_producer
    sect_write_uleb128(dg, DWARF_SECT_INFO, DW_AT_producer);
    sect_write_uleb128(dg, DWARF_SECT_INFO, DW_FORM_strp);
    sect_write32(dg, DWARF_SECT_INFO, (uint32_t)prod_off);

    // DW_AT_name
    sect_write_uleb128(dg, DWARF_SECT_INFO, DW_AT_name);
    sect_write_uleb128(dg, DWARF_SECT_INFO, DW_FORM_strp);
    sect_write32(dg, DWARF_SECT_INFO, (uint32_t)name_off);

    // DW_AT_comp_dir
    sect_write_uleb128(dg, DWARF_SECT_INFO, DW_AT_comp_dir);
    sect_write_uleb128(dg, DWARF_SECT_INFO, DW_FORM_strp);
    sect_write32(dg, DWARF_SECT_INFO, (uint32_t)dir_off);

    // DW_AT_language
    sect_write_uleb128(dg, DWARF_SECT_INFO, DW_AT_language);
    sect_write_uleb128(dg, DWARF_SECT_INFO, DW_FORM_data2);
    sect_write16(dg, DWARF_SECT_INFO, DW_LANG_C11);

    // Null terminator for DIE
    sect_write8(dg, DWARF_SECT_INFO, 0);

    *size = dg->section_sizes[DWARF_SECT_INFO] - start;
    return dg->sections[DWARF_SECT_INFO] + start;
}

uint8_t *dwarf_gen_abbrev_section(DwarfGen *dg, int *size) {
    int start = dg->section_sizes[DWARF_SECT_ABBREV];

    // Abbreviation 1: DW_TAG_compile_unit
    sect_write_uleb128(dg, DWARF_SECT_ABBREV, 1);
    sect_write_uleb128(dg, DWARF_SECT_ABBREV, DW_TAG_compile_unit);
    sect_write8(dg, DWARF_SECT_ABBREV, 1); // children: yes
    // Attributes
    sect_write_uleb128(dg, DWARF_SECT_ABBREV, DW_AT_producer);
    sect_write_uleb128(dg, DWARF_SECT_ABBREV, DW_FORM_strp);
    sect_write_uleb128(dg, DWARF_SECT_ABBREV, DW_AT_name);
    sect_write_uleb128(dg, DWARF_SECT_ABBREV, DW_FORM_strp);
    sect_write_uleb128(dg, DWARF_SECT_ABBREV, DW_AT_comp_dir);
    sect_write_uleb128(dg, DWARF_SECT_ABBREV, DW_FORM_strp);
    sect_write_uleb128(dg, DWARF_SECT_ABBREV, DW_AT_language);
    sect_write_uleb128(dg, DWARF_SECT_ABBREV, DW_FORM_data2);
    // End
    sect_write8(dg, DWARF_SECT_ABBREV, 0);
    sect_write8(dg, DWARF_SECT_ABBREV, 0);

    // Null terminator
    sect_write8(dg, DWARF_SECT_ABBREV, 0);

    *size = dg->section_sizes[DWARF_SECT_ABBREV] - start;
    return dg->sections[DWARF_SECT_ABBREV] + start;
}

uint8_t *dwarf_gen_str_section(DwarfGen *dg, int *size) {
    *size = dg->section_sizes[DWARF_SECT_STR];
    return dg->sections[DWARF_SECT_STR];
}

int dwarf_finalize(DwarfGen *dg) {
    if (!dg->include_debug) return 0;

    int line_size, info_size, abbrev_size, str_size;
    dwarf_gen_line_section(dg, &line_size);
    dwarf_gen_info_section(dg, &info_size);
    dwarf_gen_abbrev_section(dg, &abbrev_size);
    dwarf_gen_str_section(dg, &str_size);

    return 0;
}

void dwarf_free(DwarfGen *dg) {
    if (!dg) return;
    for (int i = 0; i < DWARF_SECT_COUNT; i++) {
        free(dg->sections[i]);
    }
    free(dg->lines);
    free(dg);
}

int dwarf_write_to_object(DwarfGen *dg, const char *obj_path) {
    (void)obj_path;
    // Would write ELF/COFF object file with DWARF sections
    // For now, just finalize
    return dwarf_finalize(dg);
}

int dwarf_write_to_file(DwarfGen *dg, const char *path) {
    (void)path;
    return dwarf_finalize(dg);
}

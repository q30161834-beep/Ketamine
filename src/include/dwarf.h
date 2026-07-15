#ifndef KETAMINE_DWARF_H
#define KETAMINE_DWARF_H

#include "types.h"

// ─── DWARF Debug Info Generator ────────────────────────────────────────────
// Generates DWARF debugging information for Ketamine compiled programs.
// Supports: .debug_info, .debug_line, .debug_abbrev, .debug_str sections.
// Integrates with GDB and LLDB for source-level debugging.

// ─── DWARF section types ───────────────────────────────────────────────────
typedef enum {
    DWARF_SECT_INFO,       // .debug_info
    DWARF_SECT_ABBREV,     // .debug_abbrev
    DWARF_SECT_LINE,       // .debug_line
    DWARF_SECT_STR,        // .debug_str
    DWARF_SECT_ARANGES,    // .debug_aranges
    DWARF_SECT_COUNT
} DwarfSection;

// ─── DWARF Compilation Unit ────────────────────────────────────────────────
typedef struct {
    int         unit_length;
    int         version;        // 4 or 5
    int         debug_abbrev_offset;
    int         address_size;   // 8 for 64-bit
    const char *producer;       // "Ketamine v1.0"
    const char *name;           // source file name
    const char *comp_dir;       // working directory
    bool        is_optimized;
} DwarfCU;

// ─── DWARF Line Number Program ─────────────────────────────────────────────
typedef struct {
    const char *file_name;
    int         file_index;
    int         line;
    int         column;
    int         address;        // relative to function start
    bool        is_stmt;        // beginning of statement
    bool        prologue_end;
    bool        epilogue_begin;
} DwarfLineEntry;

// ─── DWARF Generator State ─────────────────────────────────────────────────
typedef struct {
    uint8_t    *sections[DWARF_SECT_COUNT];
    int         section_sizes[DWARF_SECT_COUNT];
    int         section_caps[DWARF_SECT_COUNT];
    DwarfCU     cu;
    DwarfLineEntry *lines;
    int         line_count;
    int         line_cap;
    int         strtab_offset;
    int         abbrev_count;
    int         die_count;
    const char *output_path;
    bool        include_debug;
} DwarfGen;

// ─── API ──────────────────────────────────────────────────────────────────

/// Initialize DWARF generator
DwarfGen *dwarf_gen_new(const char *output_path, bool optimized);

/// Set compilation unit info
void dwarf_set_cu(DwarfGen *dg, const char *file, const char *dir);

/// Add a line number entry (maps source line to address)
void dwarf_add_line(DwarfGen *dg, const char *file, int line, int col,
                    int address, bool is_stmt);

/// Add a function DIE (Debugging Information Entry)
void dwarf_add_function(DwarfGen *dg, const char *name, int low_pc, int high_pc,
                        int line, const char *file);

/// Add a variable DIE
void dwarf_add_variable(DwarfGen *dg, const char *name, struct Type *type,
                        int line, int offset, bool is_param);

/// Finalize and write DWARF sections to output file
int dwarf_finalize(DwarfGen *dg);

/// Free DWARF generator
void dwarf_free(DwarfGen *dg);

/// Generate .debug_line section content (raw bytes)
uint8_t *dwarf_gen_line_section(DwarfGen *dg, int *size);

/// Generate .debug_info section content
uint8_t *dwarf_gen_info_section(DwarfGen *dg, int *size);

/// Generate .debug_abbrev section content
uint8_t *dwarf_gen_abbrev_section(DwarfGen *dg, int *size);

/// Generate .debug_str section content
uint8_t *dwarf_gen_str_section(DwarfGen *dg, int *size);

/// Write all DWARF sections to an object file (ELF or COFF)
int dwarf_write_to_object(DwarfGen *dg, const char *obj_path);

/// Write a standalone DWARF file (for debugging without object file)
int dwarf_write_to_file(DwarfGen *dg, const char *path);

#endif

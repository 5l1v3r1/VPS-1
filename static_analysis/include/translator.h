#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <set>
#include <map>
#include <mutex>
#include <thread>
#include <string>
#include <cstddef>

#include "vex.h"
#include "function.h"
#include "dump_file.h"
#include "mapped_elf.h"
#include "mapped_pe.h"
#include "memory.h"

extern "C" {
#include <valgrind/libvex.h>
}


/*!
 * \brief Class responsible for translating functions into VEX blocks.
 *
 * This class takes CFG descriptions as generated by the IDAPython exporter
 * script and information about non-returning functions and uses both to
 * generate `Function` instances. Basic blocks are mapped to VEX basic
 * blocks of type IRSB.
 */
class Translator {
private:
    /* Only one VEX instance should be present (libVEX seems to fail if
     * initialized multiple times), hence we do not want to be the sole
     * owner. */
    Vex &_vex;

    DumpFile _dump_file;
    const Memory *_memory;

    std::set<uintptr_t> _seen_blocks;
    std::map<uintptr_t, const IRSB*> _blocks;

    std::map<uintptr_t, Function> _functions;

    FileFormatType _file_format;

    mutable std::mutex _mutex;

    bool _is_finalized;

public:

    Translator(Vex &vex, const std::string &file, FileFormatType file_format,
               bool parse_on_demand=true);

    const Function &cget_function(const uintptr_t address) const;
    const Function &get_function(const uintptr_t address);
    const Function *maybe_get_function(const uintptr_t address);

    /*!
     * \brief Returns all functions known to the `Translator`.
     * \return Returns a map with the function's address as key and `Function`
     * object as value.
     */
    const std::map<uintptr_t, Function> &get_functions() const {
        std::lock_guard<std::mutex> _(_mutex);

        return _functions;
    }

    /*!
     * \brief Returns the view on memory as given by mapping the ELF/PE file.
     *
     * This is mostly used for queries on the binary (such as known memory
     * ranges).
     *
     * \return A reference of type `Memory`.
     */
    const Memory &get_memory() const {
        std::lock_guard<std::mutex> _(_mutex);

        // Since memory is only once initialized in the constructor and
        // otherwise never changed, we assume that the pointer is always set.
        return *_memory;
    }


    /*!
     * \brief Returns the format of the file (ELF/PE => Linux/Windows).
     *
     * \return Fype format type.
     */
    FileFormatType get_file_format() const {
        return _file_format;
    }


    /*!
     * \brief Finalizes `Translator` object in order to make it read-only.
     */
    void finalize();

    /*!
     * \brief Returns the function that contains the given address.
     *
     * \return A reference of type `Function` (throws runtime_error
     * exception if function does not exist).
     */
    const Function &get_containing_function(uint64_t addr) const;

    /*!
     * \brief Adds an xref address to the corresponding function object
     * (throws runtime_error exception if function does not exist).
     */
    void add_function_xref(uint64_t fct_addr, uint64_t xref_addr);

    /*!
     * \brief Adds an xref address to the corresponding function object
     * where it is handled as a virtual function (a virtual callsite)
     * (throws runtime_error exception if function does not exist).
     */
    void add_function_vfunc_xref(uint64_t fct_addr, uint64_t xref_addr);

private:
    bool process_block(Function &function, const BlockDescriptor &block);
    void finalize_block(Function &function, const BlockDescriptor &block,
                        IRSB *block_pointer);

    void parse_known_functions();
    void detect_tail_jumps(Function &function);

    Function *maybe_translate_function(const uintptr_t address);
    Function *translate_function(const std::pair<uintptr_t, FunctionBlocks>&);

    Terminator get_terminator(const IRSB &block, uint64_t block_start) const;

    /*!
     * \brief Returns all functions known to the `Translator` in a mutable way.
     * \return Returns a map with the function's address as key and `Function`
     * object as value (only possible as long as object is set to mutable).
     */
    std::map<uintptr_t, Function> &get_functions_mutable();

    friend class ModuleSSA;
    friend class ModuleFunctionXrefs;
};

#endif // TRANSLATOR_H
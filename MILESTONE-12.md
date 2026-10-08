# Misaki Milestone 12 — Native ET_DYN execution path

## Purpose

This milestone connects Misaki's **expanded portable C++ x86-64 execution backend**
to a bounded ELF64 `ET_DYN` dynamic-table loader and symbol resolver.
The diagnostic uses entirely self-authored guest ELF bytes and a test library.
It **does not** load PS4 SELF/PRX, Sony firmware, encrypted content, or games.

## Supported in this prototype

- x86-64 ELF64 ET_DYN PT_LOAD and exactly one PT_DYNAMIC program header.
- Bounded `DT_NEEDED`, `DT_STRTAB`, `DT_STRSZ`, `DT_SYMTAB`, `DT_SYMENT`,
  `DT_RELA`, `DT_RELASZ`, and `DT_RELAENT` dynamic-table entries.
- `R_X86_64_RELATIVE` (type 8), `R_X86_64_GLOB_DAT` (type 6), and
  `R_X86_64_JUMP_SLOT` (type 7) with strict validation.
- Named guest imports resolved through the existing native `ModuleRegistry`.
- Transactional guest mapping and linking: malformed/unsupported input does not
  partially alter guest memory.
- Execution by the Milestone 11 `PortableX64Backend`, *not* the old C++ CPU.
- A stable C ABI used by the Swift `NativeCoreAPI` and an in-app Core-tab button.

## Install

Extract `Misaki-Milestone12-Native-Dynamic-Modules.zip` over a current
Milestone 11 Misaki checkout, commit, and push to `main`. GitHub Actions will
run the native CMake/CTest workflow and the existing iOS workflow. The IPA
packaging pipeline is not changed.

## iPhone diagnostic

In Misaki, choose **Core > Execute native ET_DYN module**.

Expected:

```
Native ET_DYN: PASS
RAX=42
Instructions=5
Imports=1
Relative relocations=1
Entry=0x5000
Linked address=0x9000
Stack restored=true
Status=0
```

The test loads an ET_DYN image at bias `0x4000`, resolves the guest library
function `libMisakiDynamic::increment2`, applies a relative pointer relocation,
and runs MOV / CALL / ADD / RET / HLT. It is a demonstration fixture and not a
PlayStation 4 firmware boot.

## Testing

Run portable tests locally or in GitHub Actions:

```sh
cmake -S MisakiNative -B build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

All three native test executables pass in local GNU and Clang builds, and in
an ASan/UBSan build. The C ABI was type-checked through Swift 6.2 on Linux;
the complete iOS build must still be verified by GitHub Actions.

## Limitations

This is not a general ELF loader: no PLT lazy binding, TLS, IFUNC, GNU hashes,
versioned symbols, section-based relocation loading, full POSIX syscalls,
PS4 NID system module resolution, or JIT. Arbitrary imported executable files
are not run by the diagnostic UI. The native interpreter implements only a
small instruction subset. The PS4 kernel, GPU and home menu remain unsupported.

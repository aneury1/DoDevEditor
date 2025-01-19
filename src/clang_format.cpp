#include "clang_format.h"

#ifdef USING_CLANG_LIBRARY_DEFINED_ON_DEV
#include <clang/Format/Format.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

#endif ///   USING_CLANG_LIBRARY_DEFINED_ON_DEV

#include <iostream>


wxString format_cpp(const wxString& original){
#ifdef USING_CLANG_LIBRARY_DEFINED_ON_DEV
    using namespace clang::format;

    // Input code to be formatted
    const std::string code = "original.ToAscii()";

    // Formatting options (adjust as needed)
    FormatStyle style = getLLVMStyle();
    style.IndentWidth = 4; // Example customization

    // Create a memory buffer for the input code
    auto inputBuffer = llvm::MemoryBuffer::getMemBuffer(code);

    // Perform formatting
    llvm::Expected<clang::tooling::Replacements> result =
        reformat(style, inputBuffer->getBuffer(), clang::tooling::Range(0, code.size()));

    if (!result) {
        std::cerr << "Formatting failed: " << llvm::toString(result.takeError()) << std::endl;
        return wxString("HOLA1");
    }

    // Apply the replacements to get the formatted code
    auto formattedCode = clang::tooling::applyAllReplacements(code, *result);
    if (!formattedCode) {
        std::cerr << "Applying replacements failed: " << llvm::toString(formattedCode.takeError()) << std::endl;
        return wxString("HOLA1");
    }

    // Output the formatted code
    std::cout << *formattedCode << std::endl;
    wxString ret; 
    ret = *formattedCode;
    return ret;
#else
     return original;
#endif ///   USING_CLANG_LIBRARY_DEFINED_ON_DEV
}
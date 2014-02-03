//===- examples/Tooling/RemoveCStrCalls.cpp - Redundant c_str call removal ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements a tool that prints replacements that remove redundant
//  calls of c_str() on strings.
//
//  Usage:
//  remove-cstr-calls <cmake-output-dir> <file1> <file2> ...
//
//  Where <cmake-output-dir> is a CMake build directory in which a file named
//  compile_commands.json exists (enable -DCMAKE_EXPORT_COMPILE_COMMANDS in
//  CMake to get this output).
//
//  <file1> ... specify the paths of files in the CMake source tree. This path
//  is looked up in the compile command database. If the path of a file is
//  absolute, it needs to point into CMake's source tree. If the path is
//  relative, the current working directory needs to be in the CMake source
//  tree and the file must be in a subdirectory of the current working
//  directory. "./" prefixes in the relative files will be automatically
//  removed, but the rest of a relative path must be a suffix of a path in
//  the compile command line database.
//
//  For example, to use remove-cstr-calls on all files in a subtree of the
//  source tree, use:
//
//    /path/in/subtree $ find . -name '*.cpp'|
//        xargs remove-cstr-calls /path/to/build
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <sstream>

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;
using clang::tooling::newFrontendActionFactory;
using clang::tooling::Replacement;
using clang::tooling::CompilationDatabase;

// FIXME: Pull out helper methods in here into more fitting places.

// Returns the text that makes up 'node' in the source.
// Returns an empty string if the text cannot be found.
template <typename T>
static std::string getText(const SourceManager &SourceManager, const T &Node) {
  SourceLocation StartSpellingLocation =
      SourceManager.getSpellingLoc(Node.getLocStart());
  SourceLocation EndSpellingLocation =
      SourceManager.getSpellingLoc(Node.getLocEnd());
  if (!StartSpellingLocation.isValid() || !EndSpellingLocation.isValid()) {
    return std::string();
  }
  bool Invalid = true;
  const char *Text =
      SourceManager.getCharacterData(StartSpellingLocation, &Invalid);
  if (Invalid) {
    return std::string();
  }
  std::pair<FileID, unsigned> Start =
      SourceManager.getDecomposedLoc(StartSpellingLocation);
  std::pair<FileID, unsigned> End =
      SourceManager.getDecomposedLoc(Lexer::getLocForEndOfToken(
          EndSpellingLocation, 0, SourceManager, LangOptions()));
  if (Start.first != End.first) {
    // Start and end are in different files.
    return std::string();
  }
  if (End.second < Start.second) {
    // Shuffling text with macros may cause this.
    return std::string();
  }
  return std::string(Text, End.second - Start.second);
}

namespace {
class FixVoidArg : public ast_matchers::MatchFinder::MatchCallback {
 public:
  FixVoidArg(tooling::Replacements *Replace)
      : Replace(Replace) {}

  virtual void run(const ast_matchers::MatchFinder::MatchResult &Result) {
    BoundNodes Nodes = Result.Nodes;
    SourceManager const *SM = Result.SourceManager;
    if (FunctionDecl const *const Function = Nodes.getNodeAs<FunctionDecl>("fn")) {
        if (Function->isExternC()) {
            return;
        }
        std::string const text = getText(*SM, *Function);
        if (!Function->isThisDeclarationADefinition()) {
            if (text.length() > 6 && text.substr(text.length()-6) == "(void)") {
                std::string const noVoid = text.substr(0, text.length()-6) + "()";
                Replace->insert(Replacement(*Result.SourceManager, Function, noVoid));
            }
        } else if (text.length() > 0) {
            std::string::size_type end_of_decl = text.find_last_of(')', text.find_first_of('{')) + 1;
            std::string decl = text.substr(0, end_of_decl);
            if (decl.length() > 6 && decl.substr(decl.length()-6) == "(void)") {
                std::string noVoid = decl.substr(0, decl.length()-6) + "()" + text.substr(end_of_decl);
                Replace->insert(Replacement(*Result.SourceManager, Function, noVoid));
            }
        }
    }
  }

 private:
  tooling::Replacements *Replace;
};
} // end namespace

cl::opt<std::string> BuildPath(
  cl::Positional,
  cl::desc("<build-path>"));

cl::list<std::string> SourcePaths(
  cl::Positional,
  cl::desc("<source0> [... <sourceN>]"),
  cl::OneOrMore);

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::OwningPtr<CompilationDatabase> Compilations(
    tooling::FixedCompilationDatabase::loadFromCommandLine(argc, argv));
  cl::ParseCommandLineOptions(argc, argv);
  if (!Compilations) {
    std::string ErrorMessage;
    Compilations.reset(
           CompilationDatabase::loadFromDirectory(BuildPath, ErrorMessage));
    if (!Compilations)
      llvm::report_fatal_error(ErrorMessage);
    }
  tooling::RefactoringTool Tool(*Compilations, SourcePaths);
  ast_matchers::MatchFinder Finder;
  FixVoidArg Callback(&Tool.getReplacements());
  Finder.addMatcher(functionDecl(parameterCountIs(0)).bind("fn"), &Callback);
  return Tool.runAndSave(newFrontendActionFactory(&Finder));
}

add_clang_executable(remove-void-args
  RemoveVoidArgs.cpp
  )

target_link_libraries(remove-void-args
  clangEdit clangTooling clangBasic clangAST clangASTMatchers)

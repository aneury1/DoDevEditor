#include "dodev/editor/EditorCommands.h"
#include <cassert>
#include <iostream>
using namespace dodev::editor;
static std::string apply(std::string s, const Result &r) {
  assert(r.handled);
  if (r.changed)
    s.replace(r.replaceBegin, r.replaceEnd - r.replaceBegin, r.replacement);
  return s;
}
int main() {
  assert(apply("one\ntwo\n", execute("edit.toggle_line_comment", "one\ntwo\n",
                                     0, 7)) == "// one\n// two\n");
  assert(apply("// one\n// two\n", execute("edit.toggle_line_comment",
                                           "// one\n// two\n", 0, 14)) ==
         "one\ntwo\n");
  assert(apply(" x\ny\n", execute("edit.indent", " x\ny\n", 0, 4)) ==
         "     x\n    y\n");
  assert(apply("b\na\n", execute("edit.sort_asc", "b\na\n", 0, 4)) == "a\nb\n");
  assert(apply("foo\nbar\n", execute("edit.move_line_down", "foo\nbar\n", 0,
                                     0)) == "bar\nfoo\n");
  assert(apply("foo\nbar\n", execute("edit.copy_line_down", "foo\nbar\n", 0,
                                     0)) == "foo\nfoo\nbar\n");
  assert(apply("A bC", execute("edit.uppercase", "A bC", 2, 4)) == "A BC");
  assert(apply("a\nb\n", execute("edit.join_lines", "a\nb\n", 0, 3)) ==
         "a b\n");
  assert(apply(" hi ", execute("edit.trim_whitespace", " hi ", 0, 5)) == " hi");
  assert(apply("abc", execute("edit.toggle_block_comment", "abc", 0, 3)) ==
         "/*abc*/");
  assert(apply("hello", execute("edit.delete", "hello", 1, 1)) == "hllo");
  assert(execute("selection.word", "hello world", 7, 7).selectBegin == 6);
  assert(execute("selection.next_occurrence", "foo foo", 0, 3).selectBegin ==
         4);
  assert(execute("selection.all", "abc", 1, 1).selectEnd == 3);
  assert(apply("first\nlast", execute("edit.move_line_up", "first\nlast", 6,
                                      6)) == "last\nfirst");
  assert(apply("first\nlast", execute("edit.move_line_down", "first\nlast", 0,
                                      0)) == "last\nfirst");
  assert(previousCharacter(u8"añb", 3) == 1); // UTF-8 safe prev
  std::cout << "editor commands passed\n";
}

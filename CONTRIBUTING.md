Pull Requests
-------------

Check the following steps to prepare for a merge into `master` after completing
work in a topic or bugfix branch (or fork).

- Squash your commits into a single one if necessary. Each commit should
  represent a coherent set of changes.

- Wait for a code review and the test results of our CI.

- Address any review feedback and fix all issues reported by the CI.

- A maintainer will merge the pull request when all issues are resolved.

Commit Message Style
--------------------

- Summarize the changes in no more than 50 characters in the first line.
  Capitalize and write in an imperative present tense, e.g., "Fix bug" as
  opposed to "Fixes bug" or "Fixed bug".

- Suppress the dot at the end of the first line. Think of it as the header of
  the following description.

- Leave the second line empty.

- Optionally add a long description written in full sentences beginning on the
  third line. 

Coding Style
============

When contributing source code, please adhere to the following coding style,
which is loosely based on the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) and the coding
conventions used by the C++ Standard Library.


Naming
------

- All names except macros and template parameters should be
  lower case and delimited by underscores.

- Template parameter names should be written in CamelCase.

- Types and variables should be nouns, while functions performing an action
  should be "command" verbs. Classes used to implement metaprogramming
  functions also should use verbs, e.g., `remove_const`.

- Private and protected member variables use the suffix `_` while getter *and*
  setter functions use the name without suffix:

  ```cpp
  class person {
  public:
    const std::string& name() const {
      return name_
    }

    void name(const std::string& new_name) {
      name_ = new_name;
    }

  private:
    std::string name_;
  };
  ```

- Use `T` for generic, unconstrained template parameters and `x`
  for generic function arguments. Suffix both with `s` for
  template parameter packs:

  ```cpp
  template <class... Ts>
  void print(const Ts&... xs) {
    // ...
  }
  ```
General
-------

- Use 2 spaces per indentation level.

- Never use tabs.

- Never use C-style casts.

- Never declare more than one variable per line.

- Only separate functions with vertical whitespaces and use comments to
  document logcial blocks inside functions.

- Use `.hpp` as suffix for header files and `.cpp` as suffix for implementation
  files.

- Bind `*` and `&` to the *type*, e.g., `const std::string& arg`.

- Never increase the indentation level for namespaces and access modifiers.

- Use the order `public`, `protected`, and then `private` in classes.

- Always use `auto` to declare a variable unless you cannot initialize it
  immediately or if you actually want a type conversion. In the latter case,
  provide a comment why this conversion is necessary.

- Never use unwrapped, manual resource management such as `new` and `delete`.

- Prefer `using T = X` over `typedef X T`.

- Insert a whitespaces after keywords: `if (...)`, `template <...>`,
  `while (...)`, etc.

- Put opening braces on the same line:

  ```cpp
  void foo() {
    // ...
  }
  ```

- Use standard order for readability: C standard libraries, C++ standard
  libraries, OS-specific headers (usually guarded by `ifdef`), other libraries,
  and finally. 
  ```cpp
  // example.hpp
  
  #include <vector>

  #include <sys/types.h>

  #include "3rd/party.h"

  #include "magic/fwd.hpp"
  ```

  Put the implemented header always first in a `.cpp` file.

  ```cpp
  // example.cpp
  
  #include "golos/example.hpp" // header for this .cpp file
  
  #include "golos/config.hpp" // needed for #ifdef guards
  
  #include <algorithm>

  #ifdef GOLOS_WINDOWS
  #include <windows.h>
  #else
  #include <sys/socket.h>
  #endif

  #include "some/other/library.h"


  ```

- Put output parameters in functions before input parameters if unavoidable.
  This follows the parameter order from the STL.

- Protect single-argument constructors with `explicit` to avoid implicit
  conversions.

- Use `noexcept` whenever it makes sense and as long as it does not limit future
  design space. Move construction and assignment are natural candidates for
  `noexcept`.
  
Comments
--------

- Start Doxygen comments with `///`.

- Use Markdown instead of Doxygen formatters.

- Use `@cmd` rather than `\cmd`.

- Use `//` to define basic comments that should not be processed by Doxygen.

Breaking Statements
-------------------

- Break constructor initializers after the comma, use four spaces for
  indentation, and place each initializer on its own line (unless you don't
  need to break at all):

  ```cpp
  my_class::my_class():
    my_base_class(some_function()),
    greeting_("Hello there! This is my_class!"),
    some_bool_flag_(false) {
    // ok
  }
  other_class::other_class() :
    name_("tommy"),
    buddy_("michael") {
    // ok
  }
  ```

- Break function arguments after the comma for both declaration and invocation:

  ```cpp
  intptr_t channel::compare(
    const abstract_channel* lhs,
    const abstract_channel* rhs
  ) {
    // ...
  }
  ```

- Break before tenary operators and before binary operators:

  ```cpp
  if (
    today_is_a_sunny_day()
      && 
      it_is_not_too_hot_to_go_swimming()
      ) {///
    // ...
  }
  ```
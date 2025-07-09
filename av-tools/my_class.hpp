//
//  my_class.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/4/17.
//

#ifndef my_class_hpp
#define my_class_hpp

#include <cstdio>

class MyClass {
 public:
  MyClass(const void* arg = nullptr) {
    printf("MyClass - construct: %p with {%p}\n", this, arg);
  }

  MyClass(const MyClass& orig) {
    printf("MyClass - copy construct: %p = %p\n", this, &orig);
  }

  MyClass(MyClass&& orig) {
    printf("MyClass - move construct: %p <= %p\n", this, &orig);
  }

  MyClass(const MyClass&& orig) {
    printf("MyClass - FAKE construct: %p ?= %p\n", this, &orig);
  }

  MyClass& operator=(const MyClass& rhs) noexcept {
    printf("MyClass - copy asign: %p = %p\n", this, &rhs);
    return *this;
  }

  MyClass& operator=(MyClass&& rhs) noexcept {
    printf("MyClass - move asign: %p <= %p\n", this, &rhs);
    return *this;
  }

  virtual ~MyClass() {
    printf("MyClass - destruct: %p\n", this);
  }

  void operator()() && {
    printf("MyClass - call on rvalue: %p\n", this);
  }

  void operator()() const& {
    printf("MyClass - call on const lvalue: %p\n", this);
  }

  void operator()() const&& {
    printf("MyClass - call on CONST rvalue: %p\n", this);
  }
};

#endif /* my_class_hpp */

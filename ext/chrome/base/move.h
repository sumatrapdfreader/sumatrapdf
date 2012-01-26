// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MOVE_H_
#define BASE_MOVE_H_

// Macro with the boilerplate that makes a type move-only in C++03.
//
// USAGE
//
// This macro should be used instead of DISALLOW_COPY_AND_ASSIGN to create
// a "move-only" type.  Unlike DISALLOW_COPY_AND_ASSIGN, this macro should be
// the first line in a class declaration.
//
// A class using this macro must call .Pass() (or somehow be an r-value already)
// before it can be:
//
//   * Passed as a function argument
//   * Used as the right-hand side of an assignment
//   * Return from a function
//
// Each class will still need to define their own "move constructor" and "move
// operator=" to make this useful.  Here's an example of the macro, the move
// constructor, and the move operator= from the scoped_ptr class:
//
//  template <typename T>
//  class scoped_ptr {
//     MOVE_ONLY_TYPE_FOR_CPP_03(scoped_ptr, RValue)
//   public:
//    scoped_ptr(RValue& other) : ptr_(other.release()) { }
//    scoped_ptr& operator=(RValue& other) {
//      swap(other);
//      return *this;
//    }
//  };
//
// Note that the constructor must NOT be marked explicit.
//
// For consistency, the second parameter to the macro should always be RValue
// unless you have a strong reason to do otherwise.  It is only exposed as a
// macro parameter so that the move constructor and move operator= don't look
// like they're using a phantom type.
//
//
// HOW THIS WORKS
//
// For a thorough explanation of this technique, see:
//
//   http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Move_Constructor
//
// The summary is that we take advantage of 2 properties:
//
//   1) non-const references will not bind to r-values.
//   2) C++ can apply one user-defined conversion when initializing a
//      variable.
//
// The first lets us disable the copy constructor and assignment operator
// by declaring private version of them with a non-const reference parameter.
//
// For l-values, direct initialization still fails like in
// DISALLOW_COPY_AND_ASSIGN because the copy constructor and assignment
// operators are private.
//
// For r-values, the situation is different. The copy constructor and
// assignment operator are not viable due to (1), so we are trying to call
// a non-existent constructor and non-existing operator= rather than a private
// one.  Since we have not committed an error quite yet, we can provide an
// alternate conversion sequence and a constructor.  We add
//
//   * a private struct named "RValue"
//   * a user-defined conversion "operator RValue&()"
//   * a "move constructor" and "move operator=" that take the RValue& as
//     their sole parameter.
//
// Only r-values will trigger this sequence and execute our "move constructor"
// or "move operator=."  L-values will match the private copy constructor and
// operator= first giving a "private in this context" error.  This combination
// gives us a move-only type.
//
// For signaling a destructive transfer of data from an l-value, we provide a
// method named Pass() which creates an r-value for the current instance
// triggering the move constructor or move operator=.
//
// Other ways to get r-values is to use the result of an expression like a
// function call.
//
// Here's an example with comments explaining what gets triggered where:
//
//    class Foo {
//      MOVE_ONLY_TYPE_FOR_CPP_03(Foo, RValue);
//
//     public:
//       ... API ...
//       Foo(RValue& other);           // Move constructor.
//       Foo& operator=(RValue& rhs);  // Move operator=
//    };
//
//    Foo MakeFoo();  // Function that returns a Foo.
//
//    Foo f;
//    Foo f_copy(f);  // ERROR: Foo(Foo&) is private in this context.
//    Foo f_assign;
//    f_assign = f;   // ERROR: operator=(Foo&) is private in this context.
//
//
//    Foo f(MakeFoo());      // R-value so alternate conversion executed.
//    Foo f_copy(f.Pass());  // R-value so alternate conversion executed.
//    f = f_copy.Pass();     // R-value so alternate conversion executed.
//
//
// IMPLEMENTATION SUBTLETIES WITH RValue
//
// The RValue struct has subtle properties:
//
//   1) All its methods are declared, but intentionally not defined.
//   2) It is *never* instantiated.
//   3) It is a child of the move-only type.
//
// (1) is a guard against accidental violation of (2).  If an instance of
// RValue were ever created, either as a temporary, or as a copy to some
// function parameter or field of a class, the binary will not link.
//
// This ensures that RValue can only exist as a temporary which is important
// to avoid accidental danging references.
//
// (3) allows us to get around instantiations because our user-defined
// conversion can return a downcast of this pointer.
//
//    operator RValue&() { return *reinterpret_cast<RValue*>(this); }
//
// Because RValue does not extend the object size or add any virtual methods,
// this type-pun is safe.
//
// An alternative implementation would be to make RValue into a concrete
// struct that holds a reference to the type.  But in the non-optimized build,
// this causes unnecessary temporaries to be made bloating the object files.
// Also, it would then be possible to accidentally persist an RValue instance.
//
//
// COMPARED TO C++11
//
// In C++11, you would implement this functionality using an r-value reference
// and our .Pass() method would be replaced with a call to std::move().
//
// This emulation also has a deficiency where it uses up the single
// user-defined conversion allowed by C++ during initialization.  This can
// cause problems in some API edge cases.  For instance, in scoped_ptr, it is
// impossible to make an function "void Foo(scoped_ptr<Parent> p)" accept a
// value of type scoped_ptr<Child> even if you add a constructor to
// scoped_ptr<> that would make it look like it should work.  C++11 does not
// have this deficiency.
//
//
// COMPARED TO Boost.Move
//
// Our implementation is based on Boost.Move, but we keep the RValue struct
// private to the move-only type.
//
// In Boost.Move, RValue is the boost::rv<> template.  This type can be used
// when writing APIs like:
//
//   void MyFunc(boost::rv<Foo>& f)
//
// that can take advantage of rv<> to avoid extra copies of a type.  However you
// would still be able to call this version of MyFunc with an l-value:
//
//   Foo f;
//   MyFunc(f);  // Uh oh, we probably just destroyed |f| w/o calling Pass().
//
// unless someone is very careful to also declare a parallel override like:
//
//   void MyFunc(const Foo& f)
//
// that would catch the l-values first.  This was declared unsafe in C++11 and
// a C++11 compiler will explicitly fail MyFunc(f).  Unfortunately, we cannot
// ensure this in C++03.
//
// Since we have no need for writing such APIs yet, our implementation keeps
// RValue private and uses a .Pass() method to do the conversion instead of
// trying to write a version of "std::move()." Writing an API like std::move()
// would require the RValue structs to be public.
//
//
// CAVEATS
//
// If you include a move-only type as a field inside a class that does not
// explicitly declare a copy constructor, the containing class's implicit
// copy constructor will change from Containing(const Containing&) to
// Containing(Containing&).  This can cause some unexpected errors.
//
//   http://llvm.org/bugs/show_bug.cgi?id=11528
//
// The workaround is to explicitly declare your copy constructor.
//
#define MOVE_ONLY_TYPE_FOR_CPP_03(type, rvalue_type) \
 private: \
  struct rvalue_type : public type { \
    rvalue_type(); \
    ~rvalue_type(); \
    rvalue_type(const rvalue_type&); \
    void operator=(const rvalue_type&); \
  }; \
  type(type&); \
  void operator=(type&); \
 public: \
  operator rvalue_type&() { return *reinterpret_cast<rvalue_type*>(this); } \
  type Pass() { return type(*reinterpret_cast<rvalue_type*>(this)); } \
 private:

#endif  // BASE_MOVE_H_

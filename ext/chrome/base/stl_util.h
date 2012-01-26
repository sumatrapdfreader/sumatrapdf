// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Derived from google3/util/gtl/stl_util.h

#ifndef BASE_STL_UTIL_H_
#define BASE_STL_UTIL_H_
#pragma once

#include <string>
#include <vector>

// Clear internal memory of an STL object.
// STL clear()/reserve(0) does not always free internal memory allocated
// This function uses swap/destructor to ensure the internal memory is freed.
template<class T> void STLClearObject(T* obj) {
  T tmp;
  tmp.swap(*obj);
  // Sometimes "T tmp" allocates objects with memory (arena implementation?).
  // Hence using additional reserve(0) even if it doesn't always work.
  obj->reserve(0);
}

// STLDeleteContainerPointers()
//  For a range within a container of pointers, calls delete
//  (non-array version) on these pointers.
// NOTE: for these three functions, we could just implement a DeleteObject
// functor and then call for_each() on the range and functor, but this
// requires us to pull in all of algorithm.h, which seems expensive.
// For hash_[multi]set, it is important that this deletes behind the iterator
// because the hash_set may call the hash function on the iterator when it is
// advanced, which could result in the hash function trying to deference a
// stale pointer.
template <class ForwardIterator>
void STLDeleteContainerPointers(ForwardIterator begin, ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete *temp;
  }
}

// STLDeleteContainerPairPointers()
//  For a range within a container of pairs, calls delete
//  (non-array version) on BOTH items in the pairs.
// NOTE: Like STLDeleteContainerPointers, it is important that this deletes
// behind the iterator because if both the key and value are deleted, the
// container may call the hash function on the iterator when it is advanced,
// which could result in the hash function trying to dereference a stale
// pointer.
template <class ForwardIterator>
void STLDeleteContainerPairPointers(ForwardIterator begin,
                                    ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->first;
    delete temp->second;
  }
}

// STLDeleteContainerPairFirstPointers()
//  For a range within a container of pairs, calls delete (non-array version)
//  on the FIRST item in the pairs.
// NOTE: Like STLDeleteContainerPointers, deleting behind the iterator.
template <class ForwardIterator>
void STLDeleteContainerPairFirstPointers(ForwardIterator begin,
                                         ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->first;
  }
}

// STLDeleteContainerPairSecondPointers()
//  For a range within a container of pairs, calls delete
// NOTE: Like STLDeleteContainerPointers, deleting behind the iterator.
// Deleting the value does not always invalidate the iterator, but it may
// do so if the key is a pointer into the value object.
//  (non-array version) on the SECOND item in the pairs.
template <class ForwardIterator>
void STLDeleteContainerPairSecondPointers(ForwardIterator begin,
                                          ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete temp->second;
  }
}

// To treat a possibly-empty vector as an array, use these functions.
// If you know the array will never be empty, you can use &*v.begin()
// directly, but that is undefined behaviour if v is empty.

template<typename T>
inline T* vector_as_array(std::vector<T>* v) {
  return v->empty() ? NULL : &*v->begin();
}

template<typename T>
inline const T* vector_as_array(const std::vector<T>* v) {
  return v->empty() ? NULL : &*v->begin();
}

// Return a mutable char* pointing to a string's internal buffer,
// which may not be null-terminated. Writing through this pointer will
// modify the string.
//
// string_as_array(&str)[i] is valid for 0 <= i < str.size() until the
// next call to a string method that invalidates iterators.
//
// As of 2006-04, there is no standard-blessed way of getting a
// mutable reference to a string's internal buffer. However, issue 530
// (http://www.open-std.org/JTC1/SC22/WG21/docs/lwg-active.html#530)
// proposes this as the method. According to Matt Austern, this should
// already work on all current implementations.
inline char* string_as_array(std::string* str) {
  // DO NOT USE const_cast<char*>(str->data())
  return str->empty() ? NULL : &*str->begin();
}

// The following functions are useful for cleaning up STL containers
// whose elements point to allocated memory.

// STLDeleteElements() deletes all the elements in an STL container and clears
// the container.  This function is suitable for use with a vector, set,
// hash_set, or any other STL container which defines sensible begin(), end(),
// and clear() methods.
//
// If container is NULL, this function is a no-op.
//
// As an alternative to calling STLDeleteElements() directly, consider
// STLElementDeleter (defined below), which ensures that your container's
// elements are deleted when the STLElementDeleter goes out of scope.
template <class T>
void STLDeleteElements(T *container) {
  if (!container) return;
  STLDeleteContainerPointers(container->begin(), container->end());
  container->clear();
}

// Given an STL container consisting of (key, value) pairs, STLDeleteValues
// deletes all the "value" components and clears the container.  Does nothing
// in the case it's given a NULL pointer.

template <class T>
void STLDeleteValues(T *v) {
  if (!v) return;
  for (typename T::iterator i = v->begin(); i != v->end(); ++i) {
    delete i->second;
  }
  v->clear();
}


// The following classes provide a convenient way to delete all elements or
// values from STL containers when they goes out of scope.  This greatly
// simplifies code that creates temporary objects and has multiple return
// statements.  Example:
//
// vector<MyProto *> tmp_proto;
// STLElementDeleter<vector<MyProto *> > d(&tmp_proto);
// if (...) return false;
// ...
// return success;

// Given a pointer to an STL container this class will delete all the element
// pointers when it goes out of scope.

template<class STLContainer> class STLElementDeleter {
 public:
  STLElementDeleter<STLContainer>(STLContainer *ptr) : container_ptr_(ptr) {}
  ~STLElementDeleter<STLContainer>() { STLDeleteElements(container_ptr_); }
 private:
  STLContainer *container_ptr_;
};

// Given a pointer to an STL container this class will delete all the value
// pointers when it goes out of scope.

template<class STLContainer> class STLValueDeleter {
 public:
  STLValueDeleter<STLContainer>(STLContainer *ptr) : container_ptr_(ptr) {}
  ~STLValueDeleter<STLContainer>() { STLDeleteValues(container_ptr_); }
 private:
  STLContainer *container_ptr_;
};

// Test to see if a set, map, hash_set or hash_map contains a particular key.
// Returns true if the key is in the collection.
template <typename Collection, typename Key>
bool ContainsKey(const Collection& collection, const Key& key) {
  return collection.find(key) != collection.end();
}

#endif  // BASE_STL_UTIL_H_

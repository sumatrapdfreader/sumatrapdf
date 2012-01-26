// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"

#include <algorithm>

#include "base/float_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

namespace {

// Make a deep copy of |node|, but don't include empty lists or dictionaries
// in the copy. It's possible for this function to return NULL and it
// expects |node| to always be non-NULL.
Value* CopyWithoutEmptyChildren(Value* node) {
  DCHECK(node);
  switch (node->GetType()) {
    case Value::TYPE_LIST: {
      ListValue* list = static_cast<ListValue*>(node);
      ListValue* copy = new ListValue;
      for (ListValue::const_iterator it = list->begin(); it != list->end();
           ++it) {
        Value* child_copy = CopyWithoutEmptyChildren(*it);
        if (child_copy)
          copy->Append(child_copy);
      }
      if (!copy->empty())
        return copy;

      delete copy;
      return NULL;
    }

    case Value::TYPE_DICTIONARY: {
      DictionaryValue* dict = static_cast<DictionaryValue*>(node);
      DictionaryValue* copy = new DictionaryValue;
      for (DictionaryValue::key_iterator it = dict->begin_keys();
           it != dict->end_keys(); ++it) {
        Value* child = NULL;
        bool rv = dict->GetWithoutPathExpansion(*it, &child);
        DCHECK(rv);
        Value* child_copy = CopyWithoutEmptyChildren(child);
        if (child_copy)
          copy->SetWithoutPathExpansion(*it, child_copy);
      }
      if (!copy->empty())
        return copy;

      delete copy;
      return NULL;
    }

    default:
      // For everything else, just make a copy.
      return node->DeepCopy();
  }
}

// A small functor for comparing Values for std::find_if and similar.
class ValueEquals {
 public:
  // Pass the value against which all consecutive calls of the () operator will
  // compare their argument to. This Value object must not be destroyed while
  // the ValueEquals is  in use.
  ValueEquals(const Value* first) : first_(first) { }

  bool operator ()(const Value* second) const {
    return first_->Equals(second);
  }

 private:
  const Value* first_;
};

}  // namespace

namespace base {

///////////////////// Value ////////////////////

Value::~Value() {
}

// static
Value* Value::CreateNullValue() {
  return new Value(TYPE_NULL);
}

// static
FundamentalValue* Value::CreateBooleanValue(bool in_value) {
  return new FundamentalValue(in_value);
}

// static
FundamentalValue* Value::CreateIntegerValue(int in_value) {
  return new FundamentalValue(in_value);
}

// static
FundamentalValue* Value::CreateDoubleValue(double in_value) {
  return new FundamentalValue(in_value);
}

// static
StringValue* Value::CreateStringValue(const std::string& in_value) {
  return new StringValue(in_value);
}

// static
StringValue* Value::CreateStringValue(const string16& in_value) {
  return new StringValue(in_value);
}

bool Value::GetAsBoolean(bool* out_value) const {
  return false;
}

bool Value::GetAsInteger(int* out_value) const {
  return false;
}

bool Value::GetAsDouble(double* out_value) const {
  return false;
}

bool Value::GetAsString(std::string* out_value) const {
  return false;
}

bool Value::GetAsString(string16* out_value) const {
  return false;
}

bool Value::GetAsList(ListValue** out_value) {
  return false;
}

bool Value::GetAsList(const ListValue** out_value) const {
  return false;
}

bool Value::GetAsDictionary(DictionaryValue** out_value) {
  return false;
}

bool Value::GetAsDictionary(const DictionaryValue** out_value) const {
  return false;
}

Value* Value::DeepCopy() const {
  // This method should only be getting called for null Values--all subclasses
  // need to provide their own implementation;.
  DCHECK(IsType(TYPE_NULL));
  return CreateNullValue();
}

bool Value::Equals(const Value* other) const {
  // This method should only be getting called for null Values--all subclasses
  // need to provide their own implementation;.
  DCHECK(IsType(TYPE_NULL));
  return other->IsType(TYPE_NULL);
}

// static
bool Value::Equals(const Value* a, const Value* b) {
  if ((a == NULL) && (b == NULL)) return true;
  if ((a == NULL) ^  (b == NULL)) return false;
  return a->Equals(b);
}

Value::Value(Type type) : type_(type) {
}

///////////////////// FundamentalValue ////////////////////

FundamentalValue::FundamentalValue(bool in_value)
    : Value(TYPE_BOOLEAN), boolean_value_(in_value) {
}

FundamentalValue::FundamentalValue(int in_value)
    : Value(TYPE_INTEGER), integer_value_(in_value) {
}

FundamentalValue::FundamentalValue(double in_value)
    : Value(TYPE_DOUBLE), double_value_(in_value) {
  if (!IsFinite(double_value_)) {
    NOTREACHED() << "Non-finite (i.e. NaN or positive/negative infinity) "
                 << "values cannot be represented in JSON";
    double_value_ = 0.0;
  }
}

FundamentalValue::~FundamentalValue() {
}

bool FundamentalValue::GetAsBoolean(bool* out_value) const {
  if (out_value && IsType(TYPE_BOOLEAN))
    *out_value = boolean_value_;
  return (IsType(TYPE_BOOLEAN));
}

bool FundamentalValue::GetAsInteger(int* out_value) const {
  if (out_value && IsType(TYPE_INTEGER))
    *out_value = integer_value_;
  return (IsType(TYPE_INTEGER));
}

bool FundamentalValue::GetAsDouble(double* out_value) const {
  if (out_value && IsType(TYPE_DOUBLE))
    *out_value = double_value_;
  else if (out_value && IsType(TYPE_INTEGER))
    *out_value = integer_value_;
  return (IsType(TYPE_DOUBLE) || IsType(TYPE_INTEGER));
}

FundamentalValue* FundamentalValue::DeepCopy() const {
  switch (GetType()) {
    case TYPE_BOOLEAN:
      return CreateBooleanValue(boolean_value_);

    case TYPE_INTEGER:
      return CreateIntegerValue(integer_value_);

    case TYPE_DOUBLE:
      return CreateDoubleValue(double_value_);

    default:
      NOTREACHED();
      return NULL;
  }
}

bool FundamentalValue::Equals(const Value* other) const {
  if (other->GetType() != GetType())
    return false;

  switch (GetType()) {
    case TYPE_BOOLEAN: {
      bool lhs, rhs;
      return GetAsBoolean(&lhs) && other->GetAsBoolean(&rhs) && lhs == rhs;
    }
    case TYPE_INTEGER: {
      int lhs, rhs;
      return GetAsInteger(&lhs) && other->GetAsInteger(&rhs) && lhs == rhs;
    }
    case TYPE_DOUBLE: {
      double lhs, rhs;
      return GetAsDouble(&lhs) && other->GetAsDouble(&rhs) && lhs == rhs;
    }
    default:
      NOTREACHED();
      return false;
  }
}

///////////////////// StringValue ////////////////////

StringValue::StringValue(const std::string& in_value)
    : Value(TYPE_STRING),
      value_(in_value) {
  DCHECK(IsStringUTF8(in_value));
}

StringValue::StringValue(const string16& in_value)
    : Value(TYPE_STRING),
      value_(UTF16ToUTF8(in_value)) {
}

StringValue::~StringValue() {
}

bool StringValue::GetAsString(std::string* out_value) const {
  if (out_value)
    *out_value = value_;
  return true;
}

bool StringValue::GetAsString(string16* out_value) const {
  if (out_value)
    *out_value = UTF8ToUTF16(value_);
  return true;
}

StringValue* StringValue::DeepCopy() const {
  return CreateStringValue(value_);
}

bool StringValue::Equals(const Value* other) const {
  if (other->GetType() != GetType())
    return false;
  std::string lhs, rhs;
  return GetAsString(&lhs) && other->GetAsString(&rhs) && lhs == rhs;
}

///////////////////// BinaryValue ////////////////////

BinaryValue::~BinaryValue() {
  DCHECK(buffer_);
  if (buffer_)
    delete[] buffer_;
}

// static
BinaryValue* BinaryValue::Create(char* buffer, size_t size) {
  if (!buffer)
    return NULL;

  return new BinaryValue(buffer, size);
}

// static
BinaryValue* BinaryValue::CreateWithCopiedBuffer(const char* buffer,
                                                 size_t size) {
  if (!buffer)
    return NULL;

  char* buffer_copy = new char[size];
  memcpy(buffer_copy, buffer, size);
  return new BinaryValue(buffer_copy, size);
}

BinaryValue* BinaryValue::DeepCopy() const {
  return CreateWithCopiedBuffer(buffer_, size_);
}

bool BinaryValue::Equals(const Value* other) const {
  if (other->GetType() != GetType())
    return false;
  const BinaryValue* other_binary = static_cast<const BinaryValue*>(other);
  if (other_binary->size_ != size_)
    return false;
  return !memcmp(buffer_, other_binary->buffer_, size_);
}

BinaryValue::BinaryValue(char* buffer, size_t size)
  : Value(TYPE_BINARY),
    buffer_(buffer),
    size_(size) {
  DCHECK(buffer_);
}

///////////////////// DictionaryValue ////////////////////

DictionaryValue::DictionaryValue()
    : Value(TYPE_DICTIONARY) {
}

DictionaryValue::~DictionaryValue() {
  Clear();
}

bool DictionaryValue::GetAsDictionary(DictionaryValue** out_value) {
  if (out_value)
    *out_value = this;
  return true;
}

bool DictionaryValue::GetAsDictionary(const DictionaryValue** out_value) const {
  if (out_value)
    *out_value = this;
  return true;
}

bool DictionaryValue::HasKey(const std::string& key) const {
  DCHECK(IsStringUTF8(key));
  ValueMap::const_iterator current_entry = dictionary_.find(key);
  DCHECK((current_entry == dictionary_.end()) || current_entry->second);
  return current_entry != dictionary_.end();
}

void DictionaryValue::Clear() {
  ValueMap::iterator dict_iterator = dictionary_.begin();
  while (dict_iterator != dictionary_.end()) {
    delete dict_iterator->second;
    ++dict_iterator;
  }

  dictionary_.clear();
}

void DictionaryValue::Set(const std::string& path, Value* in_value) {
  DCHECK(IsStringUTF8(path));
  DCHECK(in_value);

  std::string current_path(path);
  DictionaryValue* current_dictionary = this;
  for (size_t delimiter_position = current_path.find('.');
       delimiter_position != std::string::npos;
       delimiter_position = current_path.find('.')) {
    // Assume that we're indexing into a dictionary.
    std::string key(current_path, 0, delimiter_position);
    DictionaryValue* child_dictionary = NULL;
    if (!current_dictionary->GetDictionary(key, &child_dictionary)) {
      child_dictionary = new DictionaryValue;
      current_dictionary->SetWithoutPathExpansion(key, child_dictionary);
    }

    current_dictionary = child_dictionary;
    current_path.erase(0, delimiter_position + 1);
  }

  current_dictionary->SetWithoutPathExpansion(current_path, in_value);
}

void DictionaryValue::SetBoolean(const std::string& path, bool in_value) {
  Set(path, CreateBooleanValue(in_value));
}

void DictionaryValue::SetInteger(const std::string& path, int in_value) {
  Set(path, CreateIntegerValue(in_value));
}

void DictionaryValue::SetDouble(const std::string& path, double in_value) {
  Set(path, CreateDoubleValue(in_value));
}

void DictionaryValue::SetString(const std::string& path,
                                const std::string& in_value) {
  Set(path, CreateStringValue(in_value));
}

void DictionaryValue::SetString(const std::string& path,
                                const string16& in_value) {
  Set(path, CreateStringValue(in_value));
}

void DictionaryValue::SetWithoutPathExpansion(const std::string& key,
                                              Value* in_value) {
  // If there's an existing value here, we need to delete it, because
  // we own all our children.
  std::pair<ValueMap::iterator, bool> ins_res =
      dictionary_.insert(std::make_pair(key, in_value));
  if (!ins_res.second) {
    DCHECK_NE(ins_res.first->second, in_value);  // This would be bogus
    delete ins_res.first->second;
    ins_res.first->second = in_value;
  }
}

bool DictionaryValue::Get(const std::string& path, Value** out_value) const {
  DCHECK(IsStringUTF8(path));
  std::string current_path(path);
  const DictionaryValue* current_dictionary = this;
  for (size_t delimiter_position = current_path.find('.');
       delimiter_position != std::string::npos;
       delimiter_position = current_path.find('.')) {
    DictionaryValue* child_dictionary = NULL;
    if (!current_dictionary->GetDictionary(
            current_path.substr(0, delimiter_position), &child_dictionary))
      return false;

    current_dictionary = child_dictionary;
    current_path.erase(0, delimiter_position + 1);
  }

  return current_dictionary->GetWithoutPathExpansion(current_path, out_value);
}

bool DictionaryValue::GetBoolean(const std::string& path,
                                 bool* bool_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsBoolean(bool_value);
}

bool DictionaryValue::GetInteger(const std::string& path,
                                 int* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsInteger(out_value);
}

bool DictionaryValue::GetDouble(const std::string& path,
                                double* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsDouble(out_value);
}

bool DictionaryValue::GetString(const std::string& path,
                                std::string* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsString(out_value);
}

bool DictionaryValue::GetString(const std::string& path,
                                string16* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsString(out_value);
}

bool DictionaryValue::GetStringASCII(const std::string& path,
                                     std::string* out_value) const {
  std::string out;
  if (!GetString(path, &out))
    return false;

  if (!IsStringASCII(out)) {
    NOTREACHED();
    return false;
  }

  out_value->assign(out);
  return true;
}

bool DictionaryValue::GetBinary(const std::string& path,
                                BinaryValue** out_value) const {
  Value* value;
  bool result = Get(path, &value);
  if (!result || !value->IsType(TYPE_BINARY))
    return false;

  if (out_value)
    *out_value = static_cast<BinaryValue*>(value);

  return true;
}

bool DictionaryValue::GetDictionary(const std::string& path,
                                    DictionaryValue** out_value) const {
  Value* value;
  bool result = Get(path, &value);
  if (!result || !value->IsType(TYPE_DICTIONARY))
    return false;

  if (out_value)
    *out_value = static_cast<DictionaryValue*>(value);

  return true;
}

bool DictionaryValue::GetList(const std::string& path,
                              ListValue** out_value) const {
  Value* value;
  bool result = Get(path, &value);
  if (!result || !value->IsType(TYPE_LIST))
    return false;

  if (out_value)
    *out_value = static_cast<ListValue*>(value);

  return true;
}

bool DictionaryValue::GetWithoutPathExpansion(const std::string& key,
                                              Value** out_value) const {
  DCHECK(IsStringUTF8(key));
  ValueMap::const_iterator entry_iterator = dictionary_.find(key);
  if (entry_iterator == dictionary_.end())
    return false;

  Value* entry = entry_iterator->second;
  if (out_value)
    *out_value = entry;
  return true;
}

bool DictionaryValue::GetIntegerWithoutPathExpansion(const std::string& key,
                                                     int* out_value) const {
  Value* value;
  if (!GetWithoutPathExpansion(key, &value))
    return false;

  return value->GetAsInteger(out_value);
}

bool DictionaryValue::GetDoubleWithoutPathExpansion(const std::string& key,
                                                    double* out_value) const {
  Value* value;
  if (!GetWithoutPathExpansion(key, &value))
    return false;

  return value->GetAsDouble(out_value);
}

bool DictionaryValue::GetStringWithoutPathExpansion(
    const std::string& key,
    std::string* out_value) const {
  Value* value;
  if (!GetWithoutPathExpansion(key, &value))
    return false;

  return value->GetAsString(out_value);
}

bool DictionaryValue::GetStringWithoutPathExpansion(
    const std::string& key,
    string16* out_value) const {
  Value* value;
  if (!GetWithoutPathExpansion(key, &value))
    return false;

  return value->GetAsString(out_value);
}

bool DictionaryValue::GetDictionaryWithoutPathExpansion(
    const std::string& key,
    DictionaryValue** out_value) const {
  Value* value;
  bool result = GetWithoutPathExpansion(key, &value);
  if (!result || !value->IsType(TYPE_DICTIONARY))
    return false;

  if (out_value)
    *out_value = static_cast<DictionaryValue*>(value);

  return true;
}

bool DictionaryValue::GetListWithoutPathExpansion(const std::string& key,
                                                  ListValue** out_value) const {
  Value* value;
  bool result = GetWithoutPathExpansion(key, &value);
  if (!result || !value->IsType(TYPE_LIST))
    return false;

  if (out_value)
    *out_value = static_cast<ListValue*>(value);

  return true;
}

bool DictionaryValue::Remove(const std::string& path, Value** out_value) {
  DCHECK(IsStringUTF8(path));
  std::string current_path(path);
  DictionaryValue* current_dictionary = this;
  size_t delimiter_position = current_path.rfind('.');
  if (delimiter_position != std::string::npos) {
    if (!GetDictionary(current_path.substr(0, delimiter_position),
                       &current_dictionary))
      return false;
    current_path.erase(0, delimiter_position + 1);
  }

  return current_dictionary->RemoveWithoutPathExpansion(current_path,
                                                        out_value);
}

bool DictionaryValue::RemoveWithoutPathExpansion(const std::string& key,
                                                 Value** out_value) {
  DCHECK(IsStringUTF8(key));
  ValueMap::iterator entry_iterator = dictionary_.find(key);
  if (entry_iterator == dictionary_.end())
    return false;

  Value* entry = entry_iterator->second;
  if (out_value)
    *out_value = entry;
  else
    delete entry;
  dictionary_.erase(entry_iterator);
  return true;
}

DictionaryValue* DictionaryValue::DeepCopyWithoutEmptyChildren() {
  Value* copy = CopyWithoutEmptyChildren(this);
  return copy ? static_cast<DictionaryValue*>(copy) : new DictionaryValue;
}

void DictionaryValue::MergeDictionary(const DictionaryValue* dictionary) {
  for (DictionaryValue::key_iterator key(dictionary->begin_keys());
       key != dictionary->end_keys(); ++key) {
    Value* merge_value;
    if (dictionary->GetWithoutPathExpansion(*key, &merge_value)) {
      // Check whether we have to merge dictionaries.
      if (merge_value->IsType(Value::TYPE_DICTIONARY)) {
        DictionaryValue* sub_dict;
        if (GetDictionaryWithoutPathExpansion(*key, &sub_dict)) {
          sub_dict->MergeDictionary(
              static_cast<const DictionaryValue*>(merge_value));
          continue;
        }
      }
      // All other cases: Make a copy and hook it up.
      SetWithoutPathExpansion(*key, merge_value->DeepCopy());
    }
  }
}

DictionaryValue* DictionaryValue::DeepCopy() const {
  DictionaryValue* result = new DictionaryValue;

  for (ValueMap::const_iterator current_entry(dictionary_.begin());
       current_entry != dictionary_.end(); ++current_entry) {
    result->SetWithoutPathExpansion(current_entry->first,
                                    current_entry->second->DeepCopy());
  }

  return result;
}

bool DictionaryValue::Equals(const Value* other) const {
  if (other->GetType() != GetType())
    return false;

  const DictionaryValue* other_dict =
      static_cast<const DictionaryValue*>(other);
  key_iterator lhs_it(begin_keys());
  key_iterator rhs_it(other_dict->begin_keys());
  while (lhs_it != end_keys() && rhs_it != other_dict->end_keys()) {
    Value* lhs;
    Value* rhs;
    if (*lhs_it != *rhs_it ||
        !GetWithoutPathExpansion(*lhs_it, &lhs) ||
        !other_dict->GetWithoutPathExpansion(*rhs_it, &rhs) ||
        !lhs->Equals(rhs)) {
      return false;
    }
    ++lhs_it;
    ++rhs_it;
  }
  if (lhs_it != end_keys() || rhs_it != other_dict->end_keys())
    return false;

  return true;
}

///////////////////// ListValue ////////////////////

ListValue::ListValue() : Value(TYPE_LIST) {
}

ListValue::~ListValue() {
  Clear();
}

void ListValue::Clear() {
  for (ValueVector::iterator i(list_.begin()); i != list_.end(); ++i)
    delete *i;
  list_.clear();
}

bool ListValue::Set(size_t index, Value* in_value) {
  if (!in_value)
    return false;

  if (index >= list_.size()) {
    // Pad out any intermediate indexes with null settings
    while (index > list_.size())
      Append(CreateNullValue());
    Append(in_value);
  } else {
    DCHECK(list_[index] != in_value);
    delete list_[index];
    list_[index] = in_value;
  }
  return true;
}

bool ListValue::Get(size_t index, Value** out_value) const {
  if (index >= list_.size())
    return false;

  if (out_value)
    *out_value = list_[index];

  return true;
}

bool ListValue::GetBoolean(size_t index, bool* bool_value) const {
  Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsBoolean(bool_value);
}

bool ListValue::GetInteger(size_t index, int* out_value) const {
  Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsInteger(out_value);
}

bool ListValue::GetDouble(size_t index, double* out_value) const {
  Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsDouble(out_value);
}

bool ListValue::GetString(size_t index, std::string* out_value) const {
  Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsString(out_value);
}

bool ListValue::GetString(size_t index, string16* out_value) const {
  Value* value;
  if (!Get(index, &value))
    return false;

  return value->GetAsString(out_value);
}

bool ListValue::GetBinary(size_t index, BinaryValue** out_value) const {
  Value* value;
  bool result = Get(index, &value);
  if (!result || !value->IsType(TYPE_BINARY))
    return false;

  if (out_value)
    *out_value = static_cast<BinaryValue*>(value);

  return true;
}

bool ListValue::GetDictionary(size_t index, DictionaryValue** out_value) const {
  Value* value;
  bool result = Get(index, &value);
  if (!result || !value->IsType(TYPE_DICTIONARY))
    return false;

  if (out_value)
    *out_value = static_cast<DictionaryValue*>(value);

  return true;
}

bool ListValue::GetList(size_t index, ListValue** out_value) const {
  Value* value;
  bool result = Get(index, &value);
  if (!result || !value->IsType(TYPE_LIST))
    return false;

  if (out_value)
    *out_value = static_cast<ListValue*>(value);

  return true;
}

bool ListValue::Remove(size_t index, Value** out_value) {
  if (index >= list_.size())
    return false;

  if (out_value)
    *out_value = list_[index];
  else
    delete list_[index];

  list_.erase(list_.begin() + index);
  return true;
}

bool ListValue::Remove(const Value& value, size_t* index) {
  for (ValueVector::iterator i(list_.begin()); i != list_.end(); ++i) {
    if ((*i)->Equals(&value)) {
      size_t previous_index = i - list_.begin();
      delete *i;
      list_.erase(i);

      if (index)
        *index = previous_index;
      return true;
    }
  }
  return false;
}

void ListValue::Append(Value* in_value) {
  DCHECK(in_value);
  list_.push_back(in_value);
}

bool ListValue::AppendIfNotPresent(Value* in_value) {
  DCHECK(in_value);
  for (ValueVector::const_iterator i(list_.begin()); i != list_.end(); ++i) {
    if ((*i)->Equals(in_value)) {
      delete in_value;
      return false;
    }
  }
  list_.push_back(in_value);
  return true;
}

bool ListValue::Insert(size_t index, Value* in_value) {
  DCHECK(in_value);
  if (index > list_.size())
    return false;

  list_.insert(list_.begin() + index, in_value);
  return true;
}

ListValue::const_iterator ListValue::Find(const Value& value) const {
  return std::find_if(list_.begin(), list_.end(), ValueEquals(&value));
}

bool ListValue::GetAsList(ListValue** out_value) {
  if (out_value)
    *out_value = this;
  return true;
}

bool ListValue::GetAsList(const ListValue** out_value) const {
  if (out_value)
    *out_value = this;
  return true;
}

ListValue* ListValue::DeepCopy() const {
  ListValue* result = new ListValue;

  for (ValueVector::const_iterator i(list_.begin()); i != list_.end(); ++i)
    result->Append((*i)->DeepCopy());

  return result;
}

bool ListValue::Equals(const Value* other) const {
  if (other->GetType() != GetType())
    return false;

  const ListValue* other_list =
      static_cast<const ListValue*>(other);
  const_iterator lhs_it, rhs_it;
  for (lhs_it = begin(), rhs_it = other_list->begin();
       lhs_it != end() && rhs_it != other_list->end();
       ++lhs_it, ++rhs_it) {
    if (!(*lhs_it)->Equals(*rhs_it))
      return false;
  }
  if (lhs_it != end() || rhs_it != other_list->end())
    return false;

  return true;
}

ValueSerializer::~ValueSerializer() {
}

}  // namespace base

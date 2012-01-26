// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/string_tokenizer.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "base/utf_string_conversions.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "base/time.h"

#if defined(OS_WIN)
#include "base/debug/trace_event_win.h"
#endif

class DeleteTraceLogForTesting {
 public:
  static void Delete() {
    Singleton<base::debug::TraceLog,
              StaticMemorySingletonTraits<base::debug::TraceLog> >::OnExit(0);
  }
};

namespace base {
namespace debug {

// Controls the number of trace events we will buffer in-memory
// before throwing them away.
const size_t kTraceEventBufferSize = 500000;
const size_t kTraceEventBatchSize = 1000;

#define TRACE_EVENT_MAX_CATEGORIES 100

namespace {

// Parallel arrays g_categories and g_category_enabled are separate so that
// a pointer to a member of g_category_enabled can be easily converted to an
// index into g_categories. This allows macros to deal only with char enabled
// pointers from g_category_enabled, and we can convert internally to determine
// the category name from the char enabled pointer.
const char* g_categories[TRACE_EVENT_MAX_CATEGORIES] = {
  "tracing already shutdown",
  "tracing categories exhausted; must increase TRACE_EVENT_MAX_CATEGORIES",
  "__metadata",
};
// The enabled flag is char instead of bool so that the API can be used from C.
unsigned char g_category_enabled[TRACE_EVENT_MAX_CATEGORIES] = { 0 };
const int g_category_already_shutdown = 0;
const int g_category_categories_exhausted = 1;
const int g_category_metadata = 2;
int g_category_index = 3; // skip initial 3 categories

// The most-recently captured name of the current thread
LazyInstance<ThreadLocalPointer<const char> >::Leaky
    g_current_thread_name = LAZY_INSTANCE_INITIALIZER;

void AppendValueAsJSON(unsigned char type,
                       TraceEvent::TraceValue value,
                       std::string* out) {
  std::string::size_type start_pos;
  switch (type) {
    case TRACE_VALUE_TYPE_BOOL:
      *out += value.as_bool ? "true" : "false";
      break;
    case TRACE_VALUE_TYPE_UINT:
      StringAppendF(out, "%" PRIu64, static_cast<uint64>(value.as_uint));
      break;
    case TRACE_VALUE_TYPE_INT:
      StringAppendF(out, "%" PRId64, static_cast<int64>(value.as_int));
      break;
    case TRACE_VALUE_TYPE_DOUBLE:
      StringAppendF(out, "%f", value.as_double);
      break;
    case TRACE_VALUE_TYPE_POINTER:
      // JSON only supports double and int numbers.
      // So as not to lose bits from a 64-bit pointer, output as a hex string.
      StringAppendF(out, "\"%" PRIx64 "\"", static_cast<uint64>(
                                     reinterpret_cast<intptr_t>(
                                     value.as_pointer)));
      break;
    case TRACE_VALUE_TYPE_STRING:
    case TRACE_VALUE_TYPE_COPY_STRING:
      *out += "\"";
      start_pos = out->size();
      *out += value.as_string ? value.as_string : "NULL";
      // insert backslash before special characters for proper json format.
      while ((start_pos = out->find_first_of("\\\"", start_pos)) !=
             std::string::npos) {
        out->insert(start_pos, 1, '\\');
        // skip inserted escape character and following character.
        start_pos += 2;
      }
      *out += "\"";
      break;
    default:
      NOTREACHED() << "Don't know how to print this value";
      break;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// TraceEvent
//
////////////////////////////////////////////////////////////////////////////////

namespace {

size_t GetAllocLength(const char* str) { return str ? strlen(str) + 1 : 0; }

// Copies |*member| into |*buffer|, sets |*member| to point to this new
// location, and then advances |*buffer| by the amount written.
void CopyTraceEventParameter(char** buffer,
                             const char** member,
                             const char* end) {
  if (*member) {
    size_t written = strlcpy(*buffer, *member, end - *buffer) + 1;
    DCHECK_LE(static_cast<int>(written), end - *buffer);
    *member = *buffer;
    *buffer += written;
  }
}

}  // namespace

TraceEvent::TraceEvent()
    : id_(0u),
      category_enabled_(NULL),
      name_(NULL),
      thread_id_(0),
      phase_(TRACE_EVENT_PHASE_BEGIN),
      flags_(0) {
  arg_names_[0] = NULL;
  arg_names_[1] = NULL;
}

TraceEvent::TraceEvent(int thread_id,
                       TimeTicks timestamp,
                       char phase,
                       const unsigned char* category_enabled,
                       const char* name,
                       unsigned long long id,
                       int num_args,
                       const char** arg_names,
                       const unsigned char* arg_types,
                       const unsigned long long* arg_values,
                       unsigned char flags)
    : timestamp_(timestamp),
      id_(id),
      category_enabled_(category_enabled),
      name_(name),
      thread_id_(thread_id),
      phase_(phase),
      flags_(flags) {
  // Clamp num_args since it may have been set by a third_party library.
  num_args = (num_args > kTraceMaxNumArgs) ? kTraceMaxNumArgs : num_args;
  int i = 0;
  for (; i < num_args; ++i) {
    arg_names_[i] = arg_names[i];
    arg_values_[i].as_uint = arg_values[i];
    arg_types_[i] = arg_types[i];
  }
  for (; i < kTraceMaxNumArgs; ++i) {
    arg_names_[i] = NULL;
    arg_values_[i].as_uint = 0u;
    arg_types_[i] = TRACE_VALUE_TYPE_UINT;
  }

  bool copy = !!(flags & TRACE_EVENT_FLAG_COPY);
  size_t alloc_size = 0;
  if (copy) {
    alloc_size += GetAllocLength(name);
    for (i = 0; i < num_args; ++i) {
      alloc_size += GetAllocLength(arg_names_[i]);
      if (arg_types_[i] == TRACE_VALUE_TYPE_STRING)
        arg_types_[i] = TRACE_VALUE_TYPE_COPY_STRING;
    }
  }

  bool arg_is_copy[kTraceMaxNumArgs];
  for (i = 0; i < num_args; ++i) {
    // We only take a copy of arg_vals if they are of type COPY_STRING.
    arg_is_copy[i] = (arg_types_[i] == TRACE_VALUE_TYPE_COPY_STRING);
    if (arg_is_copy[i])
      alloc_size += GetAllocLength(arg_values_[i].as_string);
  }

  if (alloc_size) {
    parameter_copy_storage_ = new base::RefCountedString;
    parameter_copy_storage_->data().resize(alloc_size);
    char* ptr = string_as_array(&parameter_copy_storage_->data());
    const char* end = ptr + alloc_size;
    if (copy) {
      CopyTraceEventParameter(&ptr, &name_, end);
      for (i = 0; i < num_args; ++i)
        CopyTraceEventParameter(&ptr, &arg_names_[i], end);
    }
    for (i = 0; i < num_args; ++i) {
      if (arg_is_copy[i])
        CopyTraceEventParameter(&ptr, &arg_values_[i].as_string, end);
    }
    DCHECK_EQ(end, ptr) << "Overrun by " << ptr - end;
  }
}

TraceEvent::~TraceEvent() {
}

void TraceEvent::AppendEventsAsJSON(const std::vector<TraceEvent>& events,
                                    size_t start,
                                    size_t count,
                                    std::string* out) {
  for (size_t i = 0; i < count && start + i < events.size(); ++i) {
    if (i > 0)
      *out += ",";
    events[i + start].AppendAsJSON(out);
  }
}

void TraceEvent::AppendAsJSON(std::string* out) const {
  int64 time_int64 = timestamp_.ToInternalValue();
  int process_id = TraceLog::GetInstance()->process_id();
  // Category name checked at category creation time.
  DCHECK(!strchr(name_, '"'));
  StringAppendF(out,
      "{\"cat\":\"%s\",\"pid\":%i,\"tid\":%i,\"ts\":%" PRId64 ","
      "\"ph\":\"%c\",\"name\":\"%s\",\"args\":{",
      TraceLog::GetCategoryName(category_enabled_),
      process_id,
      thread_id_,
      time_int64,
      phase_,
      name_);

  // Output argument names and values, stop at first NULL argument name.
  for (int i = 0; i < kTraceMaxNumArgs && arg_names_[i]; ++i) {
    if (i > 0)
      *out += ",";
    *out += "\"";
    *out += arg_names_[i];
    *out += "\":";
    AppendValueAsJSON(arg_types_[i], arg_values_[i], out);
  }
  *out += "}";

  // If id_ is set, print it out as a hex string so we don't loose any
  // bits (it might be a 64-bit pointer).
  if (flags_ & TRACE_EVENT_FLAG_HAS_ID)
    StringAppendF(out, ",\"id\":\"%" PRIx64 "\"", static_cast<uint64>(id_));
  *out += "}";
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceResultBuffer
//
////////////////////////////////////////////////////////////////////////////////

TraceResultBuffer::OutputCallback
    TraceResultBuffer::SimpleOutput::GetCallback() {
  return base::Bind(&SimpleOutput::Append, base::Unretained(this));
}

void TraceResultBuffer::SimpleOutput::Append(
    const std::string& json_trace_output) {
  json_output += json_trace_output;
}

TraceResultBuffer::TraceResultBuffer() : append_comma_(false) {
}

TraceResultBuffer::~TraceResultBuffer() {
}

void TraceResultBuffer::SetOutputCallback(
    const OutputCallback& json_chunk_callback) {
  output_callback_ = json_chunk_callback;
}

void TraceResultBuffer::Start() {
  append_comma_ = false;
  output_callback_.Run("[");
}

void TraceResultBuffer::AddFragment(const std::string& trace_fragment) {
  if (append_comma_)
    output_callback_.Run(",");
  append_comma_ = true;
  output_callback_.Run(trace_fragment);
}

void TraceResultBuffer::Finish() {
  output_callback_.Run("]");
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceLog
//
////////////////////////////////////////////////////////////////////////////////

// static
TraceLog* TraceLog::GetInstance() {
  return Singleton<TraceLog, StaticMemorySingletonTraits<TraceLog> >::get();
}

TraceLog::TraceLog()
    : enabled_(false) {
  SetProcessID(static_cast<int>(base::GetCurrentProcId()));
}

TraceLog::~TraceLog() {
}

const unsigned char* TraceLog::GetCategoryEnabled(const char* name) {
  TraceLog* tracelog = GetInstance();
  if (!tracelog) {
    DCHECK(!g_category_enabled[g_category_already_shutdown]);
    return &g_category_enabled[g_category_already_shutdown];
  }
  return tracelog->GetCategoryEnabledInternal(name);
}

const char* TraceLog::GetCategoryName(const unsigned char* category_enabled) {
  // Calculate the index of the category by finding category_enabled in
  // g_category_enabled array.
  uintptr_t category_begin = reinterpret_cast<uintptr_t>(g_category_enabled);
  uintptr_t category_ptr = reinterpret_cast<uintptr_t>(category_enabled);
  DCHECK(category_ptr >= category_begin &&
         category_ptr < reinterpret_cast<uintptr_t>(g_category_enabled +
                                               TRACE_EVENT_MAX_CATEGORIES)) <<
      "out of bounds category pointer";
  uintptr_t category_index =
      (category_ptr - category_begin) / sizeof(g_category_enabled[0]);
  return g_categories[category_index];
}

static void EnableMatchingCategory(int category_index,
                                   const std::vector<std::string>& patterns,
                                   unsigned char is_included) {
  std::vector<std::string>::const_iterator ci = patterns.begin();
  bool is_match = false;
  for (; ci != patterns.end(); ++ci) {
    is_match = MatchPattern(g_categories[category_index], ci->c_str());
    if (is_match)
      break;
  }
  ANNOTATE_BENIGN_RACE(&g_category_enabled[category_index],
                       "trace_event category enabled");
  g_category_enabled[category_index] = is_match ?
      is_included : (is_included ^ 1);
}

// Enable/disable each category based on the category filters in |patterns|.
// If the category name matches one of the patterns, its enabled status is set
// to |is_included|. Otherwise its enabled status is set to !|is_included|.
static void EnableMatchingCategories(const std::vector<std::string>& patterns,
                                     unsigned char is_included) {
  for (int i = 0; i < g_category_index; i++)
    EnableMatchingCategory(i, patterns, is_included);
}

const unsigned char* TraceLog::GetCategoryEnabledInternal(const char* name) {
  AutoLock lock(lock_);
  DCHECK(!strchr(name, '"')) << "Category names may not contain double quote";

  // Search for pre-existing category matching this name
  for (int i = 0; i < g_category_index; i++) {
    if (strcmp(g_categories[i], name) == 0)
      return &g_category_enabled[i];
  }

  // Create a new category
  DCHECK(g_category_index < TRACE_EVENT_MAX_CATEGORIES) <<
      "must increase TRACE_EVENT_MAX_CATEGORIES";
  if (g_category_index < TRACE_EVENT_MAX_CATEGORIES) {
    int new_index = g_category_index++;
    g_categories[new_index] = name;
    DCHECK(!g_category_enabled[new_index]);
    if (enabled_) {
      // Note that if both included and excluded_categories are empty, the else
      // clause below excludes nothing, thereby enabling this category.
      if (!included_categories_.empty())
        EnableMatchingCategory(new_index, included_categories_, 1);
      else
        EnableMatchingCategory(new_index, excluded_categories_, 0);
    } else {
      ANNOTATE_BENIGN_RACE(&g_category_enabled[new_index],
                           "trace_event category enabled");
      g_category_enabled[new_index] = 0;
    }
    return &g_category_enabled[new_index];
  } else {
    return &g_category_enabled[g_category_categories_exhausted];
  }
}

void TraceLog::GetKnownCategories(std::vector<std::string>* categories) {
  AutoLock lock(lock_);
  for (int i = 0; i < g_category_index; i++)
    categories->push_back(g_categories[i]);
}

void TraceLog::SetEnabled(const std::vector<std::string>& included_categories,
                          const std::vector<std::string>& excluded_categories) {
  AutoLock lock(lock_);
  if (enabled_)
    return;
  logged_events_.reserve(1024);
  enabled_ = true;
  included_categories_ = included_categories;
  excluded_categories_ = excluded_categories;
  // Note that if both included and excluded_categories are empty, the else
  // clause below excludes nothing, thereby enabling all categories.
  if (!included_categories_.empty())
    EnableMatchingCategories(included_categories_, 1);
  else
    EnableMatchingCategories(excluded_categories_, 0);
}

void TraceLog::SetEnabled(const std::string& categories) {
  std::vector<std::string> included, excluded;
  // Tokenize list of categories, delimited by ','.
  StringTokenizer tokens(categories, ",");
  while (tokens.GetNext()) {
    bool is_included = true;
    std::string category = tokens.token();
    // Excluded categories start with '-'.
    if (category.at(0) == '-') {
      // Remove '-' from category string.
      category = category.substr(1);
      is_included = false;
    }
    if (is_included)
      included.push_back(category);
    else
      excluded.push_back(category);
  }
  SetEnabled(included, excluded);
}

void TraceLog::GetEnabledTraceCategories(
    std::vector<std::string>* included_out,
    std::vector<std::string>* excluded_out) {
  AutoLock lock(lock_);
  if (enabled_) {
    *included_out = included_categories_;
    *excluded_out = excluded_categories_;
  }
}

void TraceLog::SetDisabled() {
  {
    AutoLock lock(lock_);
    if (!enabled_)
      return;

    enabled_ = false;
    included_categories_.clear();
    excluded_categories_.clear();
    for (int i = 0; i < g_category_index; i++)
      g_category_enabled[i] = 0;
    AddThreadNameMetadataEvents();
    AddClockSyncMetadataEvents();
  }  // release lock
  Flush();
}

void TraceLog::SetEnabled(bool enabled) {
  if (enabled)
    SetEnabled(std::vector<std::string>(), std::vector<std::string>());
  else
    SetDisabled();
}

float TraceLog::GetBufferPercentFull() const {
  return (float)((double)logged_events_.size()/(double)kTraceEventBufferSize);
}

void TraceLog::SetOutputCallback(const TraceLog::OutputCallback& cb) {
  AutoLock lock(lock_);
  output_callback_ = cb;
}

void TraceLog::SetBufferFullCallback(const TraceLog::BufferFullCallback& cb) {
  AutoLock lock(lock_);
  buffer_full_callback_ = cb;
}

void TraceLog::Flush() {
  std::vector<TraceEvent> previous_logged_events;
  OutputCallback output_callback_copy;
  {
    AutoLock lock(lock_);
    previous_logged_events.swap(logged_events_);
    output_callback_copy = output_callback_;
  }  // release lock

  if (output_callback_copy.is_null())
    return;

  for (size_t i = 0;
       i < previous_logged_events.size();
       i += kTraceEventBatchSize) {
    scoped_refptr<RefCountedString> json_events_str_ptr =
        new RefCountedString();
    TraceEvent::AppendEventsAsJSON(previous_logged_events,
                                   i,
                                   kTraceEventBatchSize,
                                   &(json_events_str_ptr->data));
    output_callback_copy.Run(json_events_str_ptr);
  }
}

int TraceLog::AddTraceEvent(char phase,
                            const unsigned char* category_enabled,
                            const char* name,
                            unsigned long long id,
                            int num_args,
                            const char** arg_names,
                            const unsigned char* arg_types,
                            const unsigned long long* arg_values,
                            int threshold_begin_id,
                            long long threshold,
                            unsigned char flags) {
  DCHECK(name);
  TimeTicks now = TimeTicks::HighResNow();
  BufferFullCallback buffer_full_callback_copy;
  int ret_begin_id = -1;
  {
    AutoLock lock(lock_);
    if (!*category_enabled)
      return -1;
    if (logged_events_.size() >= kTraceEventBufferSize)
      return -1;

    int thread_id = static_cast<int>(PlatformThread::CurrentId());

    const char* new_name = PlatformThread::GetName();
    // Check if the thread name has been set or changed since the previous
    // call (if any), but don't bother if the new name is empty. Note this will
    // not detect a thread name change within the same char* buffer address: we
    // favor common case performance over corner case correctness.
    if (new_name != g_current_thread_name.Get().Get() &&
        new_name && *new_name) {
      g_current_thread_name.Get().Set(new_name);
      base::hash_map<int, std::string>::iterator existing_name =
          thread_names_.find(thread_id);
      if (existing_name == thread_names_.end()) {
        // This is a new thread id, and a new name.
        thread_names_[thread_id] = new_name;
      } else {
        // This is a thread id that we've seen before, but potentially with a
        // new name.
        std::vector<base::StringPiece> existing_names;
        Tokenize(existing_name->second, ",", &existing_names);
        bool found = std::find(existing_names.begin(),
                               existing_names.end(),
                               new_name) != existing_names.end();
        if (!found) {
          existing_name->second.push_back(',');
          existing_name->second.append(new_name);
        }
      }
    }

    if (threshold_begin_id > -1) {
      DCHECK(phase == TRACE_EVENT_PHASE_END);
      size_t begin_i = static_cast<size_t>(threshold_begin_id);
      // Return now if there has been a flush since the begin event was posted.
      if (begin_i >= logged_events_.size())
        return -1;
      // Determine whether to drop the begin/end pair.
      TimeDelta elapsed = now - logged_events_[begin_i].timestamp();
      if (elapsed < TimeDelta::FromMicroseconds(threshold)) {
        // Remove begin event and do not add end event.
        // This will be expensive if there have been other events in the
        // mean time (should be rare).
        logged_events_.erase(logged_events_.begin() + begin_i);
        return -1;
      }
    }
    ret_begin_id = static_cast<int>(logged_events_.size());
    logged_events_.push_back(
        TraceEvent(thread_id,
                   now, phase, category_enabled, name, id,
                   num_args, arg_names, arg_types, arg_values,
                   flags));

    if (logged_events_.size() == kTraceEventBufferSize) {
      buffer_full_callback_copy = buffer_full_callback_;
    }
  }  // release lock

  if (!buffer_full_callback_copy.is_null())
    buffer_full_callback_copy.Run();

  return ret_begin_id;
}

void TraceLog::AddTraceEventEtw(char phase,
                                const char* name,
                                const void* id,
                                const char* extra) {
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif
  INTERNAL_TRACE_EVENT_ADD(phase, "ETW Trace Event", name,
                           TRACE_EVENT_FLAG_COPY, "id", id, "extra", extra);
}

void TraceLog::AddTraceEventEtw(char phase,
                                const char* name,
                                const void* id,
                                const std::string& extra)
{
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif
  INTERNAL_TRACE_EVENT_ADD(phase, "ETW Trace Event", name,
                           TRACE_EVENT_FLAG_COPY, "id", id, "extra", extra);
}

void TraceLog::AddClockSyncMetadataEvents() {
#if defined(OS_ANDROID)
  // Since Android does not support sched_setaffinity, we cannot establish clock
  // sync unless the scheduler clock is set to global. If the trace_clock file
  // can't be read, we will assume the kernel doesn't support tracing and do
  // nothing.
  std::string clock_mode;
  if (!file_util::ReadFileToString(
          FilePath("/sys/kernel/debug/tracing/trace_clock"),
          &clock_mode))
    return;

  if (clock_mode != "local [global]\n") {
    DLOG(WARNING) <<
        "The kernel's tracing clock must be set to global in order for " <<
        "trace_event to be synchronized with . Do this by\n" <<
        "  echo global > /sys/kerel/debug/tracing/trace_clock";
    return;
  }

  // Android's kernel trace system has a trace_marker feature: this is a file on
  // debugfs that takes the written data and pushes it onto the trace
  // buffer. So, to establish clock sync, we write our monotonic clock into that
  // trace buffer.
  TimeTicks now = TimeTicks::HighResNow();

  double now_in_seconds = now.ToInternalValue() / 1000000.0;
  std::string marker =
      StringPrintf("trace_event_clock_sync: parent_ts=%f\n",
                   now_in_seconds);
  if (file_util::WriteFile(
          FilePath("/sys/kernel/debug/tracing/trace_marker"),
          marker.c_str(), marker.size()) == -1) {
    DLOG(WARNING) << "Couldn't write to /sys/kernel/debug/tracing/trace_marker";
    return;
  }
#endif
}

void TraceLog::AddThreadNameMetadataEvents() {
  lock_.AssertAcquired();
  for(base::hash_map<int, std::string>::iterator it = thread_names_.begin();
      it != thread_names_.end();
      it++) {
    if (!it->second.empty()) {
      int num_args = 1;
      const char* arg_name = "name";
      unsigned char arg_type;
      unsigned long long arg_value;
      trace_event_internal::SetTraceValue(it->second, &arg_type, &arg_value);
      logged_events_.push_back(
          TraceEvent(it->first,
                     TimeTicks(), TRACE_EVENT_PHASE_METADATA,
                     &g_category_enabled[g_category_metadata],
                     "thread_name", trace_event_internal::kNoEventId,
                     num_args, &arg_name, &arg_type, &arg_value,
                     TRACE_EVENT_FLAG_NONE));
    }
  }
}

void TraceLog::DeleteForTesting() {
  DeleteTraceLogForTesting::Delete();
}

void TraceLog::Resurrect() {
  StaticMemorySingletonTraits<TraceLog>::Resurrect();
}

void TraceLog::SetProcessID(int process_id) {
  process_id_ = process_id;
  // Create a FNV hash from the process ID for XORing.
  // See http://isthe.com/chongo/tech/comp/fnv/ for algorithm details.
  unsigned long long offset_basis = 14695981039346656037ull;
  unsigned long long fnv_prime = 1099511628211ull;
  unsigned long long pid = static_cast<unsigned long long>(process_id_);
  process_id_hash_ = (offset_basis ^ pid) * fnv_prime;
}

}  // namespace debug
}  // namespace base

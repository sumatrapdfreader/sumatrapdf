/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace dict {

struct MapSlot;

// the initial capacity (in entries) when none is given; the table grows as needed
constexpr int kMapDefaultInitialSize = 64;

// a dictionary whose keys are strings and the values are integers
// note: StrToInt would be more natural name but it's re-#define'd in <shlwapi.h>
class MapStrToInt {
  public:
    explicit MapStrToInt(int initialSize = kMapDefaultInitialSize);
    ~MapStrToInt();
    MapStrToInt(const MapStrToInt&) = delete;
    MapStrToInt& operator=(const MapStrToInt&) = delete;

    int Count() const;

    bool Insert(Str key, int val, int* existingValOut = nullptr);

    bool Remove(Str key, int* removedValOut);
    bool Get(Str key, int* valOut) const;

  private:
    MapSlot* slots = nullptr;
    int nSlots = 0; // a power of 2, 0 until the first Insert()
    int nUsed = 0;
    int firstSlots = 0;
    // the keys, back to back: a varint length, then the bytes
    char* keys = nullptr;
    u32 keysLen = 0;
    u32 keysCap = 0;

    int Find(Str key, u32 hash) const;
    bool KeyEq(u32 keyOff, Str key) const;
    u32 AppendKey(Str key);
    bool Grow();
};

} // namespace dict

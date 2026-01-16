/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBHEIF_BOX_H
#define LIBHEIF_BOX_H

#include <cstdint>
#include "common_utils.h"
#include "libheif/heif.h"
#include "libheif/heif_experimental.h"
#include "libheif/heif_properties.h"
#include "libheif/heif_tai_timestamps.h"
#include <cinttypes>
#include <cstddef>

#include <utility>
#include <vector>
#include <string>
#include <memory>
#include <limits>
#include <istream>
#include <bitset>
#include <utility>
#include <optional>

#include "error.h"
#include "logging.h"
#include "bitstream.h"

#if !defined(__EMSCRIPTEN__) && !defined(_MSC_VER)
// std::array<bool> is not supported on some older compilers.
#define HAS_BOOL_ARRAY 1
#endif

/*
  constexpr uint32_t fourcc(const char* string)
  {
  return ((string[0]<<24) |
  (string[1]<<16) |
  (string[2]<< 8) |
  (string[3]));
  }
*/


class Fraction
{
public:
  Fraction() = default;

  Fraction(int32_t num, int32_t den);

  // may only use values up to int32_t maximum
  Fraction(uint32_t num, uint32_t den);

  // Values will be reduced until they fit into int32_t.
  Fraction(int64_t num, int64_t den);

  Fraction operator+(const Fraction&) const;

  Fraction operator-(const Fraction&) const;

  Fraction operator+(int) const;

  Fraction operator-(int) const;

  Fraction operator/(int) const;

  int32_t round_down() const;

  int32_t round_up() const;

  int32_t round() const;

  bool is_valid() const;

  double to_double() const {
    return numerator / (double)denominator;
  }

  int32_t numerator = 0;
  int32_t denominator = 1;
};


inline std::ostream& operator<<(std::ostream& str, const Fraction& f)
{
  str << f.numerator << "/" << f.denominator;
  return str;
}


class BoxHeader
{
public:
  BoxHeader();

  virtual ~BoxHeader() = default;

  constexpr static uint64_t size_until_end_of_file = 0;

  uint64_t get_box_size() const { return m_size; }

  bool has_fixed_box_size() const { return m_size != 0; }

  uint32_t get_header_size() const { return m_header_size; }

  uint32_t get_short_type() const { return m_type; }

  std::vector<uint8_t> get_type() const;

  std::string get_type_string() const;

  virtual const char* debug_box_name() const { return nullptr; }

  void set_short_type(uint32_t type) { m_type = type; }


  // should only be called if get_short_type == fourcc("uuid")
  std::vector<uint8_t> get_uuid_type() const;

  void set_uuid_type(const std::vector<uint8_t>&);


  Error parse_header(BitstreamRange& range);

  virtual std::string dump(Indent&) const;


  virtual bool is_full_box_header() const { return false; }


private:
  uint64_t m_size = 0;

  uint32_t m_type = 0;
  std::vector<uint8_t> m_uuid_type;

protected:
  uint32_t m_header_size = 0;
};


enum class BoxStorageMode {
  Undefined,
  Memory,
  Parsed,
  File
};


// Consequence of a box parse error
enum class parse_error_fatality {
  fatal,     // failure to parse this box leads to the associated item being unreable
  ignorable, // ignoring this box will lead to unexpected output, but may be better than nothing
  optional   // the box contains extra information that is not essential for viewing
};


class Box : public BoxHeader
{
public:
  Box() = default;

  void set_short_header(const BoxHeader& hdr)
  {
    *(BoxHeader*) this = hdr;
  }

  // header size without the FullBox fields (if applicable)
  int calculate_header_size(bool data64bit) const;

  static Error read(BitstreamRange& range, std::shared_ptr<Box>* box, const heif_security_limits*);

  virtual Error write(StreamWriter& writer) const;

  // check, which box version is required and set this in the (full) box header
  virtual void derive_box_version() {}

  void derive_box_version_recursive();

  virtual void patch_file_pointers(StreamWriter&, size_t offset) {}

  void patch_file_pointers_recursively(StreamWriter&, size_t offset);

  std::string dump(Indent&) const override;

  template<typename T> [[nodiscard]] std::shared_ptr<T> get_child_box() const
  {
    // TODO: we could remove the dynamic_cast<> by adding the fourcc type of each Box
    //       as a "constexpr uint32_t Box::short_type", compare to that and use static_cast<>
    for (auto& box : m_children) {
      if (auto typed_box = std::dynamic_pointer_cast<T>(box)) {
        return typed_box;
      }
    }

    return nullptr;
  }

  template<typename T> bool replace_child_box(const std::shared_ptr<T>& box)
  {
    for (auto & b : m_children) {
      if (std::dynamic_pointer_cast<T>(b) != nullptr) {
        b = box;
        return true;
      }
    }

    append_child_box(box);
    return false;
  }

  template<typename T>
  std::vector<std::shared_ptr<T>> get_child_boxes() const
  {
    std::vector<std::shared_ptr<T>> result;
    for (auto& box : m_children) {
      if (auto typed_box = std::dynamic_pointer_cast<T>(box)) {
        result.push_back(typed_box);
      }
    }

    return result;
  }

  const std::vector<std::shared_ptr<Box>>& get_all_child_boxes() const { return m_children; }

  uint32_t append_child_box(const std::shared_ptr<Box>& box)
  {
    m_children.push_back(box);
    return (int) m_children.size() - 1;
  }

  bool has_child_boxes() const { return !m_children.empty(); }

  bool remove_child_box(const std::shared_ptr<const Box>& box);

  virtual bool operator==(const Box& other) const;

  static bool equal(const std::shared_ptr<Box>& box1, const std::shared_ptr<Box>& box2);

  BoxStorageMode get_box_storage_mode() const { return m_storage_mode; }

  void set_output_position(uint64_t pos) { m_output_position = pos; }

  virtual parse_error_fatality get_parse_error_fatality() const { return parse_error_fatality::fatal; }

  // Note: this function may never be called for `ispe` items since it depends
  //       on the image item type whether the `ispe` is essential.
  virtual bool is_essential() const { return m_is_essential; } // only used for properties

  void set_is_essential(bool flag) { m_is_essential = flag; }

  virtual bool is_transformative_property() const { return false; } // only used for properties

protected:
  virtual Error parse(BitstreamRange& range, const heif_security_limits* limits);

  std::vector<std::shared_ptr<Box>> m_children;

  BoxStorageMode m_storage_mode = BoxStorageMode::Undefined;

  const static uint64_t INVALID_POSITION = 0xFFFFFFFFFFFFFFFF;

  uint64_t m_input_position = INVALID_POSITION;
  uint64_t m_output_position = INVALID_POSITION;
  std::vector<uint8_t> m_box_data; // including header

  std::string m_debug_box_type;

  bool m_is_essential = false;

  const static uint32_t READ_CHILDREN_ALL = 0xFFFFFFFF;

  Error read_children(BitstreamRange& range, uint32_t number /* READ_CHILDREN_ALL */, const heif_security_limits* limits);

  Error write_children(StreamWriter& writer) const;

  std::string dump_children(Indent&, bool with_index = false) const;


  // --- writing

  virtual size_t reserve_box_header_space(StreamWriter& writer, bool data64bit = false) const;

  Error prepend_header(StreamWriter&, size_t box_start, bool data64bit = false) const;

  virtual Error write_header(StreamWriter&, size_t total_box_size, bool data64bit = false) const;
};


class FullBox : public Box
{
public:
  bool is_full_box_header() const override { return true; }

  std::string dump(Indent& indent) const override;

  void derive_box_version() override { set_version(0); }


  Error parse_full_box_header(BitstreamRange& range);

  uint8_t get_version() const { return m_version; }

  void set_version(uint8_t version) { m_version = version; }

  uint32_t get_flags() const { return m_flags; }

  void set_flags(uint32_t flags) { m_flags = flags; }

protected:

  // --- writing

  size_t reserve_box_header_space(StreamWriter& writer, bool data64bit = false) const override;

  Error write_header(StreamWriter&, size_t total_size, bool data64bit = false) const override;

  Error unsupported_version_error(const char* box) const;

private:
  uint8_t m_version = 0;
  uint32_t m_flags = 0;
};


class Box_other : public Box
{
public:
  Box_other(uint32_t short_type)
  {
    set_short_type(short_type);
  }

  const std::vector<uint8_t>& get_raw_data() const { return m_data; }

  void set_raw_data(const std::vector<uint8_t>& data) { m_data = data; }

  Error write(StreamWriter& writer) const override;

  std::string dump(Indent&) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  std::vector<uint8_t> m_data;
};



class Box_Error : public Box
{
public:
  Box_Error(uint32_t box4cc, Error err, parse_error_fatality fatality)
  {
    set_short_type(fourcc("ERR "));

    m_box_type_with_parse_error = box4cc;
    m_error = std::move(err);
    m_fatality = fatality;
  }

  Error write(StreamWriter& writer) const override { return {heif_error_Usage_error, heif_suberror_Unspecified, "Cannot write dummy error box."}; }

  std::string dump(Indent&) const override;

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override;

  [[nodiscard]] Error get_error() const { return m_error; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override { assert(false); return Error::Ok; }

  uint32_t m_box_type_with_parse_error;
  Error m_error;
  parse_error_fatality m_fatality;
};




class Box_ftyp : public Box
{
public:
  Box_ftyp()
  {
    set_short_type(fourcc("ftyp"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "File Type"; }

  bool has_compatible_brand(uint32_t brand) const;

  std::vector<uint32_t> list_brands() const { return m_compatible_brands; }

  uint32_t get_major_brand() const { return m_major_brand; }

  void set_major_brand(heif_brand2 major_brand) { m_major_brand = major_brand; }

  uint32_t get_minor_version() const { return m_minor_version; }

  void set_minor_version(uint32_t minor_version) { m_minor_version = minor_version; }

  void clear_compatible_brands() { m_compatible_brands.clear(); }

  void add_compatible_brand(heif_brand2 brand);

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint32_t m_major_brand = 0;
  uint32_t m_minor_version = 0;
  std::vector<heif_brand2> m_compatible_brands;
};


class Box_free : public Box
{
public:
  Box_free()
  {
    set_short_type(fourcc("free"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Free Space"; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_meta : public FullBox
{
public:
  Box_meta()
  {
    set_short_type(fourcc("meta"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Metadata"; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_hdlr : public FullBox
{
public:
  Box_hdlr()
  {
    set_short_type(fourcc("hdlr"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Handler Reference"; }

  uint32_t get_handler_type() const { return m_handler_type; }

  void set_handler_type(uint32_t handler) { m_handler_type = handler; }

  Error write(StreamWriter& writer) const override;

  void set_name(std::string name) { m_name = std::move(name); }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint32_t m_pre_defined = 0;
  uint32_t m_handler_type = fourcc("pict");
  uint32_t m_reserved[3] = {0,};
  std::string m_name;
};


class Box_pitm : public FullBox
{
public:
  Box_pitm()
  {
    set_short_type(fourcc("pitm"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Primary Item"; }

  heif_item_id get_item_ID() const { return m_item_ID; }

  void set_item_ID(heif_item_id id) { m_item_ID = id; }

  void derive_box_version() override;

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  heif_item_id m_item_ID = 0;
};


class Box_iloc : public FullBox
{
public:
  Box_iloc();

  ~Box_iloc() override;

  void set_use_tmp_file(bool flag);

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Item Location"; }

  struct Extent
  {
    uint64_t index = 0;
    uint64_t offset = 0;
    uint64_t length = 0;

    std::vector<uint8_t> data; // only used when writing data
  };

  struct Item
  {
    heif_item_id item_ID = 0;
    uint8_t construction_method = 0; // >= version 1
    uint16_t data_reference_index = 0;
    uint64_t base_offset = 0;

    std::vector<Extent> extents;
  };

  const std::vector<Item>& get_items() const { return m_items; }

  Error read_data(heif_item_id item,
                  const std::shared_ptr<StreamReader>& istr,
                  const std::shared_ptr<class Box_idat>&,
                  std::vector<uint8_t>* dest,
                  const heif_security_limits* limits) const;

  // Note: size==std::numeric_limits<uint64_t>::max() reads the data until the end
  Error read_data(heif_item_id item,
                  const std::shared_ptr<StreamReader>& istr,
                  const std::shared_ptr<class Box_idat>&,
                  std::vector<uint8_t>* dest,
                  uint64_t offset, uint64_t size,
                  const heif_security_limits* limits) const;

  void set_min_version(uint8_t min_version) { m_user_defined_min_version = min_version; }

  // append bitstream data that will be written later (after iloc box)
  // TODO: use an enum for the construction method
  Error append_data(heif_item_id item_ID,
                    const std::vector<uint8_t>& data,
                    uint8_t construction_method = 0);

  Error replace_data(heif_item_id item_ID,
                     uint64_t output_offset,
                     const std::vector<uint8_t>& data,
                     uint8_t construction_method);

  // append bitstream data that already has been written (before iloc box)
  // Error write_mdat_before_iloc(heif_image_id item_ID,
  //                              std::vector<uint8_t>& data)

  // reserve data entry that will be written later
  // Error reserve_mdat_item(heif_image_id item_ID,
  //                         uint8_t construction_method,
  //                         uint32_t* slot_ID);
  // void patch_mdat_slot(uint32_t slot_ID, size_t start, size_t length);

  void derive_box_version() override;

  Error write(StreamWriter& writer) const override;

  Error write_mdat_after_iloc(StreamWriter& writer);

  void append_item(Item &item) { m_items.push_back(item); }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::vector<Item> m_items;

  mutable size_t m_iloc_box_start = 0;
  uint8_t m_user_defined_min_version = 0;
  uint8_t m_offset_size = 0;
  uint8_t m_length_size = 0;
  uint8_t m_base_offset_size = 0;
  uint8_t m_index_size = 0;

  void patch_iloc_header(StreamWriter& writer) const;

  int m_idat_offset = 0; // only for writing: offset of next data array

  bool m_use_tmpfile = false;
  int m_tmpfile_fd = 0;
  char m_tmp_filename[20];
};


class Box_infe : public FullBox
{
public:
  Box_infe()
  {
    set_short_type(fourcc("infe"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Item Info Entry"; }

  bool is_hidden_item() const { return m_hidden_item; }

  void set_hidden_item(bool hidden);

  heif_item_id get_item_ID() const { return m_item_ID; }

  void set_item_ID(heif_item_id id) { m_item_ID = id; }

  uint32_t get_item_type_4cc() const { return m_item_type_4cc; }

  void set_item_type_4cc(uint32_t type) { m_item_type_4cc = type; }

  void set_item_name(const std::string& name) { m_item_name = name; }

  const std::string& get_item_name() const { return m_item_name; }

  const std::string& get_content_type() const { return m_content_type; }

  const std::string& get_content_encoding() const { return m_content_encoding; }

  void set_content_type(const std::string& content_type) { m_content_type = content_type; }

  void set_content_encoding(const std::string& content_encoding) { m_content_encoding = content_encoding; }

  void derive_box_version() override;

  Error write(StreamWriter& writer) const override;

  const std::string& get_item_uri_type() const { return m_item_uri_type; }

  void set_item_uri_type(const std::string& uritype) { m_item_uri_type = uritype; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  heif_item_id m_item_ID = 0;
  uint16_t m_item_protection_index = 0;

  uint32_t m_item_type_4cc = 0;
  std::string m_item_name;
  std::string m_content_type;
  std::string m_content_encoding;
  std::string m_item_uri_type;

  // if set, this item should not be part of the presentation (i.e. hidden)
  bool m_hidden_item = false;
};


class Box_iinf : public FullBox
{
public:
  Box_iinf()
  {
    set_short_type(fourcc("iinf"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Item Information"; }

  void derive_box_version() override;

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  //std::vector< std::shared_ptr<Box_infe> > m_iteminfos;
};


class Box_iprp : public Box
{
public:
  Box_iprp()
  {
    set_short_type(fourcc("iprp"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Item Properties"; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_ipco : public Box
{
public:
  Box_ipco()
  {
    set_short_type(fourcc("ipco"));
  }

  uint32_t find_or_append_child_box(const std::shared_ptr<Box>& box);

  Error get_properties_for_item_ID(heif_item_id itemID,
                                   const std::shared_ptr<class Box_ipma>&,
                                   std::vector<std::shared_ptr<Box>>& out_properties) const;

  std::shared_ptr<Box> get_property_for_item_ID(heif_item_id itemID,
                                                const std::shared_ptr<class Box_ipma>&,
                                                uint32_t property_box_type) const;

  bool is_property_essential_for_item(heif_item_id itemId,
                                      const std::shared_ptr<const class Box>& property,
                                      const std::shared_ptr<class Box_ipma>&) const;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Item Property Container"; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_ispe : public FullBox
{
public:
  Box_ispe()
  {
    set_short_type(fourcc("ispe"));
  }

  uint32_t get_width() const { return m_image_width; }

  uint32_t get_height() const { return m_image_height; }

  void set_size(uint32_t width, uint32_t height)
  {
    m_image_width = width;
    m_image_height = height;
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Image Spatial Extents"; }

  Error write(StreamWriter& writer) const override;

  bool operator==(const Box& other) const override;

  // Note: this depends on the image item type. Never call this for an `ispe` property.
  bool is_essential() const override { assert(false); return false; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint32_t m_image_width = 0;
  uint32_t m_image_height = 0;
};


class Box_ipma : public FullBox
{
public:
  Box_ipma()
  {
    set_short_type(fourcc("ipma"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Item Property Association"; }

  struct PropertyAssociation
  {
    bool essential;
    uint16_t property_index;
  };

  const std::vector<PropertyAssociation>* get_properties_for_item_ID(heif_item_id itemID) const;

  bool is_property_essential_for_item(heif_item_id itemId, int propertyIndex) const;

  void add_property_for_item_ID(heif_item_id itemID,
                                PropertyAssociation assoc);

  void derive_box_version() override;

  Error write(StreamWriter& writer) const override;

  void insert_entries_from_other_ipma_box(const Box_ipma& b);

  // sorts properties such that descriptive properties precede the transformative properties
  void sort_properties(const std::shared_ptr<Box_ipco>&);

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  struct Entry
  {
    heif_item_id item_ID;
    std::vector<PropertyAssociation> associations;
  };

  std::vector<Entry> m_entries;
};


class Box_auxC : public FullBox
{
public:
  Box_auxC()
  {
    set_short_type(fourcc("auxC"));
  }

  const std::string& get_aux_type() const { return m_aux_type; }

  void set_aux_type(const std::string& type) { m_aux_type = type; }

  const std::vector<uint8_t>& get_subtypes() const { return m_aux_subtypes; }

  bool is_essential() const override { return true; }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Image Properties for Auxiliary Images"; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

private:
  std::string m_aux_type;
  std::vector<uint8_t> m_aux_subtypes;
};


class Box_irot : public Box
{
public:
  Box_irot()
  {
    set_short_type(fourcc("irot"));
  }

  bool is_essential() const override { return true; }

  bool is_transformative_property() const override { return true; }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Image Rotation"; }

  int get_rotation_ccw() const { return m_rotation; }

  // Only these multiples of 90 are allowed: 0, 90, 180, 270.
  void set_rotation_ccw(int rot) { m_rotation = rot; }

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::ignorable; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

private:
  int m_rotation = 0; // in degrees (CCW)
};


class Box_imir : public Box
{
public:
  Box_imir()
  {
    set_short_type(fourcc("imir"));
  }

  bool is_essential() const override { return true; }

  bool is_transformative_property() const override { return true; }

  heif_transform_mirror_direction get_mirror_direction() const { return m_axis; }

  void set_mirror_direction(heif_transform_mirror_direction dir) { m_axis = dir; }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Image Mirroring"; }

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::ignorable; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

private:
  heif_transform_mirror_direction m_axis = heif_transform_mirror_direction_vertical;
};


class Box_clap : public Box
{
public:
  Box_clap()
  {
    set_short_type(fourcc("clap"));
  }

  bool is_essential() const override { return true; }

  bool is_transformative_property() const override { return true; }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Clean Aperture"; }

  int left_rounded(uint32_t image_width) const;  // first column
  int right_rounded(uint32_t image_width) const; // last column that is part of the cropped image
  int top_rounded(uint32_t image_height) const;   // first row
  int bottom_rounded(uint32_t image_height) const; // last row included in the cropped image

  double left(int image_width) const;
  double top(int image_height) const;

  int get_width_rounded() const;

  int get_height_rounded() const;

  void set(uint32_t clap_width, uint32_t clap_height,
           uint32_t image_width, uint32_t image_height);

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::ignorable; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

private:
  Fraction m_clean_aperture_width;
  Fraction m_clean_aperture_height;
  Fraction m_horizontal_offset;
  Fraction m_vertical_offset;
};


class Box_iref : public FullBox
{
public:
  Box_iref()
  {
    set_short_type(fourcc("iref"));
  }

  struct Reference
  {
    BoxHeader header;

    heif_item_id from_item_ID;
    std::vector<heif_item_id> to_item_ID;
  };


  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Item Reference"; }

  bool has_references(heif_item_id itemID) const;

  std::vector<heif_item_id> get_references(heif_item_id itemID, uint32_t ref_type) const;

  std::vector<Reference> get_references_from(heif_item_id itemID) const;

  void add_references(heif_item_id from_id, uint32_t type, const std::vector<heif_item_id>& to_ids);

  void overwrite_reference(heif_item_id from_id, uint32_t type, uint32_t reference_idx, heif_item_id to_item);

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

  void derive_box_version() override;

  Error check_for_double_references() const;

private:
  std::vector<Reference> m_references;
};


class Box_idat : public Box
{
public:
  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Item Data"; }

  Error read_data(const std::shared_ptr<StreamReader>& istr,
                  uint64_t start, uint64_t length,
                  std::vector<uint8_t>& out_data,
                  const heif_security_limits* limits) const;

  int append_data(const std::vector<uint8_t>& data)
  {
    auto pos = m_data_for_writing.size();

    m_data_for_writing.insert(m_data_for_writing.end(),
                              data.begin(),
                              data.end());

    return (int) pos;
  }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  std::streampos m_data_start_pos;

  std::vector<uint8_t> m_data_for_writing;
};


class Box_grpl : public Box
{
public:
  Box_grpl()
  {
    set_short_type(fourcc("grpl"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Groups List"; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_EntityToGroup : public FullBox
{
public:
  std::string dump(Indent&) const override;

  Error write(StreamWriter& writer) const override;

  void set_group_id(heif_entity_group_id id) { group_id = id; }

  heif_entity_group_id get_group_id() const { return group_id; }

  void set_item_ids(const std::vector<heif_item_id>& ids) { entity_ids = ids; }

  const std::vector<heif_item_id>& get_item_ids() const { return entity_ids; }

protected:
  heif_entity_group_id group_id = 0;
  std::vector<heif_item_id> entity_ids;

  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  void write_entity_group_ids(StreamWriter& writer) const;
};


class Box_ster : public Box_EntityToGroup
{
public:
  Box_ster()
  {
    set_short_type(fourcc("ster"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Stereo pair"; }

  heif_item_id get_left_image() const { return entity_ids[0]; }
  heif_item_id get_right_image() const { return entity_ids[1]; }

protected:

  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_pymd : public Box_EntityToGroup
{
public:
  Box_pymd()
  {
    set_short_type(fourcc("pymd"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Image pyramid group"; }

  Error write(StreamWriter& writer) const override;

  struct LayerInfo {
    uint16_t layer_binning;
    uint16_t tiles_in_layer_row_minus1;
    uint16_t tiles_in_layer_column_minus1;
  };

  void set_layers(uint16_t _tile_size_x,
                  uint16_t _tile_size_y,
                  const std::vector<LayerInfo>& layers,
                  const std::vector<heif_item_id>& layer_item_ids) // low to high resolution
  {
    set_item_ids(layer_item_ids);
    m_layer_infos = layers;
    tile_size_x = _tile_size_x;
    tile_size_y = _tile_size_y;
  }

  const std::vector<LayerInfo>& get_layers() const { return m_layer_infos; }

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::ignorable; }

protected:
  uint16_t tile_size_x = 0;
  uint16_t tile_size_y = 0;

  std::vector<LayerInfo> m_layer_infos;

  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};




class Box_dinf : public Box
{
public:
  Box_dinf()
  {
    set_short_type(fourcc("dinf"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Data Information"; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_dref : public FullBox
{
public:
  Box_dref()
  {
    set_short_type(fourcc("dref"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Data Reference"; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_url : public FullBox
{
public:
  Box_url()
  {
    set_short_type(fourcc("url "));
    set_flags(1);
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Data Entry URL"; }

  bool is_same_file() const { return m_location.empty(); }

  void set_location(const std::string& loc) { m_location = loc; set_flags(m_location.empty() ? 1 : 0); }

  void set_location_same_file() { m_location.clear(); set_flags(1); }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  std::string m_location;
};

class Box_pixi : public FullBox
{
public:
  Box_pixi()
  {
    set_short_type(fourcc("pixi"));
  }

  int get_num_channels() const { return (int) m_bits_per_channel.size(); }

  int get_bits_per_channel(int channel) const { return m_bits_per_channel[channel]; }

  void add_channel_bits(uint8_t c)
  {
    m_bits_per_channel.push_back(c);
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Pixel Information"; }

  Error write(StreamWriter& writer) const override;

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::vector<uint8_t> m_bits_per_channel;
};


class Box_pasp : public Box
{
public:
  Box_pasp()
  {
    set_short_type(fourcc("pasp"));
  }

  uint32_t hSpacing = 1;
  uint32_t vSpacing = 1;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Pixel Aspect Ratio"; }

  Error write(StreamWriter& writer) const override;

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_lsel : public Box
{
public:
  Box_lsel()
  {
    set_short_type(fourcc("lsel"));
  }

  uint16_t layer_id = 0;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Layer Selection"; }

  Error write(StreamWriter& writer) const override;

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_clli : public Box
{
public:
  Box_clli()
  {
    set_short_type(fourcc("clli"));

    clli.max_content_light_level = 0;
    clli.max_pic_average_light_level = 0;
  }

  heif_content_light_level clli;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Content Light Level Information"; }

  Error write(StreamWriter& writer) const override;

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_mdcv : public Box
{
public:
  Box_mdcv();

  heif_mastering_display_colour_volume mdcv;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Master Display Colour Volume"; }

  Error write(StreamWriter& writer) const override;

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_amve : public Box
{
public:
  Box_amve();

  heif_ambient_viewing_environment amve;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Ambient Viewing Environment"; }

  Error write(StreamWriter& writer) const override;

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


class Box_cclv : public Box
{
public:
  Box_cclv();

  bool ccv_primaries_are_valid() const { return m_ccv_primaries_valid; }
  int32_t get_ccv_primary_x0() const { return m_ccv_primaries_x[0]; }
  int32_t get_ccv_primary_y0() const { return m_ccv_primaries_y[0]; }
  int32_t get_ccv_primary_x1() const { return m_ccv_primaries_x[1]; }
  int32_t get_ccv_primary_y1() const { return m_ccv_primaries_y[1]; }
  int32_t get_ccv_primary_x2() const { return m_ccv_primaries_x[2]; }
  int32_t get_ccv_primary_y2() const { return m_ccv_primaries_y[2]; }
  void set_primaries(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2);

  bool min_luminance_is_valid() const { return m_ccv_min_luminance_value.has_value(); }
  uint32_t get_min_luminance() const { return *m_ccv_min_luminance_value; }
  void set_min_luminance(uint32_t luminance) { m_ccv_min_luminance_value = luminance; }

  bool max_luminance_is_valid() const { return m_ccv_max_luminance_value.has_value(); }
  uint32_t get_max_luminance() const { return *m_ccv_max_luminance_value; }
  void set_max_luminance(uint32_t luminance) { m_ccv_max_luminance_value = luminance; }

  bool avg_luminance_is_valid() const { return m_ccv_avg_luminance_value.has_value(); }
  uint32_t get_avg_luminance() const { return *m_ccv_avg_luminance_value; }
  void set_avg_luminance(uint32_t luminance) { m_ccv_avg_luminance_value = luminance; }

  std::string dump(Indent&) const override;

  // TODO const char* debug_box_name() const override { return ""; }

  Error write(StreamWriter& writer) const override;

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  bool m_ccv_primaries_valid = false;
  int32_t m_ccv_primaries_x[3] {};
  int32_t m_ccv_primaries_y[3] {};

  std::optional<uint32_t> m_ccv_min_luminance_value;
  std::optional<uint32_t> m_ccv_max_luminance_value;
  std::optional<uint32_t> m_ccv_avg_luminance_value;
};


class Box_cmin : public FullBox
{
public:
  Box_cmin()
  {
    set_short_type(fourcc("cmin"));
  }

  struct AbsoluteIntrinsicMatrix;

  struct RelativeIntrinsicMatrix
  {
    double focal_length_x = 0;
    double principal_point_x = 0;
    double principal_point_y = 0;

    bool is_anisotropic = false;
    double focal_length_y = 0;
    double skew = 0;

    void compute_focal_length(int image_width, int image_height,
                              double& out_focal_length_x, double& out_focal_length_y) const;

    void compute_principal_point(int image_width, int image_height,
                                 double& out_principal_point_x, double& out_principal_point_y) const;

    struct AbsoluteIntrinsicMatrix to_absolute(int image_width, int image_height) const;
  };

  struct AbsoluteIntrinsicMatrix
  {
    double focal_length_x;
    double focal_length_y;
    double principal_point_x;
    double principal_point_y;
    double skew = 0;

    void apply_clap(const Box_clap* clap, int image_width, int image_height) {
      principal_point_x -= clap->left(image_width);
      principal_point_y -= clap->top(image_height);
    }

    void apply_imir(const Box_imir* imir, uint32_t image_width, uint32_t image_height) {
      switch (imir->get_mirror_direction()) {
        case heif_transform_mirror_direction_horizontal:
          focal_length_x *= -1;
          skew *= -1;
          principal_point_x = image_width - 1 - principal_point_x;
          break;
        case heif_transform_mirror_direction_vertical:
          focal_length_y *= -1;
          principal_point_y = image_height - 1 - principal_point_y;
          break;
        case heif_transform_mirror_direction_invalid:
          break;
      }
    }
  };

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Camera Intrinsic Matrix"; }

  RelativeIntrinsicMatrix get_intrinsic_matrix() const { return m_matrix; }

  void set_intrinsic_matrix(RelativeIntrinsicMatrix matrix);

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

private:
  RelativeIntrinsicMatrix m_matrix;

  uint32_t m_denominatorShift = 0;
  uint32_t m_skewDenominatorShift = 0;
};


class Box_cmex : public FullBox
{
public:
  Box_cmex()
  {
    set_short_type(fourcc("cmex"));
  }

  struct ExtrinsicMatrix
  {
    // in micrometers (um)
    int32_t pos_x = 0;
    int32_t pos_y = 0;
    int32_t pos_z = 0;

    bool rotation_as_quaternions = true;
    bool orientation_is_32bit = false;

    double quaternion_x = 0;
    double quaternion_y = 0;
    double quaternion_z = 0;
    double quaternion_w = 1.0;

    // rotation angle in degrees
    double rotation_yaw = 0;   //  [-180 ; 180)
    double rotation_pitch = 0; //  [-90 ; 90]
    double rotation_roll = 0;  //  [-180 ; 180)

    uint32_t world_coordinate_system_id = 0;

    // Returns rotation matrix in row-major order.
    std::array<double,9> calculate_rotation_matrix() const;
  };

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Camera Extrinsic Matrix"; }

  ExtrinsicMatrix get_extrinsic_matrix() const { return m_matrix; }

  Error set_extrinsic_matrix(ExtrinsicMatrix matrix);

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

private:
  ExtrinsicMatrix m_matrix;

  bool m_has_pos_x = false;
  bool m_has_pos_y = false;
  bool m_has_pos_z = false;
  bool m_has_orientation = false;
  bool m_has_world_coordinate_system_id = false;

  enum Flags
  {
    pos_x_present = 0x01,
    pos_y_present = 0x02,
    pos_z_present = 0x04,
    orientation_present = 0x08,
    rot_large_field_size = 0x10,
    id_present = 0x20
  };
};




/**
 * User Description property.
 *
 * Permits the association of items or entity groups with a user-defined name, description and tags;
 * there may be multiple udes properties, each with a different language code.
 *
 * See ISO/IEC 23008-12:2022(E) Section 6.5.20.
 */
class Box_udes : public FullBox
{
public:
  Box_udes()
  {
    set_short_type(fourcc("udes"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "User Description"; }

  Error write(StreamWriter& writer) const override;

  /**
   * Language tag.
   *
   * An RFC 5646 compliant language identifier for the language of the text contained in the other properties.
   * Examples: "en-AU", "de-DE", or "zh-CN“.
   * When is empty, the language is unknown or not undefined.
   */
  std::string get_lang() const { return m_lang; }

  /**
   * Set the language tag.
   *
   * An RFC 5646 compliant language identifier for the language of the text contained in the other properties.
   * Examples: "en-AU", "de-DE", or "zh-CN“.
   */
  void set_lang(const std::string lang) { m_lang = lang; }

  /**
   * Name.
   *
   * Human readable name for the item or group being described.
   * May be empty, indicating no name is applicable.
   */
  std::string get_name() const { return m_name; }

  /**
  * Set the name.
  *
  * Human readable name for the item or group being described.
  */
  void set_name(const std::string name) { m_name = name; }

  /**
   * Description.
   *
   * Human readable description for the item or group.
   * May be empty, indicating no description has been provided.
   */
  std::string get_description() const { return m_description; }

  /**
   * Set the description.
   *
   * Human readable description for the item or group.
   */
  void set_description(const std::string description) { m_description = description; }

  /**
   * Tags.
   *
   * Comma separated user defined tags applicable to the item or group.
   * May be empty, indicating no tags have been assigned.
   */
  std::string get_tags() const { return m_tags; }

  /**
   * Set the tags.
   *
   * Comma separated user defined tags applicable to the item or group.
   */
  void set_tags(const std::string tags) { m_tags = tags; }

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::string m_lang;
  std::string m_name;
  std::string m_description;
  std::string m_tags;
};


void initialize_heif_tai_clock_info(heif_tai_clock_info* taic);
void initialize_heif_tai_timestamp_packet(heif_tai_timestamp_packet* itai);


class Box_taic : public FullBox
{
public:
  Box_taic()
  {
    set_short_type(fourcc("taic"));
    initialize_heif_tai_clock_info(&m_info);
  }

  static std::string dump(const heif_tai_clock_info& info, Indent&);

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "TAI Clock Information"; }

  Error write(StreamWriter& writer) const override;

  /**
   * time_uncertainty.
   * 
   * The standard deviation measurement uncertainty in nanoseconds
   * for the timestamp generation process. 
   */
  void set_time_uncertainty(uint64_t time_uncertainty) { m_info.time_uncertainty = time_uncertainty;}
  
  /**
   * clock_resolution.
   * 
   * Specifies the resolution of the receptor clock in nanoseconds.
   * For example, a microsecond clock has a clock_resolution of 1000.
   */
  void set_clock_resolution(uint32_t clock_resolution) { m_info.clock_resolution = clock_resolution; }
  
  /**
   * clock_drift_rate.
   * 
   * The difference between the synchronized and unsynchronized
   * time, over a period of one second. 
   */
  void set_clock_drift_rate(int32_t clock_drift_rate) { m_info.clock_drift_rate = clock_drift_rate; }
  
  /**
   * clock_type.
   * 
   * 0 = Clock type is unknown
   * 1 = The clock does not synchronize to an atomic source of absolute TAI time
   * 2 = The clock can synchronize to an atomic source of absolute TAI time
   */
  void set_clock_type(uint8_t clock_type) { m_info.clock_type = clock_type; }

  uint64_t get_time_uncertainty() const { return m_info.time_uncertainty; }
  
  uint32_t get_clock_resolution() const { return m_info.clock_resolution; }
  
  int32_t get_clock_drift_rate() const { return m_info.clock_drift_rate; }
  
  uint8_t get_clock_type() const { return m_info.clock_type; }

  void set_from_tai_clock_info(const heif_tai_clock_info* info) {
    heif_tai_clock_info_copy(&m_info, info);
  }

  const heif_tai_clock_info* get_tai_clock_info() const
  {
    return &m_info;
  }

  bool operator==(const Box& other) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  heif_tai_clock_info m_info;
};

bool operator==(const heif_tai_clock_info& a,
                const heif_tai_clock_info& b);


class Box_itai : public FullBox
{
public:
  Box_itai()
  {
    set_short_type(fourcc("itai"));
    initialize_heif_tai_timestamp_packet(&m_timestamp);
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Item TAI Timestamp"; }

  Error write(StreamWriter& writer) const override;

  static std::vector<uint8_t> encode_tai_to_bitstream(const heif_tai_timestamp_packet*);

  static Result<heif_tai_timestamp_packet> decode_tai_from_vector(const std::vector<uint8_t>&);

  /**
   * The number of nanoseconds since the TAI epoch of 1958-01-01T00:00:00.0Z.
   */
  void set_tai_timestamp(uint64_t timestamp) { m_timestamp.tai_timestamp = timestamp; }

  /**
  * synchronization_state (0=unsynchronized, 1=synchronized)
  */
  void set_synchronization_state(bool state) { m_timestamp.synchronization_state = state; }

  /**
  * timestamp_generation_failure (0=generated, 1=failed)
  */
  void set_timestamp_generation_failure(bool failure) { m_timestamp.timestamp_generation_failure = failure; }

  /**
   * timestamp_is_modified (0=original 1=modified)
   */
  void set_timestamp_is_modified(bool is_modified) { m_timestamp.timestamp_is_modified = is_modified; }

  uint64_t get_tai_timestamp() const { return m_timestamp.tai_timestamp; }

  bool get_synchronization_state() const { return m_timestamp.synchronization_state; }

  bool get_timestamp_generation_failure() const { return m_timestamp.timestamp_generation_failure; }

  bool get_timestamp_is_modified() const { return m_timestamp.timestamp_is_modified; }

  void set_from_tai_timestamp_packet(const heif_tai_timestamp_packet* tai) {
    heif_tai_timestamp_packet_copy(&m_timestamp, tai);
  }

  const heif_tai_timestamp_packet* get_tai_timestamp_packet() const
  {
    return &m_timestamp;
  }

  bool operator==(const Box& other) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  heif_tai_timestamp_packet m_timestamp;
};

class Box_gimi_content_id : public Box
{
public:
  Box_gimi_content_id()
  {
    set_uuid_type(std::vector<uint8_t>{0x26, 0x1e, 0xf3, 0x74, 0x1d, 0x97, 0x5b, 0xba, 0xac, 0xbd, 0x9d, 0x2c, 0x8e, 0xa7, 0x35, 0x22});
  }

  bool is_essential() const override { return false; }

  bool is_transformative_property() const override { return false; }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "GIMI Content ID"; }

  std::string get_content_id() const { return m_content_id; }

  void set_content_id(const std::string& id) { m_content_id = id; }

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::ignorable; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

private:
  std::string m_content_id;
};


bool operator==(const heif_tai_timestamp_packet& a,
                const heif_tai_timestamp_packet& b);


/**
 * Extended language property.
 *
 * Permits the association of language information with an item.
 *
 * See ISO/IEC 23008-12:2025(E) Section 6.10.2.2 and ISO/IEC 14496-12:2022(E) Section 8.4.6.
 */
class Box_elng : public FullBox
{
public:
  Box_elng()
  {
    set_short_type(fourcc("elng"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Extended language"; }

  Error write(StreamWriter& writer) const override;

  /**
   * Language.
   *
   * An RFC 5646 (IETF BCP 47) compliant language identifier for the language of the text.
   * Examples: "en-AU", "de-DE", or "zh-CN“.
   */
  std::string get_extended_language() const { return m_lang; }

  /**
   * Set the language.
   *
   * An RFC 5646 (IETF BCP 47) compliant language identifier for the language of the text.
   * Examples: "en-AU", "de-DE", or "zh-CN“.
   */
  void set_lang(const std::string lang) { m_lang = lang; }

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::string m_lang;
};

#endif

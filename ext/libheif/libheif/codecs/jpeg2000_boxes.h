/*
 * HEIF JPEG 2000 codec.
 * Copyright (c) 2023 Brad Hards <bradh@frogmouth.net>
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

#ifndef LIBHEIF_JPEG2000_BOXES_H
#define LIBHEIF_JPEG2000_BOXES_H

#include "box.h"
#include "file.h"
#include "context.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <utility>

/**
 * JPEG 2000 Channel Definition box.
 *
 * This is defined in ITU-800 / IEC 15444-1.
 *
 * The Channel Definition box provides the type and ordering of the components
 * within the codestream. The mapping between actual components from the
 * codestream to channels is specified in the Component Mapping box. If the
 * header does not contain a Component Mapping box, then a reader shall map
 * component
 * @code{i} to channel @code{i}, for all components in the codestream.
 *
 * This box contains an array of channel descriptions. For each description,
 * three values are specified: the index of the channel described by that
 * association, the type of that channel, and the association of that channel
 * with particular colours. This box may specify multiple descriptions for a
 * single channel. If a multiple component transform is specified within the
 * codestream, the image must be in an RGB colourspace and the red, green and
 * blue colours as channels 0, 1 and 2 in the codestream, respectively.
 *
 * This box is required within the JPEG 2000 header item property (`j2kH`).
 */
class Box_cdef : public Box {
public:
    Box_cdef() { set_short_type(fourcc("cdef")); }

    std::string dump(Indent &) const override;

    Error write(StreamWriter &writer) const override;

    struct Channel {
        /**
        * Channel index (`Cn`).
        *
        * This field specifies the index of the channel for this description.
        * The value of this field represents the index of the channel as
        * defined within the Component Mapping box (or the actual component
        * from the codestream if the file does not contain a Component Mapping
        * box).
        */
        uint16_t channel_index;
        /**
        * Channel type (`Typ`).
        *
        * Each channel type value shall be equal to 0 (colour), 1 (alpha)
        * or 2 (pre-multiplied alpha).
        *
        * At most one channel type value shall be equal to 1 or 2 (i.e. a
        * single alpha channel), and the corresponding association field
        * value shall be 0.
        */
        uint16_t channel_type;
        /**
        * Channel association (`Asoc`).
        *
        * If the channel type value is 0 (colour), then the channel association
        * value shall be in the range [1, 2<sup>16</sup> -2].
        * Interpretation of this depends on the colourspace.
        */
        uint16_t channel_association;
    };

    /**
    * Get the channels in this channel definition box.
    *
    * @return the channels as a read-only vector.
    */
    const std::vector<Channel> &get_channels() const { return m_channels; }

    /**
    * Add a channel to this channel definition box.
    *
    * @param channel the channel to add
    */
    void add_channel(Channel channel) { m_channels.push_back(channel); }

    void set_channels(heif_colorspace colorspace);

protected:
    Error parse(BitstreamRange &range, const heif_security_limits* limits) override;

private:
    std::vector<Channel> m_channels;
};

/**
 * JPEG 2000 Component Mapping box.
 *
 * The Component Mapping box defines how image channels are identified from the
 * actual components decoded from the codestream. This abstraction allows a
 * single structure (the Channel Definition box) to specify the colour or type
 * of both palettised images and non-palettised images. This box contains an
 * array of CMP<sup>i</sup>, MTYP<sup>i</sup> and PCOL<sup>i</sup> fields. Each
 * group of these fields represents the definition of one channel in the image.
 * The channels are numbered in order starting with zero, and the number of
 * channels specified in the Component Mapping box is determined by the length
 * of the box.
 *
 * There shall be at most one Component Mapping box inside a JP2 Header box.
 * If the JP2 Header box contains a Palette box, then the JP2 Header box
 * shall also contain a Component Mapping box. If the JP2 Header box does
 * not contain a Component Mapping box, the components shall be mapped
 * directly to channels, such that component @code{i} is mapped to channel
 * @code {i}.
 */
class Box_cmap : public Box
{
public:
    Box_cmap() { set_short_type(fourcc("cmap")); }

    std::string dump(Indent &) const override;

    Error write(StreamWriter &writer) const override;

    struct Component
    {
        uint16_t component_index;
        uint8_t mapping_type;
        uint8_t palette_colour;
    };

    /**
    * Get the components in this component mapping box.
    *
    * @return the components as a read-only vector.
    */
    const std::vector<Component> &get_components() const { return m_components; }

    /**
    * Add a component to this component mapping box.
    *
    * @param component the component to add
    */
    void add_component(Component component) { m_components.push_back(component); }

protected:
    Error parse(BitstreamRange &range, const heif_security_limits* limits) override;

private:
      std::vector<Component> m_components;
};


/**
 * JPEG 2000 Palette box.
 *
 * This box defines the palette to be used to create multiple components
 * from a single component.
 */
class Box_pclr : public Box
{
public:
    Box_pclr()
    {
        set_short_type(fourcc("pclr"));
    }

    std::string dump(Indent &) const override;

    Error write(StreamWriter &writer) const override;

    struct PaletteEntry
    {
        // JPEG 2000 supports 38 bits and signed/unsigned, but we only
        // do up to 16 bit unsigned.
        std::vector<uint16_t> columns;
    };

    /**
     * Get the entries in this palette box.
     *
     * @return the entries as a read-only vector.
     */
    const std::vector<PaletteEntry>& get_entries() const
    {
        return m_entries;
    }

    /**
     * Get the bit depths for the columns in this palette box.
     *
     * @return the bit depths as a read-only vector.
     */
    const std::vector<uint8_t>& get_bit_depths() const
    {
        return m_bitDepths;
    }

    const uint8_t get_num_entries() const
    {
        return (uint8_t)(m_entries.size());
    }

    const uint8_t get_num_columns() const
    {
        return (uint8_t)(m_bitDepths.size());
    }

    void add_entry(const PaletteEntry entry)
    {
        m_entries.push_back(entry);
    }

    /**
     * Set columns for the palette mapping.
     *
     * Each column has the same bit depth.
     *
     * This will reset any existing columns and entries.
     *
     * @param num_columns the number of columns (e.g. 3 for RGB)
     * @param bit_depth the bit depth for each column (e.g. 8 for 24-bit RGB)
     */
    void set_columns(uint8_t num_columns, uint8_t bit_depth);

protected:
    Error parse(BitstreamRange &range, const heif_security_limits* limits) override;

private:
    std::vector<uint8_t> m_bitDepths;
    std::vector<PaletteEntry> m_entries;
};


/**
 * JPEG 2000 layers box.
 *
 * The JPEG 2000 layers box declares a list of quality and resolution layers
 * for a JPEG 2000 codestream.
 *
 * @note the JPEG 2000 codestream can contain layers not listed in the JPEG 2000
 * layers box.
 */
class Box_j2kL : public FullBox
{
public:
    Box_j2kL() { set_short_type(fourcc("j2kL")); }

    std::string dump(Indent &) const override;

    Error write(StreamWriter &writer) const override;

    struct Layer {
        /**
        * Unique identifier for the layer.
        */
        uint16_t layer_id;
        /**
        * Number of resolution levels of the codestream that can be discarded.
        */
        uint8_t discard_levels;
        /**
        * Minimum number of quality layers of the codestream to be decoded.
        */
        uint16_t decode_layers;
    };

    /**
    * Get the layers in this layer box.
    *
    * @return the layers as a read-only vector.
    */
    const std::vector<Layer> &get_layers() const { return m_layers; }

    /**
    * Add a layer to the layers box.
    *
    * @param layer the layer to add
    */
    void add_layer(Layer layer) { m_layers.push_back(layer); }

protected:
    Error parse(BitstreamRange &range, const heif_security_limits* limits) override;

private:
    std::vector<Layer> m_layers;
};

class Box_j2kH : public Box
{
public:
    Box_j2kH() { set_short_type(fourcc("j2kH")); }

    bool is_essential() const override { return true; }

    std::string dump(Indent &) const override;

    // Default write behaviour for a container is to write children

protected:
    Error parse(BitstreamRange &range, const heif_security_limits* limits) override;
};

class Jpeg2000ImageCodec
{
public:

//   static Error decode_jpeg2000_image(const HeifContext* context,
//                                      heif_item_id ID,
//                                      std::shared_ptr<HeifPixelImage>& img,
//                                      const std::vector<uint8_t>& uncompressed_data);

  static Error encode_jpeg2000_image(const std::shared_ptr<HeifFile>& heif_file,
                                     const std::shared_ptr<HeifPixelImage>& src_image,
                                     void* encoder_struct,
                                     const struct heif_encoding_options& options,
                                     std::shared_ptr<ImageItem>& out_image);
};

struct JPEG2000_SIZ_segment
{
    /**
     * Decoder capabilities bitmap (Rsiz).
     */
    uint16_t decoder_capabilities = 0;
    /**
     * Width of the reference grid (Xsiz).
     */
    uint32_t reference_grid_width = 0;
    /**
     * Height of the reference grid (Ysiz).
     */
    uint32_t reference_grid_height = 0;
    /**
     * Horizontal offset from reference grid origin to image left side (XOsiz).
     */
    uint32_t image_horizontal_offset = 0;
    /**
     * Vertical offset from reference grid origin to image top size (YOsiz).
     */
    uint32_t image_vertical_offset = 0;
    /**
     * Width of one reference tile with respect to the reference grid (XTsiz).
     */
    uint32_t tile_width = 0;
    /**
     * Height of one reference tile with respect to the reference grid (YTsiz).
     */
    uint32_t tile_height = 0;
    /**
     * Horizontal offset from the origin of the reference grid to left side of first tile (XTOsiz).
     */
    uint32_t tile_offset_x = 0;
    /**
     * Vertical offset from the origin of the reference grid to top side of first tile (YTOsiz).
     */
    uint32_t tile_offset_y = 0;

    struct component
    {
        uint8_t h_separation, v_separation;
        uint8_t precision;
        bool is_signed;
    };

    std::vector<component> components;
};

class JPEG2000_Extension_Capability {
public:
    JPEG2000_Extension_Capability(const uint8_t i) : ident(i) {}

    uint8_t getIdent() const {
        return ident;
    }

    uint16_t getValue() const {
        return value;
    }

    void setValue(uint16_t val) {
        value = val;
    }

private:
    const uint8_t ident;
    uint16_t value = 0;
};

class JPEG2000_Extension_Capability_HT : public JPEG2000_Extension_Capability
{
public:
    static const int IDENT = 15;
    JPEG2000_Extension_Capability_HT(): JPEG2000_Extension_Capability(IDENT)
    {};
};

class JPEG2000_CAP_segment
{
public:
    void push_back(JPEG2000_Extension_Capability ccap) {
        extensions.push_back(ccap);
    }
    bool hasHighThroughputExtension() const {
        for (auto &extension : extensions) {
            if (extension.getIdent() == 15) {
                return true;
            }
        }
        return false;
    }
private:
    std::vector<JPEG2000_Extension_Capability> extensions;

};

struct JPEG2000MainHeader
{
public:
    JPEG2000MainHeader() = default;

    Error parseHeader(const std::vector<uint8_t>& compressedImageData);

    // Use parseHeader instead - these are mainly for unit testing
    Error doParse();
    void setHeaderData(std::vector<uint8_t> data)
    {
        headerData = std::move(data);
    }

    heif_chroma get_chroma_format() const;

    int get_precision(uint32_t index) const
    {
        if (index >= siz.components.size()) {
            return -1;
        }
        // TODO: this is a quick hack. It is more complicated for JPEG2000 because these can be any kind of colorspace (e.g. RGB).
        return siz.components[index].precision;
    }

    bool hasHighThroughputExtension() {
        return cap.hasHighThroughputExtension();
    }

    uint32_t getXSize() const
    {
        return siz.reference_grid_width;
    }

    uint32_t getYSize() const
    {
        return siz.reference_grid_height;
    }

    const JPEG2000_SIZ_segment get_SIZ()
    {
        return siz;
    }

private:
    Error parse_SOC_segment();
    Error parse_SIZ_segment();
    Error parse_CAP_segment_body();
    void parse_Ccap15();

    static const int MARKER_LEN = 2;

    JPEG2000_SIZ_segment siz;
    JPEG2000_CAP_segment cap;

    uint8_t read8()
    {
        uint8_t res = headerData[cursor];
        cursor += 1;
        return res;
    }

    uint16_t read16()
    {
        uint16_t res = (uint16_t)((headerData[cursor] << 8) | headerData[cursor + 1]);
        cursor += 2;
        return res;
    }

    uint32_t read32()
    {
        uint32_t res = (headerData[cursor] << 24) | (headerData[cursor + 1] << 16) | (headerData[cursor + 2] << 8) | headerData[cursor + 3];
        cursor += 4;
        return res;
    }

    std::vector<uint8_t> headerData;
    size_t cursor = 0;
};


class Box_j2ki : public Box_VisualSampleEntry
{
public:
  Box_j2ki()
  {
    set_short_type(fourcc("j2ki"));
  }
};

#endif // LIBHEIF_JPEG2000_BOXES_H

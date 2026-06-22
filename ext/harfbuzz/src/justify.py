#!/usr/bin/env python3

import gi

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, HarfBuzz as hb


POOL = {}


def move_to_f(funcs, draw_data, st, to_x, to_y, user_data):
    context = POOL[draw_data]
    context.move_to(to_x, to_y)


def line_to_f(funcs, draw_data, st, to_x, to_y, user_data):
    context = POOL[draw_data]
    context.line_to(to_x, to_y)


def cubic_to_f(
    funcs,
    draw_data,
    st,
    control1_x,
    control1_y,
    control2_x,
    control2_y,
    to_x,
    to_y,
    user_data,
):
    context = POOL[draw_data]
    context.curve_to(control1_x, control1_y, control2_x, control2_y, to_x, to_y)


def close_path_f(funcs, draw_data, st, user_data):
    context = POOL[draw_data]
    context.close_path()


DFUNCS = hb.draw_funcs_create()
hb.draw_funcs_set_move_to_func(DFUNCS, move_to_f, None)
hb.draw_funcs_set_line_to_func(DFUNCS, line_to_f, None)
hb.draw_funcs_set_cubic_to_func(DFUNCS, cubic_to_f, None)
hb.draw_funcs_set_close_path_func(DFUNCS, close_path_f, None)


def push_transform_f(funcs, paint_data, xx, yx, xy, yy, dx, dy, user_data):
    raise NotImplementedError


def pop_transform_f(funcs, paint_data, user_data):
    raise NotImplementedError


def color_f(funcs, paint_data, is_foreground, color, user_data):
    context = POOL[paint_data]
    r = hb.color_get_red(color) / 255
    g = hb.color_get_green(color) / 255
    b = hb.color_get_blue(color) / 255
    a = hb.color_get_alpha(color) / 255
    context.set_source_rgba(r, g, b, a)
    context.paint()


def push_clip_rectangle_f(funcs, paint_data, xmin, ymin, xmax, ymax, user_data):
    context = POOL[paint_data]
    context.save()
    context.rectangle(xmin, ymin, xmax, ymax)
    context.clip()


def push_clip_glyph_f(funcs, paint_data, glyph, font, user_data):
    context = POOL[paint_data]
    context.save()
    context.new_path()
    hb.font_draw_glyph(font, glyph, DFUNCS, paint_data)
    context.close_path()
    context.clip()


def pop_clip_f(funcs, paint_data, user_data):
    context = POOL[paint_data]
    context.restore()


def push_group_f(funcs, paint_data, user_data):
    raise NotImplementedError


def pop_group_f(funcs, paint_data, mode, user_data):
    raise NotImplementedError


PFUNCS = hb.paint_funcs_create()
hb.paint_funcs_set_push_transform_func(PFUNCS, push_transform_f, None)
hb.paint_funcs_set_pop_transform_func(PFUNCS, pop_transform_f, None)
hb.paint_funcs_set_color_func(PFUNCS, color_f, None)
hb.paint_funcs_set_push_clip_glyph_func(PFUNCS, push_clip_glyph_f, None)
hb.paint_funcs_set_push_clip_rectangle_func(PFUNCS, push_clip_rectangle_f, None)
hb.paint_funcs_set_pop_clip_func(PFUNCS, pop_clip_f, None)
hb.paint_funcs_set_push_group_func(PFUNCS, push_group_f, None)
hb.paint_funcs_set_pop_group_func(PFUNCS, pop_group_f, None)


def makebuffer(words):
    buf = hb.buffer_create()

    text = " ".join(words)
    hb.buffer_add_codepoints(buf, [ord(c) for c in text], 0, len(text))

    hb.buffer_guess_segment_properties(buf)

    return buf


def justify(face, words, advance, target_advance):
    font = hb.font_create(face)
    buf = makebuffer(words)

    wiggle = 5
    shrink = target_advance - wiggle < advance
    expand = target_advance + wiggle > advance

    ret, advance, tag, value = hb.shape_justify(
        font,
        buf,
        None,
        None,
        target_advance,
        target_advance,
        advance,
    )

    if not ret:
        return False, buf, None

    if tag:
        variation = hb.variation_t()
        variation.tag = tag
        variation.value = value
    else:
        variation = None

    if shrink and advance > target_advance + wiggle:
        return False, buf, variation
    if expand and advance < target_advance - wiggle:
        return False, buf, variation

    return True, buf, variation


def shape(face, words):
    font = hb.font_create(face)
    buf = makebuffer(words)
    hb.shape(font, buf)
    positions = hb.buffer_get_glyph_positions(buf)
    advance = sum(p.x_advance for p in positions)
    return buf, advance


def typeset(face, text, target_advance):
    lines = []
    words = []
    for word in text.split():
        words.append(word)
        buf, advance = shape(face, words)
        if advance > target_advance:
            # Shrink
            ret, buf, variation = justify(face, words, advance, target_advance)
            if ret:
                lines.append((buf, variation))
                words = []
            # If if fails, pop the last word and shrink, and hope for the best.
            # A too short line is better than too long.
            elif len(words) > 1:
                words.pop()
                _, buf, variation = justify(face, words, advance, target_advance)
                lines.append((buf, variation))
                words = [word]
            # But if it is one word, meh.
            else:
                lines.append((buf, variation))
                words = []

    # Justify last line
    if words:
        _, buf, variation = justify(face, words, advance, target_advance)
        lines.append((buf, variation))

    return lines


def render(face, text, context, width, height, fontsize):
    font = hb.font_create(face)

    margin = fontsize * 2
    scale = fontsize / hb.face_get_upem(face)
    target_advance = (width - (margin * 2)) / scale

    lines = typeset(face, text, target_advance)

    _, extents = hb.font_get_h_extents(font)
    lineheight = extents.ascender - extents.descender + extents.line_gap
    lineheight *= scale

    context.save()
    context.translate(0, margin)
    context.set_font_size(12)
    context.set_source_rgb(1, 0, 0)
    for buf, variation in lines:
        rtl = hb.buffer_get_direction(buf) == hb.direction_t.RTL
        if rtl:
            hb.buffer_reverse(buf)
        infos = hb.buffer_get_glyph_infos(buf)
        positions = hb.buffer_get_glyph_positions(buf)
        advance = sum(p.x_advance for p in positions)

        context.translate(0, lineheight)
        context.save()

        context.save()
        context.move_to(0, -20)
        if variation:
            tag = hb.tag_to_string(variation.tag).decode("ascii")
            context.show_text(f" {tag}={variation.value:g}")
        context.move_to(0, 0)
        context.show_text(f" {advance:g}/{target_advance:g}")
        context.restore()

        if variation:
            hb.font_set_variations(font, [variation])

        context.translate(margin, 0)
        context.scale(scale, -scale)

        if rtl:
            context.translate(target_advance, 0)

        for info, pos in zip(infos, positions):
            if rtl:
                context.translate(-pos.x_advance, pos.y_advance)
            context.save()
            context.translate(pos.x_offset, pos.y_offset)
            hb.font_paint_glyph(font, info.codepoint, PFUNCS, id(context), 0, 0x0000FF)
            context.restore()
            if not rtl:
                context.translate(+pos.x_advance, pos.y_advance)

        context.restore()
    context.restore()


def main(fontpath, textpath):
    fontsize = 70

    blob = hb.blob_create_from_file(fontpath)
    face = hb.face_create(blob, 0)

    with open(textpath) as f:
        text = f.read()

    def on_draw(da, context):
        alloc = da.get_allocation()
        POOL[id(context)] = context
        render(face, text, context, alloc.width, alloc.height, fontsize)
        del POOL[id(context)]

    drawingarea = Gtk.DrawingArea()
    drawingarea.connect("draw", on_draw)

    win = Gtk.Window()
    win.connect("destroy", Gtk.main_quit)
    win.set_default_size(1000, 700)
    win.add(drawingarea)

    win.show_all()
    Gtk.main()


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="HarfBuzz justification demo.")
    parser.add_argument("fontfile", help="font file")
    parser.add_argument("textfile", help="text")
    args = parser.parse_args()
    main(args.fontfile, args.textfile)

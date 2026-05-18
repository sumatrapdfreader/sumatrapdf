mujs_src_dir = '../../../ext/mujs'

mujs_sources = files(
  mujs_src_dir / 'jsarray.c',
  mujs_src_dir / 'jsboolean.c',
  mujs_src_dir / 'jsbuiltin.c',
  mujs_src_dir / 'jscompile.c',
  mujs_src_dir / 'jsdate.c',
  mujs_src_dir / 'jsdtoa.c',
  mujs_src_dir / 'jserror.c',
  mujs_src_dir / 'jsfunction.c',
  mujs_src_dir / 'jsgc.c',
  mujs_src_dir / 'jsintern.c',
  mujs_src_dir / 'jslex.c',
  mujs_src_dir / 'jsmath.c',
  mujs_src_dir / 'jsnumber.c',
  mujs_src_dir / 'jsobject.c',
  mujs_src_dir / 'json.c',
  mujs_src_dir / 'jsparse.c',
  mujs_src_dir / 'jsproperty.c',
  mujs_src_dir / 'jsregexp.c',
  mujs_src_dir / 'jsrepr.c',
  mujs_src_dir / 'jsrun.c',
  mujs_src_dir / 'jsstate.c',
  mujs_src_dir / 'jsstring.c',
  mujs_src_dir / 'jsvalue.c',
  mujs_src_dir / 'regexp.c',
  mujs_src_dir / 'utf.c',
)

mujs_inc = include_directories(mujs_src_dir)

mujs_lib = static_library('mujs', mujs_sources,
  include_directories: mujs_inc,
)

mujs_dep = declare_dependency(
  link_with: mujs_lib,
  include_directories: mujs_inc,
)

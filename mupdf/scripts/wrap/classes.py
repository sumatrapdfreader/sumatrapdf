'''
Extra information to customise how we wrap MuPDF structs into C++ classes.
'''

import textwrap

import jlib

from . import rename
from . import state
from . import util


class ExtraMethod:
    '''
    Defines a prototype and implementation of a custom method in a generated
    class.
    '''
    def __init__( self, return_, name_args, body, comment, overload=None):
        '''
        return_:
            Return type as a string.
        name_args:
            A string describing name and args of the method:
                <method-name>(<args>)
        body:
            Implementation code including the enclosing '{...}'.
        comment:
            Optional comment; should include /* and */ or //.
        overload:
            If true, we allow auto-generation of methods with same name.
        '''
        assert name_args
        self.return_ = return_
        self.name_args = name_args
        self.body = body
        self.comment = comment
        self.overload = overload
        assert '\t' not in body
    def __str__(self):
        return f'{self.name_args} => {self.return_}'


class ExtraConstructor:
    '''
    Defines a prototype and implementation of a custom constructor in a
    generated class.
    '''
    def __init__( self, name_args, body, comment):
        '''
        name_args:
            A string of the form: (<args>)
        body:
            Implementation code including the enclosing '{...}'.
        comment:
            Optional comment; should include /* and */ or //.
        '''
        self.return_ = ''
        self.name_args = name_args
        self.body = body
        self.comment = comment
        assert '\t' not in body


class ClassExtra:
    '''
    Extra methods/features when wrapping a particular MuPDF struct into a C++
    class.
    '''
    def __init__( self,
            accessors=None,
            class_bottom='',
            class_post='',
            class_pre='',
            class_top='',
            constructor_default=True,
            constructor_excludes=None,
            constructor_prefixes=None,
            constructor_raw=True,
            constructors_extra=None,
            constructors_wrappers=None,
            copyable=True,
            extra_cpp='',
            iterator_next=None,
            methods_extra=None,
            method_wrappers=None,
            method_wrappers_static=None,
            opaque=False,
            pod=False,
            virtual_fnptrs=False,
            ):
        '''
        accessors:
            If true, we generate accessors methods for all items in the
            underlying struct.

            Defaults to True if pod is True, else False.

        class_bottom:
            Extra text at end of class definition, e.g. for member variables.

        class_post:
            Extra text after class definition, e.g. complete definition of
            iterator class.

        class_pre:
            Extra text before class definition, e.g. forward declaration of
            iterator class.

        class_top:
            Extra text at start of class definition, e.g. for enums.

        constructor_default:
            If None we set to true if `pod` is true, otherwise false. If
            true, we create a default constructor. If `pod` is true this
            constructor will default-initialise each member, otherwise it will
            set `m_internal` to null.

        constructor_excludes:
            Lists of constructor functions to ignore.

        constructor_prefixes:
            Extra fz_*() function name prefixes that can be used by class
            constructors_wrappers. We find all functions whose name starts with one of
            the specified prefixes and which returns a pointer to the relevant
            fz struct.

            For each function we find, we make a constructor that takes the
            required arguments and set m_internal to what this function
            returns.

            If there is a '-' item, we omit the default 'fz_new_<type>' prefix.

        constructor_raw:
            If true, create a constructor that takes a pointer to an instance
            of the wrapped fz_ struct. If 'default', this constructor arg
            defaults to NULL. If 'declaration_only' we declare the constructor
            but do not write out the function definition - typically this will
            be instead specified as custom code in <extra_cpp>.

        constructors_extra:
            List of ExtraConstructor's, allowing arbitrary constructors_wrappers to be
            specified.

        constructors_wrappers:
            List of fns to use as constructors_wrappers.

        copyable:
            If 'default' we allow default copy constructor to be created by C++
            compiler. This is useful for plain structs that are not referenced
            counted but can still be copied, but which we don't want to specify
            pod=True.

            Otherwise if true, generated wrapper class must be copyable. If
            pod is false, we generate a copy constructor by looking for a
            fz_keep_*() function; it's an error if we can't find this function
            [2024-01-24 fixme: actually we don't appear to raise an
            error in this case, instead we make class non-copyable.
            e.g. FzCompressedBuffer.].

            Otherwise if false we create a private copy constructor.

            [todo: need to check docs for interaction of pod/copyable.]

        extra_cpp:
            Extra text for .cpp file, e.g. implementation of iterator class
            methods.

        iterator_next:
            Support for iterating forwards over linked list.

            Should be (first, last).

            first:
                Name of element within the wrapped class that points to the
                first item in the linked list. We assume that this element will
                have 'next' pointers that terminate in NULL.

                If <first> is '', the container is itself the first element in
                the linked list.

            last:
                Currently unused, but could be used for reverse iteration in
                the future.

            We generate begin() and end() methods, plus a separate iterator
            class, to allow iteration over the linked list starting at
            <structname>::<first> and iterating to ->next until we reach NULL.

        methods_extra:
            List of ExtraMethod's, allowing arbitrary methods to be specified.

        method_wrappers:
            Extra fz_*() function names that should be wrapped in class
            methods.

            E.g. 'fz_foo_bar' is converted to a method called foo_bar()
            that takes same parameters as fz_foo_bar() except context and
            any pointer to struct and fz_context*. The implementation calls
            fz_foo_bar(), converting exceptions etc.

            The first arg that takes underlying fz_*_s type is omitted and
            implementation passes <this>.

        method_wrappers_static:
            Like <method_wrappers>, but generates static methods, where no args
            are replaced by <this>.

        opaque:
            If true, we generate a wrapper even if there's no definition
            available for the struct, i.e. it's only available as a forward
            declaration.

        pod:
            If 'inline', there is no m_internal; instead, each member of the
            underlying class is placed in the wrapper class and special method
            `internal()` returns a fake pointer to underlying class.

            If 'none', there is no m_internal member at all. Typically
            <extra_cpp> could be used to add in custom members.

            If True, underlying class is POD and m_internal is an instance of
            the underlying class instead of a pointer to it.

        virtual_fnptrs:
            If true, should be a dict with these keys:

                alloc:
                    A string containing C++ code to be embedded in the
                    virtual_fnptrs wrapper class's constructor.

                    If the wrapper class is a POD, the MuPDF struct is already
                    available as part of the wrapper class instance (as
                    m_internal, or `internal()` if inline). Otherwise this
                    code should set `m_internal` to point to a newly allocated
                    instance of the MuPDF struct.

                    Should typically set up the MuPDF struct so that `self_()`
                    can return the original C++ wrapper class instance.
                comment:
                    Extra comment for the wrapper class.
                free:
                    Optional code for freeing the virtual_fnptrs wrapper
                    class. If specified this causes creation of a destructor
                    function.

                    This is only needed for non-ref-counted classes that are
                    not marked as POD. In this case the wrapper class has a
                    pointer `m_internal` member that will not be automatically
                    freed by the destructor, and `alloc` will usually have set
                    it to a newly allocated struct.
                self_:
                    A callable that returns a string containing C++ code for
                    embedding in each low-level callback. It should returns a
                    pointer to the original C++ virtual_fnptrs wrapper class.

                    If `self_n` is None, this callable takes no args. Otherwise
                    it takes the name of the `self_n`'th arg.
                self_n:
                    Index of arg in each low-level callback, for the arg that
                    should be passed to `self_`. We use the same index for all
                    low-level callbacks.  If not specified, default is 1 (we
                    generally expect args to be (fz_context* ctx, void*, ...).
                    If None, `self_` is called with no arg; this is for if we
                    use a different mechanism such as a global variable.

            We generate a virtual_fnptrs wrapper class, derived from the main
            wrapper class, where the main wrapper class's function pointers end
            up calling the virtual_fnptrs wrapper class's virtual methods. We
            then use SWIG's 'Director' support to allow these virtual methods
            to be overridden in Python/C#. Thus one can make MuPDF function
            pointers call Python/C# code.
        '''
        if accessors is None and pod is True:
            accessors = True
        if constructor_default is None:
            constructor_default = pod
        self.accessors = accessors
        self.class_bottom = class_bottom
        self.class_post = class_post
        self.class_pre = class_pre
        self.class_top = class_top
        self.constructor_default = constructor_default
        self.constructor_excludes = constructor_excludes or []
        self.constructor_prefixes = constructor_prefixes or []
        self.constructor_raw = constructor_raw
        self.constructors_extra = constructors_extra or []
        self.constructors_wrappers = constructors_wrappers or []
        self.copyable = copyable
        self.extra_cpp = extra_cpp
        self.iterator_next = iterator_next
        self.methods_extra = methods_extra or []
        self.method_wrappers = method_wrappers or []
        self.method_wrappers_static = method_wrappers_static or []
        self.opaque = opaque
        self.pod = pod
        self.virtual_fnptrs = virtual_fnptrs

        assert self.pod in (False, True, 'inline', 'none'), f'{self.pod}'

        def assert_list_of( items, type_):
            assert isinstance( items, list)
            for item in items:
                assert isinstance( item, type_)

        assert_list_of( self.constructor_prefixes, str)
        assert_list_of( self.method_wrappers, str)
        assert_list_of( self.method_wrappers_static, str)
        assert_list_of( self.methods_extra, ExtraMethod)
        assert_list_of( self.constructors_extra, ExtraConstructor)

        if virtual_fnptrs:
            assert isinstance(virtual_fnptrs, dict), f'virtual_fnptrs={virtual_fnptrs!r}'

    def __str__( self):
        ret = ''
        ret += f' accessors={self.accessors}'
        ret += f' class_bottom={self.class_bottom}'
        ret += f' class_post={self.class_post}'
        ret += f' class_pre={self.class_pre}'
        ret += f' class_top={self.class_top}'
        ret += f' constructor_default={self.constructor_default}'
        ret += f' constructor_excludes={self.constructor_excludes}'
        ret += f' constructor_prefixes={self.constructor_prefixes}'
        ret += f' constructor_raw={self.constructor_raw}'
        ret += f' constructors_extra={self.constructors_extra}'
        ret += f' constructors_wrappers={self.constructors_wrappers}'
        ret += f' copyable={self.copyable}'
        ret += f' extra_cpp={self.extra_cpp}'
        ret += f' iterator_next={self.iterator_next}'
        ret += f' methods_extra={self.methods_extra}'
        ret += f' method_wrappers={self.method_wrappers}'
        ret += f' method_wrappers_static={self.method_wrappers_static}'
        ret += f' opaque={self.opaque}'
        ret += f' pod={self.pod}'
        ret += f' virtual_fnptrs={self.virtual_fnptrs}'
        return ret



class ClassExtras:
    '''
    Extra methods/features for each of our auto-generated C++ wrapper classes.
    '''
    def __init__( self, **namevalues):
        '''
        namevalues:
            Named args mapping from struct name (e.g. fz_document) to a
            ClassExtra.
        '''
        self.items = dict()
        for name, value in namevalues.items():
            self.items[ name] = value

    def get( self, tu, name):
        '''
        Searches for <name> and returns a ClassExtra instance. If <name> is not
        found, we insert an empty ClassExtra instance and return it. We do this
        for any name, e.g. name could be 'foo *'.

        We return None if <name> is a known enum.
        '''
        verbose = state.state_.show_details( name)
        if 0 and verbose:
            jlib.log( 'ClassExtras.get(): {=name}')
        name = util.clip( name, ('const ', 'struct '))
        if 0 and verbose:
            jlib.log( 'ClassExtras.get(): {=name}')
        if not name.startswith( ('fz_', 'pdf_')):
            return

        ret = self.items.setdefault( name, ClassExtra())

        if name in state.state_.enums[ tu]:
            #jlib.log( '*** name is an enum: {name=}')
            return None

        if ' ' not in name and not ret.pod and ret.copyable and ret.copyable != 'default':
            # Check whether there is a _keep() fn.
            keep_name = f'fz_keep_{name[3:]}' if name.startswith( 'fz_') else f'pdf_keep_{name[4:]}'
            keep_cursor = state.state_.find_function( tu, keep_name, method=True)
            if not keep_cursor:
                if ret.copyable:
                    if 0:
                        jlib.log( '*** Changing .copyable to False for {=name keep_name}')
                ret.copyable = False
        return ret

    def get_or_none( self, name):
        return self.items.get( name)


# Customisation information for selected wrapper classes.
#
# We use MuPDF struct names as keys.
#
classextras = ClassExtras(

        fz_aa_context = ClassExtra(
                pod='inline',
                ),

        fz_band_writer = ClassExtra(
                class_top = '''
                    /* We use these enums to support construction via all the relevant MuPDF functions. */

                    enum Cm
                    {
                        MONO,
                        COLOR,
                    };
                    enum P
                    {
                        PNG,
                        PNM,
                        PAM,
                        PBM,
                        PKM,
                        PS,
                        PSD,
                    };
                    ''',
                constructors_extra = [
                    ExtraConstructor(
                        f'({rename.class_("fz_output")}& out, Cm cm, const {rename.class_("fz_pcl_options")}& options)',
                        f'''
                        {{
                            ::fz_output*            out2 = out.m_internal;
                            const ::fz_pcl_options* options2 = options.m_internal;
                            if (0)  {{}}
                            else if (cm == MONO)    m_internal = {rename.ll_fn('fz_new_mono_pcl_band_writer' )}( out2, options2);
                            else if (cm == COLOR)   m_internal = {rename.ll_fn('fz_new_color_pcl_band_writer')}( out2, options2);
                            else throw std::runtime_error( "Unrecognised fz_band_writer_s Cm type");
                        }}
                        ''',
                        comment = f'/* Constructor using fz_new_mono_pcl_band_writer() or fz_new_color_pcl_band_writer(). */',
                        ),
                    ExtraConstructor(
                        f'({rename.class_("fz_output")}& out, P p)',
                        f'''
                        {{
                            ::fz_output*    out2 = out.m_internal;
                            if (0)  {{}}
                            else if (p == PNG)  m_internal = {rename.ll_fn('fz_new_png_band_writer')}( out2);
                            else if (p == PNM)  m_internal = {rename.ll_fn('fz_new_pnm_band_writer')}( out2);
                            else if (p == PAM)  m_internal = {rename.ll_fn('fz_new_pam_band_writer')}( out2);
                            else if (p == PBM)  m_internal = {rename.ll_fn('fz_new_pbm_band_writer')}( out2);
                            else if (p == PKM)  m_internal = {rename.ll_fn('fz_new_pkm_band_writer')}( out2);
                            else if (p == PS)   m_internal = {rename.ll_fn('fz_new_ps_band_writer' )}( out2);
                            else if (p == PSD)  m_internal = {rename.ll_fn('fz_new_psd_band_writer')}( out2);
                            else throw std::runtime_error( "Unrecognised fz_band_writer_s P type");
                        }}
                        ''',
                        comment = f'/* Constructor using fz_new_p*_band_writer(). */',
                        ),
                    ExtraConstructor(
                        f'({rename.class_("fz_output")}& out, Cm cm, const {rename.class_("fz_pwg_options")}& options)',
                        f'''
                        {{
                            ::fz_output*            out2 = out.m_internal;
                            const ::fz_pwg_options* options2 = &options.m_internal;
                            if (0)  {{}}
                            else if (cm == MONO)    m_internal = {rename.ll_fn('fz_new_mono_pwg_band_writer' )}( out2, options2);
                            else if (cm == COLOR)   m_internal = {rename.ll_fn('fz_new_pwg_band_writer')}( out2, options2);
                            else throw std::runtime_error( "Unrecognised fz_band_writer_s Cm type");
                        }}
                        ''',
                        comment = f'/* Constructor using fz_new_mono_pwg_band_writer() or fz_new_pwg_band_writer(). */',
                        ),
                    ],
                copyable = False,
                ),

        fz_bitmap = ClassExtra(
                accessors = True,
                ),

        fz_buffer = ClassExtra(
                constructor_raw = 'default',
                constructors_wrappers = [
                    'fz_read_file',
                    ],
                ),

        fz_color_params = ClassExtra(
                pod='inline',
                constructors_extra = [
                    ExtraConstructor('()',
                        f'''
                        {{
                            this->ri = fz_default_color_params.ri;
                            this->bp = fz_default_color_params.bp;
                            this->op = fz_default_color_params.op;
                            this->opm = fz_default_color_params.opm;
                        }}
                        ''',
                        comment = '/* Equivalent to fz_default_color_params. */',
                        ),
                    ],
                ),

        fz_colorspace = ClassExtra(
                constructors_extra = [
                    ExtraConstructor(
                        '(Fixed fixed)',
                        f'''
                        {{
                            if (0) {{}}
                            else if ( fixed == Fixed_GRAY)  m_internal = {rename.ll_fn( 'fz_device_gray')}();
                            else if ( fixed == Fixed_RGB)   m_internal = {rename.ll_fn( 'fz_device_rgb' )}();
                            else if ( fixed == Fixed_BGR)   m_internal = {rename.ll_fn( 'fz_device_bgr' )}();
                            else if ( fixed == Fixed_CMYK)  m_internal = {rename.ll_fn( 'fz_device_cmyk')}();
                            else if ( fixed == Fixed_LAB)   m_internal = {rename.ll_fn( 'fz_device_lab' )}();
                            else {{
                                std::string message = "Unrecognised fixed colorspace id";
                                throw {rename.error_class("FZ_ERROR_GENERIC")}(message.c_str());
                            }}
                            {rename.ll_fn('fz_keep_colorspace')}(m_internal);
                        }}
                        ''',
                        comment = '/* Construct using one of: fz_device_gray(), fz_device_rgb(), fz_device_bgr(), fz_device_cmyk(), fz_device_lab(). */',
                        ),
                    ],
                constructor_raw=1,
                class_top = '''
                        /* We use this enums to support construction via all the relevant MuPDF functions. */
                        enum Fixed
                        {
                            Fixed_GRAY,
                            Fixed_RGB,
                            Fixed_BGR,
                            Fixed_CMYK,
                            Fixed_LAB,
                        };
                        ''',
                ),

        fz_compressed_buffer = ClassExtra(
                methods_extra = [
                    ExtraMethod(
                        rename.class_('fz_buffer'),
                        'get_buffer()',
                        textwrap.dedent(f'''
                            {{
                                return {rename.class_('fz_buffer')}(
                                        {rename.ll_fn('fz_keep_buffer')}(m_internal->buffer)
                                        );
                            }}
                            '''),
                        '/* Returns wrapper class for fz_buffer *m_internal.buffer. */',
                        ),
                    ],
                ),

        fz_context = ClassExtra(
                copyable = False,
                ),

        fz_cookie = ClassExtra(
                constructors_extra = [
                    ExtraConstructor( '()',
                    '''
                    {
                        this->m_internal.abort = 0;
                        this->m_internal.progress = 0;
                        this->m_internal.progress_max = (size_t) -1;
                        this->m_internal.errors = 0;
                        this->m_internal.incomplete = 0;
                    }
                    ''',
                    comment = '/* Default constructor sets all fields to default values. */',
                    ),
                    ],
                constructor_raw = False,
                methods_extra = [
                    ExtraMethod(
                            'void',
                            'set_abort()',
                            '{ m_internal.abort = 1; }\n',
                            '/* Sets m_internal.abort to 1. */',
                            ),
                    ExtraMethod(
                            'void',
                            'increment_errors(int delta)',
                            '{ m_internal.errors += delta; }\n',
                            '/* Increments m_internal.errors by <delta>. */',
                            ),
                ],
                pod = True,
                # Other code asynchronously writes to our fields, so we are not
                # copyable. todo: maybe tie us to all objects to which we have
                # been associated?
                #
                copyable=False,
                ),

        fz_device = ClassExtra(
                virtual_fnptrs = dict(
                    self_ = lambda name: f'(*({rename.class_("fz_device")}2**) ({name} + 1))',
                    alloc = textwrap.dedent( f'''
                        m_internal = {rename.ll_fn("fz_new_device_of_size")}(
                                sizeof(*m_internal) + sizeof({rename.class_("fz_device")}2*)
                                );
                        *(({rename.class_("fz_device")}2**) (m_internal + 1)) = this;
                        '''),
                    ),
                constructor_raw = True,
                ),

        fz_document = ClassExtra(
                constructor_excludes = [
                    'fz_new_xhtml_document_from_document',
                    ],
                constructor_prefixes = [
                    'fz_open_accelerated_document',
                    'fz_open_document',
                    ],
                constructors_extra = [
                    ExtraConstructor( f'({rename.class_("pdf_document")}& pdfdocument)',
                        f'''
                        {{
                            m_internal = {rename.ll_fn('fz_keep_document')}(&pdfdocument.m_internal->super);
                        }}
                        ''',
                        f'/* Returns a {rename.class_("fz_document")} for pdfdocument.m_internal.super. */',
                        ),
                    ],
                constructor_raw = 'default',
                method_wrappers = [
                    'fz_load_outline',
                ],
                method_wrappers_static = [
                    'fz_new_xhtml_document_from_document',
                    ],
                ),

        # This is a little complicated. Many of the functions that we would
        # like to wrap to form constructors, have the same set of args. C++
        # does not support named constructors so we differentiate between
        # constructors with identical args using enums.
        #
        # Also, fz_document_writer is not reference counted so the wrapping
        # class is not copyable or assignable, so our normal approach of making
        # static class functions that return a newly constructed instance by
        # value, does not work.
        #
        # So instead we define enums that are passed to our constructors,
        # allowing the constructor to decide which fz_ function to use to
        # create the new fz_document_writer.
        #
        # There should be no commented-out constructors in the generated code
        # marked as 'Disabled because same args as ...'.
        #
        fz_document_writer = ClassExtra(
                class_top = '''
                    /* Used for constructor that wraps fz_ functions taking (const char *path, const char *options). */
                    enum PathType
                    {
                        PathType_CBZ,
                        PathType_DOCX,
                        PathType_ODT,
                        PathType_PAM_PIXMAP,
                        PathType_PBM_PIXMAP,
                        PathType_PCL,
                        PathType_PCLM,
                        PathType_PDF,
                        PathType_PDFOCR,
                        PathType_PGM_PIXMAP,
                        PathType_PKM_PIXMAP,
                        PathType_PNG_PIXMAP,
                        PathType_PNM_PIXMAP,
                        PathType_PPM_PIXMAP,
                        PathType_PS,
                        PathType_PWG,
                        PathType_SVG,
                    };

                    /* Used for constructor that wraps fz_ functions taking (Output& out, const char *options). */
                    enum OutputType
                    {
                        OutputType_CBZ,
                        OutputType_DOCX,
                        OutputType_ODT,
                        OutputType_PCL,
                        OutputType_PCLM,
                        OutputType_PDF,
                        OutputType_PDFOCR,
                        OutputType_PS,
                        OutputType_PWG,
                    };

                    /* Used for constructor that wraps fz_ functions taking (const char *format, const char *path, const char *options). */
                    enum FormatPathType
                    {
                        FormatPathType_DOCUMENT,
                        FormatPathType_TEXT,
                    };
                ''',
                # These excludes should match the functions called by the
                # extra constructors defined below. This ensures that we don't
                # generate commented-out constructors with a comment saying
                # 'Disabled because same args as ...'.
                constructor_excludes = [
                    'fz_new_cbz_writer',
                    'fz_new_docx_writer',
                    'fz_new_odt_writer',
                    'fz_new_pam_pixmap_writer',
                    'fz_new_pbm_pixmap_writer',
                    'fz_new_pcl_writer',
                    'fz_new_pclm_writer',
                    'fz_new_pdfocr_writer',
                    'fz_new_pdf_writer',
                    'fz_new_pgm_pixmap_writer',
                    'fz_new_pkm_pixmap_writer',
                    'fz_new_png_pixmap_writer',
                    'fz_new_pnm_pixmap_writer',
                    'fz_new_ppm_pixmap_writer',
                    'fz_new_ps_writer',
                    'fz_new_pwg_writer',
                    'fz_new_svg_writer',

                    'fz_new_cbz_writer_with_output',
                    'fz_new_docx_writer_with_output',
                    'fz_new_odt_writer_with_output',
                    'fz_new_pcl_writer_with_output',
                    'fz_new_pclm_writer_with_output',
                    'fz_new_pdf_writer_with_output',
                    'fz_new_pdfocr_writer_with_output',
                    'fz_new_ps_writer_with_output',
                    'fz_new_pwg_writer_with_output',

                    'fz_new_document_writer',
                    'fz_new_text_writer',

                    'fz_new_document_writer_with_output',
                    'fz_new_text_writer_with_output',
                    ],

                copyable=False,
                methods_extra = [
                        # 2022-08-26: we used to provide a custom wrapper of
                        # fz_begin_page(), but this is not longer necessary
                        # because function_wrapper_class_aware_body() knows
                        # that fz_begin_page() returns a borrowed reference.
                        #
                        ],
                constructors_extra = [
                    ExtraConstructor(
                        '(const char *path, const char *options, PathType path_type)',
                        f'''
                        {{
                            if (0) {{}}
                            else if (path_type == PathType_CBZ)         m_internal = {rename.ll_fn( 'fz_new_cbz_writer')}(path, options);
                            else if (path_type == PathType_DOCX)        m_internal = {rename.ll_fn( 'fz_new_docx_writer')}(path, options);
                            else if (path_type == PathType_ODT)         m_internal = {rename.ll_fn( 'fz_new_odt_writer')}(path, options);
                            else if (path_type == PathType_PAM_PIXMAP)  m_internal = {rename.ll_fn( 'fz_new_pam_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PBM_PIXMAP)  m_internal = {rename.ll_fn( 'fz_new_pbm_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PCL)         m_internal = {rename.ll_fn( 'fz_new_pcl_writer')}(path, options);
                            else if (path_type == PathType_PCLM)        m_internal = {rename.ll_fn( 'fz_new_pclm_writer')}(path, options);
                            else if (path_type == PathType_PDF)         m_internal = {rename.ll_fn( 'fz_new_pdf_writer')}(path, options);
                            else if (path_type == PathType_PDFOCR)      m_internal = {rename.ll_fn( 'fz_new_pdfocr_writer')}(path, options);
                            else if (path_type == PathType_PGM_PIXMAP)  m_internal = {rename.ll_fn( 'fz_new_pgm_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PKM_PIXMAP)  m_internal = {rename.ll_fn( 'fz_new_pkm_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PNG_PIXMAP)  m_internal = {rename.ll_fn( 'fz_new_png_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PNM_PIXMAP)  m_internal = {rename.ll_fn( 'fz_new_pnm_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PPM_PIXMAP)  m_internal = {rename.ll_fn( 'fz_new_ppm_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PS)          m_internal = {rename.ll_fn( 'fz_new_ps_writer')}(path, options);
                            else if (path_type == PathType_PWG)         m_internal = {rename.ll_fn( 'fz_new_pwg_writer')}(path, options);
                            else if (path_type == PathType_SVG)         m_internal = {rename.ll_fn( 'fz_new_svg_writer')}(path, options);
                            else throw {rename.error_class('FZ_ERROR_ABORT')}( "Unrecognised Type value");
                        }}
                        ''',
                        comment = textwrap.dedent('''
                        /* Constructor using one of:
                            fz_new_cbz_writer()
                            fz_new_docx_writer()
                            fz_new_odt_writer()
                            fz_new_pam_pixmap_writer()
                            fz_new_pbm_pixmap_writer()
                            fz_new_pcl_writer()
                            fz_new_pclm_writer()
                            fz_new_pdf_writer()
                            fz_new_pdfocr_writer()
                            fz_new_pgm_pixmap_writer()
                            fz_new_pkm_pixmap_writer()
                            fz_new_png_pixmap_writer()
                            fz_new_pnm_pixmap_writer()
                            fz_new_ppm_pixmap_writer()
                            fz_new_ps_writer()
                            fz_new_pwg_writer()
                            fz_new_svg_writer()
                        */'''),
                        ),
                    ExtraConstructor(
                        f'({rename.class_("fz_output")}& out, const char *options, OutputType output_type)',
                        f'''
                        {{
                            /* All fz_new_*_writer_with_output() functions take
                            ownership of the fz_output, even if they throw an
                            exception. So we need to set out.m_internal to null
                            here so its destructor does nothing. */
                            ::fz_output* out2 = out.m_internal;
                            out.m_internal = NULL;
                            if (0) {{}}
                            else if (output_type == OutputType_CBZ)     m_internal = {rename.ll_fn( 'fz_new_cbz_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_DOCX)    m_internal = {rename.ll_fn( 'fz_new_docx_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_ODT)     m_internal = {rename.ll_fn( 'fz_new_odt_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PCL)     m_internal = {rename.ll_fn( 'fz_new_pcl_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PCLM)    m_internal = {rename.ll_fn( 'fz_new_pclm_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PDF)     m_internal = {rename.ll_fn( 'fz_new_pdf_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PDFOCR)  m_internal = {rename.ll_fn( 'fz_new_pdfocr_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PS)      m_internal = {rename.ll_fn( 'fz_new_ps_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PWG)     m_internal = {rename.ll_fn( 'fz_new_pwg_writer_with_output')}(out2, options);
                            else
                            {{
                                /* Ensure that out2 is dropped before we return. */
                                {rename.ll_fn( 'fz_drop_output')}(out2);
                                throw {rename.error_class('FZ_ERROR_ABORT')}( "Unrecognised OutputType value");
                            }}
                        }}
                        ''',
                        comment = textwrap.dedent('''
                        /* Constructor using one of:
                            fz_new_cbz_writer_with_output()
                            fz_new_docx_writer_with_output()
                            fz_new_odt_writer_with_output()
                            fz_new_pcl_writer_with_output()
                            fz_new_pclm_writer_with_output()
                            fz_new_pdf_writer_with_output()
                            fz_new_pdfocr_writer_with_output()
                            fz_new_ps_writer_with_output()
                            fz_new_pwg_writer_with_output()

                        This constructor takes ownership of <out> -
                        out.m_internal is set to NULL after this constructor
                        returns so <out> must not be used again.
                        */
                        '''),
                        ),
                    ExtraConstructor(
                        '(const char *format, const char *path, const char *options, FormatPathType format_path_type)',
                        f'''
                        {{
                            if (0) {{}}
                            else if (format_path_type == FormatPathType_DOCUMENT)   m_internal = {rename.ll_fn( 'fz_new_document_writer')}(format, path, options);
                            else if (format_path_type == FormatPathType_TEXT)       m_internal = {rename.ll_fn( 'fz_new_text_writer')}(format, path, options);
                            else throw {rename.error_class('FZ_ERROR_ABORT')}( "Unrecognised OutputType value");
                        }}
                        ''',
                        comment = textwrap.dedent('''
                        /* Constructor using one of:
                            fz_new_document_writer()
                            fz_new_text_writer()
                        */'''),
                        ),
                    ExtraConstructor(
                        f'({rename.class_("fz_output")}& out, const char *format, const char *options)',
                        f'''
                        {{
                            /* Need to transfer ownership of <out>. */
                            ::fz_output* out2 = out.m_internal;
                            out.m_internal = NULL;
                            m_internal = {rename.ll_fn( 'fz_new_document_writer_with_output')}(out2, format, options);
                        }}
                        ''',
                        comment = textwrap.dedent('''
                        /* Constructor using fz_new_document_writer_with_output().

                        This constructor takes ownership of <out> -
                        out.m_internal is set to NULL after this constructor
                        returns so <out> must not be used again.
                        */'''),
                        ),
                    ExtraConstructor(
                        f'(const char *format, {rename.class_("fz_output")}& out, const char *options)',
                        f'''
                        {{
                            /* Need to transfer ownership of <out>. */
                            ::fz_output* out2 = out.m_internal;
                            out.m_internal = NULL;
                            m_internal = {rename.ll_fn( 'fz_new_text_writer_with_output')}(format, out2, options);
                        }}
                        ''',
                        comment = textwrap.dedent('''
                        /* Constructor using fz_new_text_writer_with_output().

                        This constructor takes ownership of <out> -
                        out.m_internal is set to NULL after this constructor
                        returns so <out> must not be used again.
                        */'''),
                        ),
                    ],

                ),

        fz_draw_options = ClassExtra(
                constructors_wrappers = [
                    'fz_parse_draw_options',
                    ],
                copyable=False,
                pod='inline',
                ),

        fz_halftone = ClassExtra(
                constructor_raw = 'default',
                ),

        fz_image = ClassExtra(
                accessors=True,
                ),

        fz_install_load_system_font_funcs_args = ClassExtra(
                pod = True,
                virtual_fnptrs = dict(
                    alloc = textwrap.dedent( f'''
                        /*
                        There can only be one active instance of the wrapper
                        class so we simply keep a pointer to it in a global
                        variable.
                        */
                        fz_install_load_system_font_funcs2_state = this;
                        '''),
                    self_ = lambda: f'({rename.class_("fz_install_load_system_font_funcs_args")}2*) fz_install_load_system_font_funcs2_state',
                    self_n = None,
                    ),
                ),

        fz_irect = ClassExtra(
                constructor_prefixes = [
                    'fz_irect_from_rect',
                    'fz_make_irect',
                    ],
                pod='inline',
                constructor_raw = True,
                ),

        fz_link = ClassExtra(
                constructors_extra = [
                    ExtraConstructor( f'({rename.class_("fz_rect")}& rect, const char *uri)',
                        f'''
                        {{
                            m_internal = {rename.ll_fn('fz_new_link_of_size')}( sizeof(fz_link), *rect.internal(), uri);
                        }}
                        ''',
                        '/* Construct by calling fz_new_link_of_size() with size=sizeof(fz_link). */',
                        )
                    ],
                accessors = True,
                iterator_next = ('', ''),
                constructor_raw = 'default',
                copyable = True,
                ),

        fz_location = ClassExtra(
                constructor_prefixes = [
                    'fz_make_location',
                    ],
                pod='inline',
                constructor_raw = True,
                ),

        fz_matrix = ClassExtra(
                constructor_prefixes = [
                    'fz_make_matrix',
                    ],
                method_wrappers_static = [
                    'fz_concat',
                    'fz_scale',
                    'fz_shear',
                    'fz_rotate',
                    'fz_translate',
                    'fz_transform_page',
                    ],
                constructors_extra = [
                    ExtraConstructor( '()',
                        '''
                        : a(1), b(0), c(0), d(1), e(0), f(0)
                        {
                        }
                        ''',
                        comment = '/* Constructs identity matrix (like fz_identity). */'),
                ],
                pod='inline',
                constructor_raw = True,
                ),

        fz_md5 = ClassExtra(
                pod = True,
                constructors_extra = [
                    ExtraConstructor(
                        '()',
                        f'''
                        {{
                            {rename.ll_fn( 'fz_md5_init')}( &m_internal);
                        }}
                        ''',
                        '/* Default constructor calls md5_init(). */',
                        )
                    ],
                ),

        fz_outline = ClassExtra(
                # We add various methods to give depth-first iteration of outlines.
                #
                constructor_prefixes = [
                    'fz_load_outline',
                    ],
                accessors=True,
                ),

        fz_outline_item = ClassExtra(
                class_top = f'''
                        FZ_FUNCTION bool valid() const;
                        FZ_FUNCTION const std::string& title() const;   /* Will throw if valid() is not true. */
                        FZ_FUNCTION const std::string& uri() const;     /* Will throw if valid() is not true. */
                        FZ_FUNCTION int is_open() const;                /* Will throw if valid() is not true. */
                        ''',
                class_bottom = f'''
                        private:
                        bool        m_valid;
                        std::string m_title;
                        std::string m_uri;
                        int         m_is_open;
                        ''',
                constructors_extra = [
                        ],
                constructor_raw = 'declaration_only',
                copyable = 'default',
                pod = 'none',
                extra_cpp = f'''
                        FZ_FUNCTION {rename.class_("fz_outline_item")}::{rename.class_("fz_outline_item")}(const ::fz_outline_item* item)
                        {{
                            if (item)
                            {{
                                m_valid = true;
                                m_title = item->title;
                                m_uri = item->uri;
                                m_is_open = item->is_open;
                            }}
                            else
                            {{
                                m_valid = false;
                            }}
                        }}
                        FZ_FUNCTION bool {rename.class_("fz_outline_item")}::valid() const
                        {{
                            return m_valid;
                        }}
                        FZ_FUNCTION const std::string& {rename.class_("fz_outline_item")}::title() const
                        {{
                            if (!m_valid) throw {rename.error_class("FZ_ERROR_GENERIC")}("fz_outline_item is invalid");
                            return m_title;
                        }}
                        FZ_FUNCTION const std::string& {rename.class_("fz_outline_item")}::uri() const
                        {{
                            if (!m_valid) throw {rename.error_class("FZ_ERROR_GENERIC")}("fz_outline_item is invalid");
                            return m_uri;
                        }}
                        FZ_FUNCTION int {rename.class_("fz_outline_item")}::is_open() const
                        {{
                            if (!m_valid) throw {rename.error_class("FZ_ERROR_GENERIC")}("fz_outline_item is invalid");
                            return m_is_open;
                        }}
                        ''',
                ),

        fz_outline_iterator = ClassExtra(
                copyable = False,
                methods_extra = [
                        ExtraMethod(
                            'int',
                            f'{rename.method("fz_outline_iterator", "fz_outline_iterator_insert")}({rename.class_("fz_outline_item")}& item)',
                            f'''
                            {{
                                /* Create a temporary fz_outline_item. */
                                ::fz_outline_item item2;
                                item2.title = (char*) item.title().c_str();
                                item2.uri = (char*) item.uri().c_str();
                                item2.is_open = item.is_open();
                                return {rename.ll_fn("fz_outline_iterator_insert")}(m_internal, &item2);
                            }}
                            ''',
                            comment = '/* Custom wrapper for fz_outline_iterator_insert(). */',
                            ),
                        ExtraMethod(
                            'void',
                            f'{rename.method("fz_outline_iterator", "fz_outline_iterator_update")}({rename.class_("fz_outline_item")}& item)',
                            f'''
                            {{
                                /* Create a temporary fz_outline_item. */
                                ::fz_outline_item item2;
                                item2.title = (char*) item.title().c_str();
                                item2.uri = (char*) item.uri().c_str();
                                item2.is_open = item.is_open();
                                return {rename.ll_fn("fz_outline_iterator_update")}(m_internal, &item2);
                            }}
                            ''',
                            comment = '/* Custom wrapper for fz_outline_iterator_update(). */',
                            ),
                        ],
                ),

        fz_output = ClassExtra(
                virtual_fnptrs = dict(
                    self_ = lambda name: f'({rename.class_("fz_output")}2*) {name}',
                    alloc = f'm_internal = {rename.ll_fn("fz_new_output")}(0 /*bufsize*/, this /*state*/, nullptr /*write*/, nullptr /*close*/, nullptr /*drop*/);\n',
                    ),
                constructor_raw = 'default',
                constructor_excludes = [
                    # These all have the same prototype, so are used by
                    # constructors_extra below.
                    'fz_new_asciihex_output',
                    'fz_new_ascii85_output',
                    'fz_new_rle_output',
                    ],
                constructors_extra = [
                    ExtraConstructor( '(Fixed out)',
                        f'''
                        {{
                            if (0)  {{}}
                            else if (out == Fixed_STDOUT) {{
                                m_internal = {rename.ll_fn('fz_stdout')}();
                            }}
                            else if (out == Fixed_STDERR) {{
                                m_internal = {rename.ll_fn('fz_stderr')}();
                            }}
                            else {{
                                throw {rename.error_class('FZ_ERROR_ABORT')}("Unrecognised Fixed value");
                            }}
                        }}
                        ''',
                        '/* Uses fz_stdout() or fz_stderr(). */',
                        # Note that it's ok to call fz_drop_output() on fz_stdout and fz_stderr.
                        ),
                    ExtraConstructor(
                        f'(const {rename.class_("fz_output")}& chain, Filter filter)',
                        f'''
                        {{
                            if (0)  {{}}
                            else if (filter == Filter_HEX) {{
                                m_internal = {rename.ll_fn('fz_new_asciihex_output')}(chain.m_internal);
                            }}
                            else if (filter == Filter_85) {{
                                m_internal = {rename.ll_fn('fz_new_ascii85_output')}(chain.m_internal);
                            }}
                            else if (filter == Filter_RLE) {{
                                m_internal = {rename.ll_fn('fz_new_rle_output')}(chain.m_internal);
                            }}
                            else {{
                                throw {rename.error_class('FZ_ERROR_ABORT')}("Unrecognised Filter value");
                            }}
                        }}
                        ''',
                        comment = '/* Calls one of: fz_new_asciihex_output(), fz_new_ascii85_output(), fz_new_rle_output(). */',
                        ),
                    ],
                class_top = '''
                    enum Fixed
                    {
                        Fixed_STDOUT=1,
                        Fixed_STDERR=2,
                    };
                    enum Filter
                    {
                        Filter_HEX,
                        Filter_85,
                        Filter_RLE,
                    };
                    '''
                    ,
                copyable=False, # No fz_keep_output() fn?
                ),

        fz_page = ClassExtra(
                constructor_prefixes = [
                    'fz_load_page',
                    'fz_load_chapter_page',
                    ],
                constructors_extra = [
                    ExtraConstructor( f'({rename.class_("pdf_page")}& pdfpage)',
                        f'''
                        {{
                            m_internal = {rename.ll_fn('fz_keep_page')}(&pdfpage.m_internal->super);
                        }}
                        ''',
                        f'/* Return {rename.class_("fz_page")} for pdfpage.m_internal.super. */',
                        ),
                    ],
                methods_extra = [
                    ExtraMethod(
                        f'{rename.class_("fz_document")}',
                        'doc()',
                        f'''
                        {{
                            return {rename.class_("fz_document")}( {rename.ll_fn('fz_keep_document')}( m_internal->doc));
                        }}
                        ''',
                        f'/* Returns wrapper for .doc member. */',
                        ),
                ],
                constructor_raw = True,
                ),

        fz_path_walker = ClassExtra(
                constructor_raw = 'default',
                virtual_fnptrs = dict(
                    self_ = lambda name: f'*({rename.class_("fz_path_walker")}2**) ((fz_path_walker*) {name} + 1)',
                    alloc = textwrap.dedent( f'''
                        m_internal = (::fz_path_walker*) {rename.ll_fn("fz_calloc")}(
                                1,
                                sizeof(*m_internal) + sizeof({rename.class_("fz_path_walker")}2*)
                                );
                        *({rename.class_("fz_path_walker")}2**) (m_internal + 1) = this;
                        '''),
                    free = f'{rename.ll_fn("fz_free")}(m_internal);\n',
                    comment = textwrap.dedent(f'''
                            /*
                            We require that the `void* arg` passed to callbacks
                            is the original `fz_path_walker*`. So, for example,
                            class-aware wrapper mupdf::fz_walk_path() should be
                            called like:

                                mupdf.FzPath path = ...;
                                struct Walker : mupdf.FzPathWalker2 {...};
                                Walker walker(...);
                                mupdf::fz_walk_path(path, walker, walker.m_internal);
                            */
                            ''')
                    ),
                ),

        fz_pcl_options = ClassExtra(
                constructors_wrappers = [
                    'fz_parse_pcl_options',
                    ],
                copyable=False,
                ),

        fz_pclm_options = ClassExtra(
                constructor_prefixes = [
                    'fz_parse_pclm_options',
                    ],
                copyable=False,
                constructors_extra = [
                    ExtraConstructor( '(const char *args)',
                        f'''
                        {{
                            {rename.ll_fn('fz_parse_pclm_options')}(m_internal, args);
                        }}
                        ''',
                        '/* Construct using fz_parse_pclm_options(). */',
                        )
                    ],
                ),

        fz_pdfocr_options = ClassExtra(
                pod = 'inline',
                methods_extra = [
                    ExtraMethod(
                        'void',
                        'language_set2(const char* language)',
                        f'''
                        {{
                            fz_strlcpy(this->language, language, sizeof(this->language));
                        }}
                        ''',
                        '/* Copies <language> into this->language, truncating if necessary. */',
                        ),
                    ExtraMethod(
                        'void',
                        'datadir_set2(const char* datadir)',
                        f'''
                        {{
                            fz_strlcpy(this->datadir, datadir, sizeof(this->datadir));
                        }}
                        ''',
                        '/* Copies <datadir> into this->datadir, truncating if necessary. */',
                        ),
                    ],
                ),

        fz_pixmap = ClassExtra(
                constructor_raw = True,
                accessors = True,
                ),

        fz_point = ClassExtra(
                method_wrappers_static = [
                    'fz_transform_point',
                    'fz_transform_point_xy',
                    'fz_transform_vector',

                    ],
                constructors_extra = [
                    ExtraConstructor( '(float x, float y)',
                        '''
                        : x(x), y(y)
                        {
                        }
                        ''',
                        comment = '/* Construct using specified values. */',
                        ),
                        ],
                methods_extra = [
                    ExtraMethod(
                        f'{rename.class_("fz_point")}&',
                        f'transform(const {rename.class_("fz_matrix")}& m)',
                        '''
                        {
                            double  old_x = x;
                            x = old_x * m.a + y * m.c + m.e;
                            y = old_x * m.b + y * m.d + m.f;
                            return *this;
                        }
                        ''',
                        comment = '/* Post-multiply *this by <m> and return *this. */',
                        ),
                ],
                pod='inline',
                constructor_raw = True,
                ),

        fz_pwg_options = ClassExtra(
                pod=True,
                ),

        fz_quad = ClassExtra(
                constructor_prefixes = [
                    'fz_transform_quad',
                    'fz_quad_from_rect'
                    ],
                pod='inline',
                constructor_raw = True,
                ),

        fz_rect = ClassExtra(
                constructor_prefixes = [
                    'fz_transform_rect',
                    'fz_bound_display_list',
                    'fz_rect_from_irect',
                    'fz_rect_from_quad',
                    ],
                method_wrappers_static = [
                    'fz_intersect_rect',
                    'fz_union_rect',
                    ],
                constructors_extra = [
                    ExtraConstructor(
                        '(double x0, double y0, double x1, double y1)',
                        '''
                        :
                        x0(x0),
                        x1(x1),
                        y0(y0),
                        y1(y1)
                        {
                        }
                        ''',
                        comment = '/* Construct from specified values. */',
                        ),
                    ExtraConstructor(
                        f'(const {rename.class_("fz_rect")}& rhs)',
                        '''
                        :
                        x0(rhs.x0),
                        y0(rhs.y0),
                        x1(rhs.x1),
                        y1(rhs.y1)
                        {
                        }
                        ''',
                        comment = '/* Copy constructor using plain copy. */',
                        ),
                    ExtraConstructor( '(Fixed fixed)',
                        f'''
                        {{
                            if (0)  {{}}
                            else if (fixed == Fixed_UNIT)       *this->internal() = {rename.c_fn('fz_unit_rect')};
                            else if (fixed == Fixed_EMPTY)      *this->internal() = {rename.c_fn('fz_empty_rect')};
                            else if (fixed == Fixed_INFINITE)   *this->internal() = {rename.c_fn('fz_infinite_rect')};
                            else throw {rename.error_class('FZ_ERROR_ABORT')}( "Unrecognised From value");
                        }}
                        ''',
                        comment = '/* Construct from fz_unit_rect, fz_empty_rect or fz_infinite_rect. */',
                        ),
                    ],
                methods_extra = [
                    ExtraMethod(
                        'void',
                        f'transform(const {rename.class_("fz_matrix")}& m)',
                        f'''
                        {{
                            *(::fz_rect*) &this->x0 = {rename.c_fn('fz_transform_rect')}(*(::fz_rect*) &this->x0, *(::fz_matrix*) &m.a);
                        }}
                        ''',
                        comment = '/* Transforms *this using fz_transform_rect() with <m>. */',
                        ),
                    ExtraMethod( 'bool', 'contains(double x, double y)',
                        '''
                        {
                            if (is_empty()) {
                                return false;
                            }
                            return true
                                    && x >= x0
                                    && x < x1
                                    && y >= y0
                                    && y < y1
                                    ;
                        }
                        ''',
                        comment = '/* Convenience method using fz_contains_rect(). */',
                        ),
                    ExtraMethod( 'bool', f'contains({rename.class_("fz_rect")}& rhs)',
                        f'''
                        {{
                            return {rename.c_fn('fz_contains_rect')}(*(::fz_rect*) &x0, *(::fz_rect*) &rhs.x0);
                        }}
                        ''',
                        comment = '/* Uses fz_contains_rect(*this, rhs). */',
                        ),
                    ExtraMethod( 'bool', 'is_empty()',
                        f'''
                        {{
                            return {rename.c_fn('fz_is_empty_rect')}(*(::fz_rect*) &x0);
                        }}
                        ''',
                        comment = '/* Uses fz_is_empty_rect(). */',
                        ),
                    ExtraMethod( 'void', f'union_({rename.class_("fz_rect")}& rhs)',
                        f'''
                        {{
                            *(::fz_rect*) &x0 = {rename.c_fn('fz_union_rect')}(*(::fz_rect*) &x0, *(::fz_rect*) &rhs.x0);
                        }}
                        ''',
                        comment = '/* Updates *this using fz_union_rect(). */',
                        ),
                    ],
                pod='inline',
                constructor_raw = True,
                copyable = True,
                class_top = '''
                    enum Fixed
                    {
                        Fixed_UNIT,
                        Fixed_EMPTY,
                        Fixed_INFINITE,
                    };
                    ''',
                ),

        fz_separations = ClassExtra(
                constructor_raw = 'default',
                opaque = True,
                ),

        fz_shade = ClassExtra(
                methods_extra = [
                    ExtraMethod( 'void',
                        f'{rename.method( "fz_shade", "fz_paint_shade_no_cache")}('
                            + f' const {rename.class_("fz_colorspace")}& override_cs'
                            + f', {rename.class_("fz_matrix")}& ctm'
                            + f', const {rename.class_("fz_pixmap")}& dest'
                            + f', {rename.class_("fz_color_params")}& color_params'
                            + f', {rename.class_("fz_irect")}& bbox'
                            + f', const {rename.class_("fz_overprint")}& eop'
                            + f')'
                            ,
                        f'''
                        {{
                            return {rename.ll_fn('fz_paint_shade')}(
                                    this->m_internal,
                                    override_cs.m_internal,
                                    *(::fz_matrix*) &ctm.a,
                                    dest.m_internal,
                                    *(::fz_color_params*) &color_params.ri,
                                    *(::fz_irect*) &bbox.x0,
                                    eop.m_internal,
                                    NULL /*cache*/
                                    );
                        }}
                        ''',
                        comment = f'/* Extra wrapper for fz_paint_shade(), passing cache=NULL. */',
                        ),
                ],
                ),

        fz_shade_color_cache = ClassExtra(
                ),

        # Our wrappers of the fz_stext_* structs all have a default copy
        # constructor - there are no fz_keep_stext_*() functions.
        #
        # We define explicit accessors for fz_stext_block::u.i.* because SWIG
        # does not handle nested unions.
        #
        fz_stext_block = ClassExtra(
                iterator_next = ('u.t.first_line', 'u.t.last_line'),
                copyable='default',
                methods_extra = [
                    ExtraMethod( f'{rename.class_("fz_matrix")}', 'i_transform()',
                        f'''
                        {{
                            if (m_internal->type != FZ_STEXT_BLOCK_IMAGE) {{
                                throw std::runtime_error("Not an image");
                            }}
                            return m_internal->u.i.transform;
                        }}
                        ''',
                        comment=f'/* Returns m_internal.u.i.transform if m_internal->type is FZ_STEXT_BLOCK_IMAGE, else throws. */',
                        ),
                    ExtraMethod( f'{rename.class_("fz_image")}', 'i_image()',
                        f'''
                        {{
                            if (m_internal->type != FZ_STEXT_BLOCK_IMAGE) {{
                                throw std::runtime_error("Not an image");
                            }}
                            return {rename.class_("fz_image")}({rename.ll_fn('fz_keep_image')}(m_internal->u.i.image));
                        }}
                        ''',
                        comment=f'/* Returns m_internal.u.i.image if m_internal->type is FZ_STEXT_BLOCK_IMAGE, else throws. */',
                        ),
                        ],
                ),

        fz_stext_char = ClassExtra(
                copyable='default',
                ),

        fz_stext_line = ClassExtra(
                iterator_next = ('first_char', 'last_char'),
                copyable='default',
                constructor_raw=True,
                ),

        fz_stext_options = ClassExtra(
                constructors_extra = [
                    ExtraConstructor( '(int flags, float scale=1.0)',
                        '''
                        :
                        flags(flags),
                        scale(scale)
                        {
                            assert(!(flags & FZ_STEXT_CLIP_RECT));
                        }
                        ''',
                        comment = '/* Construct with .flags, .scale but no clip. */',
                        ),
                    ExtraConstructor( '(int flags, fz_rect clip, float scale=1.0)',
                        '''
                        :
                        flags(flags | FZ_STEXT_CLIP_RECT),
                        scale(scale),
                        clip(clip)
                        {
                        }
                        ''',
                        comment =
                                '/* Construct with .flags, .scale and .clip; FZ_STEXT_CLIP_RECT\n'
                                'is automatically set in .flags. */'
                                ,
                        ),
                    ],
                pod='inline',
                ),

        fz_stext_page = ClassExtra(
                methods_extra = [
                    ExtraMethod(
                        'std::string',
                        f'{rename.method( "fz_stext_page", "fz_copy_selection")}('
                            + f'{rename.class_("fz_point")}& a'
                            + f', {rename.class_("fz_point")}& b'
                            + f', int crlf'
                            + f')',
                        f'''
                        {{
                            char* text = {rename.ll_fn('fz_copy_selection')}(m_internal, *(::fz_point *) &a.x, *(::fz_point *) &b.x, crlf);
                            std::string ret(text);
                            {rename.ll_fn('fz_free')}(text);
                            return ret;
                        }}
                        ''',
                        comment = f'/* Wrapper for fz_copy_selection() that returns std::string. */',
                        ),
                    ExtraMethod(
                        'std::string',
                        f'{rename.method( "fz_stext_page", "fz_copy_rectangle")}({rename.class_("fz_rect")}& area, int crlf)',
                        f'''
                        {{
                            char* text = {rename.ll_fn('fz_copy_rectangle')}(m_internal, *(::fz_rect*) &area.x0, crlf);
                            std::string ret(text);
                            {rename.ll_fn('fz_free')}(text);
                            return ret;
                        }}
                        ''',
                        comment = f'/* Wrapper for fz_copy_rectangle() that returns a std::string. */',
                        ),
                    ExtraMethod(
                        f'std::vector<{rename.class_("fz_quad")}>',
                        f'{rename.method( "fz_stext_page", "search_stext_page")}(const char* needle, int *hit_mark, int max_quads)',
                        f'''
                        {{
                            std::vector<{rename.class_("fz_quad")}> ret(max_quads);
                            int n = {rename.ll_fn('fz_search_stext_page')}(m_internal, needle, hit_mark, ret[0].internal(), max_quads);
                            ret.resize(n);
                            return ret;
                        }}
                        ''',
                        '/* Wrapper for fz_search_stext_page() that returns std::vector of Quads. */',
                        )
                    ],
                iterator_next = ('first_block', 'last_block'),
                copyable=False,
                constructor_raw = True,
                ),

        fz_text_span = ClassExtra(
                copyable=False,
                methods_extra = [
                    # We provide class-aware accessors where possible. (Some
                    # types' wrapper classes are not copyable so we can't do
                    # this for all data).
                    ExtraMethod(
                        f'{rename.class_("fz_font")}',
                        f'font()',
                        f'''
                        {{
                            return {rename.class_("fz_font")}( ll_fz_keep_font( m_internal->font));
                        }}
                        ''',
                        f'/* Gives class-aware access to m_internal->font. */',
                        ),
                    ExtraMethod(
                        f'{rename.class_("fz_matrix")}',
                        f'trm()',
                        f'''
                        {{
                            return {rename.class_("fz_matrix")}( m_internal->trm);
                        }}
                        ''',
                        f'/* Gives class-aware access to m_internal->trm. */',
                        ),
                    ExtraMethod(
                        f'fz_text_item&',
                        f'items( int i)',
                        f'''
                        {{
                            assert( i < m_internal->len);
                            return m_internal->items[i];
                        }}
                        ''',
                        '''
                        /* Gives access to m_internal->items[i].
                        Returned reference is only valid as long as `this`.
                        Provided mainly for use by SWIG bindings.
                        */
                        ''',
                        ),
                    ],
                ),

        fz_stream = ClassExtra(
                constructor_prefixes = [
                    'fz_open_file',
                    'fz_open_memory',
                    ],
                constructors_extra = [
                    ExtraConstructor( '(const std::string& filename)',
                    f'''
                    : m_internal({rename.ll_fn('fz_open_file')}(filename.c_str()))
                    {{
                    }}
                    ''',
                    comment = '/* Construct using fz_open_file(). */',
                    )
                    ],
                ),

        fz_story_element_position = ClassExtra(
                pod='inline',
                ),

        fz_transition = ClassExtra(
                pod='inline',
                constructor_raw = True,
                ),

        pdf_annot = ClassExtra(
                constructor_raw = 'default',
                ),

        pdf_clean_options = ClassExtra(
                constructors_extra = [
                    ExtraConstructor( '()',
                        f'''
                        {{
                            memset(this->internal(), 0, sizeof(*this->internal()));
                            /* Use memcpy() otherwise we get 'invalid array assignment' errors. */
                            memcpy(&this->internal()->write, &pdf_default_write_options, sizeof(this->internal()->write));
                        }}
                        ''',
                        comment = '/* Default constructor, makes copy of pdf_default_write_options. */'
                        ),
                    ExtraConstructor(
                        f'(const {rename.class_("pdf_clean_options")}& rhs)',
                        f'''
                        {{
                            *this = rhs;
                        }}
                        ''',
                        comment = '/* Copy constructor using raw memcopy(). */'
                        ),
                ],
                methods_extra = [
                    ExtraMethod(
                        f'{rename.class_("pdf_clean_options")}&',
                        f'operator=(const {rename.class_("pdf_clean_options")}& rhs)',
                        f'''
                        {{
                            memcpy(this->internal(), rhs.internal(), sizeof(*this->internal()));
                            return *this;
                        }}
                        ''',
                        comment = '/* Assignment using plain memcpy(). */',
                        ),
                    ExtraMethod(
                        f'void',
                        f'write_opwd_utf8_set(const std::string& text)',
                        f'''
                        {{
                            size_t len = std::min(text.size(), sizeof(write.opwd_utf8) - 1);
                            memcpy(write.opwd_utf8, text.c_str(), len);
                            write.opwd_utf8[len] = 0;
                        }}
                        ''',
                        '/* Copies <text> into write.opwd_utf8[]. */',
                        ),
                    ExtraMethod(
                        f'void',
                        f'write_upwd_utf8_set(const std::string& text)',
                        f'''
                        {{
                            size_t len = std::min(text.size(), sizeof(write.upwd_utf8) - 1);
                            memcpy(write.upwd_utf8, text.c_str(), len);
                            write.upwd_utf8[len] = 0;
                        }}
                        ''',
                        '/* Copies <text> into upwd_utf8[]. */',
                        ),
                ],
                pod = 'inline',
                copyable = 'default',
                ),

        pdf_document = ClassExtra(
                constructor_prefixes = [
                    'pdf_open_document',
                    'pdf_create_document',
                    'pdf_document_from_fz_document',
                    ],
                methods_extra = [
                    ExtraMethod(
                        f'{rename.class_("fz_document")}',
                        'super()',
                        f'''
                        {{
                            return {rename.class_("fz_document")}( {rename.ll_fn('fz_keep_document')}( &m_internal->super));
                        }}
                        ''',
                        f'/* Returns wrapper for .super member. */',
                        ),
                    ],
                ),

        pdf_filter_factory = ClassExtra(
                pod = 'inline',
                virtual_fnptrs = dict(
                    self_ = lambda name: f'({rename.class_("pdf_filter_factory")}2*) {name}',
                    self_n = 6,
                    alloc = f'this->options = this;\n',
                    ),
                ),

        pdf_filter_options = ClassExtra(
                pod = 'inline',
                # We don't need to allocate extra space, and because we are a
                # POD class, we can simply let our default constructor run.
                #
                # this->opaque is passed as arg[2].
                #
                virtual_fnptrs = dict(
                        self_ = lambda name: f'({rename.class_("pdf_filter_options")}2*) {name}',
                        self_n = 2,
                        alloc = f'this->opaque = this;\n',
                        ),
                constructors_extra = [
                    ExtraConstructor( '()',
                        f'''
                        {{
                            this->recurse = 0;
                            this->instance_forms = 0;
                            this->ascii = 0;
                            this->opaque = nullptr;
                            this->complete = nullptr;
                            this->filters = nullptr;
                            pdf_filter_factory eof = {{ nullptr, nullptr}};
                            m_filters.push_back( eof);
                            this->newlines = 0;
                        }}
                        ''',
                        comment = '/* Default constructor initialises all fields to null/zero. */',
                    )
                    ],
                methods_extra = [
                    ExtraMethod(
                            'void',
                            f'add_factory( const pdf_filter_factory& factory)',
                            textwrap.dedent( f'''
                                {{
                                    this->m_filters.back() = factory;
                                    pdf_filter_factory eof = {{ nullptr, nullptr}};
                                    this->m_filters.push_back( eof);
                                    this->filters = &this->m_filters[0];
                                }}
                                '''),
                            comment = f'/* Appends `factory` to internal vector and updates this->filters. */',
                            ),
                    ],
                class_bottom = textwrap.dedent( f'''
                    std::vector< pdf_filter_factory> m_filters;
                    '''),
                ),

        pdf_lexbuf = ClassExtra(
                constructors_extra = [
                    ExtraConstructor( '(int size)',
                        f'''
                        {{
                            m_internal = new ::pdf_lexbuf;
                            {rename.ll_fn('pdf_lexbuf_init')}(m_internal, size);
                        }}
                        ''',
                        comment = '/* Constructor that calls pdf_lexbuf_init(size). */',
                        ),
                    ],
                methods_extra = [
                    ExtraMethod(
                        '',
                        '~()',
                        f'''
                        {{
                            {rename.ll_fn('pdf_lexbuf_fin')}(m_internal);
                            delete m_internal;
                        }}
                        ''',
                        comment = '/* Destructor that calls pdf_lexbuf_fin(). */',
                        ),
                    ],
                ),

        pdf_layer_config = ClassExtra(
                pod = 'inline',
                ),

        pdf_layer_config_ui = ClassExtra(
                pod = 'inline',
                constructors_extra = [
                    ExtraConstructor( '()',
                        f'''
                        {{
                            this->text = nullptr;
                            this->depth = 0;
                            this->type = PDF_LAYER_UI_LABEL;
                            this->selected = 0;
                            this->locked = 0;
                        }}
                        ''',
                        comment = '/* Default constructor sets .text to null, .type to PDF_LAYER_UI_LABEL, and other fields to zero. */',
                        ),
                    ],
                ),

        pdf_obj = ClassExtra(
                constructor_raw = 'default',
                methods_extra = [
                    ExtraMethod(
                        f'{rename.class_("pdf_obj")}',
                        f'{rename.method( "pdf_obj", "pdf_dict_get")}(int key)',
                        f'''
                        {{
                            ::pdf_obj* temp = {rename.ll_fn('pdf_dict_get')}(this->m_internal, (::pdf_obj*)(uintptr_t) key);
                            {rename.ll_fn('pdf_keep_obj')}(temp);
                            auto ret = {rename.class_('pdf_obj')}(temp);
                            return ret;
                        }}
                        ''',
                        comment = '/* Typesafe wrapper for looking up things such as PDF_ENUM_NAME_Annots. */',
                        overload=True,
                        ),
                    ExtraMethod(
                        f'std::string',
                        f'{rename.method( "pdf_obj", "pdf_load_field_name2")}()',
                        f'''
                        {{
                            return {rename.namespace_fn('pdf_load_field_name2')}( *this);
                        }}
                        ''',
                        comment = f'/* Alternative to `{rename.fn("pdf_load_field_name")}()` that returns a std::string. */',
                        ),
                    ]
                ),

        pdf_page = ClassExtra(
                methods_extra = [
                    ExtraMethod(
                        f'{rename.class_("fz_page")}',
                        'super()',
                        f'''
                        {{
                            return {rename.class_("fz_page")}( {rename.ll_fn('fz_keep_page')}( &m_internal->super));
                        }}
                        ''',
                        f'/* Returns wrapper for .super member. */',
                        ),
                    ExtraMethod(
                        f'{rename.class_("pdf_document")}',
                        'doc()',
                        f'''
                        {{
                            return {rename.class_("pdf_document")}( {rename.ll_fn('pdf_keep_document')}( m_internal->doc));
                        }}
                        ''',
                        f'/* Returns wrapper for .doc member. */',
                        ),
                    ExtraMethod(
                        f'{rename.class_("pdf_obj")}',
                        'obj()',
                        f'''
                        {{
                            return {rename.class_("pdf_obj")}( {rename.ll_fn('pdf_keep_obj')}( m_internal->obj));
                        }}
                        ''',
                        f'/* Returns wrapper for .obj member. */',
                        ),
                    ],
                ),

        pdf_image_rewriter_options = ClassExtra(
                pod = 'inline',
                copyable = 'default',
                ),

        pdf_processor = ClassExtra(
                virtual_fnptrs = dict(
                    self_ = lambda name: f'(*({rename.class_("pdf_processor")}2**) ({name} + 1))',
                    alloc = textwrap.dedent( f'''
                        m_internal = (::pdf_processor*) {rename.ll_fn("pdf_new_processor")}(
                                sizeof(*m_internal)
                                + sizeof({rename.class_("pdf_processor")}2*)
                                );
                        *(({rename.class_("pdf_processor")}2**) (m_internal + 1)) = this;
                        '''),
                    ),
                ),

        pdf_recolor_options = ClassExtra(
                pod = 'inline',
                ),

        pdf_redact_options = ClassExtra(
                pod = 'inline',
                ),

        pdf_sanitize_filter_options = ClassExtra(
                pod = 'inline',
                # this->opaque is passed as arg[1].
                virtual_fnptrs = dict(
                        self_ = lambda name: f'({rename.class_("pdf_sanitize_filter_options")}2*) {name}',
                        alloc = f'this->opaque = this;\n',
                        ),
                ),

        pdf_write_options = ClassExtra(
                constructors_extra = [
                    ExtraConstructor( '()',
                        f'''
                        {{
                            /* Use memcpy() otherwise we get 'invalid array assignment' errors. */
                            memcpy(this->internal(), &pdf_default_write_options, sizeof(*this->internal()));
                        }}
                        ''',
                        comment = '/* Default constructor, makes copy of pdf_default_write_options. */'
                        ),
                    ExtraConstructor(
                        f'(const {rename.class_("pdf_write_options")}& rhs)',
                        f'''
                        {{
                            *this = rhs;
                        }}
                        ''',
                        comment = '/* Copy constructor using raw memcopy(). */'
                        ),
                ],
                methods_extra = [
                    ExtraMethod(
                        f'{rename.class_("pdf_write_options")}&',
                        f'operator=(const {rename.class_("pdf_write_options")}& rhs)',
                        f'''
                        {{
                            memcpy(this->internal(), rhs.internal(), sizeof(*this->internal()));
                            return *this;
                        }}
                        ''',
                        comment = '/* Assignment using plain memcpy(). */',
                        ),
                    ExtraMethod(
                        # Would prefer to call this opwd_utf8_set() but
                        # this conflicts with SWIG-generated accessor for
                        # opwd_utf8.
                        f'void',
                        f'opwd_utf8_set_value(const std::string& text)',
                        f'''
                        {{
                            size_t len = std::min(text.size(), sizeof(opwd_utf8) - 1);
                            memcpy(opwd_utf8, text.c_str(), len);
                            opwd_utf8[len] = 0;
                        }}
                        ''',
                        '/* Copies <text> into opwd_utf8[]. */',
                        ),
                    ExtraMethod(
                        f'void',
                        f'upwd_utf8_set_value(const std::string& text)',
                        f'''
                        {{
                            size_t len = std::min(text.size(), sizeof(upwd_utf8) - 1);
                            memcpy(upwd_utf8, text.c_str(), len);
                            upwd_utf8[len] = 0;
                        }}
                        ''',
                        '/* Copies <text> into upwd_utf8[]. */',
                        ),
                    ],
                pod = 'inline',
                copyable = 'default',
                )
        )

.. default-domain:: js

.. highlight:: javascript

PDFFilespecParams
=======================

This object represents the information found in a :term:`file specification`.
Such files may be both external files and files embedded inside the PDF document.

This Object has the properties below. If not present in the file
specification they are set to ``undefined``.

``filename``
    The name of the file.

``mimetype``
    The MIME type for an embedded file.

``size``
    The size in bytes of the embedded file contents.

``creationDate``
    The creation date of the embedded file.

``modificationDate``
    The modification date of the embedded file.

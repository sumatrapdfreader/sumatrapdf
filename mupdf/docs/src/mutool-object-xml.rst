.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_xml:

.. _mutool_run_js_api_object_xml:


`XML`
-------------


This represents an :title:`HTML` or an :title:`XML` node. It is a helper class intended to access the :title:`DOM` (:title:`Document Object Model`) content of a :ref:`Story<mutool_object_story>` object.


.. method:: body()

    Return an `XML` for the body element.

    :return: `XML`.

.. method:: documentElement()

    Return an `XML` for the top level element.

    :return: `XML`.

.. method:: createElement(tag)

    Create an element with the given tag type, but do not link it into the `XML` yet.

    :arg tag: `String`.

    :return: `XML`.


.. method:: createTextNode(text)

    Create a text node with the given text contents, but do not link it into the `XML` yet.

    :arg text: `String`.

    :return: `XML`.

.. method:: find(tag, attribute, value)

    Find the element matching the `tag`, `attribute` and `value`. Set either of those to `null` to match anything.

    :arg tag: `String`.
    :arg attribute: `String`.
    :arg value: `String`.

    :return: `XML`.

.. method:: findNext(tag, attribute, value)

    Find the next element matching the `tag`, `attribute` and `value`. Set either of those to `null` to match anything.

    :arg tag: `String`.
    :arg attribute: `String`.
    :arg value: `String`.

    :return: `XML`.


.. method:: appendChild(dom, childDom)

    Insert an element as the last child of a parent, unlinking the child from its current position if required.

    :arg dom: `XML`.
    :arg childDom: `XML`.


.. method:: insertBefore(dom, elementDom)

    Insert an element before this element, unlinking the new element from its current position if required.

    :arg dom: `XML`.
    :arg elementDom: `XML`.

.. method:: insertAfter(dom, elementDom)

    Insert an element after this element, unlinking the new element from its current position if required.

    :arg dom: `XML`.
    :arg elementDom: `XML`.


.. method:: remove()

    Remove this element from the `XML`. The element can be added back elsewhere if required.

.. method:: clone()

    Clone this element (and its children). The clone is not yet linked into the `XML`.

    :return: `XML`.

.. method:: firstChild()

    Return the first child of the element as a `XML`, or `null` if no child exist.

    :return: `XML` \| `null`.

.. method:: parent()

    Return the parent of the element as a `XML`, or `null` if no parent exists.

    :return: `XML` \| `null`.

.. method:: next()

    Return the next element as a `XML`, or `null` if no such element exists.

    :return: `XML` \| `null`.

.. method:: previous()

    Return the previous element as a `XML`, or `null` if no such element exists.

    :return: `XML` \| `null`.

.. method:: addAttribute(attribute, value)

    Add attribute with the given value, returns the updated element as an `XML`.

    :arg attribute: `String`.
    :arg value: `String`.

    :return: `XML`.

.. method:: removeAttribute(attribute)

    Remove the specified attribute from the element.

    :arg attribute: `String`.

.. method:: attribute(attribute)

    Return the element's attribute value as a `String`, or `null` if no such attribute exists.

    :arg attribute: `String`.

    :return: `String` \| `null`.

.. method:: getAttributes()

    Returns a dictionary object with properties and their values corresponding to the element's attributes and their values.

    :return: `{}`.

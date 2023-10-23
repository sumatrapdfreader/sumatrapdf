.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

----

.. default-domain:: js

.. include:: html_tags.rst

.. _mutool_object_outline_iterator:



.. _mutool_run_js_api_object_outline_iterator:



`OutlineIterator`
------------------------



An outline iterator can be used to walk over all the items in an :title:`Outline` and query their properties. To be able to insert items at the end of a list of sibling items, it can also walk one item past the end of the list. To get an instance of `OutlineIterator` use :ref:`Document outlineIterator<mutool_run_js_api_document_outlineIterator>`.

.. note::

    In the context of a :title:`PDF` file, the document's :title:`Outline` is also known as :title:`Table of Contents` or :title:`Bookmarks`.

|instance_methods|

.. method:: item()

    Return an :ref:`Outline Iterator Object<mutool_run_js_api_outline_iterator_object>` or `undefined` if out of range.

    :return: `Object`.

    |example_tag|

    .. code-block:: javascript

        var obj = outlineIterator.item();

.. method:: next()

    Move the iterator position to "next".

    :return: `Int` which is negative if this movement is not possible, `0` if the new position has a valid item, or `1` if the new position has no item but one can be inserted here.

    |example_tag|

    .. code-block:: javascript

        var result = outlineIterator.next();

.. method:: prev()

    Move the iterator position to "previous".

    :return: `Int` which is negative if this movement is not possible, `0` if the new position has a valid item, or `1` if the new position has no item but one can be inserted here.

    |example_tag|

    .. code-block:: javascript

        var result = outlineIterator.prev();

.. method:: up()

    Move the iterator position "up".

    :return: `Int` which is negative if this movement is not possible, `0` if the new position has a valid item, or `1` if the new position has no item but one can be inserted here.

    |example_tag|

    .. code-block:: javascript

        var result = outlineIterator.up();


.. method:: down()

    Move the iterator position "down".

    :return: `Int` which is negative if this movement is not possible, `0` if the new position has a valid item, or `1` if the new position has no item but one can be inserted here.

    |example_tag|

    .. code-block:: javascript

        var result = outlineIterator.down();



.. method:: insert(item)

    Insert item before the current item. The position does not change.

    :arg item: `Object` which conforms to the :ref:`Outline Iterator Object<mutool_run_js_api_outline_iterator_object>`.

    :return: `Int` which is `0` if the current position has a valid item, or `1` if the position has no valid item.

    |example_tag|

    .. code-block:: javascript

        var valid = outlineIterator.insert(item);


.. method:: delete()

    Delete the current item. This implicitly moves to the next item.

    :return: `Int` which is `0` if the new position has a valid item, or `1` if the position contains no valid item, but one may be inserted at this position.

    |example_tag|

    .. code-block:: javascript

        outlineIterator.delete();

.. method:: update(item)

    Updates the current item properties with values from the supplied item's properties.

    :arg item: `Object` which conforms to the :ref:`Outline Iterator Object<mutool_run_js_api_outline_iterator_object>`.


    |example_tag|

    .. code-block:: javascript

        outlineIterator.update(item);

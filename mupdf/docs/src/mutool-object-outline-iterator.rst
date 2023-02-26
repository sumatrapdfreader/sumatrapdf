.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_outline_iterator:



.. _mutool_run_js_api_object_outline_iterator:



`OutlineIterator`
------------------------

An outline iterator is can be used to walk over all the items in an :title:`Outline` and query their properties. To be able to insert items at the end of a list of sibling items, it can also walk one item past the end of the list.

.. note::

    In the context of a :title:`PDF` file, the document's :title:`Outline` is also known as :title:`Table of Contents` or :title:`Bookmarks`.


.. method:: item()

    Return an :ref:`Outline Iterator Object<mutool_run_js_api_outline_iterator_object>` or `undefined` if out of range.

    :return: `Object`.

.. method:: next()

    Move the iterator position to "next".

    :return: `Int`.

.. method:: prev()

    Move the iterator position to "previous".

    :return: `Int`.

.. method:: up()

    Move the iterator position "up".

    :return: `Int`.


.. method:: down()

    Move the iterator position "down".

    :return: `Int`.


.. note::

    The `next()`, `prev()`, `up()`, `down()` methods returns `0` if the new position has a valid item, or `1` if the position contains no valid item, but one may be inserted at the position. If neither of these conditions are met then `-1` is returned.



.. method:: insert(item)

    Insert item before the current item. The position does not change. Returns `0` if the position has a valid item, or `1` if the position has no valid item.

    :arg item: `Object` which conforms to the :ref:`Outline Iterator Object<mutool_run_js_api_outline_iterator_object>`.

    :return: `Int`.

.. method:: delete()

    Delete the current item. This implicitly moves to the next item. Returns 0 if the new position has a valid item, or 1 if the position contains no valid item, but one may be inserted at this position.

    :return: `Int`.

.. method:: update(item)

    Updates the current item with the properties of the supplied item.

    :arg item: `Object` which conforms to the :ref:`Outline Iterator Object<mutool_run_js_api_outline_iterator_object>`.

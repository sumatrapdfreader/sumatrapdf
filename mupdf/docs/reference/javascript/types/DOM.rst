.. default-domain:: js

.. highlight:: javascript

DOM
===

|only_mutool|

This represents an HTML or an DOM node. It is a helper class intended to
access the DOM (Document Object Model) content of a `Story`
object.

Constructors
------------

.. class:: DOM

	|no_new|

Instances of this class are returned by `Story.prototype.document()`.

Instance methods
----------------

.. method:: DOM.prototype.body()

	Return a DOM for the body element.

	:returns: `DOM`

	.. code-block::

		var result = xml.body()

.. method:: DOM.prototype.documentElement()

	Return a DOM for the top level element.

	:returns: `DOM`

	.. code-block::

		var result = xml.documentElement()

.. method:: DOM.prototype.createElement(tag)

	Create an element with the given tag type, but do not link it into
	the `DOM` yet.

	:param string tag: Tag name to use for element.

	:returns: `DOM`

	.. code-block::

		var result = xml.createElement("div")

.. method:: DOM.prototype.createTextNode(text)

	Create a text node with the given text contents, but do not link
	it into the `DOM` yet.

	:param string text: Text contents to put into element.

	:returns: `DOM`

	.. code-block::

		var result = xml.createElement("Hello world!")

.. method:: DOM.prototype.find(tag, attribute, value)

	Find the first element matching the tag, attribute and value. Set
	either of those to ``null`` to match anything.

	:param string tag: The tag of the element to look for.
	:param string attribute: The attribute of the element to look for.
	:param string value: The value of the attribute of the element to look for.

	:returns: `DOM` | null

	.. code-block::

		var result = xml.find("tag", "attribute", "value")

.. method:: DOM.prototype.findNext(tag, attribute, value)

	Find the next element matching the tag, attribute and value. Set either
	of those to ``null`` to match anything.

	:param string tag: The tag of the element to look for.
	:param string attribute: The attribute of the element to look for.
	:param string value: The value of the attribute of the element to look for.

	:returns: `DOM` | null

	.. code-block::

		var result = xml.findNext("tag", "attribute", "value")

.. method:: DOM.prototype.appendChild(dom, childDom)

	Insert an element as the last child of a parent, unlinking the child
	from its current position if required.

	:param DOM dom: The DOM that will be parent.
	:param DOM childDom: The DOM that will be a child.

	.. code-block::

		xml.appendChild(dom, childDom)

.. method:: DOM.prototype.insertBefore(dom, elementDom)

	Insert an element before this element, unlinking the new element
	from its current position if required.

	:param DOM dom: The reference DOM.
	:param DOM elementDom: The DOM that will be inserted before.

	.. code-block::

		xml.insertBefore(dom, elementDom)

.. method:: DOM.prototype.insertAfter(dom, elementDom)

	Insert an element after this element, unlinking the new element
	from its current position if required.

	:param DOM dom: The reference DOM.
	:param DOM elementDom: The DOM that will be inserted after.

	.. code-block::

		xml.insertAfter(dom, elementDom)

.. method:: DOM.prototype.remove()

	Remove this element from the `DOM`. The element can be
	added back elsewhere if required.

	.. code-block::

		var result = xml.remove()

.. method:: DOM.prototype.clone()

	Clone this element (and its children). The clone is not yet linked
	into the `DOM`.

	:returns: `DOM`

	.. code-block::

		var result = xml.clone()

.. method:: DOM.prototype.firstChild()

	Return the first child of the element as a `DOM`, or
	``null`` if no child exist.

	:returns: `DOM` | null

	.. code-block::

		var result = xml.firstChild()

.. method:: DOM.prototype.parent()

	Return the parent of the element as a `DOM`, or ``null``
	if no parent exists.

	:returns: `DOM` | null

	.. code-block::

		var result = xml.parent()

.. method:: DOM.prototype.next()

	Return the next element as a `DOM`, or null if no such
	element exists.

	:returns: `DOM` | null

	.. code-block::

		var result = xml.next()

.. method:: DOM.prototype.previous()

	Return the previous element as a `DOM`, or null if no such
	element exists.

	:returns: `DOM` | null

	.. code-block::

		var result = xml.previous()

.. method:: DOM.prototype.addAttribute(attribute, value)

	Add attribute with the given value, returns the updated element as
	a DOM.

	:param string attribute: Desired attribute name.
	:param string value: Desired attribute value.

	:returns: `DOM`

	.. code-block::

		var result = xml.addAttribute("attribute", "value")

.. method:: DOM.prototype.removeAttribute(attribute)

	Remove the specified attribute from the element, returns the
	updated element as a DOM.

	:param string attribute: The name of the attribute to remove.

	:returns: `DOM`

	.. code-block::

		xml.removeAttribute("attribute")

.. method:: DOM.prototype.getAttribute(attribute)

	Return the element's attribute value as a string, or null if no
	such attribute exists.

	:param string attribute: The name of the attribute to look up.

	:returns: string | null

	.. code-block::

		var result = xml.attribute("attribute")

.. method:: DOM.prototype.getAttributes()

	Returns a dictionary object with properties and their values
	corresponding to the element's attributes and their values.

	:returns: Object

	.. code-block::

		var dict = xml.getAttributes()

.. method:: DOM.prototype.getText()

	|only_mutool|

	Returns the text contents of the node.

	:returns: string | null

	.. code-block::

		var text = xml.getText()

.. method:: DOM.prototype.getTag()

	|only_mutool|

	Returns the tag name for the node.

	:returns: string | null

	.. code-block::

		var text = xml.getTag()

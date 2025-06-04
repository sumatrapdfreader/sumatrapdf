.. default-domain:: js

.. highlight:: javascript

Link
===================

Link objects contain information about page links, such as their bounding
box and their destination URI.

To determine whether a link is document-internal or an external link to a web page, see `Link.prototype.isExternal()`.
Finally, to resolve a document-internal link see `Document.prototype.resolveLinkDestination()`.

Constructors
------------

.. class:: Link

	|no_new|

To obtain the links on a page see `Page.prototype.getLinks()`.

To create a new link on a page see `Page.prototype.createLink()`.

Instance methods
----------------

.. method:: Link.prototype.getBounds()

	Returns a rectangle describing the link's location on the page.

	:returns: `Rect`

	.. code-block::

		var rect = link.getBounds()

.. method:: Link.prototype.setBounds(rect)

	Sets the bounds for the link's location on the page.

	:param Rect rect: Desired bounds for link.

	.. code-block:: javascript

		link.setBounds([0, 0, 100, 100])

.. method:: Link.prototype.getURI()

	Returns a string URI describing the link's destination. If
	`isExternal()` returns ``true``, this is a URI suitable for a
	browser, if it returns ``false`` pass it to `Document.prototype.resolveLink` to access
	to the destination page in the document.

	:returns: string

	.. code-block::

		var uri = link.getURI()

.. method:: Link.prototype.setURI(uri)

	Sets this link's destination to the given URI. To create links to other
	pages within the document, see `Document.prototype.formatLinkURI()`.

	:param string uri: An URI describing the desired link destination.

.. method:: Link.prototype.isExternal()

	Returns a boolean indicating if the link is external or not. URIs whose
	link URI has a valid scheme followed by colon, e.g.
	``https://example.com`` and ``mailto:test@example.com``, are defined to
	be external.

	:returns: boolean

	.. code-block::

		var isExternal = link.isExternal()

.. default-domain:: js

.. highlight:: javascript

LinkDestination
===============

A link destination points to a location within a document and how a document viewer should show that destination.

.. class:: LinkDestination(chapter, page, type, x, y, width, height)

	:param number chapter:
	:param number page:
	:param "Fit" | "FitB" | "FitH" | "FitBH" | "FitV" | "FitBV" | "FitR" | "XYZ" type:

Constants
---------

The possible type values:

.. data:: LinkDestination.FIT

	Display the page with contents zoomed to make the entire page visible.

.. data:: LinkDestination.FIT_H

	Scroll to the top coordinate and zoom to make the page width visible.

.. data:: LinkDestination.FIT_V

	Scroll to the left coordinate and zoom to make the page height visible.

.. data:: LinkDestination.FIT_B

	Zoom to fit the page bounding box.

.. data:: LinkDestination.FIT_BH

	Zoom to fit the page bounding box width.

.. data:: LinkDestination.FIT_BV

	Zoom to fit the page bounding box height.

.. data:: LinkDestination.FIT_R

	Scroll and zoom to make the specified rectangle visible.

.. data:: LinkDestination.XYZ

	Display with coordinates at the top left zoomed in to the specified magnification factor.

Instance properties
-------------------

.. attribute:: LinkDestination.prototype.chapter

    The chapter within the document.

.. attribute:: LinkDestination.prototype.page

    The page within the document.

.. attribute:: LinkDestination.prototype.type

    Either "Fit", "FitB", "FitH", "FitBH", "FitV", "FitBV", "FitR" or "XYZ".

    The type controls which of the x, y, width, height, and zoom values are used.

.. attribute:: LinkDestination.prototype.x

    The left coordinate. Used for "FitV", "FitBV", "FitR", and "XYZ".

.. attribute:: LinkDestination.prototype.y

    The top coordinate. Used for "FitH", "FitBH", "FitR", and "XYZ".

.. attribute:: LinkDestination.prototype.width

    The width of the zoomed in region. Used for "FitR".

.. attribute:: LinkDestination.prototype.height

    The height of the zoomed in region. Used for "FitR".

.. attribute:: LinkDestination.prototype.zoom

    The zoom factor. Used for "XYZ".

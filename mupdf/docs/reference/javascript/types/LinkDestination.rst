.. default-domain:: js

.. highlight:: javascript

LinkDestination
===============

A link destination points to a location within a document and how a document viewer should show that destination.

It consists of a dictionary with keys for:

``chapter``
    The chapter within the document.

``page``
    The page within the document.

``type``
    Either "Fit", "FitB", "FitH", "FitBH", "FitV", "FitBV", "FitR" or "XYZ", controlling which of the keys below exist.

``x``
    The left coordinate, valid for "FitV", "FitBV", "FitR" and "XYZ".

``y``
    The top coordinate, valid for "FitH", "FitBH", "FitR" and "XYZ".

``width``
    The width of the zoomed in region, valid for "XYZ".

``height``
    The height of the zoomed in region, valid for "XYZ".

``zoom``
    The zoom factor, valid for "XYZ".

# Test document

Test data for `test_engines <path> -model-toc` and the MarkdownModel /
MarkdownToc unit tests. Keep it small and keep the headings: the table of
contents below is what the test prints.

## Headings

Every heading becomes a table-of-contents entry, anchored by its GitHub slug.

### A nested heading

Text under a level 3 heading.

### adc_intr_ctl . TRANS_EN

The slug of this one is `adc_intr_ctl--trans_en`: underscores survive,
punctuation is dropped, each space becomes its own dash.

## Links

A [relative link](test.md) and an [external one](https://www.sumatrapdfreader.org/).

## Code

```js
let n = 1;
```

## A table

| column | meaning |
| ------ | ------- |
| one    | first   |
| two    | second  |
